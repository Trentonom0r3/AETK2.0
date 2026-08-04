; =============================================================================
; Inno Setup Script for Framesmith Plugin Suite Installer
; =============================================================================

[Setup]
AppId={{E6D012B3-75A1-42DF-B2F5-29809984CBE2}
AppName=Framesmith Plugins
AppVersion=1.0.0
AppPublisher=Framesmith
AppPublisherURL=https://framesmith.ai
DefaultDirName={commonpf}\Adobe\Common\Plug-ins\7.0\MediaCore
DisableDirPage=no
AppendDefaultDirName=no
CreateAppDir=yes
DirExistsWarning=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
OutputBaseFilename=FramesmithInstaller
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{src}\config.ini"; DestDir: "{app}"; Flags: external ignoreversion skipifsourcedoesntexist
Source: "{src}\*.aex"; DestDir: "{app}"; Flags: external ignoreversion skipifsourcedoesntexist

[Code]
var
  OptionPage: TInputOptionWizardPage;
  DownloadPage: TDownloadWizardPage;
  CoreIndex: Integer;
  CudaIndex: Integer;
  TrtIndex: Integer;

function OnDownloadProgress(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  Result := True;
end;

function ReadIniBool(const Section, Key: String; Default: Boolean; const Filename: String): Boolean;
var
  Val: String;
begin
  Val := GetIniString(Section, Key, '', Filename);
  if Val = '' then
    Result := Default
  else
    Result := (Val = '1') or (CompareText(Val, 'true') = 0) or (CompareText(Val, 'yes') = 0);
end;

procedure InitializeWizard();
var
  ConfigPath: String;
  PluginName: String;
  ShowCUDA: Boolean;
  ShowTRT: Boolean;
begin
  ConfigPath := ExpandConstant('{src}\config.ini');
  PluginName := GetIniString('Config', 'PluginName', 'Framesmith Plugins', ConfigPath);
  ShowCUDA := ReadIniBool('Config', 'EnableCUDA', True, ConfigPath);
  ShowTRT := ReadIniBool('Config', 'EnableTensorRT', True, ConfigPath);

  // Customize installer welcome texts dynamically based on config.ini
  WizardForm.Caption := PluginName + ' Installer';
  WizardForm.WelcomeLabel1.Caption := 'Welcome to the ' + PluginName + ' Setup Wizard';

  // Create the component selection checklist
  OptionPage := CreateInputOptionPage(
    wpWelcome,
    'Select Components',
    'Which components of the ONNX Runtime suite do you want to install?',
    'Select the components you want the installer to configure. Core runtime is required, while GPU packages are optional (Requires NVIDIA GPU).',
    False, // Checkboxes
    False  // No grouping
  );

  // Dynamically add checklist elements
  CoreIndex := OptionPage.Add('Core ONNX Runtime (Required)');
  OptionPage.Values[CoreIndex] := True;

  if ShowCUDA then begin
    CudaIndex := OptionPage.Add('CUDA Provider (Optional - Requires NVIDIA GPU)');
    OptionPage.Values[CudaIndex] := True; // Default checked
  end else begin
    CudaIndex := -1;
  end;

  if ShowTRT then begin
    TrtIndex := OptionPage.Add('TensorRT Provider (Optional - Requires NVIDIA GPU)');
    OptionPage.Values[TrtIndex] := False; // Default unchecked
  end else begin
    TrtIndex := -1;
  end;

  // Initialize the download progress page
  DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), 'Downloading dynamic dependencies (ONNX Runtime)...', @OnDownloadProgress);
end;

procedure OptionPageCheckOnClick(Sender: TObject; Index: Integer; var Checked: Boolean);
begin
  // Force the Core component to remain selected
  if Index = CoreIndex then
    Checked := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ConfigPath: String;
  CoreURL: String;
  CudaURL: String;
  TrtURL: String;
begin
  if CurPageID = OptionPage.ID then begin
    DownloadPage.Clear;
    ConfigPath := ExpandConstant('{src}\config.ini');
    
    // Read URLs from config.ini, falling back to release defaults
    CoreURL := GetIniString('Config', 'CoreURL', 'https://github.com/Framesmith-Labs/Framesmith-Host/releases/download/Stable/ort_core.zip', ConfigPath);
    CudaURL := GetIniString('Config', 'CudaURL', 'https://github.com/Framesmith-Labs/Framesmith-Host/releases/download/Stable/ort_cuda.zip', ConfigPath);
    TrtURL := GetIniString('Config', 'TrtURL', 'https://github.com/Framesmith-Labs/Framesmith-Host/releases/download/Stable/ort_trt.zip', ConfigPath);
    
    if OptionPage.Values[CoreIndex] then
      DownloadPage.Add(CoreURL, 'ort_core.zip', '');
      
    if (CudaIndex <> -1) and OptionPage.Values[CudaIndex] then
      DownloadPage.Add(CudaURL, 'ort_cuda.zip', '');
      
    if (TrtIndex <> -1) and OptionPage.Values[TrtIndex] then
      DownloadPage.Add(TrtURL, 'ort_trt.zip', '');
  end;

  if CurPageID = wpReady then begin
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
        Result := True;
      except
        SuppressibleMsgBox('Failed to download required dependencies: ' + GetExceptionMessage, mbCriticalError, MB_OK, MB_OK);
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end else
    Result := True;
