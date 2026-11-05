; Guided wizard; integrity checks and transactional copying live in Install.ps1.
Unicode true
ManifestDPIAware true
RequestExecutionLevel admin
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!ifndef VERSION
!error "Pass -DVERSION=<suite version> through Installer/Release.ps1"
!endif
!ifndef PAYLOAD
!error "Pass -DPAYLOAD=<verified payload folder>"
!endif
!ifndef OUTPUT
!define OUTPUT "..\build\release\ItStealsSuite-Setup.exe"
!endif
!ifndef CHANNEL
!define CHANNEL ""
!endif
Name "Elite-Suite ${VERSION}${CHANNEL}"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\Elite-Suite"
InstallDirRegKey HKLM "Software\mab\ItStealsPracticeSuite" "InstallDir"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Elite-Suite"
VIAddVersionKey "FileDescription" "Elite-Suite Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "CompanyName" "mab"
VIAddVersionKey "Comments" "Vibe-coded project. See the repository for development credits."
VIAddVersionKey "LegalCopyright" "See project credits and bundled third-party licenses."
Icon "..\assets\suite.ico"
UninstallIcon "..\assets\suite.ico"
!define MUI_ICON "..\assets\suite.ico"
!define MUI_UNICON "..\assets\suite.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\assets\setup-sidebar.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "..\assets\setup-header.bmp"
!define MUI_BGCOLOR "101B20"
!define MUI_TEXTCOLOR "E8F5F1"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Ready for your next run."
!define MUI_WELCOMEPAGE_TEXT "Elite-Suite v${VERSION}${CHANNEL}$\r$\n$\r$\nPractice. Understand. Improve.$\r$\n$\r$\nInstall the trainer, video encoder, and game mod. Setup finds your Steam installation, installs MelonLoader when needed, and creates your shortcuts.$\r$\n$\r$\nClose It Steals and Elite-Suite before continuing."
!insertmacro MUI_PAGE_WELCOME
Page custom GamePage GameLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TITLE "Elite-Suite is ready."
!define MUI_FINISHPAGE_TEXT "Start Elite-Suite from the desktop or Start menu. The trainer starts It Steals automatically; this is optional under Settings > System.$\r$\n$\r$\nPress F3 in the game to open the menu. Use windowed or borderless mode.$\r$\n$\r$\nExisting replays, settings, other mods, and a complete MelonLoader installation are preserved."
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Open the Elite-Suite folder"
!define MUI_FINISHPAGE_RUN_FUNCTION OpenSuiteFolder
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Var GameDir
Var GameEdit
Var DesktopCheck
Var DesktopEnabled
Var Powershell
Var GameHint

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "Elite-Suite requires 64-bit Windows."
        Abort
    ${EndIf}
    ${IfNot} ${AtLeastWin10}
        MessageBox MB_ICONSTOP "Elite-Suite requires Windows 10 or later."
        Abort
    ${EndIf}
    SetRegView 64
    SetShellVarContext all
    ReadRegStr $0 HKLM "Software\mab\ItStealsPracticeSuite" "InstallDir"
    ${If} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}
    StrCpy $Powershell "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
    StrCpy $DesktopEnabled ${BST_CHECKED}
    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File /oname=Install.ps1 "Install.ps1"
    Call DiscoverGame
FunctionEnd
Function DiscoverGame
    nsExec::ExecToStack '"$Powershell" -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "$PLUGINSDIR\Install.ps1" -Mode Discover -InstallDir "$INSTDIR" -DiscoveryFile "$PLUGINSDIR\detected.ini"'
    Pop $0
    Pop $1
    StrCpy $GameDir ""
    ${If} $0 == 0
        ReadINIStr $GameDir "$PLUGINSDIR\detected.ini" "Game" "Directory"
    ${EndIf}
FunctionEnd
Function GamePage
    !insertmacro MUI_HEADER_TEXT "Choose It Steals" "Select the folder containing It Steals.exe."
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}
    ${NSD_CreateLabel} 0 0 100% 38u "Setup installs MelonLoader only when no complete loader is present. Original game files, replays, and other mods are kept."
    Pop $0
    ${NSD_CreateDirRequest} 0 48u 64% 14u "$GameDir"
    Pop $GameEdit
    ${NSD_CreateBrowseButton} 66% 47u 17% 16u "Browse..."
    Pop $0
    ${NSD_OnClick} $0 BrowseGame
    ${NSD_CreateButton} 85% 47u 15% 16u "Detect"
    Pop $0
    ${NSD_OnClick} $0 DetectGame
    ${NSD_CreateCheckbox} 0 82u 100% 14u "Create a desktop shortcut"
    Pop $DesktopCheck
    ${NSD_SetState} $DesktopCheck $DesktopEnabled
    ${If} $GameDir != ""
        ${NSD_CreateLabel} 0 110u 100% 40u "Your game folder was detected automatically. Confirm it above, then continue."
    ${Else}
        ${NSD_CreateLabel} 0 110u 100% 40u "No game folder found. Open Steam and click Detect, or use Browse to select the folder containing It Steals.exe."
    ${EndIf}
    Pop $GameHint
    nsDialogs::Show
