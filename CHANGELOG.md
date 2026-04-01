# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-04-01

### Added

- `Localizable` — thread-safe base class owning an ICU `ResourceBundle` chain,
  the current `icu::Locale`, and a locale-change listener map. All public
  methods are safe to call concurrently after construction.
- `LocalizableFor<Self, Parent>` — CRTP mixin that wires a concrete class into
  the bundle chain. Two specialisations:
  - `LocalizableFor<Self, void>` for the root of a localizable hierarchy.
  - `LocalizableFor<Self, Parent>` for classes that extend a localizable parent.
  - `bundle_root` can be redeclared independently per class, allowing
    cross-library subclasses to store their bundles under their own install path
    rather than the parent library's.
- `LocaleSubscription` — move-only RAII handle returned by
  `addLocaleListener()`; automatically deregisters the listener when destroyed
  or move-assigned over.
- `Resource` — non-owning `(Localizable&, key)` handle with lazy
  `getString()`, `getInteger()`, `getDouble()`, and `getTable()` accessors that
  always reflect the source's current locale.
- `Resourceful` — mixin for objects that display content from an external
  `Localizable`; manages a `Resource` binding and an optional
  `LocaleChangeCallback` with automatic subscription lifecycle.
- Shared library build with `ICUI18N_EXPORT` visibility annotations
  (`dllexport`/`dllimport` on MSVC, `visibility("default")` on GCC/Clang).
- CMake install rules exporting `icui18nTargets.cmake` and generating
  `icui18nConfig.cmake` / `icui18nConfigVersion.cmake` for
  `find_package(icui18n)` support.
- GitHub Actions CI workflow: Linux/GCC, Linux/Clang, ASan+UBSan, TSan, and
  macOS/AppleClang build and test jobs; automated GitHub Release on `v*` tags.
- Apache 2.0 license.

[1.0.0]: https://github.com/clydegerber/icui18n/releases/tag/v1.0.0
