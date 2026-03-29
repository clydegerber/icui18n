cmake_minimum_required(VERSION 3.21)
project(icui18n VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── ICU ───────────────────────────────────────────────────────────────────────
# If ICU is not installed system-wide, point CMake at a local build with:
#   cmake -DICU_ROOT=~/projects/icu-install …
find_package(ICU REQUIRED COMPONENTS uc i18n)

# ── Library target (header-only) ──────────────────────────────────────────────
add_library(icui18n INTERFACE)
add_library(icui18n::icui18n ALIAS icui18n)

target_include_directories(icui18n INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(icui18n INTERFACE ICU::uc ICU::i18n)
target_compile_features(icui18n INTERFACE cxx_std_20)
