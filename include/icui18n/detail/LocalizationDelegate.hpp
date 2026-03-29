#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <unicode/locid.h>
#include <unicode/resbund.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

#include "BundleNode.hpp"
#include "../LocaleSubscription.hpp"

namespace icui18n::detail {

// Core delegation class embedded in every Localizable object.
//
// Responsibilities:
//   - Owns the BundleNode chain (most-derived → hierarchy root).
//   - Manages the object's current icu::Locale under a shared_mutex:
//       reads (getBundleLocale, getString, …) take a shared lock;
//       writes (setBundleLocale) take an exclusive lock.
//   - Rebuilds the chain in-place on locale change.
//   - Manages locale-change listeners; fires them outside the lock to prevent
//     deadlock when a listener calls back into the same Localizable.
//
// Thread safety: all public methods are safe to call concurrently after
// construction. appendBundle / prependBundle are called only from Localizable
// constructors, which are inherently single-threaded.
class LocalizationDelegate
{
public:
    using ListenerId     = std::uint64_t;
    using LocaleListener = std::function<void(const icu::Locale& prev,
                                              const icu::Locale& next)>;

    // Default: locale initialised to icu::Locale::getDefault().
    LocalizationDelegate()
        : locale_(icu::Locale::getDefault())
    {}

    // Locale-copy constructor: used by LocalizableBase's copy constructor to
    // create a fresh delegate at the source's locale. The chain is empty; the
    // Localizable subclass constructors rebuild it via appendBundle /
    // prependBundle.
    explicit LocalizationDelegate(const icu::Locale& locale)
        : locale_(locale)
    {}

    ~LocalizationDelegate()
    {
        alive_->store(false, std::memory_order_relaxed);
    }

