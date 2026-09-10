; NSIS Modern User Interface installer script for Sony Device Center
!include "MUI2.nsh"

Name "Sony Device Center"
OutFile "SonyDeviceCenter-Installer.exe"
InstallDir "$PROGRAMFILES64\SonyDeviceCenter"
InstallDirRegKey HKLM "Software\SonyDeviceCenter" "Install_Dir"
RequestExecutionLevel admin

!define MUI_ABORTWARNING
!define MUI_ICON "..\linux\sony-device-center.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Sony Device Center (required)"
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "bin\*.*"
    
    WriteRegStr HKLM "Software\SonyDeviceCenter" "Install_Dir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    CreateDirectory "$SMPROGRAMS\Sony Device Center"
    CreateShortCut "$SMPROGRAMS\Sony Device Center\Sony Device Center.lnk" "$INSTDIR\sony-device-center.exe"
    CreateShortCut "$SMPROGRAMS\Sony Device Center\Uninstall.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    DeleteRegKey HKLM "Software\SonyDeviceCenter"
    RMDir /r "$INSTDIR"
    RMDir /r "$SMPROGRAMS\Sony Device Center"
SectionEnd
