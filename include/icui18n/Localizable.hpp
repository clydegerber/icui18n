#pragma once

#include "Concepts.hpp"
#include "detail/LocalizableBase.hpp"

namespace icui18n {

// ── Forward declaration ───────────────────────────────────────────────────────

template<typename Self, typename Parent = void>
    requires LocalizableClass<Self> && IsLocalizableBase<Parent>
class Localizable;


// ── Root specialisation ───────────────────────────────────────────────────────
//
// Inherit from this when Self is the root of a localizable hierarchy (no
// localizable superclass). Inherits from detail::LocalizableBase, which owns
// the LocalizationDelegate.
//
// Example:
//   class Service : public icui18n::Localizable<Service>
//   {
//       static constexpr std::string_view bundle_root = "/usr/lib/acme/i18n";
//       static constexpr std::string_view bundle_name = "com/example/ServiceBundle";
//   };

template<typename Self>
    requires LocalizableClass<Self>
class Localizable<Self, void> : public detail::LocalizableBase
{
public:
    Localizable()
    {
        delegate().appendBundle(Self::bundle_root,
                                Self::bundle_name,
                                getBundleLocale());
    }

    // Copy: LocalizableBase copy ctor gives a fresh delegate at source's locale;
    // we then load Self's bundle at that locale.
    Localizable(const Localizable& other) : detail::LocalizableBase(other)
    {
        delegate().appendBundle(Self::bundle_root,
                                Self::bundle_name,
                                getBundleLocale());
    }

    Localizable(Localizable&&) noexcept            = default;
    Localizable& operator=(Localizable&&) noexcept = default;
    Localizable& operator=(const Localizable&)     = delete;
};


// ── Extension specialisation ──────────────────────────────────────────────────
//
// Inherit from this when Self has a localizable superclass (Parent).
// Inherits from Parent (which ultimately derives from detail::LocalizableBase),
// then prepends Self's bundle at the head of the shared chain.
//
// Example:
//   class UserService : public icui18n::Localizable<UserService, Service>
//   {
//       // bundle_root inherited from Service
//       static constexpr std::string_view bundle_name = "com/example/UserServiceBundle";
//   };

template<typename Self, typename Parent>
    requires LocalizableClass<Self> && IsLocalizableBase<Parent>
class Localizable : public Parent
{
public:
    Localizable()
    {
        this->delegate().prependBundle(Self::bundle_root,
                                       Self::bundle_name,
                                       this->getBundleLocale());
    }

    // Copy: Parent's copy ctor rebuilds Parent's portion of the chain
    // (recursively); we then prepend Self's bundle at the head.
    Localizable(const Localizable& other) : Parent(other)
    {
        this->delegate().prependBundle(Self::bundle_root,
                                       Self::bundle_name,
                                       this->getBundleLocale());
    }

    Localizable(Localizable&&) noexcept            = default;
    Localizable& operator=(Localizable&&) noexcept = default;
    Localizable& operator=(const Localizable&)     = delete;
};

} // namespace icui18n
