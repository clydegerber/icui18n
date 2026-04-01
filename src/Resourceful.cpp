#include "icui18n/Resourceful.hpp"

namespace icui18n
{

void Resourceful::setResource(Resource resource, LocaleChangeCallback on_change)
{
    // Cancel the old subscription before overwriting any fields so that the
    // old callback is not called after setResource() returns.
    subscription_ = LocaleSubscription{};
    resource_     = std::move(resource);
    on_change_    = std::move(on_change);
    if (on_change_)
    {
        subscription_ = resource_->source().addLocaleListener(on_change_);
    }
}

void Resourceful::clearResource()
{
    subscription_ = LocaleSubscription{};
    resource_.reset();
    on_change_    = {};
}

} // namespace icui18n
