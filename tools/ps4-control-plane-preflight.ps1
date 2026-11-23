$ErrorActionPreference = "Stop"
if (-not $env:PS4_IP) { throw "PS4_IP Actions variable is missing; refusing network access." }
if (-not $env:RUNNER_TEMP) { throw "RUNNER_TEMP is missing." }
$root = Join-Path $env:RUNNER_TEMP ("nr-control-plane-preflight-" + $env:GITHUB_RUN_ID)
if (Test-Path -LiteralPath $root) { throw "Evidence directory already exists; refusing overwrite: $root" }
New-Item -ItemType Directory -Path $root -Force | Out-Null
$evidence = [System.Collections.Generic.List[object]]::new()
$base = "ftp://$($env:PS4_IP):2121"
$credential = New-Object System.Net.NetworkCredential("anonymous", "anonymous@example.com")
function New-FtpRequest([string]$remotePath, [string]$method) {
  $request = [System.Net.FtpWebRequest]::Create($base + $remotePath)
  $request.Method = $method; $request.Credentials = $credential
  $request.UsePassive = $true; $request.UseBinary = $true; $request.KeepAlive = $false
  $request.Timeout = 20000; $request.ReadWriteTimeout = 30000
  return $request
}
# Strictly read-only FTP: LIST, GETSIZE and GET. Preserve LIST bytes verbatim.
function Get-LocalKey([string]$value) {
  $sha=[Security.Cryptography.SHA256]::Create()
  try { $bytes=[Text.Encoding]::UTF8.GetBytes($value); return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-","").ToLowerInvariant() }
  finally { $sha.Dispose() }
}
function Save-Listing([string]$remotePath) {
  $key=Get-LocalKey $remotePath
  $destination=Join-Path $root ("listing-"+$key+".raw.bin")
  $started=[DateTime]::UtcNow.ToString("o")
  try {
    $response=(New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::ListDirectoryDetails)).GetResponse()
    try {
      $sourceStream=$response.GetResponseStream()
      $output=[IO.File]::Open($destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
      try { $sourceStream.CopyTo($output); $output.Flush($true) }
      finally { $output.Dispose(); $sourceStream.Dispose() }
    } finally { $response.Dispose() }
    $item=Get-Item -LiteralPath $destination
    $hash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    $evidence.Add([pscustomobject]@{operation="LIST";path=$remotePath;result="ok";started_utc=$started;completed_utc=[DateTime]::UtcNow.ToString("o");local_bytes=$item.Length;sha256=$hash;local_file=[IO.Path]::GetFileName($destination)})
  } catch {
    $detail=$_.Exception.GetBaseException().Message
    if(Test-Path -LiteralPath $destination) {
      $partial=$destination+".partial"; Move-Item -LiteralPath $destination -Destination $partial
      $item=Get-Item -LiteralPath $partial; $hash=(Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant()
      $evidence.Add([pscustomobject]@{operation="LIST";path=$remotePath;result="partial";started_utc=$started;completed_utc=[DateTime]::UtcNow.ToString("o");error=$detail;local_bytes=$item.Length;sha256=$hash;local_file=[IO.Path]::GetFileName($partial)})
    } else { $evidence.Add([pscustomobject]@{operation="LIST";path=$remotePath;result="unavailable";started_utc=$started;completed_utc=[DateTime]::UtcNow.ToString("o");error=$detail}) }
  }
}
$directories=@("/data/","/data/pkg/","/data/GoldHEN/","/data/GoldHEN/plugins/","/user/app/","/user/app/PNEU00004/","/user/app/PNEU00001/")
foreach($remotePath in $directories){Save-Listing $remotePath}

# Only NR-owned registration/PRX, app metadata and loader log are downloaded.
function Save-RemoteFile([string]$remotePath) {
  $key=Get-LocalKey $remotePath
  $destination=Join-Path $root ("file-"+$key+".raw.bin")
  $started=[DateTime]::UtcNow.ToString("o")
  $remoteBefore=$null
  try {
    $sizeResponse=(New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::GetFileSize)).GetResponse()
    try {$remoteBefore=[int64]$sizeResponse.ContentLength} finally {$sizeResponse.Dispose()}
    if($remoteBefore -gt 16777216){$evidence.Add([pscustomobject]@{operation="GET";path=$remotePath;result="skipped_size_limit";remote_bytes_before=$remoteBefore;started_utc=$started});return}
    $response=(New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::DownloadFile)).GetResponse()
    try {
      $sourceStream=$response.GetResponseStream()
      $output=[IO.File]::Open($destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
      try {$sourceStream.CopyTo($output);$output.Flush($true)}
      finally {$output.Dispose();$sourceStream.Dispose()}
    } finally {$response.Dispose()}
    $remoteAfter=$null
    $sizeResponse=(New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::GetFileSize)).GetResponse()
    try {$remoteAfter=[int64]$sizeResponse.ContentLength} finally {$sizeResponse.Dispose()}
    $item=Get-Item -LiteralPath $destination
    $hash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    $stable=($remoteBefore -eq $remoteAfter -and $remoteAfter -eq $item.Length)
    $evidence.Add([pscustomobject]@{operation="GET";path=$remotePath;result=$(if($stable){"ok"}else{"size_or_mutability_mismatch"});started_utc=$started;completed_utc=[DateTime]::UtcNow.ToString("o");remote_bytes_before=$remoteBefore;remote_bytes_after=$remoteAfter;local_bytes=$item.Length;size_stable=$stable;sha256=$hash;local_file=[IO.Path]::GetFileName($destination)})
  } catch {
    $detail=$_.Exception.GetBaseException().Message
    if(Test-Path -LiteralPath $destination) {
      $partial=$destination+".partial"; Move-Item -LiteralPath $destination -Destination $partial
      $item=Get-Item -LiteralPath $partial; $hash=(Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant()
      $evidence.Add([pscustomobject]@{operation="GET";path=$remotePath;result="partial";started_utc=$started;completed_utc=[DateTime]::UtcNow.ToString("o");remote_bytes_before=$remoteBefore;error=$detail;local_bytes=$item.Length;sha256=$hash;local_file=[IO.Path]::GetFileName($partial)})
    } else {$evidence.Add([pscustomobject]@{operation="GET";path=$remotePath;result="unavailable";started_utc=$started;completed_utc=[DateTime]::UtcNow.ToString("o");remote_bytes_before=$remoteBefore;error=$detail})}
  }
}
$files=@("/data/GoldHEN/plugins.ini","/data/GoldHEN/plugins/ps4_neural_engine.prx","/user/app/PNEU00004/app.json","/user/app/PNEU00004/app.xml","/user/app/PNEU00001/app.json","/user/app/PNEU00001/app.xml","/data/Loader_Logs.txt")
foreach($remotePath in $files){Save-RemoteFile $remotePath}
# Safe RPI fingerprint: HTTP GET only on TCP 12800. Never probe BinLoader 9090.
$rpi=[ordered]@{host=$env:PS4_IP;port=12800;probe="GET /api; GET /api/status; optional GET /api/v2/system";mutating_requests_sent=$false;backend_fingerprint="not_confirmed";api_status=$null;api_identity=$null;observations=@()}
function Invoke-ReadOnlyHttpGet([string]$url) {
  $request=[System.Net.HttpWebRequest]::Create($url);$request.Method="GET";$request.Proxy=$null;$request.Timeout=3000;$request.ReadWriteTimeout=3000;$request.AllowAutoRedirect=$false
  try {$response=[System.Net.HttpWebResponse]$request.GetResponse()} catch [System.Net.WebException] {
    if($_.Exception.Response){$response=[System.Net.HttpWebResponse]$_.Exception.Response}else{return [pscustomobject]@{status=$null;body="";error=$_.Exception.GetBaseException().Message}}
  }
  try {$reader=New-Object System.IO.StreamReader($response.GetResponseStream());try {$body=$reader.ReadToEnd()}finally{$reader.Dispose()};if($body.Length -gt 8192){$body=$body.Substring(0,8192)};return [pscustomobject]@{status=[int]$response.StatusCode;body=$body;error=$null}} finally {$response.Dispose()}
}
$api=Invoke-ReadOnlyHttpGet "http://$($env:PS4_IP):12800/api"
$rpi.observations+= [pscustomobject]@{path="/api";status=$api.status;body_prefix=$api.body;error=$api.error}
if(($api.status -in @(400,405)) -and ($api.body -match 'Unsupported method|"fail"')){
  $rpi.backend_fingerprint="rpi_api_fingerprint_detected"
  $status=Invoke-ReadOnlyHttpGet "http://$($env:PS4_IP):12800/api/status"
  $rpi.observations+= [pscustomobject]@{path="/api/status";status=$status.status;body_prefix=$status.body;error=$status.error}
  if($status.status -eq 200 -and $status.body -match '"busy"\s*:'){$rpi.api_status="json_status_endpoint_detected"}
  $identity=Invoke-ReadOnlyHttpGet "http://$($env:PS4_IP):12800/api/v2/system"
  $rpi.observations+= [pscustomobject]@{path="/api/v2/system";status=$identity.status;body_prefix=$identity.body;error=$identity.error}
  if($identity.status -eq 200 -and $identity.body -match '"name"\s*:'){$rpi.api_identity="v2_identity_response_detected"}
}
$report=[ordered]@{
  schema="nr.ps4.control-plane-preflight.v2";timestamp_utc=[DateTime]::UtcNow.ToString("o")
  runner=$env:COMPUTERNAME;runner_identity="demolidorreddragons-NR.PS4-WINDOWS-BRIDGE"
  build_run_id=$env:BUILD_RUN_ID;build_commit=$env:BUILD_HEAD_SHA;build_branch=$env:BUILD_BRANCH
  target=[ordered]@{ps4_ip_source="repository_actions_variable";firmware_reported="14.02";firmware_evidence="user_reported";goldhen_reported="2.4b18.14";goldhen_distribution="GamerHack host; patnawa mirror";payload_sha256_published_by_distribution="c5480587d99efc2e3cc132352d9b7cf052dd3068280f1686e7fbb37d57e9ba8b";installed_payload_hash="not_read_from_device"}
  ftp_operations="LIST_GETSIZE_GET_ONLY";ftp_evidence=$evidence;remote_package_installer=$rpi
  binloader_9090_probed=$false;remote_files_modified=$false;pkg_installed=$false;prx_activated=$false;goldhen_restarted=$false
  scope="read_only_discovery_and_identity_snapshot"
}
$json=$report|ConvertTo-Json -Depth 10
$reportPath=Join-Path $root "control-plane-preflight.json"
[System.IO.File]::WriteAllText($reportPath,$json,[System.Text.UTF8Encoding]::new($false))
$json|Set-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding UTF8
Write-Host $json
Get-ChildItem -LiteralPath $root -File|ForEach-Object {$hash=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant();Write-Host ("LOCAL_EVIDENCE file={0} bytes={1} sha256={2}" -f $_.Name,$_.Length,$hash)}
Write-Host "CONTROL_PLANE_READ_ONLY_COMPLETE"
