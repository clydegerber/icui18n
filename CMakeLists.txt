cmake_minimum_required(VERSION 3.21)
project(icui18n VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── ICU ───────────────────────────────────────────────────────────────────────
# If ICU is not installed system-wide, point CMake at a local build or a
# keg-only Homebrew install with:
#   cmake -DICU_ROOT=/opt/homebrew/Cellar/icu4c@78/78.2 …
# or:
#   cmake -DICU_ROOT=~/projects/icu-install …
find_package(ICU REQUIRED COMPONENTS uc i18n)

# ── Library target ────────────────────────────────────────────────────────────
add_library(icui18n SHARED
    src/Localizable.cpp
    src/Resourceful.cpp
)
add_library(icui18n::icui18n ALIAS icui18n)

# Hide all symbols by default on GCC/Clang; only those marked ICUI18N_EXPORT
# are part of the public ABI.  MSVC hides by default already.
set_target_properties(icui18n PROPERTIES
    CXX_VISIBILITY_PRESET     hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(icui18n PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(icui18n PUBLIC ICU::uc ICU::i18n)
target_compile_features(icui18n PUBLIC cxx_std_20)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
