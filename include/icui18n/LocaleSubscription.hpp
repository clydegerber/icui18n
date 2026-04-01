#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include "export.h"

namespace icui18n
{

class Localizable;

// RAII handle returned by Localizable::addLocaleListener.
// The listener is automatically deregistered when this object is destroyed
// or move-assigned over.
//
// Non-copyable; move-only.
//
// Lifetime: the subscription must not outlive the Localizable that issued it.
// If the issuing Localizable is moved, outstanding subscriptions become
// no-ops: the move sets the shared alive_ flag to false, so cancel() and
// the destructor skip removeListener().  Listeners are not transferred to
// the moved-to object; callers must re-subscribe on the moved-to object.
//
// cancel(), the destructor, and the move operations are defined in
// Localizable.cpp because their bodies require Localizable to be a complete
// type (they call removeListener()).
class ICUI18N_EXPORT LocaleSubscription
{
public:
    // Default-constructed subscription is a no-op: cancel() and the
    // destructor do nothing.  Used as a null state by Resourceful.
    LocaleSubscription() = default;

    ~LocaleSubscription();

    LocaleSubscription(LocaleSubscription&& other) noexcept;
    LocaleSubscription& operator=(LocaleSubscription&& other) noexcept;

    LocaleSubscription(const LocaleSubscription&)            = delete;
    LocaleSubscription& operator=(const LocaleSubscription&) = delete;

private:
    friend class Localizable;

    LocaleSubscription(const Localizable*                  delegate,
                       std::uint64_t                       id,
                       std::shared_ptr<std::atomic<bool>> alive)
        : delegate_(delegate), id_(id), alive_(std::move(alive))
    {}

    void cancel() noexcept;

    const Localizable*                 delegate_{nullptr};
    std::uint64_t                      id_      {0};
    std::shared_ptr<std::atomic<bool>> alive_;
};

} // namespace icui18n
