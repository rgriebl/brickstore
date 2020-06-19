#ifndef SOURCE_DIR
#define SOURCE_DIR "C:\Users\sandman\git\build-brickstore-Desktop_Qt_5_12_9_MSVC2017_32bit-Release\src\bin"
; #define SOURCE_DIR "."
#endif

#define ApplicationVersionFull GetFileVersion(SOURCE_DIR + "\Brickstore.exe")
#define ApplicationVersion RemoveFileExt(ApplicationVersionFull)

[Setup]
AppName=Brickstore
AppVersion={#ApplicationVersion}
VersionInfoVersion={#ApplicationVersionFull}
DefaultDirName={pf}\Brickstore
DefaultGroupName=Brickstore
UninstallDisplayIcon={app}\brickstore.exe
; Since no icons will be created in "{group}", we do not need the wizard
; to ask for a Start Menu folder name:
DisableProgramGroupPage=yes
SourceDir={#SOURCE_DIR}
OutputBaseFilename=Brickstore Installer
CloseApplications=yes
RestartApplications=yes
ChangesAssociations=yes

WizardSmallImageFile={#SourcePath}\installer-logo.bmp

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "slovenian"; MessagesFile: "compiler:Languages\Slovenian.isl"

[Files]
Source: "brickstore.exe"; DestDir: "{app}"
Source: "translations\*.qm"; DestDir: "{app}\translations"
Source: "*.dll"; DestDir: "{app}"; Flags: recursesubdirs
; MSVC
Source: "vc_redist.x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Run]
Filename: "{tmp}\vc_redist.x86.exe"; StatusMsg: "Microsoft C/C++ runtime"; Parameters: "/quiet"; Flags: waituntilterminated


[Icons]
Name: "{commonprograms}\Brickstore"; Filename: "{app}\brickstore.exe";

[Registry]
; Definition
Root: HKCR; Subkey: "Brickstore.Document"; ValueType: string; ValueData: "Brickstore Document"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Brickstore.Document\DefaultIcon"; ValueType: string; ValueData: "{app}\brickstore.exe,1"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Brickstore.Document\shell\open\command"; ValueType: string; ValueData: """{app}\brickstore.exe"" ""%1"""; Flags: uninsdeletekey

; Association
Root: HKCR; Subkey: ".bsx"; ValueType: string; ValueData: "Brickstore.Document"; Flags: uninsdeletevalue uninsdeletekeyifempty
