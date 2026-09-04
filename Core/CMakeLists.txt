add_library(AmbientCore STATIC
    src/Params.cpp
    src/Tuning.cpp
    src/Voice.cpp
    src/Effects.cpp
    src/Engine.cpp
    src/Presets.cpp
    src/Cosmos.cpp
    src/Gesture.cpp
    src/Osc.cpp
    src/Recorder.cpp
    src/Sources.cpp
    src/WavFile.cpp
    src/PresetMeta.cpp
    src/PresetPacks.cpp
    src/PresetMap.cpp
    src/Convolution.cpp
    src/Route.cpp
    src/ZPlane.cpp
    src/Timeline.cpp
)
if(WIN32)
    target_link_libraries(AmbientCore PUBLIC ws2_32)
endif()
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(AmbientCore PUBLIC Threads::Threads)
endif()
target_include_directories(AmbientCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(AmbientCore PUBLIC cxx_std_20)
set_target_properties(AmbientCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(AmbientCore PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(AmbientCore PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(AmbientCore PRIVATE -Wall -Wextra)
endif()
