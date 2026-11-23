$ErrorActionPreference = "Stop"
if (-not $env:PS4_IP) { throw "PS4_IP Actions variable is missing." }
$ip = $null
if (-not [System.Net.IPAddress]::TryParse($env:PS4_IP,[ref]$ip) -or $ip.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) { throw "PS4_IP must be a literal IPv4 address." }
if (-not $env:RUNNER_TEMP -or -not $env:GITHUB_RUN_ID) { throw "Unique evidence location is missing." }
$root = Join-Path $env:RUNNER_TEMP ("nr-rpi-readiness-" + $env:GITHUB_RUN_ID)
if (Test-Path -LiteralPath $root) { throw "Refusing evidence overwrite." }
New-Item -ItemType Directory -Path $root -Force | Out-Null
$base = "http://$($env:PS4_IP):12800"
function Safe-Text([string]$Value) {
  if ($null -eq $Value) { return $null }
  $safe = $Value -replace [regex]::Escape($env:PS4_IP), "[redacted-ip]"
  $safe = [regex]::Replace($safe, '\b(?:\d{1,3}\.){3}\d{1,3}\b', "[redacted-ip]")
  if ($safe.Length -gt 8192) { $safe = $safe.Substring(0,8192) }
  return $safe
}
function Invoke-Http([string]$Path,[string]$Method,[string]$Body=$null) {
  $request = [System.Net.HttpWebRequest]::Create($base + $Path)
  $request.Method = $Method
  $request.Proxy = $null
  $request.AllowAutoRedirect = $false
  $request.Timeout = 5000
  $request.ReadWriteTimeout = 5000
  if ($Method -eq "POST") {
    $bytes = [Text.Encoding]::UTF8.GetBytes($Body)
    $request.ContentType = "application/json"
    $request.ContentLength = $bytes.Length
    $stream = $request.GetRequestStream()
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
  }
  try { $response = [System.Net.HttpWebResponse]$request.GetResponse() }
  catch [System.Net.WebException] {
    if ($_.Exception.Response) { $response = [System.Net.HttpWebResponse]$_.Exception.Response }
    else { return [pscustomobject]@{status=$null;body="";error=(Safe-Text $_.Exception.GetBaseException().Message)} }
  }
  try {
    $reader = New-Object IO.StreamReader($response.GetResponseStream())
    try { $text = $reader.ReadToEnd() } finally { $reader.Dispose() }
    return [pscustomobject]@{status=[int]$response.StatusCode;body=(Safe-Text $text);error=$null}
  } finally { $response.Dispose() }
}
$started = [DateTime]::UtcNow.ToString("o")
$getApi = Invoke-Http "/api" "GET"
$fingerprint = ($getApi.status -in @(400,405)) -and ($getApi.body -match 'Unsupported method|"fail"')
$observations = [System.Collections.Generic.List[object]]::new()
$observations.Add([pscustomobject]@{path="/api";method="GET";status=$getApi.status;body_prefix=$getApi.body;error=$getApi.error})
$statusSummary = $null
$identitySummary = $null
$existsSummary = $null
$existsValid = $false
if ($fingerprint) {
  $status = Invoke-Http "/api/status" "GET"
  $observations.Add([pscustomobject]@{path="/api/status";method="GET";status=$status.status;body_prefix=$status.body;error=$status.error})
  if ($status.status -eq 200) {
    try { $statusJson = $status.body | ConvertFrom-Json; $statusSummary = [pscustomobject]@{busy=$statusJson.busy;active=$statusJson.active} } catch { $statusSummary = $null }
  }
  $identity = Invoke-Http "/api/v2/system" "GET"
  $observations.Add([pscustomobject]@{path="/api/v2/system";method="GET";status=$identity.status;body_prefix="redacted; see identity_summary";error=$identity.error})
  if ($identity.status -eq 200) {
    try {
      $idJson = $identity.body | ConvertFrom-Json
      $identitySummary = [pscustomobject]@{name=$idJson.data.name;version=$idJson.data.version;title_id=$idJson.data.title_id;port=$idJson.data.port;network=$idJson.data.network;ip_redacted=$true}
    } catch { $identitySummary = $null }
  }
  # Read-only query, explicitly supported by the pinned RPI-NG probe contract.
  $exists = Invoke-Http "/api/is_exists" "POST" '{"title_id":"PNEU00001"}'
  if ($exists.status -eq 200) {
    try {
      $existsJson = $exists.body | ConvertFrom-Json
      $existsValid = ($existsJson.status -eq "success")
      $existsSummary = [pscustomobject]@{status=$existsJson.status;exists=$existsJson.exists;size=$existsJson.size}
    } catch { $existsValid = $false }
  }
  $observations.Add([pscustomobject]@{path="/api/is_exists";method="POST_READ_ONLY_QUERY";status=$exists.status;body_prefix=$exists.body;error=$exists.error})
}
$report = [ordered]@{
  schema="nr.ps4.rpi-readiness.v1"
  started_utc=$started
  completed_utc=[DateTime]::UtcNow.ToString("o")
  target_port=12800
  target_ip="redacted"
  backend_fingerprint=$(if($fingerprint){"RPI_API_CONFIRMED"}else{"NOT_CONFIRMED"})
  read_only_title_id_query=$existsValid
  exists_summary=$existsSummary
  status_summary=$statusSummary
  identity_summary=$identitySummary
  observations=$observations
  install_requests_sent=$false
  task_control_requests_sent=$false
  binloader_9090_probed=$false
  remote_files_modified=$false
  pkg_installed=$false
}
$json = $report | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText((Join-Path $root "rpi-readiness.json"),$json,[Text.UTF8Encoding]::new($false))
Write-Host "RPI_BACKEND=$($report.backend_fingerprint)"
Write-Host "RPI_READ_ONLY_TITLE_QUERY=$existsValid"
Write-Host "RPI_INSTALL_REQUESTS_SENT=false"
Write-Host "RPI_READINESS_COMPLETE"
