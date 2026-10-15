; SPDX-License-Identifier: AGPL-3.0-or-later
; Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE
;
; Installeur Windows de Confluence (NSIS 3). Appelé par packaging/windows/build-windows.sh :
;   makensis -INPUTCHARSET UTF8 -DVERSION=1.0.4 -DPLUGIN_DIR=<Confluence.vst3> -DDOCS_DIR=<textes> -DOUTFILE=<setup.exe>
;            [-DSOURCE_URL=<adresse du dépôt>] installer.nsi
; Le plugin va dans C:\Program Files\Common Files\VST3\Confluence.vst3 (dossier VST3 standard de Windows),
; les textes et le désinstalleur dans C:\Program Files\GRVTY Records\Confluence.

Unicode true

!ifndef VERSION
  !error "VERSION manquant (-DVERSION=1.0.4)"
!endif
!ifndef PLUGIN_DIR
  !error "PLUGIN_DIR manquant (dossier Confluence.vst3 compilé)"
!endif
!ifndef DOCS_DIR
  !error "DOCS_DIR manquant (dossier des textes : LICENSE, NOTICE...)"
!endif
!ifndef OUTFILE
  !error "OUTFILE manquant (chemin de l'installeur .exe)"
!endif

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!define PRODUCT     "Confluence"
!define PUBLISHER   "GRVTY Records"
!define UNINST_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\GRVTY Records Confluence"
!define UNINST_EXE  "Desinstaller Confluence.exe"
!define VST3_DIR    "$COMMONFILES64\VST3\Confluence.vst3"

Name "${PRODUCT} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\GRVTY Records\Confluence"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "GRVTY Records — ${PRODUCT} ${VERSION}"
ShowInstDetails show
ShowUninstDetails show

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Installation de Confluence ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "Confluence transforme une note en accord complet, envoyé en MIDI à ton synthé.$\r$\n$\r$\nUne création du label et collectif GRVTY Records, développée par Jasperr. Logiciel libre et gratuit (GNU AGPL v3).$\r$\n$\r$\nLe plugin VST3 sera installé dans :$\r$\nC:\Program Files\Common Files\VST3$\r$\n$\r$\nFerme Ableton Live avant de continuer."
!define MUI_LICENSEPAGE_TEXT_TOP "Confluence est un logiciel libre, fourni sans aucune garantie dans les limites permises par la loi."
!define MUI_LICENSEPAGE_BUTTON "Suivant >"
!define MUI_LICENSEPAGE_TEXT_BOTTOM "Cette licence te donne des droits (utiliser, étudier, modifier, redistribuer) ; tu n'as rien à accepter pour utiliser Confluence. La musique que tu crées avec t'appartient entièrement."
!define MUI_FINISHPAGE_TITLE "Confluence est installé"
!define MUI_FINISHPAGE_TEXT "Dans Ableton Live : Options > Settings (Préférences dans Live 11) > Plug-Ins, active « Use VST3 Plug-in System Folders », puis clique sur Rescan. Confluence apparaît dans Plug-Ins > VST3 > GRVTY Records.$\r$\n$\r$\nBranchement vers ton synthé : ROUTAGE-ABLETON.md dans C:\Program Files\GRVTY Records\Confluence."
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\LISEZMOI.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Ouvrir LISEZMOI.txt"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${DOCS_DIR}\LICENSE"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "French"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=${LANG_FRENCH} "ProductName" "${PRODUCT}"
VIAddVersionKey /LANG=${LANG_FRENCH} "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=${LANG_FRENCH} "LegalCopyright" "Copyright (C) 2026 GRVTY Records. Logiciel libre, GNU AGPL v3."
VIAddVersionKey /LANG=${LANG_FRENCH} "FileDescription" "Installeur de ${PRODUCT} (VST3)"
VIAddVersionKey /LANG=${LANG_FRENCH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_FRENCH} "ProductVersion" "${VERSION}"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Confluence nécessite une version 64 bits de Windows." /SD IDOK
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Confluence"
  ; le plugin : on retire d'abord une ancienne version pour ne pas garder de fichiers périmés
  RMDir /r "${VST3_DIR}"
  SetOutPath "${VST3_DIR}"
  File /r "${PLUGIN_DIR}\*.*"

  ; les textes et le désinstalleur
  SetOutPath "$INSTDIR"
  File "${DOCS_DIR}\LICENSE"
  File "${DOCS_DIR}\NOTICE"
  File "${DOCS_DIR}\THIRD-PARTY-NOTICES.md"
  File "${DOCS_DIR}\LISEZMOI.txt"
  File "${DOCS_DIR}\ROUTAGE-ABLETON.md"
  File "${DOCS_DIR}\BUILD-INFO.txt"
  WriteUninstaller "$INSTDIR\${UNINST_EXE}"

  ; « Ajouter ou supprimer des programmes »
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${PRODUCT} (VST3)"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${PUBLISHER}"
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\${UNINST_EXE}"'
  WriteRegStr   HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\${UNINST_EXE}" /S'
  !ifdef SOURCE_URL
    WriteRegStr HKLM "${UNINST_KEY}" "URLInfoAbout"    "${SOURCE_URL}"
  !endif
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  RMDir /r "${VST3_DIR}"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\NOTICE"
  Delete "$INSTDIR\THIRD-PARTY-NOTICES.md"
  Delete "$INSTDIR\LISEZMOI.txt"
  Delete "$INSTDIR\ROUTAGE-ABLETON.md"
  Delete "$INSTDIR\BUILD-INFO.txt"
  Delete "$INSTDIR\${UNINST_EXE}"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\GRVTY Records"   ; seulement s'il est vide
  DeleteRegKey HKLM "${UNINST_KEY}"
  ; les presets personnels (Documents\GRVTY Records\Confluence\Presets) sont conservés
SectionEnd
