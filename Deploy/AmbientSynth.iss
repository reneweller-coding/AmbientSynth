; AmbientSynth -- the Windows installer.
;
; Built by Deploy/build_release.ps1, which stages everything under Deploy/stage first. Nothing in
; here reaches into the build tree: what is in the staging folder is exactly what gets installed,
; so the setup can be looked at before it is run.
;
;   ISCC.exe /DVersion=1.0.0 Deploy\AmbientSynth.iss
;
; The binaries are linked against the static runtime (AMBIENT_STATIC_RUNTIME), so there is no
; redistributable to chase and no DLL beside the executable: two files and the presets.

#ifndef Version
  #define Version "1.0.0"
#endif
#define AppName "AmbientSynth"
#define Publisher "Rene Weller"
#define AppURL "https://github.com/reneweller-coding/AmbientSynth"
#define Stage "stage"

[Setup]
AppId={{7C3B9E14-5A2D-4C88-9E1F-2B6A0D5F3A41}
AppName={#AppName}
AppVersion={#Version}
AppVerName={#AppName} {#Version}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile={#Stage}\LICENSE.txt
OutputDir=out
OutputBaseFilename={#AppName}-{#Version}-Setup
SetupIconFile={#Stage}\logo.ico
UninstallDisplayIcon={app}\AmbientSynth.exe
UninstallDisplayName={#AppName} {#Version}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; For everybody on the machine by default -- the VST3 belongs in the shared plug-in folder, which
; needs administrator rights. Anyone who does not have them can choose "just for me" in the first
; dialog (or pass /CURRENTUSER) and gets the per-user plug-in folder instead; the instrument looks
; in both. Every path below is an {auto...} one, which is what makes the two modes the same script.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
; The compiler warns that a per-user folder is written while the default mode is admin. It is
; guarded: the {localappdata} line runs only under "Check: not IsAdminInstallMode", which is the
; per-user mode itself. Silenced knowingly rather than left as noise on every build.
UsedUserAreasWarning=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
VersionInfoVersion={#Version}
VersionInfoCompany={#Publisher}
VersionInfoDescription={#AppName} installer

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[CustomMessages]
en.CompStandalone=Standalone application
en.CompVst3=VST3 plug-in (for a DAW)
en.CompPacks=Preset library (25 packs, 5000 presets)
en.TaskDesktop=Create a desktop shortcut
de.CompStandalone=Eigenstaendiges Programm
de.CompVst3=VST3-Plugin (fuer eine DAW)
de.CompPacks=Preset-Bibliothek (25 Pakete, 5000 Presets)
de.TaskDesktop=Verknuepfung auf dem Desktop anlegen

[Types]
Name: "full"; Description: "{code:FullTypeName}"
Name: "custom"; Description: "{code:CustomTypeName}"; Flags: iscustom

[Components]
Name: "standalone"; Description: "{cm:CompStandalone}"; Types: full custom; Flags: fixed
Name: "vst3";       Description: "{cm:CompVst3}";       Types: full custom
Name: "packs";      Description: "{cm:CompPacks}";      Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Components: standalone

[Files]
Source: "{#Stage}\AmbientSynth.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#Stage}\LICENSE.txt";      DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#Stage}\README.txt";       DestDir: "{app}"; Components: standalone; Flags: ignoreversion isreadme
; The VST3 is a bundle: a folder that the host reads as one plug-in.
Source: "{#Stage}\AmbientSynth.vst3\*"; DestDir: "{autocf}\VST3\AmbientSynth.vst3"; \
    Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; Packs go into a folder of the installer's own -- machine-wide or, for an install without
; administrator rights, per user -- so that removing them again can never take a pack the user put
; there themselves along with it. The instrument reads both of these as well as each user's own
; Documents\AmbientSynth\Packs (see Core/src/PresetPacks.cpp).
Source: "{#Stage}\Packs\*.ambientpack"; DestDir: "{commonappdata}\AmbientSynth\Packs"; \
    Check: IsAdminInstallMode; Components: packs; Flags: ignoreversion
Source: "{#Stage}\Packs\*.ambientpack"; DestDir: "{localappdata}\AmbientSynth\Packs"; \
    Check: not IsAdminInstallMode; Components: packs; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\AmbientSynth.exe"; Components: standalone
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\AmbientSynth.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AmbientSynth.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; \
    Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
; The plug-in bundle and the pack folders are ones the installer made; what a user put in their
; own Documents is theirs and is never touched.
Type: filesandordirs; Name: "{autocf}\VST3\AmbientSynth.vst3"
Type: filesandordirs; Name: "{commonappdata}\AmbientSynth\Packs"
Type: dirifempty;     Name: "{commonappdata}\AmbientSynth"
Type: filesandordirs; Name: "{localappdata}\AmbientSynth\Packs"
Type: dirifempty;     Name: "{localappdata}\AmbientSynth"

[Code]
function FullTypeName(Param: String): String;
begin
  if ActiveLanguage = 'de' then Result := 'Vollstaendig' else Result := 'Full installation';
end;

function CustomTypeName(Param: String): String;
begin
  if ActiveLanguage = 'de' then Result := 'Benutzerdefiniert' else Result := 'Custom installation';
end;
