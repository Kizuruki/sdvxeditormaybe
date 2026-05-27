cmake_minimum_required(VERSION 3.15)

# 1. SETTINGS
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(SDVX_Editor)

add_compile_definitions(
    _ENABLE_EXTENDED_ALIGNED_STORAGE
)

# 2. MSVC GLOBAL OPTIONS
if(MSVC)
    add_compile_options(
        /std:c++20
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /utf-8
    )

    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# 3. FIND LIBRARIES
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 4. ADD SUBDIRECTORIES
add_subdirectory(external/libkson)

# 5. CREATE YOUR APP TARGET
# We need to add the implementation (.cpp) files for the classes you are using
file(GLOB_RECURSE APP_SOURCES
    "MikuMikuWorld/*.cpp"
    "SdvxEditor/*.cpp"
)

list(APPEND APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/glad/src/glad.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/stb_vorbis/stb_vorbis.c"
)

add_executable(SDVX_Editor ${APP_SOURCES})

# 7. INCLUDE PATHS & LINKING
target_include_directories(SDVX_Editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(SDVX_Editor PRIVATE Depends/stb_vorbis)
target_include_directories(SDVX_Editor PRIVATE Depends/miniaudio)
target_include_directories(SDVX_Editor PRIVATE Depends/stb_image) # Added this line

target_include_directories(SDVX_Editor PRIVATE MikuMikuWorld)
target_include_directories(SDVX_Editor PRIVATE external/libkson/include)
target_include_directories(SDVX_Editor PRIVATE SdvxEditor)

target_link_libraries(SDVX_Editor PRIVATE 
    imgui::imgui 
    glfw 
    nlohmann_json::nlohmann_json
    kson
    opengl32.lib
    Shell32.lib
    User32.lib
    Gdi32.lib
    Comdlg32.lib
    Version.lib
)