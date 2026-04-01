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

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <unicode/locid.h>
#include <unicode/resbund.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

#include "export.h"
#include "LocaleSubscription.hpp"
#include "Concepts.hpp"

namespace icui18n
{

// Base class inherited (indirectly) by every LocalizableFor<Self, …> instantiation.
// Owns the full localization mechanism directly — bundle chain, locale, listeners.
//
// Responsibilities:
//   - Owns the BundleNode chain (most-derived → hierarchy root).
//   - Manages the object's current icu::Locale under a shared_mutex:
//       reads (getBundleLocale, getString, …) take a shared lock;
//       writes (setBundleLocale) take an exclusive lock.
//   - Rebuilds the chain in-place on locale change.
//   - Manages locale-change listeners; fires them outside the lock to prevent
//     deadlock when a listener calls back into the same Localizable.
//
// Thread safety: all public methods are safe to call concurrently after
// construction. appendBundle / prependBundle are called only from LocalizableFor
// constructors, which are inherently single-threaded.
//
// Copy construction creates a fresh object at the source's locale with an
// empty chain and no listeners; LocalizableFor subclass constructors rebuild
// the chain via appendBundle / prependBundle.
// Move construction transfers the chain, listeners, and the alive_ handle.
class ICUI18N_EXPORT Localizable
{
public:
    using ListenerId     = std::uint64_t;
    using LocaleListener = std::function<void(const icu::Locale& prev,
                                              const icu::Locale& next)>;

    // ── Locale ────────────────────────────────────────────────────────────────

    // Returns a copy of the current locale captured under the shared lock.
    // Returning by reference would release the lock before the caller could
    // safely use the value.
    icu::Locale getBundleLocale() const;

    // Rebuilds the entire chain at the new locale, then fires listeners
    // outside the lock.
    void setBundleLocale(const icu::Locale& next);

    // ── Value access ──────────────────────────────────────────────────────────
    // Each method walks the chain from head (most-derived) to tail (root).
    // ICU handles per-bundle locale fallback (e.g. en_US → en → root)
    // automatically within each node.

    std::optional<icu::UnicodeString>  getString(std::string_view key) const;
    std::optional<std::int32_t>        getInteger(std::string_view key) const;
    std::optional<double>              getDouble(std::string_view key) const;
    std::optional<icu::ResourceBundle> getTable(std::string_view key) const;

    // ── Listeners ─────────────────────────────────────────────────────────────

    [[nodiscard("Dropping this immediately cancels the listener")]]
    LocaleSubscription addLocaleListener(LocaleListener cb) const;

    void removeListener(ListenerId id) const;

protected:
    Localizable();

    // Locale-copy constructor: used by LocalizableFor's copy constructor to
    // create a fresh base at the source's locale. Chain and listeners are
    // empty; LocalizableFor subclass constructors rebuild the chain.
    explicit Localizable(const Localizable& other);

    // Move: transfers the bundle chain and locale only.  Listeners are NOT
    // transferred — subscription handles held by callers have delegate_
    // pointing to the moved-from object and cannot cancel a listener in the
    // moved-to object.  The original alive_ flag is set to false so that
    // outstanding subscriptions become no-ops (cancel() skips removeListener).
    // The moved-to object starts with an empty listener map and a fresh alive_
    // flag; callers must re-subscribe on the moved-to object if needed.
    Localizable(Localizable&&) noexcept;

    ~Localizable();

    // Move assignment is deleted: reassigning while listeners and subscriptions
    // are in flight would require atomically transferring the alive_ handle and
    // re-pointing every outstanding LocaleSubscription, which cannot be done
    // safely without exposing the internal subscription list.
    Localizable& operator=(Localizable&&)      = delete;
    Localizable& operator=(const Localizable&) = delete;

    // ── Chain construction ─────────────────────────────────────────────────
    // Called only from LocalizableFor constructors; no locking required.

    // Appends a new node at the tail of the chain (used by the root
    // LocalizableFor<Self, void> constructor).
    void appendBundle(std::string_view root, std::string_view name,
                      const icu::Locale& locale);

    // Prepends a new node at the head of the chain (used by the extension
    // LocalizableFor<Self, Parent> constructor).
    void prependBundle(std::string_view root, std::string_view name,
                       const icu::Locale& locale);

private:
    friend class LocaleSubscription;

    // One node in the per-object bundle chain.
    // The chain runs most-derived → ... → hierarchy root, mirroring the class
    // hierarchy. Key lookup walks head → parent; ICU handles locale fallback
    // (e.g. en_US → en → root) within each individual node automatically.
    // Both bundle_root and bundle_name are retained so the chain can be rebuilt
    // in-place when the locale changes.
    //
    // icu::ResourceBundle has no default constructor (deleted in ICU), so
    // BundleNode requires the ResourceBundle to be passed at construction.
    // icu::ResourceBundle has no move constructor in ICU 78; copy is used.
    struct BundleNode
    {
        std::string         bundle_root;
        std::string         bundle_name;
        icu::ResourceBundle bundle;
        std::unique_ptr<BundleNode> parent;

        BundleNode(std::string_view root, std::string_view name,
                   const icu::ResourceBundle& rb)
            : bundle_root(root)
            , bundle_name(name)
            , bundle(rb)
        {}
    };

    static std::unique_ptr<BundleNode> makeNode(std::string_view root,
                                                std::string_view name,
                                                const icu::Locale& locale);
    void reloadChain();

