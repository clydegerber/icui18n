#pragma once

#include <concepts>
#include <string_view>

namespace icui18n {

namespace detail { class LocalizableBase; }

// Satisfied when T declares:  static constexpr std::string_view bundle_name
template<typename T>
concept HasBundleName = requires {
    { T::bundle_name } -> std::convertible_to<std::string_view>;
};

// Satisfied when T declares:  static constexpr std::string_view bundle_root
// Derived classes that do not redeclare bundle_root inherit it from their
// base class via normal static-member lookup.
template<typename T>
concept HasBundleRoot = requires {
    { T::bundle_root } -> std::convertible_to<std::string_view>;
};

// Both members required for a type to participate in the Localizable hierarchy.
template<typename T>
concept LocalizableClass = HasBundleName<T> && HasBundleRoot<T>;

// Valid as the Parent parameter of Localizable<Self, Parent>:
// either void (root of a hierarchy) or a type that derives from LocalizableBase.
template<typename T>
concept IsLocalizableBase =
    std::is_void_v<T> ||
    std::derived_from<T, detail::LocalizableBase>;

} // namespace icui18n
