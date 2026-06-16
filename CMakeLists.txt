cmake_minimum_required(VERSION 3.25)

# ============================================================================
#  EZStudio — personal DAW (Tracktion Engine + JUCE 8, C++20, macOS arm64)
#  License: GPL-3.0-or-later (builds incorporate JUCE under AGPL-3.0)
#
#  Targets:
#    daw_core  STATIC engine glue, ZERO GUI includes; owns the TRACKTION_* defines
#    daw_tests Catch2 console runner, links daw_core only (never the GUI app)
#    EZStudio  juce_add_gui_app, the application
#
#  Generator: Ninja only (never Xcode). Canonical commands: see CLAUDE.md / presets.
# ============================================================================

project(daw VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DAW_BUILD_TESTS "Build the daw_tests Catch2 runner" ON)

# ----------------------------------------------------------------------------
#  Rubber Band wiring
#
#  The engine's tracktion_engine.cpp pulls in the single-file Rubber Band build
#  via  #include <rubberband/single/RubberBandSingle.cpp>  with a fallback to
#  "../3rd_party/rubberband/single/RubberBandSingle.cpp" (see
#  modules/tracktion_engine/tracktion_engine.h, gated on TRACKTION_BUILD_RUBBERBAND).
#  So we link our pinned libs/rubberband into the engine's modules/3rd_party tree
#  and add modules/3rd_party to daw_core's include path (matching the engine's
#  own TestRunner). Symlink is preferred; fall back to copy. SoundTouch stays on
#  as the A/B fallback time-stretcher.
# ----------------------------------------------------------------------------
set(DAW_RUBBERBAND_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/libs/rubberband")
set(DAW_ENGINE_3RDPARTY "${CMAKE_CURRENT_SOURCE_DIR}/libs/tracktion_engine/modules/3rd_party")
set(DAW_RUBBERBAND_LINK "${DAW_ENGINE_3RDPARTY}/rubberband")

if(NOT EXISTS "${DAW_RUBBERBAND_LINK}/single/RubberBandSingle.cpp")
    file(REMOVE_RECURSE "${DAW_RUBBERBAND_LINK}")
    message(STATUS "Linking Rubber Band into engine: ${DAW_RUBBERBAND_LINK}")
    file(CREATE_LINK "${DAW_RUBBERBAND_SRC}" "${DAW_RUBBERBAND_LINK}"
         RESULT _daw_rb_link_result SYMBOLIC)
    if(NOT _daw_rb_link_result EQUAL 0)
        message(STATUS "Symlink failed (${_daw_rb_link_result}); copying Rubber Band instead")
        file(COPY "${DAW_RUBBERBAND_SRC}/" DESTINATION "${DAW_RUBBERBAND_LINK}")
    endif()
endif()
if(NOT EXISTS "${DAW_RUBBERBAND_LINK}/single/RubberBandSingle.cpp")
    message(FATAL_ERROR "Rubber Band wiring failed: ${DAW_RUBBERBAND_LINK} is not usable")
endif()

# ----------------------------------------------------------------------------
#  Engine + its JUCE (build against the engine's own JUCE submodule ONLY).
#  TE_ADD_EXAMPLES OFF: never build the engine's examples/tests (their CMake uses
#  -Werror and trips pre-existing warnings under Apple clang 21).
# ----------------------------------------------------------------------------
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/tracktion_engine)

# ============================================================================
#  daw_engine — INTERNAL static lib that amalgamates the tracktion_engine +
#  JUCE module sources (and the single-file Rubber Band build) exactly once.
#
#  It deliberately carries NO precompiled header: the JUCE module amalgamation
#  TUs (e.g. tracktion_core.cpp) #error if ANYTHING — including a force-included
#  PCH — is seen before them, and they mix C++/Objective-C++ which a single PCH
#  can't satisfy. So PCH lives on the my-code targets (daw_core / EZStudio),
#  never here. Modules are linked PRIVATE so their INTERFACE sources compile only
#  into daw_engine; the engine/JUCE usage requirements (include dirs, defines)
#  are re-exported PUBLIC so my code can #include <tracktion_engine/...>.
# ============================================================================
add_library(daw_engine STATIC src/engine/EngineAnchor.cpp)

# Rubber Band single-file build resolves its includes relative to modules/3rd_party.
target_include_directories(daw_engine PRIVATE "${DAW_ENGINE_3RDPARTY}")

target_compile_features(daw_engine PUBLIC cxx_std_20)

target_compile_definitions(daw_engine PUBLIC
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=1
    TRACKTION_BUILD_RUBBERBAND=1)

# Module sources compile into daw_engine only (PRIVATE) ...
target_link_libraries(daw_engine PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_recommended_warning_flags)

# ... and the SAME module targets are re-exported under $<LINK_ONLY:...> so that
# consumers (daw_core, EZStudio, daw_tests) inherit the JUCE frameworks + link
# flags WITHOUT pulling the module INTERFACE_SOURCES in to recompile. The header
# include dirs + the TRACKTION_* / JUCE config defines ARE re-exported (consumers
# must be able to #include the umbrellas), but no .cpp/.mm sources cross over.
target_link_libraries(daw_engine PUBLIC
    $<LINK_ONLY:tracktion::tracktion_core>
    $<LINK_ONLY:tracktion::tracktion_engine>
    $<LINK_ONLY:tracktion::tracktion_graph>)
