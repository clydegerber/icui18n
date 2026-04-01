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

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <unicode/resbund.h>
#include <unicode/unistr.h>

#include "export.h"
#include "Localizable.hpp"

namespace icui18n
{

// A (source, key) handle that lazily fetches a localized value from an
// external Localizable.  All accessors always reflect the source's current
// locale — no caching, no event subscription.
//
// Non-owning: the source Localizable must outlive this Resource and any
// Resourceful that holds it (same lifetime contract as LocaleSubscription).
class ICUI18N_EXPORT Resource
{
public:
    Resource(const Localizable& source, std::string key)
        : source_(&source), key_(std::move(key))
    {}

    std::optional<icu::UnicodeString>  getString()  const { return source_->getString(key_);  }
    std::optional<std::int32_t>        getInteger() const { return source_->getInteger(key_); }
    // ICU has no native double entry type; the bundle must store the value as
    // a string.  Returns nullopt if the key is absent or the string cannot be
    // parsed as a double.  Use getString() first to distinguish the two cases.
    std::optional<double>              getDouble()  const { return source_->getDouble(key_);  }
    std::optional<icu::ResourceBundle> getTable()   const { return source_->getTable(key_);   }

    const Localizable& source() const { return *source_; }
    std::string_view   key()    const { return key_; }

private:
    const Localizable* source_;
    std::string        key_;
};

} // namespace icui18n
