cmake_minimum_required(VERSION 3.21)

# ── genrb (ICU resource bundle compiler) ─────────────────────────────────────
find_program(GENRB_EXECUTABLE genrb REQUIRED
    HINTS
        "${ICU_ROOT}/bin"
        "/opt/homebrew/Cellar/icu4c@78/78.2/bin"
)

# ── Compile test bundles (.txt → .res) ───────────────────────────────────────
# ICU's ures_open(packagePath, locale, status) looks for:
#   <packagePath>/<locale>.res
# so each named bundle lives in its own subdirectory; locale files are named
# by locale (root.res, fr.res, …).
#
# Source layout: data/com/example/<Bundle>/<locale>.txt
# Output layout: testdata/com/example/<Bundle>/<locale>.res

set(TESTDATA_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata")

set(BUNDLE_SOURCES
    data/com/example/ServiceBundle/root.txt
    data/com/example/ServiceBundle/fr.txt
    data/com/example/UserServiceBundle/root.txt
    data/com/example/UserServiceBundle/fr.txt
)

set(BUNDLE_OUTPUTS)
foreach(BUNDLE_TXT IN LISTS BUNDLE_SOURCES)
    get_filename_component(LOCALE_BASENAME "${BUNDLE_TXT}" NAME_WE)
    get_filename_component(BUNDLE_RELDIR   "${BUNDLE_TXT}" DIRECTORY)

    # Strip the leading "data/" prefix to get the output subdirectory.
    string(REPLACE "data/" "" BUNDLE_SUBDIR "${BUNDLE_RELDIR}")

    set(BUNDLE_OUTDIR "${TESTDATA_OUTPUT_DIR}/${BUNDLE_SUBDIR}")
    set(BUNDLE_OUT    "${BUNDLE_OUTDIR}/${LOCALE_BASENAME}.res")

    add_custom_command(
        OUTPUT  "${BUNDLE_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${BUNDLE_OUTDIR}"
        COMMAND "${GENRB_EXECUTABLE}" -d "${BUNDLE_OUTDIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${BUNDLE_TXT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${BUNDLE_TXT}"
        COMMENT "Compiling ${LOCALE_BASENAME}.res"
        VERBATIM
    )
    list(APPEND BUNDLE_OUTPUTS "${BUNDLE_OUT}")
endforeach()

add_custom_target(test_bundles ALL DEPENDS ${BUNDLE_OUTPUTS})

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(test_localizable test_localizable.cpp)
add_dependencies(test_localizable test_bundles)
target_link_libraries(test_localizable PRIVATE icui18n::icui18n)

# TEST_DATA_DIR points to the root of the compiled testdata tree.
# bundle_name = "com/example/ServiceBundle" → full path = TEST_DATA_DIR/com/example/ServiceBundle
# ICU then appends /<locale>.res to find the actual bundle file.
target_compile_definitions(test_localizable PRIVATE
    TEST_DATA_DIR="${TESTDATA_OUTPUT_DIR}"
)

add_test(NAME localizable COMMAND test_localizable)

# ── Resourceful / Resource tests ──────────────────────────────────────────────
add_executable(test_resourceful test_resourceful.cpp)
add_dependencies(test_resourceful test_bundles)
target_link_libraries(test_resourceful PRIVATE icui18n::icui18n)
target_compile_definitions(test_resourceful PRIVATE
    TEST_DATA_DIR="${TESTDATA_OUTPUT_DIR}"
)
add_test(NAME resourceful COMMAND test_resourceful)
