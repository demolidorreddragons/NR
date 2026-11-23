$ErrorActionPreference = "Stop"
$serviceName = "actions.runner.demolidorreddragons-NR.PS4-WINDOWS-BRIDGE"
if (-not $env:RUNNER_TEMP -or -not $env:GITHUB_RUN_ID) { throw "Missing unique evidence location." }
$root = Join-Path $env:RUNNER_TEMP ("nr-windows-runner-service-" + $env:GITHUB_RUN_ID)
if (Test-Path -LiteralPath $root) { throw "Refusing to overwrite existing evidence." }
New-Item -ItemType Directory -Path $root -Force | Out-Null
Write-Host "RUNNER_DIAG_PHASE=service-control"
function Read-Sc([string[]]$Arguments) {
  $out = & sc.exe @Arguments 2>&1
  return [pscustomobject]@{command=("sc.exe " + ($Arguments -join " "));exit_code=$LASTEXITCODE;output=(@($out) -join [Environment]::NewLine)}
}
$sc = @(
  (Read-Sc @("qc",$serviceName)),
  (Read-Sc @("queryex",$serviceName)),
  (Read-Sc @("qfailure",$serviceName)),
  (Read-Sc @("qsidtype",$serviceName))
)
$svc = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
$regPath = "HKLM:\SYSTEM\CurrentControlSet\Services\$serviceName"
$reg = $null
$runnerRoot = $null
$binary = $null
if (Test-Path -LiteralPath $regPath) {
  $raw = Get-ItemProperty -LiteralPath $regPath
  $reg = [ordered]@{}
  foreach ($key in @("Start","ObjectName","ImagePath","DelayedAutoStart","DependOnService","ServiceSidType","RequiredPrivileges","FailureActions")) {
    if ($raw.PSObject.Properties.Name -contains $key) { $reg[$key] = $raw.$key }
  }
  if ($raw.ImagePath -match '(?i)"?([A-Z]:\\.*?\\bin\\RunnerService\.exe)') {
    $binary = $matches[1]
    $runnerRoot = Split-Path -Parent (Split-Path -Parent $binary)
  }
}
Write-Host "RUNNER_DIAG_PHASE=process-and-acl"
$processes = @(Get-Process -Name "RunnerService","Runner.Listener","Runner.Worker" -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,StartTime)
$aclEvidence = @()
foreach ($path in @($binary,$runnerRoot)) {
  if ($path -and (Test-Path -LiteralPath $path)) {
    try {
      $item = Get-Item -LiteralPath $path
      $acl = Get-Acl -LiteralPath $path
      $aclEvidence += [pscustomobject]@{path=$path;is_directory=$item.PSIsContainer;owner=$acl.Owner;rules=@($acl.Access | Select-Object -First 12 IdentityReference,FileSystemRights,AccessControlType,IsInherited)}
    } catch { $aclEvidence += [pscustomobject]@{path=$path;error=$_.Exception.GetBaseException().Message} }
  }
}
Write-Host "RUNNER_DIAG_PHASE=bounded-event-logs"
function Get-EventEvidence([string]$LogName,[int[]]$Ids,[int]$Newest) {
  try {
    return @(Get-EventLog -LogName $LogName -Newest $Newest -ErrorAction Stop | Where-Object { $_.EventID -in $Ids } | Select-Object -First 40 | ForEach-Object {
      $msg=[string]$_.Message
      if ($msg -match '(?i)(token|password|authorization|bearer|secret|credential)') { $msg="[suppressed possible credential-bearing event]" }
      elseif ($msg.Length -gt 1600) { $msg=$msg.Substring(0,1600) }
      [pscustomobject]@{time_utc=$_.TimeGenerated.ToUniversalTime().ToString("o");id=$_.EventID;source=$_.Source;entry_type=$_.EntryType;message=$msg}
    })
  } catch { return @([pscustomobject]@{error=$_.Exception.GetBaseException().Message}) }
}
$systemEvents = Get-EventEvidence "System" @(7000,7001,7009,7011,7023,7024,7031,7034,7036,7040) 500
$appEvents = Get-EventEvidence "Application" @(1000,1026) 200
Write-Host "RUNNER_DIAG_PHASE=runner-diagnostic-file-inventory"
$diagFiles = @()
if ($runnerRoot) {
  $diagPath = Join-Path $runnerRoot "_diag"
  if (Test-Path -LiteralPath $diagPath) {
    $diagFiles = @(Get-ChildItem -LiteralPath $diagPath -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 20 | ForEach-Object {
      [pscustomobject]@{name=$_.Name;bytes=$_.Length;last_write_utc=$_.LastWriteTimeUtc.ToString("o")}
    })
  }
}
$startType = $null
if ($reg -and $null -ne $reg.Start) {
  $startType = switch ([int]$reg.Start) { 2 {"automatic"} 3 {"manual"} 4 {"disabled"} default {"unknown"} }
}
$report = [ordered]@{
  schema="nr.windows.runner-service-diagnostics.v2"
  collected_utc=[DateTime]::UtcNow.ToString("o")
  computer=$env:COMPUTERNAME
  service_name=$serviceName
  service_state=$(if($svc){[string]$svc.Status}else{"not_found"})
  start_type=$startType
  service_control=$sc
  registry_read_only=$reg
  runner_root=$runnerRoot
  binary_acl=$aclEvidence
  related_processes=$processes
  system_service_events=$systemEvents
  application_crash_events=$appEvents
  runner_diag_files=$diagFiles
  mutations_performed=$false
  services_started_or_stopped=$false
  registry_modified=$false
  runner_reconfigured=$false
  ps4_state_modified=$false
}
$json=$report | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText((Join-Path $root "runner-service-diagnostics.json"),$json,[Text.UTF8Encoding]::new($false))
$summary=@("Service state: $($report.service_state)","Start type: $startType","Service registry account: $($reg.ObjectName)","Binary: $($reg.ImagePath)","Dependencies: $(@($reg.DependOnService) -join ', ')","Runner root: $runnerRoot","System events captured: $(@($systemEvents).Count)","Application events captured: $(@($appEvents).Count)","Runner diagnostic files inventoried: $(@($diagFiles).Count)","Read-only: no service, registry, runner configuration, or PS4 state changed.") -join [Environment]::NewLine
[IO.File]::WriteAllText((Join-Path $root "summary.txt"),$summary,[Text.UTF8Encoding]::new($false))
Write-Host $summary
Write-Host "RUNNER_SERVICE_DIAGNOSTICS_READ_ONLY_COMPLETE"
