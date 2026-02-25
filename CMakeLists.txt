cmake_minimum_required(VERSION 3.10)
project(KeyNav VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find X11 and Cairo
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(XTST REQUIRED xtst)
pkg_check_modules(XRANDR REQUIRED xrandr)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GTK_LAYER_SHELL REQUIRED gtk-layer-shell-0)

# Include directories
include_directories(src)
include_directories(src/core)
include_directories(src/platform/linux)
include_directories(${X11_INCLUDE_DIRS})
include_directories(${CAIRO_INCLUDE_DIRS})
include_directories(${XTST_INCLUDE_DIRS})
include_directories(${XRANDR_INCLUDE_DIRS})
include_directories(${GTK3_INCLUDE_DIRS})
include_directories(${GTK_LAYER_SHELL_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/core/Engine.cpp
    src/platform/linux/X11Platform.cpp
    src/platform/linux/X11Overlay.cpp
    src/platform/linux/WaylandOverlay.cpp
    src/platform/linux/X11Input.cpp
    src/platform/linux/EvdevInput.cpp
)

# Executable
add_executable(KeyNav ${SOURCES})

# Link libraries
target_link_libraries(KeyNav
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${XTST_LIBRARIES}
    ${XRANDR_LIBRARIES}
    ${GTK3_LIBRARIES}
    ${GTK_LAYER_SHELL_LIBRARIES}
    pthread
)
