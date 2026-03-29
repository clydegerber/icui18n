#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

namespace icui18n::detail { class LocalizationDelegate; }

namespace icui18n {

// RAII handle returned by Localizable::addLocaleListener.
// The listener is automatically deregistered when this object is destroyed
// or move-assigned over.
//
// Non-copyable; move-only.
//
// Lifetime: the subscription must not outlive the Localizable that issued it.
// If the issuing Localizable is moved, outstanding subscriptions for the
// moved-from object are silently cancelled via the shared alive_ flag.
class LocaleSubscription
{
public:
    ~LocaleSubscription()
    {
        cancel();
    }

    LocaleSubscription(LocaleSubscription&& other) noexcept
        : delegate_(std::exchange(other.delegate_, nullptr))
        , id_      (other.id_)
        , alive_   (std::move(other.alive_))
    {}

    LocaleSubscription& operator=(LocaleSubscription&& other) noexcept
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

    LocaleSubscription(const LocaleSubscription&)            = delete;
    LocaleSubscription& operator=(const LocaleSubscription&) = delete;

private:
    friend class detail::LocalizationDelegate;

    using ListenerId = std::uint64_t;

    LocaleSubscription(detail::LocalizationDelegate*         delegate,
                       ListenerId                             id,
                       std::shared_ptr<std::atomic<bool>>    alive)
        : delegate_(delegate), id_(id), alive_(std::move(alive))
    {}

    void cancel() noexcept
    {
        // alive_ is null after a move; the load guards against a delegate
        // that has already been destroyed or moved-from.
        if (alive_ && alive_->load(std::memory_order_relaxed))
            delegate_->removeListener(id_);
    }

    detail::LocalizationDelegate*      delegate_{nullptr};
    ListenerId                         id_      {0};
    std::shared_ptr<std::atomic<bool>> alive_;
};

} // namespace icui18n
