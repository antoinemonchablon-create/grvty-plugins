# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE

cmake_minimum_required(VERSION 3.22)

# Numéro de build fourni par build.command (incrémenté à chaque compilation).
# Changer de version force macOS et Ableton à recharger le plugin au lieu de garder l'ancien en cache.
if(NOT DEFINED FF_BUILD_NUMBER)
    set(FF_BUILD_NUMBER 1)
endif()
math(EXPR FF_MINOR "${FF_BUILD_NUMBER} / 256")
math(EXPR FF_PATCH "${FF_BUILD_NUMBER} % 256")

project(FlowFilter VERSION 1.${FF_MINOR}.${FF_PATCH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sur Mac : binaire universel Apple Silicon + Intel, compatible macOS 11+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# --- JUCE ------------------------------------------------------------------
# Si tu as déjà JUCE sur ton disque :  cmake -B build -DJUCE_PATH=~/JUCE
# Sinon il est téléchargé automatiquement depuis GitHub.
set(JUCE_PATH "" CACHE PATH "Chemin vers ton dossier JUCE (optionnel)")
# Adresse publique du code source (dépôt), affichée dans le panneau « À propos » ; vide = archive publiée avec chaque version
set(FF_SOURCE_URL "" CACHE STRING "Adresse publique du code source de Flow Filter (optionnel)")

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
option(FF_INSTALL_AFTER_BUILD "Copier le plugin dans le dossier des plugins après la compilation" ON)

juce_add_plugin(FlowFilter
    COMPANY_NAME                "GRVTY Records"   # fabricant affiché dans Ableton (Plug-Ins > GRVTY Records)
    COMPANY_COPYRIGHT           "Copyright (C) 2026 GRVTY Records. Logiciel libre, GNU AGPL v3."
    BUNDLE_ID                   com.Jasperr.FlowFilter   # identifiant d'origine gardé (macOS, Audio Units)
    PLUGIN_MANUFACTURER_CODE    Jspr      # code interne figé : le changer casserait les projets Ableton existants
    PLUGIN_CODE                 Flwf
    PRODUCT_NAME                "Flow Filter"
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${FF_INSTALL_AFTER_BUILD}   # copie auto dans ~/Library/Audio/Plug-Ins (pas pour les installeurs)
    VST3_CATEGORIES             Fx Filter
    AU_MAIN_TYPE                kAudioUnitType_Effect)

juce_generate_juce_header(FlowFilter)

# Images intégrées au plugin : logo officiel « VST Compatible » de Steinberg (panneau À propos)
juce_add_binary_data(FlowFilterData SOURCES docs/images/VST_Compatible_Logo_Steinberg_with_TM.png)

target_sources(FlowFilter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AboutPanel.cpp)

target_compile_definitions(FlowFilter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0          # décodeurs inutiles ici (moins de code tiers dans le plugin)
    JUCE_USE_OGGVORBIS=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FlowFilter
    PRIVATE
        FlowFilterData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FF_SOURCE_URL)
    target_compile_definitions(FlowFilter PRIVATE "GRVTY_SOURCE_URL=\"${FF_SOURCE_URL}\"")
endif()

# --- Banc de tests (Linux / Mac) : cmake -DFF_BUILD_TESTS=ON puis ./FlowFilterTests -----------------------------
option(FF_BUILD_TESTS "Compiler le petit banc de tests (tests/)" OFF)
if(FF_BUILD_TESTS)
    juce_add_console_app(FlowFilterTests PRODUCT_NAME "FlowFilterTests")
    juce_generate_juce_header(FlowFilterTests)
    target_sources(FlowFilterTests PRIVATE tests/FlowFilterTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/AboutPanel.cpp)
    target_include_directories(FlowFilterTests PRIVATE Source)
    target_compile_definitions(FlowFilterTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Flow Filter"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0)
    target_link_libraries(FlowFilterTests PRIVATE FlowFilterData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
