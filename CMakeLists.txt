cmake_minimum_required(VERSION 3.25)
project(VVChain VERSION 1.0.21 LANGUAGES C CXX)

include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VVCHAIN_ENABLE_AAX "Build AAX target when the local environment provides the SDK" OFF)
option(VVCHAIN_BUILD_STANDALONE "Build standalone target" ON)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(VVCHAIN_FORMATS VST3)
if (VVCHAIN_BUILD_STANDALONE)
    list(APPEND VVCHAIN_FORMATS Standalone)
endif()
if (VVCHAIN_ENABLE_AAX)
    list(APPEND VVCHAIN_FORMATS AAX)
endif()

juce_add_plugin(VVChain
    COMPANY_NAME "VVChain"
    BUNDLE_ID "com.vvchain.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE VVCn
    PLUGIN_CODE VvCh
    FORMATS ${VVCHAIN_FORMATS}
    PRODUCT_NAME "VVChain"
)

target_sources(VVChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/ChainDSP.cpp
    Source/DSP/ChainDSP.h
)

target_compile_definitions(VVChain PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VVChain PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)

juce_generate_juce_header(VVChain)

enable_testing()
add_test(NAME vvchain_reference_stress
    COMMAND ${CMAKE_COMMAND} -E echo "Run Tests/reference_stress.py with Python 3.11+")
