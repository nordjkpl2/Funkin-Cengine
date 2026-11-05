
cmake_minimum_required(VERSION 3.21)

project(FNF_C_ENGINE LANGUAGES C)

# =========
# COMPILER
# ==
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "this build code requires LLVMClang")
endif()

if(NOT WIN32)
    message(FATAL_ERROR "Windows target required.")
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
# ======
# DIRECTORIES
# ====
set(RAYLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/raylib55")
set(RAYLIB_SRC "${RAYLIB_ROOT}/src")

if(NOT EXISTS "${RAYLIB_SRC}/raylib.h")
    message(FATAL_ERROR "Raylib 5.5 not found in lib/raylib55.")
endif()

if(NOT EXISTS "${RAYLIB_SRC}/rglfw.c")
    message(FATAL_ERROR "Raylib GLFW module not found.")
endif()
# =========
# ENGINE SOURCES
# ======
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
# =============
# OTHER LIBRARIES
# ======
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c"
)
# Exclude Raylib sources from this collection
list(FILTER LIB_SOURCES EXCLUDE REGEX "raylib55")
# ============
# RAYLIB 5.5 SOURCES
# =====

# Non-recursive: only the actual module .c files.
# Excludes external and platform-specific standalone sources.

file(GLOB RAYLIB_SOURCES CONFIGURE_DEPENDS
    "${RAYLIB_SRC}/*.c"
)

# =========
# OPTIMIZATION FLAGS
# ====

set(FNF_COMPILE_FLAGS
    -Os
    -flto=thin
    -ffunction-sections
    -fdata-sections
    -funroll-loops
    -fomit-frame-pointer
    -fno-math-errno
    -fno-common
    -DNDEBUG
)

# ========
# RAYLIB OBJECTS
# ====
add_library(raylib_objects OBJECT
    ${RAYLIB_SOURCES}
)

target_include_directories(raylib_objects PRIVATE
    "${RAYLIB_SRC}"
    "${RAYLIB_SRC}/external/glfw/include"
)

target_compile_definitions(raylib_objects PRIVATE
    PLATFORM_DESKTOP_GLFW
    GRAPHICS_API_OPENGL_33
)

target_compile_options(raylib_objects PRIVATE
    ${FNF_COMPILE_FLAGS}
)
# =============
# EXECUTABLE
# ======
add_executable(FNF_C_ENGINE WIN32
    main.c
    ${ENGINE_SOURCES}
    ${LIB_SOURCES}
    $<TARGET_OBJECTS:raylib_objects>
)

target_include_directories(FNF_C_ENGINE PRIVATE
    "${RAYLIB_SRC}"
    "${RAYLIB_SRC}/external/glfw/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/headers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

target_compile_options(FNF_C_ENGINE PRIVATE
    ${FNF_COMPILE_FLAGS}
)
# ========
# LINKER
# ====

target_link_options(FNF_C_ENGINE PRIVATE
    -Os
    -flto=thin
    -fuse-ld=lld
    -Wl,--gc-sections
    -s
)

target_link_libraries(FNF_C_ENGINE PRIVATE
    opengl32
    gdi32
    winmm
    user32
    shell32
)
# ======
# OUTPUT
# ===
set_target_properties(FNF_C_ENGINE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
# ======
# BUILD INFO
# ===
message(STATUS "====================================")
message(STATUS "FNF_C_ENGINE")
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "Standard: C23")
message(STATUS "Optimization: -Os")
message(STATUS "LTO: ThinLTO")
message(STATUS "Linker: LLD")
message(STATUS "Raylib: 5.5 SOURCE")
message(STATUS "Platform: Windows GLFW")
message(STATUS "Graphics: OpenGL 3.3")
message(STATUS "Raylib static archive: NONE")
message(STATUS "====================================")