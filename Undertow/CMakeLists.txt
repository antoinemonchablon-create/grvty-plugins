# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE

cmake_minimum_required(VERSION 3.22)

# Numéro de build fourni par build.command (incrémenté à chaque compilation).
# Changer de version force macOS et Ableton à recharger le plugin au lieu de garder l'ancien en cache.
if(NOT DEFINED UT_BUILD_NUMBER)
    set(UT_BUILD_NUMBER 1)
endif()
math(EXPR UT_MINOR "${UT_BUILD_NUMBER} / 256")
math(EXPR UT_PATCH "${UT_BUILD_NUMBER} % 256")

project(Undertow VERSION 1.${UT_MINOR}.${UT_PATCH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(UT_BUILD_TESTS "Compiler le banc de tests (tests/)" OFF)

# --- JUCE ------------------------------------------------------------------
set(JUCE_PATH "" CACHE PATH "Chemin vers ton dossier JUCE (optionnel)")
# Adresse publique du code source (dépôt), affichée dans le panneau « À propos » ; vide = archive publiée avec chaque version
set(UT_SOURCE_URL "" CACHE STRING "Adresse publique du code source d'Undertow (optionnel)")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.12
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin ----------------------------------------------------------------
# --- Windows (Visual Studio) : sources en UTF-8, runtime C++ intégré au plugin -----------------------------
if(MSVC)
    add_compile_options(/utf-8)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
option(UT_INSTALL_AFTER_BUILD "Copier le plugin dans le dossier des plugins après la compilation" ON)

juce_add_plugin(Undertow
    COMPANY_NAME                "GRVTY Records"   # fabricant affiché dans Ableton (Plug-Ins > GRVTY Records)
    COMPANY_COPYRIGHT           "Copyright (C) 2026 GRVTY Records. Logiciel libre, GNU AGPL v3."
    BUNDLE_ID                   com.Jasperr.Undertow   # identifiant d'origine gardé (macOS, Audio Units)
    PLUGIN_MANUFACTURER_CODE    Jspr      # code interne figé : le changer casserait les projets Ableton existants
    PLUGIN_CODE                 Undt
    PRODUCT_NAME                "Undertow"
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${UT_INSTALL_AFTER_BUILD}   # copie auto dans ~/Library/Audio/Plug-Ins (pas pour les installeurs)
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

juce_generate_juce_header(Undertow)

# Images intégrées au plugin : logo officiel « VST Compatible » de Steinberg (panneau À propos)
juce_add_binary_data(UndertowData SOURCES docs/images/VST_Compatible_Logo_Steinberg_with_TM.png)

set(UT_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/LiquidUI.cpp
    Source/ui/PresetBar.cpp
    Source/ui/FmScreen.cpp
    Source/ui/SeqGrid.cpp
    Source/ui/AboutPanel.cpp)

target_sources(Undertow PRIVATE ${UT_SOURCES})

set(UT_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0          # décodeurs inutiles ici (moins de code tiers dans le plugin)
    JUCE_USE_OGGVORBIS=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UT_SOURCE_URL)
    list(APPEND UT_DEFINITIONS "GRVTY_SOURCE_URL=\"${UT_SOURCE_URL}\"")
endif()

target_compile_definitions(Undertow PUBLIC ${UT_DEFINITIONS})

target_link_libraries(Undertow
    PRIVATE
        UndertowData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Banc de tests -----------------------------------------------------------
if(UT_BUILD_TESTS)
    foreach(T UndertowTests UndertowProfile)
        juce_add_console_app(${T} PRODUCT_NAME "${T}")
        juce_generate_juce_header(${T})
        target_sources(${T} PRIVATE tests/UndertowTests.cpp ${UT_SOURCES})
        target_include_directories(${T} PRIVATE Source)
        target_compile_definitions(${T} PRIVATE ${UT_DEFINITIONS}
            JUCE_MODAL_LOOPS_PERMITTED=1
            JucePlugin_Name="Undertow"
            JucePlugin_VersionString="${PROJECT_VERSION}"
            JucePlugin_IsSynth=1
            JucePlugin_WantsMidiInput=1
            JucePlugin_ProducesMidiOutput=0
            JucePlugin_IsMidiEffect=0)
        target_link_libraries(${T} PRIVATE
            UndertowData
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags)
    endforeach()
    target_compile_definitions(UndertowProfile PRIVATE UNDERTOW_PROFILE=1 UT_PROFILE_ONLY=1)
endif()
