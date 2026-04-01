#include <cassert>
#include <iostream>
#include <string>
#include <utility>

#include <unicode/locid.h>
#include <unicode/resbund.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

#include <icui18n/Localizable.hpp>
#include <icui18n/Resource.hpp>
#include <icui18n/Resourceful.hpp>

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

// Minimal Resourceful subclass used as a test double.
class Widget : public icui18n::Resourceful
{
public:
    Widget()           = default;
    Widget(Widget&&)   = default;
    Widget& operator=(Widget&&) = default;
};

// Resourceful subclass whose resource is a table entry rather than a plain string.
// Each call to getMessage() reads directly from the current table, so locale
// changes are reflected automatically without a callback.
class ErrorPanel : public icui18n::Resourceful
{
public:
    ErrorPanel()              = default;
    ErrorPanel(ErrorPanel&&)  = default;
    ErrorPanel& operator=(ErrorPanel&&) = default;

    std::optional<icu::UnicodeString> getMessage(const char* key) const
    {
        if (!getResource())
        {
            return std::nullopt;
        }
        auto tbl = getResource()->getTable();
        if (!tbl)
        {
            return std::nullopt;
        }
        UErrorCode status = U_ZERO_ERROR;
        icu::UnicodeString val = tbl->getStringEx(key, status);
        if (U_FAILURE(status))
        {
            return std::nullopt;
        }
        return val;
    }
};

// ── Helper ────────────────────────────────────────────────────────────────────

static std::string str(const icu::UnicodeString& us)
{
    std::string s;
    return us.toUTF8String(s);
}

// ── Resource tests ────────────────────────────────────────────────────────────

void test_resource_getString_returns_value()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    icui18n::Resource r{svc, "greeting"};
    auto val = r.getString();
    assert(val.has_value());
    assert(str(*val) == "Hello");
    std::cout << "PASS  test_resource_getString_returns_value\n";
}

void test_resource_getString_missing_key_returns_nullopt()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    icui18n::Resource r{svc, "no_such_key"};
    assert(!r.getString().has_value());
    std::cout << "PASS  test_resource_getString_missing_key_returns_nullopt\n";
}

// Resource is lazy: getString() always queries the source at its current locale.
// No subscription or re-binding is needed when the source changes locale.
void test_resource_reflects_locale_change_lazily()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    icui18n::Resource r{svc, "greeting"};
    assert(str(*r.getString()) == "Hello");

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(str(*r.getString()) == "Bonjour");
    std::cout << "PASS  test_resource_reflects_locale_change_lazily\n";
}

void test_resource_source_and_key_accessors()
{
    Service svc;
    icui18n::Resource r{svc, "farewell"};

    assert(&r.source() == &svc);
    assert(r.key() == "farewell");
    std::cout << "PASS  test_resource_source_and_key_accessors\n";
}

// ── Resourceful tests ─────────────────────────────────────────────────────────

void test_resourceful_default_has_no_resource()
{
    Widget w;
    assert(!w.getResource().has_value());
    std::cout << "PASS  test_resourceful_default_has_no_resource\n";
}

void test_resourceful_setResource_stores_resource()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    Widget w;
    w.setResource({svc, "greeting"});

    assert(w.getResource().has_value());
    assert(str(*w.getResource()->getString()) == "Hello");
    std::cout << "PASS  test_resourceful_setResource_stores_resource\n";
}

void test_resourceful_callback_fires_on_locale_change()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    Widget w;
    int fire_count = 0;
    icu::Locale observed_next = icu::Locale::getEnglish();

    w.setResource({svc, "greeting"},
        [&](const icu::Locale& /*prev*/, const icu::Locale& next)
        {
            ++fire_count;
            observed_next = next;
        });

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(fire_count == 1);
    assert(observed_next == icu::Locale::getFrench());
    std::cout << "PASS  test_resourceful_callback_fires_on_locale_change\n";
}

void test_resourceful_no_callback_locale_change_is_silent()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    Widget w;
    w.setResource({svc, "greeting"}); // no callback — must not crash

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(str(*w.getResource()->getString()) == "Bonjour"); // lazily up-to-date
    std::cout << "PASS  test_resourceful_no_callback_locale_change_is_silent\n";
}

