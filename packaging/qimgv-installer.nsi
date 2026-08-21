; qimgv Windows installer (NSIS / MUI2)
;
; Usage:
;   makensis /DVER=1.0.3-alpha-179 /DBUILD_DIR=./qimgv-x64_v1.0.3-alpha-179-g5bb87ff8 qimgv-installer.nsi
;
;   VER       - version string used for the output file name (qimgv-x64_<VER>.exe)
;   BUILD_DIR - path to the portable build directory whose contents get installed
;   APP_ICON  - path to the qimgv icon (.ico), optional

!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VER
  !define VER "dev"
!endif
!ifndef BUILD_DIR
  !define BUILD_DIR "."
!endif
!ifndef APP_ICON
  !define APP_ICON "..\qimgv\res\icons\common\logo\app\qimgv.ico"
!endif

Name "qimgv"
!ifndef PACKAGE_NAME
  !define PACKAGE_NAME "qimgv-x64_${VER}"
!endif
OutFile "${PACKAGE_NAME}.exe"
InstallDir "$PROGRAMFILES64\qimgv"
InstallDirRegKey HKLM "Software\qimgv" "InstallDir"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma

; installer / uninstaller icons
Icon "${APP_ICON}"
UninstallIcon "${APP_ICON}"
InstallButtonText "安装(&I)"
ShowInstDetails show
ShowUninstDetails show

; ---------- MUI pages ----------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ---------- languages ----------
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

VIProductVersion "1.0.3.0"
VIAddVersionKey "ProductName" "qimgv"
VIAddVersionKey "FileDescription" "qimgv - free image viewer"
VIAddVersionKey "FileVersion" "${VER}"
VIAddVersionKey "ProductVersion" "${VER}"
VIAddVersionKey "LegalCopyright" "qimgv contributors"

; ---------- install section ----------
Section "qimgv (必需)" SecApp
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${BUILD_DIR}\*"

  ; start menu shortcut + uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\qimgv"
  CreateShortCut "$SMPROGRAMS\qimgv\qimgv.lnk" "$INSTDIR\qimgv.exe" "" "$INSTDIR\qimgv.exe" 0
  CreateShortCut "$SMPROGRAMS\qimgv\卸载 qimgv.lnk" "$INSTDIR\Uninstall.exe"

  ; Add/Remove Programs entry
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "DisplayName" "qimgv"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "DisplayVersion" "${VER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "Publisher" "qimgv contributors"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "DisplayIcon" "$INSTDIR\qimgv.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "NoModify" "1"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "NoRepair" "1"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\qimgv" "InstallDir" "$INSTDIR"
SectionEnd

Section "桌面快捷方式" SecDesktop
  CreateShortCut "$DESKTOP\qimgv.lnk" "$INSTDIR\qimgv.exe" "" "$INSTDIR\qimgv.exe" 0
SectionEnd

; ---------- descriptions ----------
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} "qimgv 主程序及其运行库。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "在桌面上创建 qimgv 快捷方式。"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------- uninstall section ----------
Section "Uninstall"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$DESKTOP\qimgv.lnk"
  RMDir /r "$SMPROGRAMS\qimgv"
  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\qimgv"
  DeleteRegKey HKLM "Software\qimgv"
SectionEnd
