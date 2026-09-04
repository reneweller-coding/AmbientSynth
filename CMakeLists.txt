cmake_minimum_required(VERSION 3.22)
project(AmbientSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AMBIENT_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" ON)
option(AMBIENT_BUILD_TOOLS  "Build the offline render tool and the self test" ON)
set(AMBIENT_JUCE_TAG "9.0.1" CACHE STRING "JUCE git tag to fetch")

if(MSVC)
    add_compile_options(/MP)
endif()

add_subdirectory(Core)

if(AMBIENT_BUILD_TOOLS)
    enable_testing()
    add_subdirectory(Tools/render)
    add_subdirectory(Tests)
endif()

if(AMBIENT_BUILD_PLUGIN)
    include(FetchContent)
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${AMBIENT_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(Plugin)
endif()
