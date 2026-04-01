#pragma once

#include <functional>
#include <optional>

#include <unicode/locid.h>

#include "export.h"
#include "LocaleSubscription.hpp"
#include "Resource.hpp"

namespace icui18n
{

// Mixin base class for objects that display localized content sourced from an
// external Localizable rather than owning their own bundle.
//
// setResource() binds a (source, key) pair and an optional LocaleChangeCallback.
// The callback fires whenever the source Localizable changes locale, giving the
// owner a hook to refresh any derived state (re-render a widget, reformat a
// message, etc.).  The subscription is cancelled automatically when a new
// resource is set, clearResource() is called, or this object is destroyed.
//
// Move-only: the locale-change subscription cannot be shared.  Copy is deleted
// to prevent the common mistake of copying a lambda that captures the original
// object's `this`.  Derived classes that need copy semantics should define their
// own copy constructor and call setResource() with a fresh callback.
class ICUI18N_EXPORT Resourceful
{
public:
    using LocaleChangeCallback =
        std::function<void(const icu::Locale& prev, const icu::Locale& next)>;

    // Bind a resource and an optional locale-change callback.
    // Cancels the subscription to any previously-bound source.
    void setResource(Resource resource, LocaleChangeCallback on_change = {});

    // Cancel the current subscription and clear the resource.
    void clearResource();

    const std::optional<Resource>& getResource() const { return resource_; }

protected:
    Resourceful()  = default;
    ~Resourceful() = default;

    Resourceful(Resourceful&&) noexcept            = default;
    Resourceful& operator=(Resourceful&&) noexcept = default;

    Resourceful(const Resourceful&)            = delete;
    Resourceful& operator=(const Resourceful&) = delete;

private:
    std::optional<Resource> resource_;
    LocaleSubscription      subscription_;  // default: no-op
    LocaleChangeCallback    on_change_;
};

} // namespace icui18n