    std::unique_ptr<BundleNode>  head_;
    BundleNode*                  tail_{nullptr};  // O(1) appendBundle
    icu::Locale                  locale_;

    mutable std::shared_mutex    mutex_;

    // Listener state is mutable: adding/removing a listener does not affect the
    // logical value of the Localizable (its locale and bundle content), so
    // addLocaleListener / removeListener are const-qualified.
    mutable std::unordered_map<ListenerId, LocaleListener> listeners_;
    mutable std::atomic<ListenerId>                        next_id_{0};

    // Shared with every LocaleSubscription this object has issued.
    // Set to false in the destructor and on move-from so outstanding
    // subscriptions can detect that the object is no longer live.
    mutable std::shared_ptr<std::atomic<bool>> alive_{
        std::make_shared<std::atomic<bool>>(true)};
};


// ── LocalizableFor ────────────────────────────────────────────────────────────
//
// CRTP mixin that wires a concrete class into the Localizable bundle chain.
//
// LocalizableClass<Self> cannot be checked at class-template instantiation
// time because Self is an incomplete type when the base-class list is
// processed — the class body (including bundle_name / bundle_root) has not
// been parsed yet.  It is instead enforced via static_assert in each
// constructor, which fires when the constructor is instantiated (i.e. when
// the user first constructs an object of type Self) at which point Self is
// always complete.

template<typename Self, typename Parent = void>
class LocalizableFor;


// ── Root specialisation ───────────────────────────────────────────────────────
//
// Inherit from this when Self is the root of a localizable hierarchy.
//
// bundle_root and bundle_name must be public: concept checks run outside the
// class scope and cannot access private or protected members.
//
// Example:
//   class Service : public icui18n::LocalizableFor<Service>
//   {
//   public:
//       static constexpr std::string_view bundle_root = "/usr/lib/acme/i18n";
//       static constexpr std::string_view bundle_name = "com/example/ServiceBundle";
//   };

template<typename Self>
class LocalizableFor<Self, void> : public Localizable
{
public:
    LocalizableFor()
    {
        static_assert(LocalizableClass<Self>,
            "icui18n: Self must declare "
            "public static constexpr std::string_view bundle_name "
            "and bundle_root");
        appendBundle(Self::bundle_root, Self::bundle_name, getBundleLocale());
    }

    LocalizableFor(const LocalizableFor& other) : Localizable(other)
    {
        static_assert(LocalizableClass<Self>,
            "icui18n: Self must declare "
            "public static constexpr std::string_view bundle_name "
            "and bundle_root");
        appendBundle(Self::bundle_root, Self::bundle_name, getBundleLocale());
    }

    // After a move the source's alive_ flag is false: outstanding subscriptions
    // become no-ops.  Listeners are not transferred.  Re-subscribe if needed.
    LocalizableFor(LocalizableFor&&) noexcept        = default;
    LocalizableFor& operator=(LocalizableFor&&)      = delete;
    LocalizableFor& operator=(const LocalizableFor&) = delete;
};


// ── Extension specialisation ──────────────────────────────────────────────────
//
// Inherit from this when Self has a localizable superclass (Parent).
// IsLocalizable<Parent> is checked on the class template — Parent is
// always already complete at the point LocalizableFor<Self, Parent> is
// instantiated.
//
// bundle_root for Self's own bundle is resolved via static-member lookup
// starting at Self, so it can be:
//
//   a) Omitted — Self inherits Parent::bundle_root.  Use this when Self
//      lives in the same library as Parent and shares its bundle directory.
//
//      class UserService : public icui18n::LocalizableFor<UserService, Service>
//      {
//      public:
//          // bundle_root inherited from Service
//          static constexpr std::string_view bundle_name = "com/example/UserServiceBundle";
//      };
//
//   b) Redeclared — Self supplies its own bundle_root.  Use this when Self
//      lives in a different library and must store its bundles under its own
//      install path, independent of Parent's library.
//
//      class ExtService : public icui18n::LocalizableFor<ExtService, Service>
//      {
//      public:
//          static constexpr std::string_view bundle_root = "/usr/lib/ext/i18n";
//          static constexpr std::string_view bundle_name = "com/ext/ExtServiceBundle";
//      };

template<typename Self, typename Parent>
    requires IsLocalizable<Parent>
class LocalizableFor<Self, Parent> : public Parent
{
public:
    LocalizableFor()
    {
        static_assert(LocalizableClass<Self>,
            "icui18n: Self must declare "
            "public static constexpr std::string_view bundle_name "
            "and bundle_root");
        this->prependBundle(Self::bundle_root,
                            Self::bundle_name,
                            this->getBundleLocale());
    }

    LocalizableFor(const LocalizableFor& other) : Parent(other)
    {
        static_assert(LocalizableClass<Self>,
            "icui18n: Self must declare "
            "public static constexpr std::string_view bundle_name "
            "and bundle_root");
        this->prependBundle(Self::bundle_root,
                            Self::bundle_name,
                            this->getBundleLocale());
    }

    // After a move the source's alive_ flag is false: outstanding subscriptions
    // become no-ops.  Listeners are not transferred.  Re-subscribe if needed.
    LocalizableFor(LocalizableFor&&) noexcept        = default;
    LocalizableFor& operator=(LocalizableFor&&)      = delete;
    LocalizableFor& operator=(const LocalizableFor&) = delete;
};

} // namespace icui18n
