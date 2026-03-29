#pragma once

#include <optional>
#include <string_view>

#include <unicode/locid.h>
#include <unicode/resbund.h>
#include <unicode/unistr.h>

#include "LocalizationDelegate.hpp"
#include "../LocaleSubscription.hpp"

namespace icui18n::detail {

// Base class inherited (indirectly) by every Localizable<Self, …> instantiation.
// Owns the LocalizationDelegate and exposes the public API.
//
// Copy construction creates a fresh delegate at the source's locale with an
// empty chain; the Localizable subclass constructors rebuild the chain via
// appendBundle / prependBundle.
//
// Copy assignment is deleted for this version. Move construction and move
// assignment transfer the delegate (including listeners); existing
// LocaleSubscription instances for the moved-from object are silently
// cancelled via the shared alive_ flag in LocalizationDelegate.
class LocalizableBase
{
public:
    // ── Locale ────────────────────────────────────────────────────────────────
    const icu::Locale& getBundleLocale() const { return delegate_.getBundleLocale(); }
    void               setBundleLocale(const icu::Locale& l) { delegate_.setBundleLocale(l); }

    // ── Value access ──────────────────────────────────────────────────────────
    std::optional<icu::UnicodeString>  getString(std::string_view key) const { return delegate_.getString(key); }
    std::optional<std::int32_t>        getInteger(std::string_view key) const { return delegate_.getInteger(key); }
    std::optional<double>              getDouble(std::string_view key) const { return delegate_.getDouble(key); }
    std::optional<icu::ResourceBundle> getTable(std::string_view key) const { return delegate_.getTable(key); }

    // ── Listeners ─────────────────────────────────────────────────────────────
    [[nodiscard("Dropping this immediately cancels the listener")]]
    LocaleSubscription addLocaleListener(LocalizationDelegate::LocaleListener cb)
    {
        return delegate_.addListener(std::move(cb));
    }

protected:
    LocalizableBase() = default;

    // Copy: fresh delegate initialised with source's locale only.
    // No chain, no listeners — subclass constructors rebuild the chain.
    explicit LocalizableBase(const LocalizableBase& other)
        : delegate_(other.getBundleLocale())
    {}

    LocalizableBase(LocalizableBase&&) noexcept            = default;
    LocalizableBase& operator=(LocalizableBase&&) noexcept = default;
    LocalizableBase& operator=(const LocalizableBase&)     = delete;

    LocalizationDelegate& delegate() { return delegate_; }

private:
    LocalizationDelegate delegate_;
};

} // namespace icui18n::detail
