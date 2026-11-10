cmake_minimum_required(VERSION 3.22)

project(TENSION VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCE: either a local checkout (JUCE_DIR) or downloaded automatically.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Tension
    COMPANY_NAME "Chamba Audio"
    BUNDLE_ID com.chambaaudio.tension
    PLUGIN_MANUFACTURER_CODE Chmb
    PLUGIN_CODE Tnsn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "TENSION"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb Filter
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Tension PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Tension PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Tension
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline render / screenshot tool (not part of the plugin)
option(TENSION_BUILD_TESTS "Build offline render tool" OFF)
if(TENSION_BUILD_TESTS)
    juce_add_console_app(TensionRender PRODUCT_NAME "TensionRender")
    target_sources(TensionRender PRIVATE tests/Render.cpp)
    target_compile_definitions(TensionRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(TensionRender PRIVATE Tension juce::juce_audio_utils juce::juce_dsp)
endif()
