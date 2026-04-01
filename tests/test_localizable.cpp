#include <cassert>
#include <iostream>
#include <string>
#include <utility>

#include <unicode/locid.h>
#include <unicode/unistr.h>

#include <icui18n/Localizable.hpp>

// TEST_DATA_DIR is injected by CMake as a string literal.
#ifndef TEST_DATA_DIR
#  error "TEST_DATA_DIR must be defined by the build system"
#endif

// ── Test classes ──────────────────────────────────────────────────────────────

class Service : public icui18n::LocalizableFor<Service>
{
public:
    static constexpr std::string_view bundle_root = TEST_DATA_DIR;
    static constexpr std::string_view bundle_name = "com/example/ServiceBundle";
};

// UserService overrides "greeting"; "farewell" falls back to ServiceBundle.
// bundle_root is NOT redeclared: it is inherited from Service, so both bundles
// live in the same TEST_DATA_DIR root directory.
class UserService : public icui18n::LocalizableFor<UserService, Service>
{
public:
    static constexpr std::string_view bundle_name = "com/example/UserServiceBundle";
};

// ExtService also extends Service but redeclares bundle_root to point to a
// separate directory (TEST_DATA_DIR "/ext"), modelling a cross-library
// subclass whose bundles are stored independently from the parent library.
// Only "greeting" is defined in ExtServiceBundle; "farewell" falls back to
// ServiceBundle in the parent root.
class ExtService : public icui18n::LocalizableFor<ExtService, Service>
{
public:
    static constexpr std::string_view bundle_root = TEST_DATA_DIR "/ext";
    static constexpr std::string_view bundle_name = "com/ext/ExtServiceBundle";
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string str(const icu::UnicodeString& us)
{
    std::string s;
    return us.toUTF8String(s);
}

// ── Tests ─────────────────────────────────────────────────────────────────────

// ── Service (single bundle) ───────────────────────────────────────────────────

void test_service_loads_greeting()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("greeting");
    assert(val.has_value());
    assert(str(*val) == "Hello");
    std::cout << "PASS  test_service_loads_greeting\n";
}

void test_service_loads_farewell()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("farewell");
    assert(val.has_value());
    assert(str(*val) == "Goodbye");
    std::cout << "PASS  test_service_loads_farewell\n";
}

// ── Shared bundle root (UserService) ─────────────────────────────────────────
// UserService does not redeclare bundle_root, so its bundle is loaded from
// the same root directory as Service's bundle.  The two .res files coexist
// in that directory and are each loaded into their own chain node.

// UserService defines its own "greeting" — should take precedence.
void test_userservice_overrides_greeting()
{
    UserService svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("greeting");
    assert(val.has_value());
    assert(str(*val) == "Welcome, user");
    std::cout << "PASS  test_userservice_overrides_greeting\n";
}

// UserService does NOT define "farewell" — should fall back to ServiceBundle.
void test_userservice_inherits_farewell_from_service()
{
    UserService svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("farewell");
    assert(val.has_value());
    assert(str(*val) == "Goodbye");
    std::cout << "PASS  test_userservice_inherits_farewell_from_service\n";
}

void test_locale_change_reloads_bundle()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    assert(str(*svc.getString("greeting")) == "Hello");

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(str(*svc.getString("greeting")) == "Bonjour");
    assert(str(*svc.getString("farewell"))  == "Au revoir");
    std::cout << "PASS  test_locale_change_reloads_bundle\n";
}

// Hierarchy locale change: both levels of the chain must be reloaded.
void test_userservice_locale_change()
{
    UserService svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    assert(str(*svc.getString("greeting")) == "Welcome, user");
    assert(str(*svc.getString("farewell"))  == "Goodbye");

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(str(*svc.getString("greeting")) == "Bienvenue, utilisateur");
    assert(str(*svc.getString("farewell"))  == "Au revoir"); // from ServiceBundle_fr
    std::cout << "PASS  test_userservice_locale_change\n";
}

void test_missing_key_returns_nullopt()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("no_such_key");
    assert(!val.has_value());
    std::cout << "PASS  test_missing_key_returns_nullopt\n";
}

// ── Own bundle root (ExtService) ──────────────────────────────────────────────
// ExtService redeclares bundle_root to TEST_DATA_DIR "/ext", a directory
// separate from Service's TEST_DATA_DIR root.  This models a cross-library
// subclass that ships and installs its bundles independently.

// ExtService's own key is found in its bundle under the separate root.
void test_own_root_loads_from_separate_directory()
{
    ExtService svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("greeting");
    assert(val.has_value());
    assert(str(*val) == "Hello from ExtService");
    std::cout << "PASS  test_own_root_loads_from_separate_directory\n";
}

// A key absent from ExtServiceBundle falls back to ServiceBundle, which lives
// in a different root directory — fallback crosses bundle root boundaries.
void test_own_root_falls_back_to_parent_bundle()
{
    ExtService svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    auto val = svc.getString("farewell");
    assert(val.has_value());
    assert(str(*val) == "Goodbye");
    std::cout << "PASS  test_own_root_falls_back_to_parent_bundle\n";
}

