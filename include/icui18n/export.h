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

// ── ICUI18N_EXPORT ────────────────────────────────────────────────────────────
// Applied to all classes in the public API.
//
// When building icui18n as a shared library, CMake defines icui18n_EXPORTS
// for the library's own translation units:
//   MSVC       — __declspec(dllexport)
//   GCC/Clang  — __attribute__((visibility("default")))
//
// When consuming icui18n as a shared library:
//   MSVC       — __declspec(dllimport)
//   GCC/Clang  — __attribute__((visibility("default")))
//
// Define ICUI18N_STATIC_DEFINE before including any icui18n header to
// suppress all visibility annotations (required when linking the static
// archive directly into an executable or another library).

#ifdef ICUI18N_STATIC_DEFINE
#  define ICUI18N_EXPORT
#elif defined(_MSC_VER)
#  ifdef icui18n_EXPORTS
#    define ICUI18N_EXPORT __declspec(dllexport)
#  else
#    define ICUI18N_EXPORT __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define ICUI18N_EXPORT __attribute__((visibility("default")))
#else
#  define ICUI18N_EXPORT
#endif
