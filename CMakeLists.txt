cmake_minimum_required(VERSION 3.22)

# Tested toolchain: CMake 4.4.3, Apple clang 21, JUCE 8.0.17-security.
# Pass an explicit architecture/deployment target on the first configuration.
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
project(LabRC VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)

option(LABRC_BUILD_PLUGIN "Build plugin formats, processor/UI tests and renderer" ON)
option(LABRC_ENABLE_SANITIZERS "ASan/UBSan for a local test build" OFF)
set(LABRC_KIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Assets/ImplementationKit" CACHE PATH
    "Read-only implementation kit used by offline tests")
set(LABRC_FINAL_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Assets/FinalTouch" CACHE PATH
    "Original Final Touch backgrounds and transparent drum kit")
set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/juce" CACHE PATH
    "Local JUCE 8.0.17-security, prepared by scripts/prepare_juce.py")
include(cmake/LocalJuce.cmake)

add_library(labrc_build_options INTERFACE)
target_compile_definitions(labrc_build_options INTERFACE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)
if(MSVC)
    target_compile_options(labrc_build_options INTERFACE /utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(LABRC_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "LABRC_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
    target_compile_options(labrc_build_options INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(labrc_build_options INTERFACE -fsanitize=address,undefined)
endif()

file(GLOB labrc_dsp_sources CONFIGURE_DEPENDS "Source/DSP/*.cpp")
add_library(labrc_dsp STATIC ${labrc_dsp_sources})
set_target_properties(labrc_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(labrc_dsp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_link_libraries(labrc_dsp PUBLIC labrc_build_options
    PRIVATE juce::juce_dsp juce::juce_audio_formats)

if(LABRC_BUILD_PLUGIN)
    set(labrc_assets "${LABRC_FINAL_ASSET_ROOT}/boost_this.png"
        "${LABRC_FINAL_ASSET_ROOT}/approved_metal.png"
        "${LABRC_FINAL_ASSET_ROOT}/approved_screw.png")
    foreach(room IN ITEMS kentucky center_field live_room)
        list(APPEND labrc_assets "${LABRC_FINAL_ASSET_ROOT}/${room}_clean.png")
        foreach(pair IN ITEMS EARS FRONT FLOOR)
            list(APPEND labrc_assets
                "${LABRC_KIT_ROOT}/assets/ir/v06/${room}_${pair}_wet_matrix.wav")
        endforeach()
    endforeach()
    juce_add_binary_data(LabRCAssets HEADER_NAME LabRCAssets.h NAMESPACE LabRCAssets
        SOURCES ${labrc_assets})
    set_target_properties(LabRCAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    set(labrc_formats Standalone VST3)
    if(APPLE)
        list(APPEND labrc_formats AU)
    endif()
    include(cmake/WindowsVst3Wrapper.cmake)
    labrc_configure_windows_vst3_wrapper()
    juce_add_plugin(LabRC
        COMPANY_NAME VITAPLUG
        BUNDLE_ID com.vitaplug.labrc
        PLUGIN_MANUFACTURER_CODE Vtpl
        PLUGIN_CODE LbRc
        PRODUCT_NAME "LAB-RC"
        DESCRIPTION "Manza's Rooms & Chambers - A Tribute to Steve Albini"
        AU_EXPORT_PREFIX LabRC
        AU_MAIN_TYPE kAudioUnitType_Effect
        AU_SANDBOX_SAFE TRUE
        FORMATS ${labrc_formats}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Reverb)
    file(GLOB labrc_ui_sources CONFIGURE_DEPENDS "Source/UI/*.cpp")
    target_sources(LabRC PRIVATE Source/PluginProcessor.cpp Source/Resources.cpp
        Source/PluginEditor.cpp ${labrc_ui_sources})
    target_include_directories(LabRC PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_link_libraries(LabRC PRIVATE labrc_dsp LabRCAssets
        juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
        PUBLIC labrc_build_options juce::juce_recommended_config_flags)
    if(APPLE)
        target_compile_definitions(LabRC_AU PRIVATE AUSDK_NO_LOGGING=1)
    endif()

    function(labrc_link_shared_code target)
        target_link_libraries(${target} PRIVATE LabRC)
        target_include_directories(${target} PRIVATE
            $<TARGET_PROPERTY:LabRC,INCLUDE_DIRECTORIES>)
        target_compile_definitions(${target} PRIVATE
            $<FILTER:$<TARGET_PROPERTY:LabRC,COMPILE_DEFINITIONS>,EXCLUDE,^JUCE_STANDALONE_APPLICATION=>)
    endfunction()

    add_executable(labrc_render_ui Tests/RenderUI.cpp)
    labrc_link_shared_code(labrc_render_ui)
    set_target_properties(labrc_render_ui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
endif()

if(BUILD_TESTING)
    add_executable(labrc_tests Tests/TestMain.cpp Tests/DSPTests.cpp)
    target_compile_definitions(labrc_tests PRIVATE LABRC_KIT_ROOT="${LABRC_KIT_ROOT}"
        LABRC_FINAL_ASSET_ROOT="${LABRC_FINAL_ASSET_ROOT}")
    if(LABRC_BUILD_PLUGIN)
        target_sources(labrc_tests PRIVATE Tests/ProcessorTests.cpp Tests/UITests.cpp)
        target_compile_definitions(labrc_tests PRIVATE LABRC_TEST_GUI=1)
        labrc_link_shared_code(labrc_tests)

        add_executable(labrc_final_touch_regression Tests/FinalTouchRegression.cpp)
        labrc_link_shared_code(labrc_final_touch_regression)
        set_target_properties(labrc_final_touch_regression PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    else()
        target_link_libraries(labrc_tests PRIVATE labrc_dsp
            juce::juce_dsp juce::juce_audio_formats)
        target_compile_definitions(labrc_tests PRIVATE LABRC_TEST_GUI=0)
    endif()
    set_target_properties(labrc_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME DSP COMMAND labrc_tests --category DSP)
    set_tests_properties(DSP PROPERTIES LABELS "DSP" TIMEOUT 1200)
    if(LABRC_BUILD_PLUGIN)
        add_test(NAME Processor COMMAND labrc_tests --category Processor)
        add_test(NAME UI COMMAND labrc_tests --category UI)
        set_tests_properties(Processor UI PROPERTIES TIMEOUT 180)
        set_tests_properties(Processor PROPERTIES LABELS "Processor")
        set_tests_properties(UI PROPERTIES LABELS "UI")
    endif()
    if(APPLE)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME BuildTools COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_build_tools.py")
        add_test(NAME Bootstrap COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_prepare_juce.py")
        set_tests_properties(BuildTools PROPERTIES LABELS "Build" TIMEOUT 120
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        set_tests_properties(Bootstrap PROPERTIES LABELS "Build" TIMEOUT 120
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
endif()
