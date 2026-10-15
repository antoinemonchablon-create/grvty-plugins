# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE

cmake_minimum_required(VERSION 3.22)

# Numéro de build fourni par build.command (incrémenté à chaque compilation).
# Changer de version force macOS et Ableton à recharger le plugin au lieu de garder l'ancien en cache.
if(NOT DEFINED EB_BUILD_NUMBER)
    set(EB_BUILD_NUMBER 1)
endif()
math(EXPR EB_MINOR "${EB_BUILD_NUMBER} / 256")
math(EXPR EB_PATCH "${EB_BUILD_NUMBER} % 256")

project(Ebb VERSION 1.${EB_MINOR}.${EB_PATCH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(EB_BUILD_TESTS "Compiler le banc de tests (tests/)" OFF)

# --- JUCE ------------------------------------------------------------------
set(JUCE_PATH "" CACHE PATH "Chemin vers ton dossier JUCE (optionnel)")
# Adresse publique du code source (dépôt), affichée dans le panneau « À propos » ; vide = archive publiée avec chaque version
set(EB_SOURCE_URL "" CACHE STRING "Adresse publique du code source d'Ebb (optionnel)")
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
# Effet audio avec entrée sidechain (mode AUDIO) et entrée MIDI (mode MIDI).
# AU de type « Music Effect » (aumf) : c'est le type d'effet Audio Unit qui accepte le MIDI.
# --- Windows (Visual Studio) : sources en UTF-8, runtime C++ intégré au plugin -----------------------------
if(MSVC)
    add_compile_options(/utf-8)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
option(EB_INSTALL_AFTER_BUILD "Copier le plugin dans le dossier des plugins après la compilation" ON)

juce_add_plugin(Ebb
    COMPANY_NAME                "GRVTY Records"   # fabricant affiché dans Ableton (Plug-Ins > GRVTY Records)
    COMPANY_COPYRIGHT           "Copyright (C) 2026 GRVTY Records. Logiciel libre, GNU AGPL v3."
    BUNDLE_ID                   com.jasperr.ebb   # identifiant d'origine gardé (macOS, Audio Units)
    PLUGIN_MANUFACTURER_CODE    Jspr      # code interne figé : le changer casserait les projets Ableton existants
    PLUGIN_CODE                 Ebbp
    PRODUCT_NAME                "Ebb"
    FORMATS                     VST3 AU
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${EB_INSTALL_AFTER_BUILD}   # copie auto dans ~/Library/Audio/Plug-Ins (pas pour les installeurs)
    VST3_CATEGORIES             Fx Dynamics
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect)

juce_generate_juce_header(Ebb)

# Images intégrées au plugin : logo officiel « VST Compatible » de Steinberg (panneau À propos)
juce_add_binary_data(EbbData SOURCES docs/images/VST_Compatible_Logo_Steinberg_with_TM.png)

set(EB_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/LiquidUI.cpp
    Source/ui/Controls.cpp
    Source/ui/CurveView.cpp
    Source/ui/CurveStrip.cpp
    Source/ui/AboutPanel.cpp)

target_sources(Ebb PRIVATE ${EB_SOURCES})

set(EB_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0          # décodeurs inutiles ici (moins de code tiers dans le plugin)
    JUCE_USE_OGGVORBIS=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(EB_SOURCE_URL)
    list(APPEND EB_DEFINITIONS "GRVTY_SOURCE_URL=\"${EB_SOURCE_URL}\"")
endif()

target_compile_definitions(Ebb PUBLIC ${EB_DEFINITIONS})

target_link_libraries(Ebb
    PRIVATE
        EbbData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Banc de tests -----------------------------------------------------------
if(EB_BUILD_TESTS)
    juce_add_console_app(EbbTests PRODUCT_NAME "EbbTests")
    juce_generate_juce_header(EbbTests)
    target_sources(EbbTests PRIVATE tests/EbbTests.cpp ${EB_SOURCES})
    target_include_directories(EbbTests PRIVATE Source)
    target_compile_definitions(EbbTests PRIVATE ${EB_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Ebb"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0)
    target_link_libraries(EbbTests PRIVATE
        EbbData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
