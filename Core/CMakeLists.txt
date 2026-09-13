add_library(AmbientCore STATIC
    src/Params.cpp
    src/Tuning.cpp
    src/Voice.cpp
    src/Effects.cpp
    src/Cloud.cpp
    src/Memory.cpp
    src/Shifter.cpp
    src/Engine.cpp
    src/EngineControl.cpp
    src/EngineRender.cpp
    src/Presets.cpp
    src/Cosmos.cpp
    src/Gesture.cpp
    src/Osc.cpp
    src/Recorder.cpp
    src/Sources.cpp
    src/SourcesNear.cpp
    src/CycleTable.cpp
    src/WavFile.cpp
    src/PresetMeta.cpp
    src/PresetClusters.cpp
    src/PresetText.cpp
    src/PresetPacks.cpp
    src/PresetMap.cpp
    src/Modulation.cpp
    src/Loudness.cpp
    src/Convolution.cpp
    src/Route.cpp
    src/ZPlane.cpp
    src/Filter.cpp
    src/Clock.cpp
    src/Body.cpp
    src/Help.cpp
    src/Timeline.cpp
    src/Score.cpp
)
if(WIN32)
    target_link_libraries(AmbientCore PUBLIC ws2_32)
endif()
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(AmbientCore PUBLIC Threads::Threads)
endif()
target_include_directories(AmbientCore
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    # dr_flac, one public-domain header: the sample library ships as FLAC, and the core reads it
    # without a framework the way it reads WAV. PRIVATE -- nobody outside needs the decoder.
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/dr_libs)
target_compile_features(AmbientCore PUBLIC cxx_std_20)
set_target_properties(AmbientCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(AmbientCore PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(AmbientCore PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(AmbientCore PRIVATE -Wall -Wextra)
endif()

# AVX2 for the phasor banks (Simd.h), which are most of the instrument's arithmetic. Every x86-64
# processor since 2013 has it; switch this off to build for anything older. The arm64 build (the
# Quest) never sees it and takes the scalar path, which is the same arithmetic in another order.
option(AMBIENT_AVX2 "Build the x86-64 core with AVX2" ON)
if(AMBIENT_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|x64)")
    if(MSVC)
        target_compile_options(AmbientCore PUBLIC /arch:AVX2)
    else()
        target_compile_options(AmbientCore PUBLIC -mavx2 -mfma)
    endif()
    target_compile_definitions(AmbientCore PUBLIC AMBIENT_AVX=1)
endif()
