cmake_minimum_required(VERSION 3.20)

project(asteroid_pilot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AP_BUILD_JET "Build the Jet native smoke renderer" ON)
option(AP_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)

include(CTest)

function(ap_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(AP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(AP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(ap_core STATIC
    core/src/game_state.cpp
)
target_include_directories(ap_core PUBLIC core/include)
target_compile_features(ap_core PUBLIC cxx_std_17)
ap_strict_warnings(ap_core)

add_executable(ap_sim
    host/simulator/main.cpp
)
target_link_libraries(ap_sim PRIVATE ap_core)
ap_strict_warnings(ap_sim)

add_executable(ap_tests
    tests/unit/test_game.cpp
)
target_link_libraries(ap_tests PRIVATE ap_core)
ap_strict_warnings(ap_tests)
add_test(NAME ap_tests COMMAND ap_tests)

if(AP_BUILD_JET)
    include(FetchContent)

    # Jet's repository CMakeLists is an ESP-IDF component definition, so for
    # native CI we populate the source and create a normal CMake library from
    # its portable C++ sources, matching JetExamples' screenshot harness.
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    FetchContent_Declare(
        jet
        GIT_REPOSITORY https://github.com/CubeCoders/Jet.git
        GIT_TAG c56dfc0c012ba7a09a6e49b4123340ec24981441
        GIT_SHALLOW FALSE
    )
    FetchContent_GetProperties(jet)
    if(NOT jet_POPULATED)
        FetchContent_Populate(jet)
    endif()

    file(GLOB JET_SOURCES CONFIGURE_DEPENDS "${jet_SOURCE_DIR}/src/*.cpp")
    add_library(jet_native STATIC ${JET_SOURCES})
    target_include_directories(jet_native PUBLIC
        host/jet_config
        "${jet_SOURCE_DIR}/src"
    )
    target_compile_features(jet_native PUBLIC cxx_std_17)

    add_executable(ap_jet_smoke
        host/render_test/jet_smoke.cpp
    )
    target_link_libraries(ap_jet_smoke PRIVATE jet_native)
    ap_strict_warnings(ap_jet_smoke)
endif()
