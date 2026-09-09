add_executable(ambient_selftest selftest.cpp)
target_link_libraries(ambient_selftest PRIVATE AmbientCore)
if(MSVC)
    target_compile_options(ambient_selftest PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ambient_selftest PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Several Engines live on the stack at once in some tests (an Engine is a few hundred KB).
    target_link_options(ambient_selftest PRIVATE /STACK:16777216)
endif()
add_test(NAME selftest COMMAND ambient_selftest)

# The handovers between the rendering thread and the one that answers the mouse, under load. No
# framework, so it builds on Linux too -- which is the only place a sanitizer can watch a data
# race happen (see the file's own notes). On Windows it is a stress test.
add_executable(ambient_racetest racetest.cpp)
target_link_libraries(ambient_racetest PRIVATE AmbientCore)
if(MSVC)
    target_compile_options(ambient_racetest PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ambient_racetest PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_options(ambient_racetest PRIVATE /STACK:16777216)
endif()
add_test(NAME racetest COMMAND ambient_racetest 4)

# The host contract: rates and block sizes, state round trip, programs while playing, an editor
# opened under load, parameters written from another thread. Needs the plugin's shared code, so it
# is only built when the plugin is.
if(TARGET AmbientSynth)
    add_executable(ambient_hosttest hosttest.cpp)
    # The plugin's shared code carries the JUCE modules and their include paths; linking the
    # wrapper target alone gives neither.
    # The plugin's shared code holds the processor and editor; the JUCE modules are linked to it
    # privately, so the test asks for them again.
    # juce_add_plugin makes AmbientSynth the shared-code library itself (it builds as
    # AmbientSynth_SharedCode.lib), so linking that one target gives the processor, the editor and
    # the JucePlugin_* definitions; the module target adds the JUCE headers, which the plugin links
    # privately and therefore does not pass on.
    target_link_libraries(ambient_hosttest PRIVATE AmbientSynth AmbientCore
                          juce::juce_audio_utils juce::juce_recommended_config_flags)
    # The test is an application, not a plugin, so it may run a dispatch loop; that is how the
    # editor's timers and async calls get a chance to run while it is open.
    target_compile_definitions(ambient_hosttest PRIVATE JUCE_STANDALONE_APPLICATION=1
                               JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
    target_include_directories(ambient_hosttest PRIVATE ${CMAKE_SOURCE_DIR}/Plugin)
    if(MSVC)
        target_compile_options(ambient_hosttest PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(ambient_hosttest PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_link_options(ambient_hosttest PRIVATE /STACK:16777216)
    endif()
    add_test(NAME hosttest COMMAND ambient_hosttest)
endif()
