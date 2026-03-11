; Artifact v1.0.0 — Windows Installer Script
; Built with Inno Setup 6.x — https://jrsoftware.org/isinfo.php

#define AppName      "Artifact"
#define AppVersion   "1.0.0"
#define AppPublisher "evrshade"
#define AppURL       "https://yourwebsite.com"
#define SourceDir    "C:\Users\lickm\OneDrive\Documents\JUCE Projects\Artifact\Builds\VisualStudio2026\x64\Release"

[Setup]
AppId={{A8F3D210-7B44-4E29-B8C1-2D4F9A0E3C57}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
DefaultDirName={autopf}\{#AppPublisher}\{#AppName}
DefaultGroupName={#AppPublisher}\{#AppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=Artifact_v{#AppVersion}_Windows_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Full Installation (VST3 + Manual)"
Name: "vst3";   Description: "VST3 Plugin Only"
Name: "custom"; Description: "Custom Installation"; Flags: iscustom

[Components]
Name: "vst3";   Description: "VST3 Plugin";       Types: full vst3
Name: "manual"; Description: "User Manual (PDF)"; Types: full

[Files]
; VST3 plugin folder structure
Source: "{#SourceDir}\VST3\Artifact.vst3\Contents\x86_64-win\*"; \
    DestDir: "{commoncf}\VST3\Artifact.vst3\Contents\x86_64-win"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; \
    Components: vst3

; User manual — place Artifact_Manual_v1.0.0.pdf in the same folder as this .iss file
Source: "Artifact_Manual_v1.0.0.pdf"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist; \
    Components: manual

[Icons]
Name: "{group}\User Manual"; \
    Filename: "{app}\Artifact_Manual_v1.0.0.pdf"; \
    Components: manual
Name: "{group}\Uninstall {#AppName}"; \
    Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Artifact_Manual_v1.0.0.pdf"; \
    Description: "View User Manual"; \
    Flags: postinstall shellexec skipifsilent unchecked; \
    Components: manual

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Artifact.vst3"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
    if CurStep = ssPostInstall then
    begin
        MsgBox('Artifact has been installed.' + #13#10 + #13#10 +
               'VST3 location:' + #13#10 +
               'C:\Program Files\Common Files\VST3\Artifact.vst3' + #13#10 + #13#10 +
               'Please rescan your plugin folders in your DAW.',
               mbInformation, MB_OK);
    end;
end;
