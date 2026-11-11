cmake_minimum_required(VERSION 3.15)
project("GameStat Overlay" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 pkgconf glfw3)
find_package(X11 REQUIRED)

# Add Dear ImGui source files from project directory
set(IMGUI_DIR "/home/user/Project/gamestat/imgui")

# Collect ImGui source files
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)

# Collect ImGui backend files (GLFW + OpenGL3)
file(GLOB IMGUI_BACKEND_SOURCES
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

file(GLOB IMGUI_BACKEND_HEADERS
    "${IMGUI_DIR}/imgui.h"
    "${IMGUI_DIR}/imgui_internal.h"
    "${IMGUI_DIR}/imconfig.h"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.h"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.h"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.h"
    "${IMGUI_DIR}/imstb_rectpack.h"
    "${IMGUI_DIR}/imstb_texfont.h"
    "${IMGUI_DIR}/imstb_textedit.h"
    "${IMGUI_DIR}/imstb_truetype.h"
)

# Application source files
set(APP_SOURCES
    main.cpp
    backend.cpp
    overlay.cpp
    metrics.cpp
)

# Application headers
set(APP_HEADERS
    backend.h
    overlay.h
    metrics.h
)

add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES})
target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

add_executable(game_stat_overlay
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_link_libraries(game_stat_overlay PRIVATE
    imgui
    ${GLFW3_LIBRARIES}
    m
)

# Include directories
target_include_directories(game_stat_overlay PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "/home/user/Project/gamestat/front/nlohmann"
    "/home/user/Project/gamestat/front"
    ${GLFW3_LIBRARIES}_INCLUDE_DIR
    ${X11_LIBRARIES}_INCLUDE_DIR
)

# Set target properties for better performance and behavior
set_target_properties(game_stat_overlay PROPERTIES
    CXX_VISIBILITY_PRESET "hidden"
    SYMBOLOGY "DEBUG"
    BUILD_RPATH "$ORIGIN"
)