    // Move: transfers chain, listeners, and the alive_ handle.
    // Gives the moved-from delegate a new dead alive_ so any outstanding
    // LocaleSubscription instances for the old object silently cancel.
    LocalizationDelegate(LocalizationDelegate&& other) noexcept
    {
        std::unique_lock lock(other.mutex_);
        locale_    = std::move(other.locale_);
        head_      = std::move(other.head_);
        listeners_ = std::move(other.listeners_);
        alive_     = std::move(other.alive_);
        next_id_.store(other.next_id_.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
        other.alive_ = std::make_shared<std::atomic<bool>>(false);
    }

    LocalizationDelegate& operator=(LocalizationDelegate&&)      = delete;
    LocalizationDelegate(const LocalizationDelegate&)            = delete;
    LocalizationDelegate& operator=(const LocalizationDelegate&) = delete;

    // ── Chain construction ────────────────────────────────────────────────────
    // Called only from Localizable constructors; no locking required.

    // Appends a new node at the tail of the chain (used by the root
    // Localizable<Self, void> constructor).
    void appendBundle(std::string_view root, std::string_view name,
                      const icu::Locale& locale)
    {
        auto node = makeNode(root, name, locale);
        if (!head_)
        {
            head_ = std::move(node);
            return;
        }
        BundleNode* tail = head_.get();
        while (tail->parent) tail = tail->parent.get();
        tail->parent = std::move(node);
    }

    // Prepends a new node at the head of the chain (used by the extension
    // Localizable<Self, Parent> constructor).
    void prependBundle(std::string_view root, std::string_view name,
                       const icu::Locale& locale)
    {
        auto node    = makeNode(root, name, locale);
        node->parent = std::move(head_);
        head_        = std::move(node);
    }

    // ── Locale ────────────────────────────────────────────────────────────────

    const icu::Locale& getBundleLocale() const
    {
        std::shared_lock lock(mutex_);
        return locale_;
    }

    // Rebuilds the entire chain at the new locale, then fires listeners
    // outside the lock.
    void setBundleLocale(const icu::Locale& next)
    {
        std::vector<LocaleListener> snapshot;
        icu::Locale prev;
        {
            std::unique_lock lock(mutex_);
            prev    = locale_;
            locale_ = next;
            reloadChain();
            snapshot.reserve(listeners_.size());
            for (auto& [id, cb] : listeners_) snapshot.push_back(cb);
        }
        for (auto& cb : snapshot) cb(prev, next);
    }

    // ── Value access ──────────────────────────────────────────────────────────
    // Each method walks the chain from head (most-derived) to tail (root).
    // ICU handles per-bundle locale fallback (e.g. en_US → en → root)
    // automatically within each node.

    std::optional<icu::UnicodeString> getString(std::string_view key) const
    {
        std::shared_lock lock(mutex_);
        for (const BundleNode* n = head_.get(); n; n = n->parent.get())
        {
            UErrorCode status = U_ZERO_ERROR;
            icu::UnicodeString val = n->bundle.getStringEx(key.data(), status);
            if (U_SUCCESS(status)) return val;
        }
        return std::nullopt;
    }

    std::optional<std::int32_t> getInteger(std::string_view key) const
    {
        std::shared_lock lock(mutex_);
        for (const BundleNode* n = head_.get(); n; n = n->parent.get())
        {
            UErrorCode status = U_ZERO_ERROR;
            std::int32_t val = n->bundle.getInt(key.data(), status);
            if (U_SUCCESS(status)) return val;
        }
        return std::nullopt;
    }

    std::optional<double> getDouble(std::string_view key) const
    {
        std::shared_lock lock(mutex_);
        for (const BundleNode* n = head_.get(); n; n = n->parent.get())
        {
            UErrorCode status = U_ZERO_ERROR;
            // ICU ResourceBundle has no native getDouble; retrieve as string
            // and convert. Bundles that need floating-point values should
            // store them as string entries.
            icu::UnicodeString str = n->bundle.getStringEx(key.data(), status);
            if (U_SUCCESS(status))
            {
                std::string utf8;
                str.toUTF8String(utf8);
                try   { return std::stod(utf8); }
                catch (...) {}
            }
        }
        return std::nullopt;
    }

    std::optional<icu::ResourceBundle> getTable(std::string_view key) const
    {
        std::shared_lock lock(mutex_);
        for (const BundleNode* n = head_.get(); n; n = n->parent.get())
        {
            UErrorCode status = U_ZERO_ERROR;
            icu::ResourceBundle tbl = n->bundle.get(key.data(), status);
            if (U_SUCCESS(status) && tbl.getType() == URES_TABLE)
                return tbl;
        }
        return std::nullopt;
    }

    // ── Listeners ─────────────────────────────────────────────────────────────

    [[nodiscard("Dropping this immediately cancels the listener")]]
    LocaleSubscription addListener(LocaleListener cb)
    {
        std::unique_lock lock(mutex_);
        const auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
        listeners_[id] = std::move(cb);
        return LocaleSubscription(this, id, alive_);
    }

    void removeListener(ListenerId id)
    {
        std::unique_lock lock(mutex_);
        listeners_.erase(id);
    }

private:
    static std::unique_ptr<BundleNode>
    makeNode(std::string_view root, std::string_view name,
             const icu::Locale& locale)
    {
        auto node         = std::make_unique<BundleNode>();
        node->bundle_root = std::string(root);
        node->bundle_name = std::string(name);
        const std::string path = node->bundle_root + "/" + node->bundle_name;
        UErrorCode status = U_ZERO_ERROR;
        node->bundle = icu::ResourceBundle(path.c_str(), locale, status);
        if (U_FAILURE(status))
            throw std::runtime_error(
                "icui18n: failed to load bundle '" + path +
                "' for locale "  + locale.getName());
        return node;
    }

    // Rebuilds every node in the chain at the current locale_.
    // Called under exclusive lock from setBundleLocale.
    // Throws std::runtime_error if any node fails to reload — the caller
    // should treat this as a fatal misconfiguration rather than attempt
    // recovery from a partially-rebuilt chain.
    void reloadChain()
    {
        for (BundleNode* n = head_.get(); n; n = n->parent.get())
        {
            const std::string path = n->bundle_root + "/" + n->bundle_name;
            UErrorCode status = U_ZERO_ERROR;
            n->bundle = icu::ResourceBundle(path.c_str(), locale_, status);
            if (U_FAILURE(status))
                throw std::runtime_error(
                    "icui18n: failed to reload bundle '" + path +
                    "' for locale " + locale_.getName());
        }
    }

    std::unique_ptr<BundleNode>  head_;
    icu::Locale                  locale_;

    mutable std::shared_mutex    mutex_;

    std::unordered_map<ListenerId, LocaleListener> listeners_;
    std::atomic<ListenerId>      next_id_{0};

    // Shared with every LocaleSubscription this delegate has issued.
    // Set to false in the destructor and on move-from so outstanding
    // subscriptions can detect that the delegate is no longer live.
    std::shared_ptr<std::atomic<bool>> alive_{
        std::make_shared<std::atomic<bool>>(true)};
};

} // namespace icui18n::detail