// Rebinding to a new source must cancel the subscription to the old source.
void test_resourceful_rebind_cancels_old_subscription()
{
    Service svc1, svc2;
    svc1.setBundleLocale(icu::Locale::getEnglish());
    svc2.setBundleLocale(icu::Locale::getEnglish());

    Widget w;
    int count1 = 0, count2 = 0;

    w.setResource({svc1, "greeting"}, [&](auto&, auto&) { ++count1; });
    w.setResource({svc2, "greeting"}, [&](auto&, auto&) { ++count2; });

    svc1.setBundleLocale(icu::Locale::getFrench()); // old source — must NOT fire
    assert(count1 == 0);

    svc2.setBundleLocale(icu::Locale::getFrench()); // new source — MUST fire
    assert(count2 == 1);
    std::cout << "PASS  test_resourceful_rebind_cancels_old_subscription\n";
}

void test_resourceful_clearResource_cancels_subscription()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    Widget w;
    int fire_count = 0;
    w.setResource({svc, "greeting"}, [&](auto&, auto&) { ++fire_count; });

    w.clearResource();
    assert(!w.getResource().has_value());

    svc.setBundleLocale(icu::Locale::getFrench()); // must NOT fire after clear
    assert(fire_count == 0);
    std::cout << "PASS  test_resourceful_clearResource_cancels_subscription\n";
}

// Moving a Resourceful transfers the subscription: the callback fires exactly
// once (via w2), confirming that w1's subscription was cancelled by the move.
void test_resourceful_move_transfers_subscription()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    Widget w1;
    int fire_count = 0;
    w1.setResource({svc, "greeting"}, [&](auto&, auto&) { ++fire_count; });

    Widget w2 = std::move(w1);

    assert(w2.getResource().has_value());

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(fire_count == 1); // fired exactly once via w2; w1's sub is dead
    std::cout << "PASS  test_resourceful_move_transfers_subscription\n";
}

// ── ErrorPanel / table resource tests ────────────────────────────────────────

// The resource key resolves to a table in the bundle; getMessage() reads
// individual sub-keys from that table on every call, so it reflects locale
// changes without any callback or re-bind.
void test_table_resource_reads_sub_keys()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    ErrorPanel panel;
    panel.setResource({svc, "errors"});

    assert(panel.getResource().has_value());
    assert(str(*panel.getMessage("not_found")) == "Not found");
    assert(str(*panel.getMessage("forbidden")) == "Forbidden");
    std::cout << "PASS  test_table_resource_reads_sub_keys\n";
}

void test_table_resource_reflects_locale_change()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    ErrorPanel panel;
    int fire_count = 0;
    panel.setResource({svc, "errors"}, [&](auto&, auto&) { ++fire_count; });

    assert(str(*panel.getMessage("not_found")) == "Not found");
    assert(str(*panel.getMessage("forbidden")) == "Forbidden");

    svc.setBundleLocale(icu::Locale::getFrench());
    assert(fire_count == 1);
    assert(str(*panel.getMessage("not_found")) == "Introuvable");
    assert(str(*panel.getMessage("forbidden")) == "Interdit");
    std::cout << "PASS  test_table_resource_reflects_locale_change\n";
}

void test_table_resource_missing_sub_key_returns_nullopt()
{
    Service svc;
    svc.setBundleLocale(icu::Locale::getEnglish());

    ErrorPanel panel;
    panel.setResource({svc, "errors"});

    assert(!panel.getMessage("no_such_key").has_value());
    std::cout << "PASS  test_table_resource_missing_sub_key_returns_nullopt\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main()
{
    test_resource_getString_returns_value();
    test_resource_getString_missing_key_returns_nullopt();
    test_resource_reflects_locale_change_lazily();
    test_resource_source_and_key_accessors();

    test_resourceful_default_has_no_resource();
    test_resourceful_setResource_stores_resource();
    test_resourceful_callback_fires_on_locale_change();
    test_resourceful_no_callback_locale_change_is_silent();
    test_resourceful_rebind_cancels_old_subscription();
    test_resourceful_clearResource_cancels_subscription();
    test_resourceful_move_transfers_subscription();

    test_table_resource_reads_sub_keys();
    test_table_resource_reflects_locale_change();
    test_table_resource_missing_sub_key_returns_nullopt();

    std::cout << "\nAll tests passed.\n";
    return 0;
}