FunctionEnd
Function DetectGame
    Pop $0
    Call DiscoverGame
    ${If} $GameDir != ""
        ${NSD_SetText} $GameEdit $GameDir
        ${NSD_SetText} $GameHint "Your game folder was detected automatically. Confirm it above, then continue."
    ${Else}
        ${NSD_SetText} $GameHint "No game folder found. Open Steam and click Detect, or select the game folder with Browse."
    ${EndIf}
FunctionEnd
Function BrowseGame
    Pop $0
    nsDialogs::SelectFolderDialog "Choose the It Steals game folder" "$GameDir"
    Pop $0
    ${If} $0 != error
        ${NSD_SetText} $GameEdit $0
    ${EndIf}
FunctionEnd
Function GameLeave
    ${NSD_GetText} $GameEdit $GameDir
    ${NSD_GetState} $DesktopCheck $DesktopEnabled
    ${IfNot} ${FileExists} "$GameDir\It Steals.exe"
        MessageBox MB_ICONEXCLAMATION "Choose the folder containing It Steals.exe."
        Abort
    ${EndIf}
    ${IfNot} ${FileExists} "$GameDir\It Steals_Data\Managed\Assembly-CSharp.dll"
        MessageBox MB_ICONEXCLAMATION "The selected folder is not a supported Mono game installation."
        Abort
    ${EndIf}
FunctionEnd
Function OpenSuiteFolder
    ExecShell "open" "$INSTDIR"
FunctionEnd
Section "Elite-Suite" SEC_MAIN
    SetOutPath "$PLUGINSDIR\payload"
    File /r "${PAYLOAD}\*"
    nsExec::ExecToLog '"$Powershell" -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "$PLUGINSDIR\Install.ps1" -Mode Install -GameDir "$GameDir" -InstallDir "$INSTDIR" -PayloadDir "$PLUGINSDIR\payload"'
    Pop $0
    ${If} $0 != 0
        MessageBox MB_ICONSTOP "Setup could not finish. See the installation log above. Previous files were restored if a copy failed."
        Abort
    ${EndIf}
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\Elite-Suite"
    CreateShortcut "$SMPROGRAMS\Elite-Suite\Elite-Suite.lnk" "$INSTDIR\ItStealsTrainer.exe" "" "$INSTDIR\ItStealsTrainer.exe" 0
    ${If} $DesktopEnabled == ${BST_CHECKED}
        CreateShortcut "$DESKTOP\Elite-Suite.lnk" "$INSTDIR\ItStealsTrainer.exe" "" "$INSTDIR\ItStealsTrainer.exe" 0
    ${EndIf}
    WriteRegStr HKLM "Software\mab\ItStealsPracticeSuite" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\mab\ItStealsPracticeSuite" "GameDir" "$GameDir"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "DisplayName" "Elite-Suite"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "Publisher" "mab"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "DisplayIcon" "$INSTDIR\ItStealsTrainer.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite" "NoRepair" 1
SectionEnd
Function un.onInit
    SetRegView 64
    SetShellVarContext all
    StrCpy $Powershell "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
FunctionEnd
Section "Uninstall"
    InitPluginsDir
    CopyFiles /SILENT "$INSTDIR\Installer\Install.ps1" "$PLUGINSDIR\Install.ps1"
    nsExec::ExecToLog '"$Powershell" -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "$PLUGINSDIR\Install.ps1" -Mode Uninstall -InstallDir "$INSTDIR"'
    Pop $0
    ${If} $0 != 0
        MessageBox MB_ICONSTOP "Close the game and trainer, then retry. See the log for details."
        Abort
    ${EndIf}
    Delete "$DESKTOP\Elite-Suite.lnk"
    Delete "$SMPROGRAMS\Elite-Suite\Elite-Suite.lnk"
    RMDir "$SMPROGRAMS\Elite-Suite"
    Delete "$DESKTOP\It Steals PracticeSuite.lnk"
    Delete "$SMPROGRAMS\It Steals PracticeSuite\PracticeSuite.lnk"
    RMDir "$SMPROGRAMS\It Steals PracticeSuite"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite"
    DeleteRegKey HKLM "Software\mab\ItStealsPracticeSuite"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR\Installer"
    RMDir "$INSTDIR\Encoder"
    RMDir "$INSTDIR"
SectionEnd
