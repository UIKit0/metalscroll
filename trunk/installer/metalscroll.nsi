!include "MUI.nsh"
!include "Sections.nsh"

;--------------------------------
; General.
;--------------------------------

Name "MetalScroll"
OutFile "MetalScrollSetup.exe"
InstallDir "$PROGRAMFILES\MetalScroll"
SetCompressor "lzma"
RequestExecutionLevel admin

;--------------------------------
; Interface Settings.
;--------------------------------

!define MUI_ABORTWARNING
!define MUI_HEADERIMAGE

;--------------------------------
; Pages.
;--------------------------------

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.rtf"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
  
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Sections.
;--------------------------------

InstType "Normal install"

SubSection "MetalScroll" SecMetalScroll
	Section "Files" SecFiles
		SectionIn 1 RO
		
		CreateDirectory $INSTDIR
		SetOutPath $INSTDIR
		File "license.rtf"
		File "..\Release\MetalScroll.dll"
		RegDLL "$INSTDIR\MetalScroll.dll"
		
		; Uninstaller.
		WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "DisplayName" "MetalScroll"
		WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "UninstallString" "$INSTDIR\MetalScrollUninstall.exe"
		WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "InstallLocation" $INSTDIR
		WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "Publisher" "Griffin Software"
		WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "DisplayVersion" "1.0.7.67"
		WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "NoModify" 1
		WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll" "NoRepair" 1
		WriteUninstaller "$INSTDIR\MetalScrollUninstall.exe"
	SectionEnd
	
SubSectionEnd

;--------------------------------
; Uninstaller.
;--------------------------------

Section "Uninstall"
	UnRegDLL "$INSTDIR\MetalScroll.dll"
	RMDir /r /REBOOTOK $INSTDIR
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalScroll"
SectionEnd
