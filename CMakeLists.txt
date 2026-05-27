cmake_minimum_required(VERSION 3.15)
project(SDVX_Editor)

# 1. SETUP VCPKG (The library manager)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

# 2. FIND THE LIBRARIES MMW USES
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 3. TELL CMAKE WHERE THE SOURCE CODE IS
# Looking at your folder tree, most code is in the "MikuMikuWorld" folder
include_directories(MikuMikuWorld)

# 4. CREATE YOUR NEW APP
# We will start by telling it to look for a main.cpp file
add_executable(SDVX_Editor
    MikuMikuWorld/Main.cpp
    SdvxEditor/ChartManager.cpp
    SdvxEditor/SDVXTrackRenderer.cpp
)

# 5. LINK EVERYTHING TOGETHER
target_link_libraries(SDVX_Editor PRIVATE 
    imgui::imgui 
    glfw 
    nlohmann_json::nlohmann_json
)

# 1. Include the libkson directory
add_subdirectory(external/libkson)

# 2. Add it to your include path so you can use #include <kson/kson.hpp>
target_include_directories(SDVX_Editor PRIVATE external/libkson/include)

# 3. Link it to your executable
target_link_libraries(SDVX_Editor PRIVATE libkson)