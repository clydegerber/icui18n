# icui18n

[![CI](https://github.com/YOUR_USERNAME/icui18n/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/icui18n/actions/workflows/ci.yml)

A C++20 library that layers polymorphic resource inheritance and locale-change events on top of ICU's `ResourceBundle` API.

## Features

- **Polymorphic bundle chains** — each class in a hierarchy loads its own ICU resource bundle; key lookup walks from the most-derived class to the hierarchy root, with ICU handling locale fallback (`en_US → en → root`) within each bundle automatically.
- **Locale-change events** — subscribe to locale changes with a RAII `LocaleSubscription` handle; the subscription cancels itself automatically when destroyed.
- **`Resourceful` mixin** — for objects that display content sourced from an external `Localizable` (a non-owning source + key handle), with an optional callback for refreshing derived state on locale change.
- **Thread-safe** — concurrent reads and locale switches are safe via `std::shared_mutex`; locale changes are applied atomically and listeners are fired outside the lock.
- **Shared library** — all public symbols are annotated with `ICUI18N_EXPORT` for correct `dllexport`/`dllimport` on MSVC and `visibility("default")` on GCC/Clang.

## Requirements

- C++20 compiler (GCC 11+, Clang 13+, MSVC 19.29+)
- [ICU4C](https://icu.unicode.org) (tested with ICU 78)
- CMake 3.21+

## Integration

The recommended way to consume icui18n is via CMake's built-in `FetchContent` module. Add the following to your project's `CMakeLists.txt` **before** any `target_link_libraries` calls that reference `icui18n::icui18n`:

```cmake
include(FetchContent)

FetchContent_Declare(
    icui18n
    GIT_REPOSITORY https://github.com/YOUR_USERNAME/icui18n.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(icui18n)
```

Then link to your target as usual:

```cmake
target_link_libraries(my_target PRIVATE icui18n::icui18n)
```

`FetchContent_MakeAvailable` downloads the source at configure time, builds it as part of your project, and makes the `icui18n::icui18n` target available immediately — no separate install step is required.

**ICU must be installed on the build machine.** `FetchContent` pulls in icui18n's `CMakeLists.txt`, which calls `find_package(ICU REQUIRED)`. If ICU is not on the default search path, set `ICU_ROOT` before or alongside the `FetchContent_Declare` call:

```cmake
set(ICU_ROOT /opt/homebrew/opt/icu4c)   # example: keg-only Homebrew install
```

## Building from source

```bash
cmake -B build -DICU_ROOT=/path/to/icu   # omit ICU_ROOT if ICU is on the system path
cmake --build build
ctest --test-dir build --output-on-failure
```

To install to a prefix (required for `find_package(icui18n)` without FetchContent):

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

## Quick Start

### 1. Define a localizable class

Inherit from `icui18n::LocalizableFor<Self>` and declare two `public static constexpr std::string_view` members:

```cpp
#include <icui18n/Localizable.hpp>

class Service : public icui18n::LocalizableFor<Service>
{
public:
    static constexpr std::string_view bundle_root = "/usr/share/myapp/i18n";
    static constexpr std::string_view bundle_name = "com/example/ServiceBundle";
};
```

ICU will look for `<bundle_root>/<bundle_name>/<locale>.res` at construction time. If the bundle cannot be loaded a `std::runtime_error` is thrown.

### 2. Extend the hierarchy

A subclass that adds its own bundle inherits from `LocalizableFor<Self, Parent>`:

```cpp
class UserService : public icui18n::LocalizableFor<UserService, Service>
{
public:
    // bundle_root is inherited from Service via static-member lookup
    static constexpr std::string_view bundle_name = "com/example/UserServiceBundle";
};
```

Key lookup checks `UserServiceBundle` first, then falls back to `ServiceBundle`.

### 3. Read values

```cpp
Service svc;
svc.setBundleLocale(icu::Locale::getFrench());

std::optional<icu::UnicodeString> greeting = svc.getString("greeting"); // "Bonjour"
std::optional<std::int32_t>       count    = svc.getInteger("max_retries");
std::optional<double>             ratio    = svc.getDouble("threshold");  // stored as string in bundle
std::optional<icu::ResourceBundle> errors  = svc.getTable("errors");
```

### 4. Subscribe to locale changes

```cpp
auto sub = svc.addLocaleListener(
    [](const icu::Locale& prev, const icu::Locale& next)
    {
        // refresh UI, reformat messages, etc.
    });

svc.setBundleLocale(icu::Locale::getGerman()); // listener fires here

// sub goes out of scope → listener is automatically deregistered
```

### 5. Use `Resourceful` for view objects

A view or widget that displays content from a `Service` but does not own a bundle:

```cpp
#include <icui18n/Resourceful.hpp>

class GreetingLabel : public icui18n::Resourceful
{
public:
    void bind(const Service& svc)
    {
        setResource({svc, "greeting"},
            [this](const icu::Locale&, const icu::Locale&)
            {
                refresh(); // called whenever svc changes locale
            });
    }

    void refresh()
    {
        if (auto r = getResource())
            display(r->getString());
    }
};
```

The subscription is cancelled automatically when the `GreetingLabel` is destroyed or `clearResource()` is called.

## Bundle File Format

ICU resource bundles are plain-text `.txt` files compiled to binary `.res` files by `genrb`. The root bundle serves as the fallback for all locales:

```
// com/example/ServiceBundle/root.txt
root {
    greeting { "Hello" }
    farewell  { "Goodbye" }
    errors {
        not_found { "Not found" }
        forbidden { "Forbidden" }
    }
}
```

```
// com/example/ServiceBundle/fr.txt
fr {
    greeting { "Bonjour" }
    farewell  { "Au revoir" }
}
```

Compile with:

```bash
genrb -d /usr/share/myapp/i18n/com/example/ServiceBundle  root.txt  fr.txt
```

The `bundle_name` passed to `LocalizableFor` must not include the locale suffix or `.res` extension — ICU appends those itself.

## Thread Safety

All `Localizable` public methods are safe to call concurrently after construction:

- `getString`, `getInteger`, `getDouble`, `getTable`, `getBundleLocale`, `addLocaleListener` — take a shared lock; safe to call concurrently with each other.
- `setBundleLocale` — takes an exclusive lock during bundle installation; safe to call while readers are active. Listeners are fired outside the lock.
- `setBundleLocale` is not safe to call concurrently with itself; it is a write operation.

## Move Semantics

`Localizable` is movable but listeners are **not** transferred on move. Moving a `Localizable` marks all outstanding `LocaleSubscription` handles as no-ops (they will not fire and will not crash on destruction). The moved-to object starts with an empty listener map; call `addLocaleListener()` on it to re-subscribe.

`Resourceful` is movable and its subscription is transferred correctly on move.

## Using as a Static Library

If you link the static archive directly (rather than the shared library), define `ICUI18N_STATIC_DEFINE` before including any icui18n header to suppress `dllimport` annotations:

```cmake
target_compile_definitions(my_target PRIVATE ICUI18N_STATIC_DEFINE)
```

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
