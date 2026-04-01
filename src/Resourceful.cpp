// Copyright 2026 Clyde Gerber
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
