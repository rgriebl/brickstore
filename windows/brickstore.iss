; Copyright (C) 2004-2023 Robert Griebl
; SPDX-License-Identifier: GPL-3.0-only

; BrickStore Inno-Setup Installer

#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif
#ifndef ARCH
#define ARCH "x64"
#endif

#define ApplicationVersionFull GetVersionNumbersString(SOURCE_DIR + "\BrickStore.exe")
#define ApplicationVersion RemoveFileExt(ApplicationVersionFull)
#define ApplicationPublisher GetFileCompany(SOURCE_DIR + "\BrickStore.exe")

[Setup]
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

ArchitecturesInstallIn64BitMode={#ARCH}
ArchitecturesAllowed={#ARCH}

PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
Source: "*.exe"; DestDir: "{app}"; Flags: ignoreversion; Excludes: "vc_redist.*.exe"
Source: "*.dll"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion skipifsourcedoesntexist
Source: "qmldir"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
Source: "*.qmltypes"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
; MSVC
Source: "vc_redist.{#ARCH}.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Run]
Filename: "{tmp}\vc_redist.{#ARCH}.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated; \
    Check: noMSVCInstalled('{#ARCH}')

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

[Code]
function noMSVCInstalled(Arch: String): Boolean;
var
    Version: Int64;
begin
    Version := PackVersionComponents(14, 29, 30037, 0);
    if Arch = 'arm64' then
        Result := not IsMsiProductInstalled('{DC9BAE42-810B-423A-9E25-E4073F1C7B00}', Version)
    else if Arch = 'x64' then
        Result := not IsMsiProductInstalled('{36F68A90-239C-34DF-B58C-64B30153CE35}', Version)
    else
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
