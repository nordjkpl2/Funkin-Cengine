# ## FunkinCengine by nordjkpl2 ## #
cmake_minimum_required(VERSION 3.21)
project(FNF_C_ENGINE LANGUAGES C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(FNF_COMPILE_FLAGS
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
    -DPLATFORM_DESKTOP
    -DGRAPHICS_API_OPENGL_33
)

# COMPILER
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Not Using Clang")
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Not Windows target")
endif()
# ARQUITETURA: WIN32 aqui e so o subsistema (GUI), nao a arquitetura.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build 64-bit obrigatorio (use clang x86_64/x64).")
endif()
message(STATUS "FNF_C_ENGINE arquitetura: ${CMAKE_C_COMPILER_ARCHITECTURE_ID} (64-bit)")

# ## Raylib by Ramon Santamaria ## #
set(RAYLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/raylib55")
set(RAYLIB_SRC "${RAYLIB_ROOT}/src")
if(NOT EXISTS "${RAYLIB_SRC}/raylib.h")
    message(FATAL_ERROR "raylib.h not found in lib/raylib55.")
endif()

# ENGINE SOURCES
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
# OTHER LIBRARIES
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c"
)
list(FILTER LIB_SOURCES EXCLUDE REGEX "raylib55")
list(FILTER LIB_SOURCES EXCLUDE REGEX "wuffs")
# RAYLIB 5.5
set(RAYLIB_MODULES
    "${RAYLIB_SRC}/raudio.c"
    "${RAYLIB_SRC}/rcore.c"
    "${RAYLIB_SRC}/rmodels.c"
    "${RAYLIB_SRC}/rshapes.c"
    "${RAYLIB_SRC}/rtext.c"
    "${RAYLIB_SRC}/rtextures.c"
    "${RAYLIB_SRC}/utils.c"
    "${RAYLIB_SRC}/rglfw.c"
)

# Overlay de debug (F3): tempos do quadro e draw calls. Desligado por padrao.
# Ligar: cmake -DFNF_DEBUG_STATS=ON (reconfigurar o cache).
option(FNF_DEBUG_STATS "F3 overlay with frame times and draw calls" OFF)
if(FNF_DEBUG_STATS)
    list(APPEND FNF_COMPILE_FLAGS -DFNF_DEBUG_STATS)
endif()

# =========
# RAYLIB OBJECT LIBRARY
# =========
add_library(raylib_objects OBJECT
    ${RAYLIB_MODULES}
)

target_include_directories(raylib_objects PRIVATE
    "${RAYLIB_SRC}"
    "${RAYLIB_SRC}/external/glfw/include"
)

target_compile_options(raylib_objects PRIVATE
    ${FNF_COMPILE_FLAGS}
)

# (Wuffs) sem lto (fito)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/cpu/PngDecode.c"
    PROPERTIES COMPILE_OPTIONS "-O3;-fno-lto;-fuse-ld=lld;-ffunction-sections;-fdata-sections;-funroll-loops;-fomit-frame-pointer;-fno-math-errno;-fno-stack-protector;-fno-common;-DNDEBUG")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/lib/wuffs-unsupported-snapshot.c"
    PROPERTIES COMPILE_OPTIONS "-Os;-fno-lto;-fuse-ld=lld;-ffunction-sections;-fdata-sections;-funroll-loops;-fomit-frame-pointer;-fno-math-errno;-fno-stack-protector;-fno-common;-DNDEBUG")

add_executable(FNF_C_ENGINE WIN32
    main.c
    ${ENGINE_SOURCES}
    ${LIB_SOURCES}
    $<TARGET_OBJECTS:raylib_objects>
)

target_link_options(FNF_C_ENGINE PRIVATE
    -Os
    -flto=thin
    -fuse-ld=lld
    -Wl,--gc-sections,--icf=all,-O3
)
target_link_libraries(FNF_C_ENGINE PRIVATE
    opengl32
    gdi32
    winmm
    user32
    shell32
)
target_include_directories(FNF_C_ENGINE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/headers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    "${RAYLIB_SRC}"
    "${RAYLIB_SRC}/external/glfw/include"
)
target_compile_options(FNF_C_ENGINE PRIVATE
    ${FNF_COMPILE_FLAGS}
)

set_target_properties(FNF_C_ENGINE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