target_include_directories(daw_engine SYSTEM PUBLIC
    $<TARGET_PROPERTY:tracktion_engine,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:tracktion_core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:tracktion_graph,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(daw_engine PUBLIC
    $<TARGET_PROPERTY:tracktion_engine,INTERFACE_COMPILE_DEFINITIONS>)

if(APPLE)
    # Matches the engine's own targets; avoids aligned-alloc ABI surprises.
    target_compile_options(daw_engine PRIVATE "-fno-aligned-allocation")
endif()

# ============================================================================
#  daw_core — STATIC engine-glue library (my code). ZERO GUI includes.
#  Links daw_engine; this is where my PCH lives (few TUs, no module sources).
#  EZStudio and daw_tests link daw_core.
# ============================================================================
#  src/rt/RealtimeGuard.cpp is the SINGLE TU that replaces global operator
#  new/delete for the debug allocation tripwire; it must live in exactly one
#  daw_core object file (never duplicated, never in a header).
add_library(daw_core STATIC
    src/engine/EngineInfo.cpp
    src/engine/SineToneEdit.cpp
    src/engine/SineDemoSession.cpp
    src/engine/ArrangementEdit.cpp
    src/engine/ClipImporter.cpp
    src/engine/ClipOps.cpp
    src/engine/EditUndo.cpp
    src/rt/RealtimeGuard.cpp)

target_include_directories(daw_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/readerwriterqueue")

target_compile_features(daw_core PUBLIC cxx_std_20)

# RT debug allocation guard: ON in Debug, OFF (zero overhead) otherwise. PUBLIC
# so consumers (daw_tests) see the same DAW_RT_CHECKS state as the guard TU did.
target_compile_definitions(daw_core PUBLIC
    $<IF:$<CONFIG:Debug>,DAW_RT_CHECKS=1,DAW_RT_CHECKS=0>)

target_link_libraries(daw_core PUBLIC daw_engine)
target_link_libraries(daw_core PRIVATE juce::juce_recommended_warning_flags)

if(APPLE)
    target_compile_options(daw_core PRIVATE "-fno-aligned-allocation")
endif()

# PCH: JUCE + tracktion umbrella headers (no unity builds). Safe here because
# daw_core compiles only my own C++ TUs (no JUCE module amalgamation files).
# The wrapper is __cplusplus-guarded so the C-language PCH CMake also emits
# (project enables C) is a harmless no-op.
target_precompile_headers(daw_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/DawCorePCH.h")

# ============================================================================
#  EZStudio — the GUI application
# ============================================================================
juce_add_gui_app(EZStudio
    PRODUCT_NAME "EZStudio"
    BUNDLE_ID "com.parthivnair.ezstudio"
    COMPANY_NAME "Parthiv Nair")

juce_generate_juce_header(EZStudio)

target_sources(EZStudio PRIVATE src/ui/Main.cpp)

target_compile_features(EZStudio PRIVATE cxx_std_20)

set_target_properties(EZStudio PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

# Link daw_core only: it re-exports (via daw_engine, $<LINK_ONLY>) all the JUCE
# modules + frameworks already compiled into libdaw_engine.a, so the app does NOT
# recompile any JUCE module source — which is also what lets EZStudio carry a PCH
# (no JUCE module amalgamation .mm files land in this target to conflict with it).
target_link_libraries(EZStudio PRIVATE
    daw_core
    juce::juce_recommended_warning_flags)

target_precompile_headers(EZStudio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/EZStudioPCH.h")

if(APPLE)
    target_compile_options(EZStudio PRIVATE "-fno-aligned-allocation")
endif()

# ============================================================================
#  daw_tests — Catch2 console runner (links daw_core ONLY, never EZStudio)
# ============================================================================
if(DAW_BUILD_TESTS)
    enable_testing()

    add_subdirectory(libs/Catch2)

    # melatonin_test_helpers + melatonin_audio_sparklines are JUCE modules.
    juce_add_module("${CMAKE_CURRENT_SOURCE_DIR}/libs/melatonin_audio_sparklines")
    juce_add_module("${CMAKE_CURRENT_SOURCE_DIR}/libs/melatonin_test_helpers")

    add_executable(daw_tests
        tests/TestMain.cpp
        tests/SanityTest.cpp
        tests/RtSpscQueueTest.cpp
        tests/RtGuardTest.cpp
        tests/render/SineRenderTest.cpp
        tests/render/ImportClipRenderTest.cpp
        tests/render/ClipOpsRenderTest.cpp
        tests/render/UndoNullRenderTest.cpp)

    # tests/render/ headers include themselves via the "render/" prefix.
    target_include_directories(daw_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    target_compile_features(daw_tests PRIVATE cxx_std_20)

    target_link_libraries(daw_tests PRIVATE
        daw_core
        Catch2::Catch2WithMain
        melatonin_audio_sparklines
        melatonin_test_helpers
        juce::juce_dsp
        juce::juce_recommended_warning_flags)

    if(APPLE)
        target_compile_options(daw_tests PRIVATE "-fno-aligned-allocation")
    endif()

    include(Catch)
    catch_discover_tests(daw_tests)
endif()

# ============================================================================
#  rt_violation_demo — rtsan NEGATIVE-test target.
#
#  Proves the rtsan lane detects a real-time violation: its main() calls an
#  RT_NONBLOCKING function that heap-allocates, which must abort with an RTSan
#  diagnostic under -fsanitize=realtime. Built ONLY when the realtime sanitizer
#  flag is active (the rtsan preset), and deliberately NOT registered with ctest
#  — it is SUPPOSED to crash, so it must never join the default green suite.
#  Run it by hand: build/rtsan/rt_violation_demo  (expected: non-zero exit).
# ============================================================================
if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=realtime")
    add_executable(rt_violation_demo tests/rt_violation_demo.cpp)
    target_compile_features(rt_violation_demo PRIVATE cxx_std_20)
    # Header-only need: just src/ on the include path for rt/RTAnnotations.h.
    target_include_directories(rt_violation_demo PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(APPLE)
        target_compile_options(rt_violation_demo PRIVATE "-fno-aligned-allocation")
    endif()
endif()
