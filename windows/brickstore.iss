; BrickStore Inno-Setup Installer

#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif

#define ApplicationVersionFull GetFileVersion(SOURCE_DIR + "\BrickStore.exe")
#define ApplicationVersion RemoveFileExt(ApplicationVersionFull)

[Setup]
AppName=BrickStore
AppVersion={#ApplicationVersion}
VersionInfoVersion={#ApplicationVersionFull}
DefaultDirName={commonpf}\BrickStore
DefaultGroupName=BrickStore
UninstallDisplayIcon={app}\BrickStore.exe
; Since no icons will be created in "{group}", we do not need the wizard
; to ask for a Start Menu folder name:
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableWelcomePage=yes
SourceDir={#SOURCE_DIR}
OutputBaseFilename=BrickStore Installer
CloseApplications=yes
RestartApplications=yes
ChangesAssociations=yes

WizardImageAlphaFormat=defined
WizardSmallImageFile={#SourcePath}\..\assets\generated-installers\windows-installer.bmp

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "BrickStore.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "*.dll"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
Source: "qmldir"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
Source: "*.qmltypes"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
; MSVC
Source: "vc_redist.x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist
Source: "vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Run]
Filename: "{tmp}\vc_redist.x86.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated skipifdoesntexist; \

Filename: "{tmp}\vc_redist.x64.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated skipifdoesntexist; \

[Icons]
Name: "{commonprograms}\BrickStore"; Filename: "{app}\BrickStore.exe";

[Registry]
; Definition
Root: HKCR; Subkey: "BrickStore.Document"; ValueType: string; \
    ValueData: "BrickStore Document"; Flags: uninsdeletekey
Root: HKCR; Subkey: "BrickStore.Document\DefaultIcon"; ValueType: string; \
    ValueData: "{app}\BrickStore.exe,1"; Flags: uninsdeletekey
Root: HKCR; Subkey: "BrickStore.Document\shell\open\command"; ValueType: string; \
    ValueData: """{app}\BrickStore.exe"" ""%1"""; Flags: uninsdeletekey

; Association
Root: HKCR; Subkey: ".bsx"; ValueType: string; \
    ValueData: "BrickStore.Document"; Flags: uninsdeletevalue uninsdeletekeyifempty
