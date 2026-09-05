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

# Link-time optimisation, off by default: measured on MSVC over a cloud- and sub-heavy render it
# made no difference at all (1.05-1.16 s either way), so it only costs link time here. Worth
# trying on the clang/NDK build. Fast-math is deliberately NOT enabled: the offline render is the
# determinism oracle of the self test, and reassociation makes it drift -- moving one line of
# processBlock once changed a result by 35 % (see docs/concept.md).
option(AMBIENT_LTO "Link-time optimisation in Release" OFF)
if(AMBIENT_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT AMBIENT_IPO OUTPUT AMBIENT_IPO_MSG)
    if(AMBIENT_IPO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(STATUS "LTO unavailable: ${AMBIENT_IPO_MSG}")
    endif()
endif()

add_subdirectory(Core)

if(AMBIENT_BUILD_TOOLS)
    enable_testing()
    add_subdirectory(Tools/render)
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

# After the plugin, so the host-contract test can link against its shared code.
if(AMBIENT_BUILD_TOOLS)
    add_subdirectory(Tests)
endif()
