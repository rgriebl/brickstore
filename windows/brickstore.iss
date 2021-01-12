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
WizardSmallImageFile={#SourcePath}\..\assets\generated-misc\windows-installer.bmp

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "slovenian"; MessagesFile: "compiler:Languages\Slovenian.isl"

[Files]
Source: "BrickStore.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "*.dll"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
; MSVC
Source: "vc_redist.x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist
Source: "vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

; these are the MSVC2010 vcredists needed for the OpenSSL libraries
Source: "vcredist_x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist
Source: "vcredist_x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Run]
Filename: "{tmp}\vc_redist.x86.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated skipifdoesntexist; \
    Check: VCRedist2019x86NeedsInstall

Filename: "{tmp}\vc_redist.x64.exe"; StatusMsg: "Microsoft C/C++ runtime"; \
    Parameters: "/quiet /norestart"; Flags: waituntilterminated skipifdoesntexist; \
    Check: VCRedist2019x64NeedsInstall

; these are the MSVC2010 vcredists needed for the OpenSSL libraries
Filename: "{tmp}\vcredist_x86.exe"; StatusMsg: "Microsoft C/C++ runtime for OpenSSL"; \
    Parameters: "/q /norestart"; Flags: waituntilterminated skipifdoesntexist; \
    Check: VCRedist2010x86NeedsInstall
Filename: "{tmp}\vcredist_x64.exe"; StatusMsg: "Microsoft C/C++ runtime for OpenSSL"; \
    Parameters: "/q /norestart"; Flags: waituntilterminated skipifdoesntexist; \
    Check: VCRedist2010x64NeedsInstall

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
type
  INSTALLSTATE = Longint;
const
  INSTALLSTATE_INVALIDARG = -2;  { An invalid parameter was passed to the function. }
  INSTALLSTATE_UNKNOWN = -1;     { The product is neither advertised or installed. }
  INSTALLSTATE_ADVERTISED = 1;   { The product is advertised but not installed. }
  INSTALLSTATE_ABSENT = 2;       { The product is installed for a different user. }
  INSTALLSTATE_DEFAULT = 5;      { The product is installed for the current user. }

  VC_2010_REDIST_X86 = '{196BB40D-1578-3D01-B289-BEFC77A11A1E}';
  VC_2010_REDIST_X64 = '{DA5E371C-6333-3D8A-93A4-6FD5B20BCC6E}';
  VC_2010_SP1_REDIST_X86 = '{F0C3E5D1-1ADE-321E-8167-68EF0DE699A5}';
  VC_2010_SP1_REDIST_X64 = '{1D8E6291-B0D5-35EC-8441-6616F567A0F7}';

  VC_2019_X86_MIN = '{0D01A812-82A1-481F-8546-8E28E976F8DF}';
  VC_2019_X64_MIN = '{8A3F7D5B-422D-49D9-84F7-8DC1B7782967}';

  function MsiQueryProductState(szProduct: string): INSTALLSTATE;
    external 'MsiQueryProductStateW@msi.dll stdcall';

  function VCVersionInstalled(const ProductID: string): Boolean;
  begin
    Result := MsiQueryProductState(ProductID) = INSTALLSTATE_DEFAULT;
  end;

  function VCRedist2019x86NeedsInstall: Boolean;
  begin
    Result := not (VCVersionInstalled(VC_2019_X86_MIN));
  end;

  function VCRedist2019x64NeedsInstall: Boolean;
  begin
    Result := not (VCVersionInstalled(VC_2019_X64_MIN));
  end;

  function VCRedist2010x86NeedsInstall: Boolean;
  begin
    Result := not (VCVersionInstalled(VC_2010_REDIST_X86) and
      VCVersionInstalled(VC_2010_SP1_REDIST_X86));
  end;

  function VCRedist2010x64NeedsInstall: Boolean;
  begin
    Result := not (VCVersionInstalled(VC_2010_REDIST_X64) and
      VCVersionInstalled(VC_2010_SP1_REDIST_X64));
  end;
