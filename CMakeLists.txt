cmake_minimum_required(VERSION 3.22)
project(Plosivore VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# --- JUCE: use -DJUCE_PATH=/path/to/JUCE if you have it, otherwise download it.
if (JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Formats. AU only exists on macOS. AAX (Pro Tools) needs Avid's SDK:
#     pass -DAAX_SDK_PATH=/path/to/aax-sdk to enable it.
set(PLOSIVORE_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND PLOSIVORE_FORMATS AU)
endif()
if (AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND PLOSIVORE_FORMATS AAX)
endif()

option(PLOSIVORE_COPY_AFTER_BUILD "Install plugin into the system plugin folder after building" OFF)

juce_add_plugin(Plosivore
    COMPANY_NAME "Plosivore Audio"
    PRODUCT_NAME "Plosivore"
    PLUGIN_MANUFACTURER_CODE Plsv
    PLUGIN_CODE Pv01
    FORMATS ${PLOSIVORE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Plosivore needs audio input to remove pops from your microphone in standalone mode."
    COPY_PLUGIN_AFTER_BUILD ${PLOSIVORE_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_Dynamics
    BUNDLE_ID com.plosivore.plosivore)

juce_add_binary_data(PlosivoreAssets SOURCES
    Resources/Bricolage-ExtraBold.ttf
    Resources/Bricolage-Medium.ttf)

target_sources(Plosivore PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Plosivore PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Plosivore
    PRIVATE
        PlosivoreAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------ Dev tools
# Headless Linux check build (CI sandboxes without ALSA/Xrandr headers):
option(PLOSIVORE_LINUX_MINIMAL "Disable optional Linux system deps (ALSA/JACK/Xrandr/Xinerama/Xcursor)" OFF)
if (PLOSIVORE_LINUX_MINIMAL)
    target_compile_definitions(Plosivore PUBLIC JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0
                                                JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

# Renders the editor while the real processor plays a test vocal, then saves a PNG.
option(PLOSIVORE_BUILD_SNAPSHOT_TOOL "Build the UI snapshot dev tool" OFF)
if (PLOSIVORE_BUILD_SNAPSHOT_TOOL)
    juce_add_console_app(PlosivoreSnapshot PRODUCT_NAME "PlosivoreSnapshot")
    target_sources(PlosivoreSnapshot PRIVATE Tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(PlosivoreSnapshot PRIVATE JucePlugin_Name="Plosivore" JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PlosivoreSnapshot PRIVATE PlosivoreAssets juce::juce_audio_utils juce::juce_dsp
                          juce::juce_recommended_config_flags)
endif()
