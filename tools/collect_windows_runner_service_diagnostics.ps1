$ErrorActionPreference = "Stop"
$serviceName = "actions.runner.demolidorreddragons-NR.PS4-WINDOWS-BRIDGE"
if (-not $env:RUNNER_TEMP -or -not $env:GITHUB_RUN_ID) { throw "Missing unique evidence location." }
$root = Join-Path $env:RUNNER_TEMP ("nr-windows-runner-service-" + $env:GITHUB_RUN_ID)
if (Test-Path -LiteralPath $root) { throw "Refusing overwrite." }
New-Item -ItemType Directory -Path $root -Force | Out-Null
$service = Get-CimInstance Win32_Service -Filter ("Name='" + $serviceName + "'")
if (-not $service) { throw "Registered runner service not found." }
function Read-Sc([string[]]$args) {
  $out = & sc.exe @args 2>&1
  return [pscustomobject]@{ command=("sc.exe " + ($args -join " ")); exit_code=$LASTEXITCODE; output=(@($out) -join [Environment]::NewLine) }
}
$sc = @((Read-Sc @("qc",$serviceName)),(Read-Sc @("queryex",$serviceName)),(Read-Sc @("qfailure",$serviceName)),(Read-Sc @("qsidtype",$serviceName)))
$regPath = "HKLM:\SYSTEM\CurrentControlSet\Services\$serviceName"
$reg = $null
if (Test-Path -LiteralPath $regPath) {
  $item = Get-ItemProperty -LiteralPath $regPath
  $reg = [ordered]@{}
  foreach ($key in @("Start","ObjectName","ImagePath","DelayedAutoStart","DependOnService","ServiceSidType","RequiredPrivileges")) {
    if ($item.PSObject.Properties.Name -contains $key) { $reg[$key] = $item.$key }
  }
}
$runnerRoot = $null
if ($service.PathName -match '(?i)"?([A-Z]:\\.*?\\bin\\RunnerService\.exe)') {
  $runnerRoot = Split-Path -Parent (Split-Path -Parent $matches[1])
}
$files = @()
if ($runnerRoot) {
  foreach ($p in @((Join-Path $runnerRoot "run.cmd"),(Join-Path $runnerRoot "_diag"))) {
    if (Test-Path -LiteralPath $p) {
      $x = Get-Item -LiteralPath $p
      $acl = Get-Acl -LiteralPath $p
      $files += [pscustomobject]@{path=$p;is_directory=$x.PSIsContainer;owner=$acl.Owner;acl=@($acl.Access | Select-Object -First 12 IdentityReference,FileSystemRights,AccessControlType,IsInherited)}
    }
  }
}
$processes = @(Get-CimInstance Win32_Process | Where-Object {$_.Name -in @("RunnerService.exe","Runner.Listener.exe","Runner.Worker.exe")} | Select-Object Name,ProcessId,ParentProcessId,ExecutablePath,CreationDate)
$since = (Get-Date).AddDays(-30)
$events = @()
try {
  $events = @(Get-WinEvent -FilterHashtable @{LogName="System";ProviderName="Service Control Manager";Id=@(7000,7001,7009,7011,7023,7024,7031,7034,7036,7040);StartTime=$since} -MaxEvents 40 | ForEach-Object {
    $msg=[string]$_.Message
    if ($msg -match '(?i)(token|password|authorization|bearer|secret|credential)') {$msg="[suppressed possible credential-bearing event]"}
    elseif ($msg.Length -gt 1800) {$msg=$msg.Substring(0,1800)}
    [pscustomobject]@{time_utc=$_.TimeCreated.ToUniversalTime().ToString("o");id=$_.Id;provider=$_.ProviderName;message=$msg}
  })
} catch {$events=@([pscustomobject]@{error=$_.Exception.GetBaseException().Message})}
$diag = @()
if ($runnerRoot -and (Test-Path (Join-Path $runnerRoot "_diag"))) {
  $diagPath=Join-Path $runnerRoot "_diag"
  $diagFiles=@(Get-ChildItem $diagPath -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 10 | ForEach-Object {[pscustomobject]@{name=$_.Name;bytes=$_.Length;last_write_utc=$_.LastWriteTimeUtc.ToString("o")}})
  foreach ($f in (Get-ChildItem $diagPath -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 5)) {
    foreach ($line in (Get-Content $f.FullName -Tail 1000)) {
      if ($line -match '(?i)(service.*(fail|start|stop)|runner.*(fail|error|exception)|1068|1053|access is denied|logon failure|dependency|fatal)' -and $line -notmatch '(?i)(token|password|authorization|bearer|secret|credential)') {
        if ($line.Length -gt 500) {$line=$line.Substring(0,500)}
        $diag += [pscustomobject]@{file=$f.Name;line=$line}
      }
    }
  }
} else {$diagFiles=@()}
$report=[ordered]@{
  schema="nr.windows.runner-service-diagnostics.v1";collected_utc=[DateTime]::UtcNow.ToString("o");computer=$env:COMPUTERNAME
  service_name=$serviceName;service=[ordered]@{state=$service.State;start_mode=$service.StartMode;start_name=$service.StartName;process_id=$service.ProcessId;exit_code=$service.ExitCode;service_specific_exit_code=$service.ServiceSpecificExitCode;path_name=$service.PathName;dependencies=@($service.Dependencies);description=$service.Description}
  service_control=$sc;registry_read_only=$reg;runner_root=$runnerRoot;filesystem_acl=$files;related_processes=$processes;system_service_events=$events;runner_diag_files=$diagFiles;runner_diag_error_excerpts=@($diag | Select-Object -Last 120)
  mutations_performed=$false;services_started_or_stopped=$false;registry_modified=$false;runner_reconfigured=$false
}
$json=$report | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText((Join-Path $root "runner-service-diagnostics.json"),$json,[Text.UTF8Encoding]::new($false))
$summary=@("Runner service state: $($service.State)","Start mode: $($service.StartMode)","Logon account: $($service.StartName)","Exit code: $($service.ExitCode)","Service-specific exit: $($service.ServiceSpecificExitCode)","Binary: $($service.PathName)","Dependencies: $(@($service.Dependencies) -join ', ')","Runner root: $runnerRoot","Service events: $(@($events).Count)","Diagnostic excerpts: $(@($diag).Count)","Read-only; no service, registry, runner configuration, or PS4 state changed.") -join [Environment]::NewLine
[IO.File]::WriteAllText((Join-Path $root "summary.txt"),$summary,[Text.UTF8Encoding]::new($false))
Write-Host $summary
Write-Host "RUNNER_SERVICE_DIAGNOSTICS_READ_ONLY_COMPLETE"
