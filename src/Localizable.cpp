#include <stdexcept>
#include <string>
#include <vector>

#include "icui18n/Localizable.hpp"

// Including Localizable.hpp above makes Localizable a complete type.
// LocaleSubscription's method bodies are therefore defined here rather than
// in a deferred header: they call removeListener(), which requires the full
// definition of Localizable.

namespace icui18n
{

// ── LocaleSubscription ────────────────────────────────────────────────────────

void LocaleSubscription::cancel() noexcept
{
    if (alive_ && alive_->load(std::memory_order_relaxed))
    {
        delegate_->removeListener(id_);
    }
}

LocaleSubscription::~LocaleSubscription()
{
    cancel();
}

LocaleSubscription::LocaleSubscription(LocaleSubscription&& other) noexcept
    : delegate_(std::exchange(other.delegate_, nullptr))
    , id_      (other.id_)
    , alive_   (std::move(other.alive_))
{}

LocaleSubscription& LocaleSubscription::operator=(LocaleSubscription&& other) noexcept
{
    if (this != &other)
    {
        cancel();
        delegate_ = std::exchange(other.delegate_, nullptr);
        id_       = other.id_;
        alive_    = std::move(other.alive_);
    }
    return *this;
}

// ── Localizable ───────────────────────────────────────────────────────────────

Localizable::Localizable()
    : locale_(icu::Locale::getDefault())
{}

Localizable::Localizable(const Localizable& other)
    : locale_(other.getBundleLocale())
{}

Localizable::Localizable(Localizable&& other) noexcept
{
    std::unique_lock lock(other.mutex_);
    locale_ = std::move(other.locale_);
    head_   = std::move(other.head_);
    tail_   = std::exchange(other.tail_, nullptr);
    // Mark all outstanding subscriptions for the moved-from object as dead:
    // store false into the shared alive_ flag so that cancel() and the
    // destructor skip removeListener().  The moved-to object receives a
    // fresh alive_ flag (default-initialised in the member declaration) and
    // an empty listener map.  Listeners are intentionally NOT transferred:
    // the subscription handles held by callers have delegate_ pointing to the
    // moved-from object and cannot cancel a listener installed in the moved-to
    // object.  Callers that want to observe locale changes on the moved-to
    // object must call addLocaleListener() on it directly.
    other.alive_->store(false, std::memory_order_relaxed);
}

Localizable::~Localizable()
{
    alive_->store(false, std::memory_order_relaxed);
}

icu::Locale Localizable::getBundleLocale() const
{
    std::shared_lock lock(mutex_);
    return locale_;
}

void Localizable::setBundleLocale(const icu::Locale& next)
{
    // Build the new chain speculatively outside the lock so that a failed
    // ICU load does not leave locale_ updated but the chain broken.
    // reloadChain() reads locale_ and head_ without the lock; this is safe
    // because setBundleLocale() must not be called concurrently with itself
    // (it is a write operation).
    const icu::Locale prev = getBundleLocale();

    // Walk the existing chain and rebuild each node at the new locale.
    // On any failure the speculative bundles are discarded and locale_ is
    // left unchanged, preserving the previous consistent state.
    std::vector<icu::ResourceBundle> newBundles;
    {
        std::shared_lock lock(mutex_);
        newBundles.reserve(8);
        for (const BundleNode* n = head_.get(); n; n = n->parent.get())
        {
            const std::string path = n->bundle_root + "/" + n->bundle_name;
            UErrorCode status = U_ZERO_ERROR;
            icu::ResourceBundle rb(path.c_str(), next, status);
            if (U_FAILURE(status))
            {
                throw std::runtime_error(
                    "icui18n: failed to reload bundle '" + path +
                    "' for locale " + next.getName());
            }
            newBundles.push_back(std::move(rb));
        }
    }

    std::vector<LocaleListener> snapshot;
    {
        std::unique_lock lock(mutex_);
        // Install the new bundles (all loaded successfully).
        std::size_t i = 0;
        for (BundleNode* n = head_.get(); n; n = n->parent.get(), ++i)
        {
            n->bundle = newBundles[i];
        }
        locale_ = next;
        snapshot.reserve(listeners_.size());
        for (auto& [id, cb] : listeners_)
        {
            snapshot.push_back(cb);
        }
    }
    for (auto& cb : snapshot)
    {
        cb(prev, next);
    }
}

std::optional<icu::UnicodeString> Localizable::getString(std::string_view key) const
{
    // key must be null-terminated for ICU; std::string_view is not guaranteed
    // to be null-terminated, so a temporary std::string is used.
    const std::string k{key};
    std::shared_lock lock(mutex_);
    for (const BundleNode* n = head_.get(); n; n = n->parent.get())
    {
        UErrorCode status = U_ZERO_ERROR;
        icu::UnicodeString val = n->bundle.getStringEx(k.c_str(), status);
        if (U_SUCCESS(status))
        {
            return val;
        }
    }
    return std::nullopt;
}

std::optional<std::int32_t> Localizable::getInteger(std::string_view key) const
{
    // key must be null-terminated for ICU; std::string_view is not guaranteed
    // to be null-terminated, so a temporary std::string is used.
    const std::string k{key};
    std::shared_lock lock(mutex_);
    for (const BundleNode* n = head_.get(); n; n = n->parent.get())
    {
        // ResourceBundle::getInt() operates on the current element; keyed
        // access requires a two-step get(key) then getInt().
        UErrorCode status = U_ZERO_ERROR;
        icu::ResourceBundle sub = n->bundle.get(k.c_str(), status);
        if (U_SUCCESS(status))
        {
            UErrorCode status2 = U_ZERO_ERROR;
            std::int32_t val = sub.getInt(status2);
            if (U_SUCCESS(status2))
            {
                return val;
            }
        }
    }
    return std::nullopt;
}

std::optional<double> Localizable::getDouble(std::string_view key) const
{
    // ICU ResourceBundle has no native getDouble; the value is stored as a
    // string entry and converted here.  Returns nullopt when the key is absent
    // AND when the string value cannot be parsed as a double — callers that
    // need to distinguish these cases should call getString() first.
    //
    // key must be null-terminated for ICU; std::string_view is not guaranteed
    // to be null-terminated, so a temporary std::string is used.
    const std::string k{key};
    std::shared_lock lock(mutex_);
    for (const BundleNode* n = head_.get(); n; n = n->parent.get())
    {
        UErrorCode status = U_ZERO_ERROR;
        icu::UnicodeString str = n->bundle.getStringEx(k.c_str(), status);
        if (U_SUCCESS(status))
        {
            std::string utf8;
            str.toUTF8String(utf8);
            try
            {
                return std::stod(utf8);
            }
            catch (const std::invalid_argument&)
            {}
            catch (const std::out_of_range&)
            {}
        }
    }
    return std::nullopt;
}

std::optional<icu::ResourceBundle> Localizable::getTable(std::string_view key) const
{
    // key must be null-terminated for ICU; std::string_view is not guaranteed
    // to be null-terminated, so a temporary std::string is used.
    const std::string k{key};
    std::shared_lock lock(mutex_);
    for (const BundleNode* n = head_.get(); n; n = n->parent.get())
    {
        UErrorCode status = U_ZERO_ERROR;
        icu::ResourceBundle tbl = n->bundle.get(k.c_str(), status);
        if (U_SUCCESS(status) && tbl.getType() == URES_TABLE)
        {
            return tbl;
        }
    }
    return std::nullopt;
}

LocaleSubscription Localizable::addLocaleListener(LocaleListener cb) const
{
    std::unique_lock lock(mutex_);
    const auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
    listeners_[id] = std::move(cb);
    return LocaleSubscription(this, id, alive_);
}

void Localizable::removeListener(ListenerId id) const
{
    std::unique_lock lock(mutex_);
    listeners_.erase(id);
}

void Localizable::appendBundle(std::string_view root, std::string_view name,
                               const icu::Locale& locale)
{
    auto node = makeNode(root, name, locale);
    BundleNode* raw = node.get();
    if (!head_)
    {
        head_ = std::move(node);
        tail_ = raw;
        return;
    }
    tail_->parent = std::move(node);
    tail_ = raw;
}

void Localizable::prependBundle(std::string_view root, std::string_view name,
                                const icu::Locale& locale)
{
    auto node    = makeNode(root, name, locale);
    if (!head_)
    {
        tail_ = node.get();
    }
    node->parent = std::move(head_);
    head_        = std::move(node);
}

auto Localizable::makeNode(std::string_view root, std::string_view name,
                           const icu::Locale& locale) -> std::unique_ptr<BundleNode>
{
    // Construct the ResourceBundle before the node: icu::ResourceBundle
    // has no default constructor, so BundleNode requires it at init time.
    // icu::ResourceBundle has no move constructor in ICU 78; copy is used.
    const std::string path = std::string(root) + "/" + std::string(name);
    UErrorCode status = U_ZERO_ERROR;
    icu::ResourceBundle rb(path.c_str(), locale, status);
    if (U_FAILURE(status))
    {
        throw std::runtime_error(
            "icui18n: failed to load bundle '" + path +
            "' for locale "  + locale.getName());
    }
    return std::make_unique<BundleNode>(root, name, rb);
}

} // namespace icui18n
