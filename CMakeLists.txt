# ## FunkinCengine by nordjkpl2 ## #
cmake_minimum_required(VERSION 3.21)

# COMPILADOR
# Padrao: clang. Se nenhum compilador foi passado, procura o clang no PATH antes do project().
# GCC (GNU) nao e padrao: so entra no else, e so com confirmacao: -DFNF_ALLOW_GCC=ON.
option(FNF_ALLOW_GCC "Confirma o uso do GCC (GNU) quando o clang nao for usado/funcionar" OFF)
if(NOT CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    find_program(FNF_CLANG_EXE NAMES clang)
    if(FNF_CLANG_EXE)
        set(CMAKE_C_COMPILER "${FNF_CLANG_EXE}")
    endif()
endif()

project(FNF_C_ENGINE LANGUAGES C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(FNF_TOOLCHAIN clang)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(NOT FNF_ALLOW_GCC)
        message(FATAL_ERROR
            "Compilador GNU (gcc) detectado, mas o padrao e o clang.\n"
            "Para usar o gcc mesmo assim, reconfigure com -DFNF_ALLOW_GCC=ON.")
    endif()
    message(WARNING "Compilando com GCC (GNU): suporte nao e o principal, pode quebrar.")
    set(FNF_TOOLCHAIN gcc)
else()
    message(FATAL_ERROR "Compilador nao suportado: ${CMAKE_C_COMPILER_ID}. Use clang (padrao).")
endif()

# PLATAFORMA
# Windows: exe GUI (WIN32). Linux: exe de console comum, linkando pthread/dl/m/X11.
if(WIN32)
    set(FNF_PLATFORM windows)
    set(FNF_EXE_TYPE WIN32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FNF_PLATFORM linux)
    set(FNF_EXE_TYPE "")
else()
    message(FATAL_ERROR "Plataforma ainda nao suportada: ${CMAKE_SYSTEM_NAME}")
endif()

# ARQUITETURA: WIN32 aqui e so o subsistema (GUI), nao a arquitetura.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build 64-bit obrigatorio (use clang x86_64/x64).")
endif()
message(STATUS "FNF_C_ENGINE: ${FNF_PLATFORM} / ${FNF_TOOLCHAIN} / arquitetura ${CMAKE_C_COMPILER_ARCHITECTURE_ID} (64-bit)")

# FLAGS
set(FNF_COMPILE_FLAGS
    -Os
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
# LTO e linker: thin/lld/icf so no clang; gcc usa -flto normal e o ld padrao.
if(FNF_TOOLCHAIN STREQUAL "clang")
    list(APPEND FNF_COMPILE_FLAGS -flto=thin)
    set(FNF_LINK_FLAGS -Os -flto=thin -fuse-ld=lld -Wl,--gc-sections,--icf=all,-O3)
    set(FNF_NOLTO_FLAGS -fno-lto -fuse-ld=lld)
else()
    list(APPEND FNF_COMPILE_FLAGS -flto)
    set(FNF_LINK_FLAGS -Os -flto -Wl,--gc-sections)
    set(FNF_NOLTO_FLAGS -fno-lto)
endif()
list(APPEND FNF_NOLTO_FLAGS -ffunction-sections -fdata-sections -funroll-loops -fomit-frame-pointer
     -fno-math-errno -fno-stack-protector -fno-common -DNDEBUG)

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

# Dependencias do Linux: X11 (headers do raylib/GLFW), threads e libm.
set(FNF_RAYLIB_INCLUDES
    "${RAYLIB_SRC}"
    "${RAYLIB_SRC}/external/glfw/include"
)
if(FNF_PLATFORM STREQUAL "linux")
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    list(APPEND FNF_RAYLIB_INCLUDES "${X11_INCLUDE_DIR}")
endif()

# =========
# RAYLIB OBJECT LIBRARY
# =========
add_library(raylib_objects OBJECT
    ${RAYLIB_MODULES}
)

target_include_directories(raylib_objects PRIVATE
    ${FNF_RAYLIB_INCLUDES}
)

target_compile_options(raylib_objects PRIVATE
    ${FNF_COMPILE_FLAGS}
)

# (Wuffs) sem lto (fito)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/cpu/PngDecode.c"
    PROPERTIES COMPILE_OPTIONS "-O3;${FNF_NOLTO_FLAGS}")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/lib/wuffs-unsupported-snapshot.c"
    PROPERTIES COMPILE_OPTIONS "-Os;${FNF_NOLTO_FLAGS}")

add_executable(FNF_C_ENGINE ${FNF_EXE_TYPE}
    main.c
    ${ENGINE_SOURCES}
    ${LIB_SOURCES}
    $<TARGET_OBJECTS:raylib_objects>
)

target_link_options(FNF_C_ENGINE PRIVATE
    ${FNF_LINK_FLAGS}
)

if(FNF_PLATFORM STREQUAL "windows")
    target_link_libraries(FNF_C_ENGINE PRIVATE
        opengl32
        gdi32
        winmm
        user32
        shell32
    )
else()
    target_link_libraries(FNF_C_ENGINE PRIVATE
        ${X11_LIBRARIES}
        Threads::Threads
        m
        ${CMAKE_DL_LIBS}
    )
endif()

target_include_directories(FNF_C_ENGINE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/headers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    ${FNF_RAYLIB_INCLUDES}
)
target_compile_options(FNF_C_ENGINE PRIVATE
    ${FNF_COMPILE_FLAGS}
)

set_target_properties(FNF_C_ENGINE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