// A locale change must reload both the child's bundle (separate root) and
// the parent's bundle (parent root) so that all keys reflect the new locale.
void test_own_root_locale_change_reloads_both_roots()
{
    ExtService svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    assert(str(*svc.getString("greeting")) == "Hello from ExtService");
    assert(str(*svc.getString("farewell"))  == "Goodbye");

    svc.setBundleLocale(icu::Locale::getFrench());
    // Own bundle (separate root) reloaded at French locale.
    assert(str(*svc.getString("greeting")) == "Bonjour d ExtService");
    // Parent bundle (parent root) also reloaded; fallback key is now French.
    assert(str(*svc.getString("farewell"))  == "Au revoir");
    std::cout << "PASS  test_own_root_locale_change_reloads_both_roots\n";
}

// ── Locale listeners / copy / move ───────────────────────────────────────────

void test_locale_listener_fires_on_change()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    int        fire_count   = 0;
    icu::Locale observed_next = icu::Locale::getEnglish();

    auto sub = svc.addLocaleListener(
        [&](const icu::Locale& /*prev*/, const icu::Locale& next)
        {
            ++fire_count;
            observed_next = next;
        });

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(fire_count == 1);
    assert(observed_next == icu::Locale::getFrench());
    std::cout << "PASS  test_locale_listener_fires_on_change\n";
}

void test_subscription_cancels_on_destruction()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    int fire_count = 0;

    {
        auto sub = svc.addLocaleListener(
            [&](const icu::Locale&, const icu::Locale&) { ++fire_count; });
        svc.setBundleLocale(icu::Locale::getFrench());
        assert(fire_count == 1);
    } // sub destroyed here — listener deregistered

    svc.setBundleLocale(icu::Locale::getEnglish());
    assert(fire_count == 1); // must not have fired again
    std::cout << "PASS  test_subscription_cancels_on_destruction\n";
}

void test_copy_has_same_locale_and_bundle()
{
    Service original;
    original.setBundleLocale(icu::Locale::getFrench());

    Service copy{original};

    assert(copy.getBundleLocale() == icu::Locale::getFrench());
    assert(str(*copy.getString("greeting")) == "Bonjour");
    std::cout << "PASS  test_copy_has_same_locale_and_bundle\n";
}

void test_copy_does_not_share_listeners()
{
    Service original;
    original.setBundleLocale(icu::Locale::getEnglish());

    int original_fires = 0;
    auto sub = original.addLocaleListener(
        [&](const icu::Locale&, const icu::Locale&) { ++original_fires; });

    Service copy{original};
    copy.setBundleLocale(icu::Locale::getFrench()); // must NOT fire original's listener
    assert(original_fires == 0);

    original.setBundleLocale(icu::Locale::getFrench()); // MUST fire original's listener
    assert(original_fires == 1);
    std::cout << "PASS  test_copy_does_not_share_listeners\n";
}

void test_copy_is_independent()
{
    Service original;
    original.setBundleLocale(icu::Locale::getEnglish());

    Service copy{original};
    copy.setBundleLocale(icu::Locale::getFrench());

    // original must be unaffected
    assert(original.getBundleLocale() == icu::Locale::getEnglish());
    assert(str(*original.getString("greeting")) == "Hello");
    std::cout << "PASS  test_copy_is_independent\n";
}

// A subscription taken before moving the source must NOT fire on the moved-to
// object: the alive_ flag is replaced with a dead one in the moved-from object,
// so cancel() skips removeListener() and the callback is never called.
void test_move_source_subscription_does_not_fire()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());
    int fire_count = 0;
    auto sub = svc.addLocaleListener(
        [&](const icu::Locale&, const icu::Locale&) { ++fire_count; });

    Service moved = std::move(svc);

    // Changing locale on the moved-to object must NOT trigger the old sub.
    moved.setBundleLocale(icu::Locale::getFrench());
    assert(fire_count == 0);

    // Destroying the old sub must not crash (alive_ is dead, removeListener
    // is skipped).
    sub = {}; // explicit reset; destructor also safe
    std::cout << "PASS  test_move_source_subscription_does_not_fire\n";
}

// A subscription taken before moving the source must be safe to destroy after
// the move: cancel() checks alive_ (now false) and skips removeListener(),
// avoiding a call into the dead moved-from object.
void test_move_source_subscription_safe_to_destroy()
{
    auto sub = []() -> icui18n::LocaleSubscription
    {
        Service svc;
        auto s = svc.addLocaleListener([](const icu::Locale&, const icu::Locale&) {});
        Service moved = std::move(svc);
        // svc is now the moved-from object (alive_ = false).
        // Return the subscription — it outlives svc.
        return s;
    }();
    // sub's destructor runs here: must not crash.
    std::cout << "PASS  test_move_source_subscription_safe_to_destroy\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main()
{
    test_service_loads_greeting();
    test_service_loads_farewell();
    test_userservice_overrides_greeting();
    test_userservice_inherits_farewell_from_service();
    test_locale_change_reloads_bundle();
    test_userservice_locale_change();
    test_missing_key_returns_nullopt();
    test_own_root_loads_from_separate_directory();
    test_own_root_falls_back_to_parent_bundle();
    test_own_root_locale_change_reloads_both_roots();
    test_locale_listener_fires_on_change();
    test_subscription_cancels_on_destruction();
    test_copy_has_same_locale_and_bundle();
    test_copy_does_not_share_listeners();
    test_copy_is_independent();
    test_move_source_subscription_does_not_fire();
    test_move_source_subscription_safe_to_destroy();

    std::cout << "\nAll tests passed.\n";
    return 0;
}
