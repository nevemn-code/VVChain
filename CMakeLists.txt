cmake_minimum_required(VERSION 3.25)
project(VVChain VERSION 1.0.71 LANGUAGES C CXX)

include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VVCHAIN_ENABLE_AAX "Build AAX target when the local environment provides the SDK" OFF)
option(VVCHAIN_BUILD_STANDALONE "Build standalone target" ON)
option(VVCHAIN_COPY_PLUGIN_AFTER_BUILD "Copy plugin to the local plugin folder after build" ON)

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

juce_add_binary_data(VVChainAssets
    HEADER_NAME VVChainAssets.h
    NAMESPACE VVChainAssets
    SOURCES
        docs/assets/ui/png/ivory_panel.png
        docs/assets/ui/png/ivory_module.png
        docs/assets/ui/png/ivory_graph.png
        docs/assets/ui/png/ivory_knob_strip.png
        docs/assets/ui/png/ivory_button_off.png
        docs/assets/ui/png/ivory_button_on.png
        docs/assets/ui/png/ivory_button_pressed.png
        docs/assets/ui/png/ivory_led_off.png
        docs/assets/ui/png/ivory_led_on.png
        docs/assets/ui/png/ivory_power_off.png
        docs/assets/ui/png/ivory_power_on.png
        docs/assets/ui/png/ivory_screw.png
        docs/assets/ui/png/ivory_slider_track.png
        docs/assets/ui/png/ivory_slider_thumb.png
        docs/assets/ui/png/studio_panel.png
        docs/assets/ui/png/studio_module.png
        docs/assets/ui/png/studio_graph.png
        docs/assets/ui/png/studio_knob_strip.png
        docs/assets/ui/png/studio_button_off.png
        docs/assets/ui/png/studio_button_on.png
        docs/assets/ui/png/studio_button_pressed.png
        docs/assets/ui/png/studio_led_off.png
        docs/assets/ui/png/studio_led_on.png
        docs/assets/ui/png/studio_power_off.png
        docs/assets/ui/png/studio_power_on.png
        docs/assets/ui/png/studio_screw.png
        docs/assets/ui/png/studio_slider_track.png
        docs/assets/ui/png/studio_slider_thumb.png
        docs/assets/ui/png/muted_panel.png
        docs/assets/ui/png/muted_module.png
        docs/assets/ui/png/muted_graph.png
        docs/assets/ui/png/muted_knob_strip.png
        docs/assets/ui/png/muted_button_off.png
        docs/assets/ui/png/muted_button_on.png
        docs/assets/ui/png/muted_button_pressed.png
        docs/assets/ui/png/muted_led_off.png
        docs/assets/ui/png/muted_led_on.png
        docs/assets/ui/png/muted_power_off.png
        docs/assets/ui/png/muted_power_on.png
        docs/assets/ui/png/muted_screw.png
        docs/assets/ui/png/muted_slider_track.png
        docs/assets/ui/png/muted_slider_thumb.png
        docs/assets/ui/png/button_disabled.png
        docs/assets/ui/png/bypass_led_red.png
)

juce_add_plugin(VVChain
    COMPANY_NAME "VVChain"
    BUNDLE_ID "com.vvchain.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${VVCHAIN_COPY_PLUGIN_AFTER_BUILD}
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
    Source/SettingsPanel.cpp
    Source/SettingsPanel.h
    Source/DSP/ChainDSP.cpp
    Source/DSP/ChainDSP.h
)

execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE VVCHAIN_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT VVCHAIN_GIT_COMMIT)
    set(VVCHAIN_GIT_COMMIT "unknown")
endif()

target_compile_definitions(VVChain PRIVATE
    VVCHAIN_GIT_COMMIT="${VVCHAIN_GIT_COMMIT}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VVChain PRIVATE
    VVChainAssets
    juce::juce_audio_utils
    juce::juce_dsp
)

juce_generate_juce_header(VVChain)

