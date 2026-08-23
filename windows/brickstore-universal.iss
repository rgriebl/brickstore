; Copyright (C) 2004-2026 Robert Griebl
; SPDX-License-Identifier: GPL-3.0-only

; BrickStore Inno-Setup Installer, carrying both the x64 and the arm64 build.
; SOURCE_DIR is expected to hold one deployed tree per architecture:
;     <SOURCE_DIR>\x64\BrickStore.exe ...
;     <SOURCE_DIR>\arm64\BrickStore.exe ...
; The setup binary itself is 32bit, so it runs on arm64 through emulation and can
; then install the native arm64 payload.

#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif

#define ApplicationVersionFull GetVersionNumbersString(SOURCE_DIR + "\x64\BrickStore.exe")
#define ApplicationVersion RemoveFileExt(ApplicationVersionFull)
#define ApplicationPublisher GetFileCompany(SOURCE_DIR + "\x64\BrickStore.exe")

; the MSVC runtime we require is the one windeployqt bundled from the build toolset
#define VCRedistVersionX64 GetVersionNumbersString(SOURCE_DIR + "\x64\vc_redist.x64.exe")
#define VCRedistVersionArm64 GetVersionNumbersString(SOURCE_DIR + "\arm64\vc_redist.arm64.exe")

[Setup]
; Windows 10 or Windows Server 2019, version 1809
MinVersion=10.0.17763
AppName=BrickStore
AppVersion={#ApplicationVersion}
AppPublisher={#ApplicationPublisher}
AppPublisherURL={#ApplicationPublisher}
VersionInfoVersion={#ApplicationVersionFull}
DefaultDirName={autopf}\BrickStore
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

; x64compatible also matches arm64, which can emulate x64 - the arm64 term is what
; lets the installer recognize an arm64 machine and pick the native payload below
ArchitecturesInstallIn64BitMode=x64compatible or arm64
ArchitecturesAllowed=x64compatible or arm64

PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
; native arm64 on arm64, x64 everywhere else
Source: "arm64\*.exe"; DestDir: "{app}"; Check: IsArm64; \
    Flags: ignoreversion; Excludes: "vc_redist.arm64.exe"
Source: "arm64\*.dll"; DestDir: "{app}"; Check: IsArm64; \
    Flags: recursesubdirs ignoreversion skipifsourcedoesntexist
Source: "arm64\qmldir"; DestDir: "{app}"; Check: IsArm64; Flags: recursesubdirs ignoreversion
Source: "arm64\*.qmltypes"; DestDir: "{app}"; Check: IsArm64; Flags: recursesubdirs ignoreversion

Source: "x64\*.exe"; DestDir: "{app}"; Check: not IsArm64; \
    Flags: ignoreversion; Excludes: "vc_redist.x64.exe"
Source: "x64\*.dll"; DestDir: "{app}"; Check: not IsArm64; \
    Flags: recursesubdirs ignoreversion skipifsourcedoesntexist
Source: "x64\qmldir"; DestDir: "{app}"; Check: not IsArm64; Flags: recursesubdirs ignoreversion
Source: "x64\*.qmltypes"; DestDir: "{app}"; Check: not IsArm64; Flags: recursesubdirs ignoreversion

; MSVC
Source: "arm64\vc_redist.arm64.exe"; DestDir: "{tmp}"; Check: IsArm64; Flags: deleteafterinstall
Source: "x64\vc_redist.x64.exe"; DestDir: "{tmp}"; Check: not IsArm64; Flags: deleteafterinstall

[Run]
Filename: "{tmp}\vc_redist.arm64.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated; \
    Check: IsArm64 and noMSVCInstalled('arm64')
Filename: "{tmp}\vc_redist.x64.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated; \
    Check: not IsArm64 and noMSVCInstalled('x64')

Filename: {app}\BrickStore.exe; Flags: postinstall nowait skipifsilent

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

; Remove old plugins that might get loaded into an incompatible Qt or
; that might pull in broken libs (e.g. outdated openssl libs in %PATH%)
; This also covers switching between the x64 and the arm64 payload in place.
[InstallDelete]
Type: filesandordirs; Name: "{app}\Qt*"
Type: filesandordirs; Name: "{app}\iconengines"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\multimedia"
Type: filesandordirs; Name: "{app}\networkinformation"
Type: filesandordirs; Name: "{app}\qmltooling"
Type: filesandordirs; Name: "{app}\sqldrivers"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\tls"
Type: files; Name: "{app}\d3dcompiler*"
Type: files; Name: "{app}\vc_redist.x64.exe"
Type: files; Name: "{app}\vc_redist.arm64.exe"

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
function noMSVCInstalled(Arch: String): Boolean;
var
    Version: Int64;
begin
    if Arch = 'arm64' then begin
        StrToVersion('{#VCRedistVersionArm64}', Version);
        Result := not IsMsiProductInstalled('{DC9BAE42-810B-423A-9E25-E4073F1C7B00}', Version);
    end else if Arch = 'x64' then begin
        StrToVersion('{#VCRedistVersionX64}', Version);
        Result := not IsMsiProductInstalled('{36F68A90-239C-34DF-B58C-64B30153CE35}', Version);
    end else
        Result := True;
end;

{ BrickStore pre 2022.4.1 always got installed as a 32bit app (even in 64bit mode) }
{ We need to detect this and remove the old 32bit installation when upgrading }
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
    UninstallString: String;
    ResultCode: Integer;
begin
    RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\BrickStore_is1', 'UninstallString', UninstallString);
    if UninstallString <> '' then begin
         UninstallString := RemoveQuotes(UninstallString);
         ShellExec('open', 'taskkill.exe', '/f /im BrickStore.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
         Sleep(2000)
         if not Exec(UninstallString, '/SILENT', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
             Result := 'Failed to remove the old 32bit BrickStore installer. Please manually uninstall the old version first.';
    end
end;

{ Inno Setup does not kill the app on uninstall, but the uninstallation isn't done }
{ correctly if the app is running, so we have to kill it ourselves }

function InitializeUninstall(): Boolean;
    var ResultCode: Integer;
begin
    ShellExec('open', 'taskkill.exe', '/f /im BrickStore.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(2000)
    result := True;
end;
