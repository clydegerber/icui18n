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

#include <concepts>
#include <string_view>

namespace icui18n
{

class Localizable;

// Satisfied when T declares:  static constexpr std::string_view bundle_name
template<typename T>
concept HasBundleName = requires
{
    { T::bundle_name } -> std::convertible_to<std::string_view>;
};

// Satisfied when T declares:  static constexpr std::string_view bundle_root
//
// bundle_root controls where ICU looks for the compiled .res files for T's
// own bundle.  Each class in a LocalizableFor hierarchy may declare its own
// bundle_root independently; classes that do not redeclare it inherit the
// value from their base class via normal static-member lookup.
//
// Cross-library subclasses should always redeclare bundle_root so their
// bundles are stored under their own library's install path rather than the
// parent library's path.
template<typename T>
concept HasBundleRoot = requires
{
    { T::bundle_root } -> std::convertible_to<std::string_view>;
};

// Both members required for a type to participate in the Localizable hierarchy.
template<typename T>
concept LocalizableClass = HasBundleName<T> && HasBundleRoot<T>;

// Valid as the Parent parameter of LocalizableFor<Self, Parent>:
// either void (root of a hierarchy) or a type that derives from Localizable.
template<typename T>
concept IsLocalizable =
    std::is_void_v<T> ||
    std::derived_from<T, Localizable>;

} // namespace icui18n