end;

procedure ExtractAndFlatten(ZipFile: String; DestDir: String);
var
  TempExtractDir: String;
  ScriptFile: String;
  LogFile: String;
  ScriptContent: String;
  ResultCode: Integer;
  ErrorMsg: String;
  AnsiErrorMsg: AnsiString;
begin
  TempExtractDir := ExpandConstant('{tmp}\temp_extract_' + ExtractFileName(ZipFile));
  ScriptFile := ExpandConstant('{tmp}\extract_script_' + ExtractFileName(ZipFile) + '.ps1');
  LogFile := ExpandConstant('{tmp}\extract_log_' + ExtractFileName(ZipFile) + '.txt');
  
  // Clean up any old extraction directory and log if present
  DelTree(TempExtractDir, True, True, True);
  DeleteFile(ScriptFile);
  DeleteFile(LogFile);
  
  // Build the script content with Try/Catch error logging
  ScriptContent :=
    '$ErrorActionPreference = "Stop"' + #13#10 +
    'try {' + #13#10 +
    '  Expand-Archive -Path "' + ZipFile + '" -DestinationPath "' + TempExtractDir + '" -Force' + #13#10 +
    '  Get-ChildItem -Path "' + TempExtractDir + '" -File -Recurse | Move-Item -Destination "' + DestDir + '" -Force' + #13#10 +
    '  Remove-Item -Path "' + TempExtractDir + '" -Recurse -Force' + #13#10 +
    '} catch {' + #13#10 +
    '  $_ | Out-File -FilePath "' + LogFile + '" -Encoding utf8' + #13#10 +
    '  exit 1' + #13#10 +
    '}';
    
  // Save the script to a temporary file
  if not SaveStringToFile(ScriptFile, ScriptContent, False) then
  begin
    SuppressibleMsgBox('Failed to write extraction script.', mbError, MB_OK, MB_OK);
    exit;
  end;
  
  // Execute the PowerShell script from the file
  if not Exec('powershell.exe', '-NoProfile -ExecutionPolicy Bypass -File "' + ScriptFile + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
  begin
    ErrorMsg := 'Failed to extract ' + ExtractFileName(ZipFile) + ' (Exit Code: ' + IntToStr(ResultCode) + ').';
    if FileExists(LogFile) then
    begin
      if LoadStringFromFile(LogFile, AnsiErrorMsg) then
      begin
        ErrorMsg := 'Failed to extract ' + ExtractFileName(ZipFile) + ':' + #13#10#13#10 + String(AnsiErrorMsg);
      end;
    end;
    SuppressibleMsgBox(ErrorMsg, mbError, MB_OK, MB_OK);
  end;
  
  // Clean up the script and log files
  DeleteFile(ScriptFile);
  DeleteFile(LogFile);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  CoreDir: String;
begin
  if CurStep = ssPostInstall then begin
    CoreDir := ExpandConstant('{app}\framesmith_core');
    
    // Diagnostic popup to verify execution and target directory
    SuppressibleMsgBox('Installer entering ssPostInstall phase.' + #13#10 + 'Target Path: ' + CoreDir, mbInformation, MB_OK, MB_OK);
    
    if not ForceDirectories(CoreDir) then begin
      SuppressibleMsgBox('Failed to create target framesmith_core directory.', mbCriticalError, MB_OK, MB_OK);
      exit;
    end;
    
    // Extract and flatten selected components
    if FileExists(ExpandConstant('{tmp}\ort_core.zip')) then
      ExtractAndFlatten(ExpandConstant('{tmp}\ort_core.zip'), CoreDir)
    else if OptionPage.Values[CoreIndex] then
      SuppressibleMsgBox('Download file not found: ' + ExpandConstant('{tmp}\ort_core.zip'), mbError, MB_OK, MB_OK);
      
    if FileExists(ExpandConstant('{tmp}\ort_cuda.zip')) then
      ExtractAndFlatten(ExpandConstant('{tmp}\ort_cuda.zip'), CoreDir)
    else if (CudaIndex <> -1) and OptionPage.Values[CudaIndex] then
      SuppressibleMsgBox('Download file not found: ' + ExpandConstant('{tmp}\ort_cuda.zip'), mbError, MB_OK, MB_OK);
      
    if FileExists(ExpandConstant('{tmp}\ort_trt.zip')) then
      ExtractAndFlatten(ExpandConstant('{tmp}\ort_trt.zip'), CoreDir)
    else if (TrtIndex <> -1) and OptionPage.Values[TrtIndex] then
      SuppressibleMsgBox('Download file not found: ' + ExpandConstant('{tmp}\ort_trt.zip'), mbError, MB_OK, MB_OK);
  end;
end;
