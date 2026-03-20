cmake_minimum_required(VERSION 3.22)

# ── aine-pm: APK package manager ──────────────────────────────────────────
# Components:
#   zip.c     — minimal ZIP reader (uses libz for DEFLATE)
#   axml.c    — Android binary XML manifest parser
#   apk.c     — APK extraction (ZIP + AXML)
#   pm.c      — package registry (/tmp/aine/packages.db)
#   main.c    — CLI binary: aine-pm install/list/query/remove/run

set(AINE_PM_SOURCES
    zip.c
    axml.c
    apk.c
    pm.c
)

# Static library for testing
add_library(aine-pm-lib STATIC ${AINE_PM_SOURCES})
target_include_directories(aine-pm-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aine-pm-lib PRIVATE z)   # libz for DEFLATE
target_compile_options(aine-pm-lib PRIVATE -Wno-missing-field-initializers)

# CLI binary
add_executable(aine-pm main.c)
target_link_libraries(aine-pm PRIVATE aine-pm-lib)
set_target_properties(aine-pm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
