cmake_minimum_required(VERSION 3.15)

# 1. SETTINGS THAT MUST COME BEFORE PROJECT()
# We removed the toolchain line here because you pass it via command line
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(SDVX_Editor)

# 2. MSVC GLOBAL OPTIONS
if(MSVC)
    add_compile_options(/std:c++20 /permissive- /Zc:__cplusplus /EHsc)
endif()

# 3. FIND LIBRARIES
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 4. ADD SUBDIRECTORIES
add_subdirectory(external/libkson)

# 5. CREATE YOUR APP TARGET
add_executable(SDVX_Editor
    MikuMikuWorld/Main.cpp
    SdvxEditor/ChartManager.cpp
    SdvxEditor/SDVXTrackRenderer.cpp
)

# 6. APP-SPECIFIC FIXES
if(MSVC)
    target_compile_definitions(SDVX_Editor PRIVATE 
        WIN32_LEAN_AND_MEAN 
        NOMINMAX
    )
endif()

# 7. INCLUDE PATHS & LINKING
target_include_directories(SDVX_Editor PRIVATE MikuMikuWorld)
target_include_directories(SDVX_Editor PRIVATE external/libkson/include)
target_include_directories(SDVX_Editor PRIVATE SdvxEditor)

target_link_libraries(SDVX_Editor PRIVATE 
    imgui::imgui 
    glfw 
    nlohmann_json::nlohmann_json
    kson  # Linked to the correct target name
)