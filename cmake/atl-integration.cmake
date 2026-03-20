# cmake/atl-integration.cmake
# Configura cómo AINE usa el código de ATL desde vendor/atl/

set(ATL_DIR "${CMAKE_SOURCE_DIR}/vendor/atl"
    CACHE PATH "Ruta al directorio de ATL (submódulo)")

if(NOT EXISTS "${ATL_DIR}/CMakeLists.txt")
    message(WARNING
        "[AINE] vendor/atl/ no encontrado o vacío. "
        "Ejecuta: git submodule update --init --recursive\n"
        "O: ./scripts/init.sh")
    # No hacer FATAL_ERROR — permite compilar aine-shim sin ATL completo
    return()
endif()

message(STATUS "[AINE] ATL found at: ${ATL_DIR}")

# ─── Headers de ATL que AINE incluye ───────────────────────────────────────
# Estos headers son los mismos en Linux y macOS — son las interfaces Java/C++ de Android
set(ATL_INCLUDE_DIRS
    "${ATL_DIR}/frameworks/base/include"
    "${ATL_DIR}/art/libnativebridge/include"
    "${ATL_DIR}/art/libdexfile/include"
)

foreach(inc ${ATL_INCLUDE_DIRS})
    if(EXISTS "${inc}")
        include_directories("${inc}")
    endif()
endforeach()

# ─── Función helper para linkear con ATL ───────────────────────────────────
# Uso: aine_link_atl(mi-target art binder)
function(aine_link_atl target)
    foreach(atl_lib ${ARGN})
        set(lib_path "${ATL_DIR}/build/${atl_lib}/lib${atl_lib}.a")
        if(EXISTS "${lib_path}")
            target_link_libraries(${target} PRIVATE "${lib_path}")
        else()
            # ATL no compilado aún — añadir como interfaz para que CMake no falle
            message(STATUS "[AINE] ATL lib ${atl_lib} not built yet — skipping link")
        endif()
    endforeach()
endfunction()

# ─── Exportar variable ATL_DIR para subdirectorios ─────────────────────────
set(ATL_DIR "${ATL_DIR}" PARENT_SCOPE)

# ─── Información del commit de ATL ─────────────────────────────────────────
execute_process(
    COMMAND git -C "${ATL_DIR}" rev-parse --short HEAD
    OUTPUT_VARIABLE ATL_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(ATL_COMMIT)
    message(STATUS "[AINE] ATL commit: ${ATL_COMMIT}")
    add_compile_definitions(AINE_ATL_COMMIT="${ATL_COMMIT}")
endif()
