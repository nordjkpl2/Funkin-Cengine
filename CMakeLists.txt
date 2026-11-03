
cmake_minimum_required(VERSION 3.21)

project(FNF_C_ENGINE LANGUAGES C)
# Release otimizado para tamanho, não para tamanho de código agressivo.
set(CMAKE_C_FLAGS_RELEASE
    "-Os -DNDEBUG"
    CACHE STRING "" FORCE
)

set(CMAKE_EXE_LINKER_FLAGS_RELEASE
    "-Os -DNDEBUG"
    CACHE STRING "" FORCE
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Este build requer LLVM Clang.")
endif()

# ============================================================
# RAYLIB 5.5 ESTATICA - JA COMPILADA
# ============================================================

set(RAYLIB_LIBRARY
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/libraylib_clang.a"
)

if(NOT EXISTS "${RAYLIB_LIBRARY}")
    message(FATAL_ERROR "libraylib_clang.a nao encontrada.")
endif()

add_library(raylib_clang STATIC IMPORTED GLOBAL)

set_target_properties(raylib_clang PROPERTIES
    IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/headers"
)

# ============================================================
# FONTES DO ENGINE
# ============================================================

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Outras bibliotecas C em /lib.
# Exclui os fontes originais da Raylib.

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c"
)

list(FILTER LIB_SOURCES EXCLUDE REGEX "raylib55")

# ============================================================
# EXECUTAVEL
# ============================================================

add_executable(FNF_C_ENGINE WIN32
    main.c
    ${ENGINE_SOURCES}
    ${LIB_SOURCES}
)

target_include_directories(FNF_C_ENGINE PRIVATE
    headers
    src
    lib
)

# ============================================================
# COMPILACAO E OTIMIZACAO
# ============================================================

target_compile_options(FNF_C_ENGINE PRIVATE
    -Os
    -flto=thin
    -ffunction-sections
    -fdata-sections
    -fmerge-all-constants
    -funroll-loops
    -fomit-frame-pointer
    -fno-math-errno
    -fno-common
    -DNDEBUG
)

target_link_options(FNF_C_ENGINE PRIVATE
    -flto=thin
    -fuse-ld=lld
    -Wl,--gc-sections
    -s
)

# ============================================================
# DEPENDENCIAS
# ============================================================

target_link_libraries(FNF_C_ENGINE PRIVATE
    raylib_clang
    opengl32
    gdi32
    winmm
    user32
    shell32
)

set_target_properties(FNF_C_ENGINE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


message(STATUS "====================================")
message(STATUS "FNF_C_ENGINE BUILD CONFIGURATION")
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard: C23")
message(STATUS "Optimization: -Os")
message(STATUS "LTO: ${FNF_LTO}")
message(STATUS "Linker: ${FNF_LINKER}")
message(STATUS "Raylib: 5.5 SOURCE")
message(STATUS "GLFW: OBJECT")
message(STATUS "====================================")