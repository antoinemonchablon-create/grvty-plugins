# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE

cmake_minimum_required(VERSION 3.22)

# Numéro de build fourni par build.command (incrémenté à chaque compilation).
# Changer de version force Ableton à recharger le plugin au lieu de garder l'ancien en cache.
# Sans numéro fourni, on lit .build_number (suivi par git) : une version compilée depuis un tag garde son numéro.
if(NOT DEFINED CF_BUILD_NUMBER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.build_number")
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.build_number" CF_BUILD_NUMBER LIMIT_COUNT 1)
    else()
        set(CF_BUILD_NUMBER 1)
    endif()
endif()
math(EXPR CF_MINOR "${CF_BUILD_NUMBER} / 256")
math(EXPR CF_PATCH "${CF_BUILD_NUMBER} % 256")

project(Confluence VERSION 1.${CF_MINOR}.${CF_PATCH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(CF_BUILD_TESTS "Compiler le banc de tests (tests/)" OFF)
option(CF_INSTALL_AFTER_BUILD "Copier le plugin dans le dossier des plugins après la compilation" ON)

# --- JUCE ------------------------------------------------------------------
set(JUCE_PATH "" CACHE PATH "Chemin vers ton dossier JUCE (optionnel)")
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
# Instrument VST3 qui reçoit du MIDI et renvoie du MIDI : c'est la seule façon, dans Ableton, de placer
# un générateur d'accords en tête de piste et de router sa sortie MIDI vers la piste d'un synthé.
# Pas d'AU : Ableton ne récupère pas la sortie MIDI d'un Audio Unit.
# Windows : le runtime C++ de Visual Studio est intégré au plugin (aucune DLL à installer, pas de conflit
# avec une autre version déjà chargée par l'hôte)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

juce_add_plugin(Confluence
    COMPANY_NAME                "GRVTY Records"   # fabricant affiché dans Ableton (Plug-Ins > VST3 > GRVTY Records)
    COMPANY_COPYRIGHT           "Copyright (C) 2026 GRVTY Records. Logiciel libre, GNU AGPL v3."
    BUNDLE_ID                   com.jasperr.confluence
    PLUGIN_MANUFACTURER_CODE    Jspr      # code interne figé : le changer casserait les projets Ableton existants
    PLUGIN_CODE                 Cnfl
    PRODUCT_NAME                "Confluence"
    FORMATS                     VST3
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${CF_INSTALL_AFTER_BUILD}   # copie auto dans ~/Library/Audio/Plug-Ins/VST3 (pas pour release.command)
    VST3_CATEGORIES             Instrument Tools)

juce_generate_juce_header(Confluence)

# Images intégrées au plugin : logo officiel « VST Compatible » de Steinberg (boîte À propos)
juce_add_binary_data(ConfluenceData SOURCES docs/images/VST_Compatible_Logo_Steinberg_with_TM.png)

set(CF_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/ui/LiquidUI.cpp
    Source/ui/PresetBar.cpp
    Source/ui/SeaView.cpp
    Source/ui/Whirlpool.cpp
    Source/ui/PadGrid.cpp
    Source/ui/AboutPanel.cpp)

target_sources(Confluence PRIVATE ${CF_SOURCES})

# Le code de Confluence n'appelle aucune fonction réseau (seul le panneau À propos peut demander au navigateur d'ouvrir
# une page). Ces options retirent en plus le navigateur web intégré de JUCE, curl (Linux) et les décodeurs inutiles.
set(CF_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Adresse publique du code source (dépôt), affichée dans le panneau « À propos ».
# Vide tant que le dépôt n'est pas publié : le panneau renvoie alors à l'archive source publiée avec chaque version.
set(CF_SOURCE_URL "" CACHE STRING "Adresse publique du code source de Confluence")
if(CF_SOURCE_URL)
    list(APPEND CF_DEFINITIONS "CONFLUENCE_SOURCE_URL=\"${CF_SOURCE_URL}\"")
endif()

target_compile_definitions(Confluence PUBLIC ${CF_DEFINITIONS})

# Visual Studio : les sources sont en UTF-8 (commentaires accentués)
if(MSVC)
    target_compile_options(Confluence PUBLIC /utf-8)
endif()

target_link_libraries(Confluence
    PRIVATE
        ConfluenceData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Banc de tests -----------------------------------------------------------
if(CF_BUILD_TESTS)
    juce_add_console_app(ConfluenceTests PRODUCT_NAME "ConfluenceTests")
    juce_generate_juce_header(ConfluenceTests)
    target_sources(ConfluenceTests PRIVATE tests/ConfluenceTests.cpp ${CF_SOURCES})
    target_include_directories(ConfluenceTests PRIVATE Source)
    target_compile_definitions(ConfluenceTests PRIVATE ${CF_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Confluence"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=1
        JucePlugin_IsMidiEffect=0)
    if(MSVC)
        target_compile_options(ConfluenceTests PRIVATE /utf-8)
    endif()
    target_link_libraries(ConfluenceTests PRIVATE
        ConfluenceData
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
