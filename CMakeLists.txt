cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HARMONY_VERSION LIMIT_COUNT 1)
project(YannosayHarmony VERSION ${HARMONY_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(HARMONY_BUILD_TESTS "Build the Harmony test runner" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(HarmonyAssets
    HEADER_NAME HarmonyAssets.h
    NAMESPACE HarmonyAssets
    SOURCES
        assets/fonts/Unbounded-Light.ttf
        assets/fonts/Unbounded-Regular.ttf
        assets/fonts/Unbounded-Medium.ttf
        assets/fonts/Unbounded-SemiBold.ttf
        assets/fonts/Unbounded-Bold.ttf
        assets/brand/harmony-mark.svg
        assets/brand/harmony-icon-256.png)

set(HARMONY_CORE_SOURCES
    src/model/Ids.h
    src/model/Project.h
    src/model/Project.cpp
    src/model/MusicTheory.h
    src/model/MusicTheory.cpp
    src/model/ChordGenerator.h
    src/model/ChordGenerator.cpp
    src/model/DrumGenerator.h
    src/model/DrumGenerator.cpp
    src/engine/Snapshot.h
    src/engine/SnapshotBuilder.h
    src/engine/SnapshotBuilder.cpp
    src/engine/ProjectSync.h
    src/engine/ProjectSync.cpp
    src/engine/Gain.h
    src/engine/ChannelNodes.h
    src/engine/ChannelNodes.cpp
    src/engine/MixerNode.h
    src/engine/MixerNode.cpp
    src/engine/AudioEngine.h
    src/engine/AudioEngine.cpp
    src/engine/PluginManager.h
    src/engine/PluginManager.cpp
    src/engine/Renderer.h
    src/engine/Renderer.cpp
    src/update/Version.h
    src/update/Updater.h
    src/update/Updater.cpp)

set(HARMONY_UI_SOURCES
    src/Main.cpp
    src/app/Application.h
    src/app/Application.cpp
    src/app/CommandIds.h
    src/app/Context.h
    src/app/MainComponent.h
    src/app/MainComponent.cpp
    src/app/History.h
    src/app/History.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/SmoothView.h
    src/ui/SmoothView.cpp
    src/ui/Widgets.h
    src/ui/Widgets.cpp
    src/ui/TopBar.h
    src/ui/TopBar.cpp
    src/ui/Browser.h
    src/ui/Browser.cpp
    src/ui/ChannelRack.h
    src/ui/ChannelRack.cpp
    src/ui/PianoRoll.h
    src/ui/PianoRoll.cpp
    src/ui/Playlist.h
    src/ui/Playlist.cpp
    src/ui/MixerView.h
    src/ui/MixerView.cpp
    src/ui/PluginWindows.h
    src/ui/PluginWindows.cpp
    src/ui/Dialogs.h
    src/ui/Dialogs.cpp
    src/ui/CommandPalette.h
    src/ui/CommandPalette.cpp)

set(HARMONY_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STANDALONE_APPLICATION=1
    HARMONY_VERSION_STRING="${PROJECT_VERSION}")

set(HARMONY_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_cryptography
    juce::juce_gui_extra
    juce::juce_animation)

add_library(HarmonyCore STATIC ${HARMONY_CORE_SOURCES} src/engine/TinySoundFont.cpp)
target_include_directories(HarmonyCore PUBLIC src third_party third_party/fst)
target_compile_definitions(HarmonyCore PUBLIC ${HARMONY_DEFINITIONS})
target_link_libraries(HarmonyCore
    PRIVATE ${HARMONY_JUCE_MODULES}
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
target_include_directories(HarmonyCore INTERFACE $<TARGET_PROPERTY:HarmonyCore,INCLUDE_DIRECTORIES>)
target_compile_definitions(HarmonyCore INTERFACE $<TARGET_PROPERTY:HarmonyCore,COMPILE_DEFINITIONS>)

juce_add_gui_app(Harmony
    PRODUCT_NAME "Yannosay Harmony"
    COMPANY_NAME "Yannosay Productions"
    COMPANY_WEBSITE "https://harmony.yannosay.com"
    BUNDLE_ID com.yannosay.harmony
    ICON_BIG assets/brand/harmony-icon-1024.png
    ICON_SMALL assets/brand/harmony-icon-256.png
    VERSION ${PROJECT_VERSION}
    DOCUMENT_EXTENSIONS hmy
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Harmony records audio from your input devices.")

target_sources(Harmony PRIVATE ${HARMONY_UI_SOURCES})
target_link_libraries(Harmony PRIVATE HarmonyCore HarmonyAssets)
set_target_properties(Harmony PROPERTIES OUTPUT_NAME "Harmony")

if(HARMONY_BUILD_TESTS)
    juce_add_console_app(HarmonyTests PRODUCT_NAME "HarmonyTests")
    target_sources(HarmonyTests PRIVATE
        tests/TestMain.cpp
        tests/ModelTests.cpp
        tests/TheoryTests.cpp
        tests/EngineTests.cpp
        tests/UpdaterTests.cpp
        tests/PluginHostTests.cpp)
    target_link_libraries(HarmonyTests PRIVATE HarmonyCore)
    enable_testing()
    add_test(NAME HarmonyTests COMMAND HarmonyTests)
endif()
