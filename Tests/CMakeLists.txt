add_executable(ambient_selftest selftest.cpp)
target_link_libraries(ambient_selftest PRIVATE AmbientCore)
if(MSVC)
    target_compile_options(ambient_selftest PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ambient_selftest PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Several Engines live on the stack at once in some tests (an Engine is a few hundred KB).
    target_link_options(ambient_selftest PRIVATE /STACK:16777216)
endif()
add_test(NAME selftest COMMAND ambient_selftest)
