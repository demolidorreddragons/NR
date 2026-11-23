$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

if (-not $env:PS4_IP -or $env:PS4_IP -notmatch '^\d{1,3}(\.\d{1,3}){3}$') { throw "PS4_IP must be a literal IPv4 address." }
$parsed = $null
if (-not [System.Net.IPAddress]::TryParse($env:PS4_IP, [ref]$parsed) -or $parsed.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) { throw "PS4_IP is not a valid IPv4 address." }
if (-not $env:EVIDENCE_INPUT -or -not (Test-Path -LiteralPath $env:EVIDENCE_INPUT -PathType Container)) { throw "Offline inspection artifact directory is missing." }
if (-not $env:EVIDENCE_DIR) { throw "EVIDENCE_DIR is required." }
if (-not $env:SOURCE_OFFLINE_RUN_ID -or $env:SOURCE_OFFLINE_RUN_ID -notmatch '^\d+$') { throw "SOURCE_OFFLINE_RUN_ID is invalid." }

New-Item -ItemType Directory -Path $env:EVIDENCE_DIR -Force | Out-Null
$reportFiles = @(Get-ChildItem -LiteralPath $env:EVIDENCE_INPUT -Filter "installed-package-inspection.json" -File -Recurse)
if ($reportFiles.Count -ne 1) { throw "Expected exactly one installed-package-inspection.json in the source artifact." }
$report = Get-Content -LiteralPath $reportFiles[0].FullName -Raw | ConvertFrom-Json
if ($report.schema -ne "nr.ps4.installed-package-inspection.v1") { throw "Unexpected offline inspection schema." }
if ([string]$report.preflight_run_id -notmatch '^\d+$') { throw "Offline report is missing the source preflight run ID." }
if ([string]$report.collector_commit -notmatch '^[a-fA-F0-9]{40}$') { throw "Offline report is missing the exact collector commit." }
if ($report.hardware_validation -ne "not_performed") { throw "Unexpected hardware-validation state; archive gate is closed." }
if ($report.comparisons.installed_matches_app_json_source -ne $true) { throw "Installed app.pkg does not match app.json source; archive gate is closed." }

$installed = @($report.results | Where-Object { $_.role -eq "installed_app" })
$source = @($report.results | Where-Object { $_.role -eq "app_json_source" })
$staged = @($report.results | Where-Object { $_.role -like "staged_*" })
if ($installed.Count -ne 1 -or $source.Count -ne 1) { throw "Installed/source package identity entries are ambiguous." }
if ($installed[0].status -ne "PASS_PC_ONLY" -or $source[0].status -ne "PASS_PC_ONLY") { throw "Installed/source package QA status is not PASS_PC_ONLY." }
if ($installed[0].title_id -ne "PNEU00001" -or $installed[0].category -ne "gde" -or $installed[0].storage_type -ne "digital50") { throw "Installed NR application identity is not the expected Full/Base gde package." }
if ($installed[0].sha256 -ne $source[0].sha256 -or $installed[0].sha256 -ne $report.comparisons.installed_app_sha256) { throw "Installed/source SHA-256 chain does not agree." }
if ($staged.Count -lt 1) { throw "No staged NR packages were validated." }

$canonical = @($staged | Where-Object { $_.role -eq "staged_canonical_015" })
if ($canonical.Count -ne 1) { throw "Expected exactly one canonical staged PKG 015." }
if ($canonical[0].remote_path -ne "/data/pkg/PS4-Neural-Engine-015.pkg") { throw "Canonical staged PKG path changed." }
if ($canonical[0].status -ne "PASS_PC_ONLY" -or $canonical[0].sha256 -ne $installed[0].sha256) { throw "Canonical staged PKG 015 is not byte-identical to installed app.pkg." }
$installedVersion = [version]$installed[0].app_ver

foreach ($pkg in $staged) {
  if ($pkg.status -ne "PASS_PC_ONLY") { throw "Refusing to archive unvalidated package: $($pkg.remote_path)" }
  if ($pkg.title_id -ne "PNEU00001" -or $pkg.category -ne "gde" -or $pkg.storage_type -ne "digital50") { throw "Refusing non-NR or non-Full/Base package: $($pkg.remote_path)" }
  if ([version]$pkg.app_ver -gt $installedVersion) { throw "A staged package is newer than the installed app; refusing archive cleanup." }
  if ($pkg.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or [int64]$pkg.bytes -le 0) { throw "Package identity is incomplete: $($pkg.remote_path)" }
  if ($pkg.remote_path -notmatch '^/data/pkg/PS4-Neural-Engine-[A-Za-z0-9._-]+\.pkg$') { throw "Unexpected staged package path: $($pkg.remote_path)" }
}

function New-FtpRequest([string]$remotePath, [string]$method) {
  $uri = "ftp://$($env:PS4_IP):2121$remotePath"
  $request = [System.Net.FtpWebRequest]::Create($uri)
  $request.Method = $method
  $request.Credentials = New-Object System.Net.NetworkCredential("anonymous", "anonymous@example.com")
  $request.UseBinary = $true
  $request.UsePassive = $true
  $request.KeepAlive = $false
  $request.Timeout = 60000
  $request.ReadWriteTimeout = 60000
  return $request
}

function Get-RemoteSize([string]$remotePath) {
  $response = $null
  try {
    $response = (New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::GetFileSize)).GetResponse()
    return [int64]$response.ContentLength
  } catch {
    $ftpResponse = $_.Exception.Response
    if ($ftpResponse -and [int]$ftpResponse.StatusCode -eq 550) { return $null }
    throw
  } finally {
    if ($response) { $response.Dispose() }
  }
}

function Save-RemoteListing([string]$remotePath, [string]$localPath) {
  $response = $null
  $inputStream = $null
  $outputStream = $null
  try {
    $response = (New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::ListDirectoryDetails)).GetResponse()
    $inputStream = $response.GetResponseStream()
    $outputStream = [IO.File]::Open($localPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $inputStream.CopyTo($outputStream)
    $outputStream.Flush($true)
  } finally {
    if ($outputStream) { $outputStream.Dispose() }
    if ($inputStream) { $inputStream.Dispose() }
    if ($response) { $response.Dispose() }
  }
  return [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($localPath))
}

function ConvertFrom-FtpListing([string]$listingText) {
  $items = @()
  foreach ($line in ($listingText -split '\r?\n')) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split '\s+'
    if ($parts.Count -lt 9) { continue }
    $name = $parts[-1]
    if ($name -eq "." -or $name -eq "..") { continue }
    $items += [pscustomobject]@{ mode=[string]$parts[0]; name=[string]$name; raw_line=[string]$line }
  }
  return $items
}

function Save-RemoteFile([string]$remotePath, [string]$localPath) {
  $started = [DateTime]::UtcNow.ToString("o")
  $before = Get-RemoteSize $remotePath
  if ($null -eq $before) { throw "Remote file is unavailable: $remotePath" }
  $response = $null
  $inputStream = $null
  $outputStream = $null
  try {
    $response = (New-FtpRequest $remotePath ([System.Net.WebRequestMethods+Ftp]::DownloadFile)).GetResponse()
    $inputStream = $response.GetResponseStream()
    $outputStream = [IO.File]::Open($localPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $inputStream.CopyTo($outputStream)
    $outputStream.Flush($true)
  } finally {
    if ($outputStream) { $outputStream.Dispose() }
    if ($inputStream) { $inputStream.Dispose() }
    if ($response) { $response.Dispose() }
  }
  $after = Get-RemoteSize $remotePath
  $item = Get-Item -LiteralPath $localPath
  $hash = (Get-FileHash -LiteralPath $localPath -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($before -ne $after -or $after -ne $item.Length) { throw "Remote size changed during GET: $remotePath" }
  return [pscustomobject]@{ remote_path=$remotePath; bytes=[int64]$item.Length; sha256=$hash; remote_bytes_before=$before; remote_bytes_after=$after; started_utc=$started; completed_utc=[DateTime]::UtcNow.ToString("o"); local_file=[IO.Path]::GetFileName($localPath) }
}

function Invoke-FtpRename([string]$sourcePath, [string]$destinationPath) {
  $request = New-FtpRequest $sourcePath ([System.Net.WebRequestMethods+Ftp]::Rename)
  $request.RenameTo = $destinationPath
  $response = $request.GetResponse()
  try { return [string]$response.StatusDescription } finally { $response.Dispose() }
}

$rootListingPath = Join-Path $env:EVIDENCE_DIR "pkg-root-before.raw.bin"
$oldListingPath = Join-Path $env:EVIDENCE_DIR "pkg-old-before.raw.bin"
$rootListingText = Save-RemoteListing "/data/pkg/" $rootListingPath
$oldListingText = Save-RemoteListing "/data/pkg/old/" $oldListingPath
$rootEntries = @(ConvertFrom-FtpListing $rootListingText)
$oldEntries = @(ConvertFrom-FtpListing $oldListingText)
$reservedOldNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($oldEntry in $oldEntries) { [void]$reservedOldNames.Add([string]$oldEntry.name) }
$oldDirectory = @($rootEntries | Where-Object { $_.name -eq "old" })
if ($oldDirectory.Count -ne 1 -or -not $oldDirectory[0].mode.StartsWith("d")) { throw "/data/pkg/old is not a verified directory; refusing archive." }
$rootNr = @($rootEntries | Where-Object { $_.name -like "PS4-Neural-Engine-*.pkg" })
if (@($rootNr | Where-Object { -not $_.mode.StartsWith("-") }).Count -gt 0) { throw "An NR PKG path is not a regular file; refusing to move it." }

$actualPaths = @($rootNr | ForEach-Object { "/data/pkg/$($_.name)" } | Sort-Object -Unique)
$reportedPaths = @($staged | ForEach-Object { [string]$_.remote_path } | Sort-Object -Unique)
if ($rootNr.Count -ne $reportedPaths.Count -or @(Compare-Object -ReferenceObject $reportedPaths -DifferenceObject $actualPaths).Count -gt 0) { throw "Current NR package root differs from the validated offline inspection inventory." }

$nonNrBefore = @($rootEntries | Where-Object { $_.name -notlike "PS4-Neural-Engine-*.pkg" } | ForEach-Object { $_.name } | Sort-Object)

$liveInstalled = Save-RemoteFile "/user/app/PNEU00001/app.pkg" (Join-Path $env:EVIDENCE_DIR "live-installed-app.pkg")
$liveSource = Save-RemoteFile "/user/data/pkg/PS4-Neural-Engine-015.pkg" (Join-Path $env:EVIDENCE_DIR "live-app-json-source.pkg")
$liveCanonical = Save-RemoteFile "/data/pkg/PS4-Neural-Engine-015.pkg" (Join-Path $env:EVIDENCE_DIR "live-canonical-staged.pkg")
if ($liveInstalled.sha256 -ne $installed[0].sha256 -or $liveSource.sha256 -ne $source[0].sha256 -or $liveCanonical.sha256 -ne $canonical[0].sha256) { throw "Live installed/source/canonical package identity changed since offline inspection." }

$manifest = [ordered]@{
  schema = "nr.ps4.archive-superseded-packages.v1"
  started_utc = [DateTime]::UtcNow.ToString("o")
  archive_run_id = $env:GITHUB_RUN_ID
  source_offline_run_id = $env:SOURCE_OFFLINE_RUN_ID
  source_preflight_run_id = [string]$report.preflight_run_id
  source_build_run_id = [string]$report.build_run_id
  source_build_commit = [string]$report.build_commit
  collector_commit = [string]$report.collector_commit
  installed_app_sha256 = [string]$installed[0].sha256
  canonical_pkg_path = [string]$canonical[0].remote_path
  canonical_pkg_sha256 = [string]$canonical[0].sha256
  archive_directory = "/data/pkg/old/"
  files = [System.Collections.Generic.List[object]]::new()
  final_root_nr_packages = @()
  third_party_root_names_unchanged = $false
  installation_performed = $false
  prx_modified = $false
  plugins_ini_modified = $false
  goldhen_restarted = $false
  status = "ARCHIVE_IN_PROGRESS"
}
$manifestPath = Join-Path $env:EVIDENCE_DIR "archive-manifest.json"
function Save-Manifest {
  $manifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $manifestPath -Encoding utf8
}
Save-Manifest

try {
  foreach ($pkg in ($staged | Where-Object { $_.role -ne "staged_canonical_015" })) {
    $sourcePath = [string]$pkg.remote_path
    $sourceName = [IO.Path]::GetFileName($sourcePath)
    $baseName = [IO.Path]::GetFileNameWithoutExtension($sourceName)
    $expectedHash = ([string]$pkg.sha256).ToLowerInvariant()
    $expectedBytes = [int64]$pkg.bytes
    $sourceListed = @($rootNr | Where-Object { $_.name -eq $sourceName }).Count -eq 1
    $sourceSize = Get-RemoteSize $sourcePath

    if ($null -eq $sourceSize) {
      if ($sourceListed) { throw "FTP 550 for a source package that is still listed in /data/pkg; treating it as access denied, not absent: $sourcePath" }
      $matchingArchives = @($oldEntries | Where-Object { $_.name -like "$baseName-archived-$($env:SOURCE_OFFLINE_RUN_ID)-*.pkg" })
      $already = $false
      foreach ($existing in $matchingArchives) {
        $archivePath = "/data/pkg/old/$($existing.name)"
        $localExisting = Join-Path $env:EVIDENCE_DIR ("already-" + $existing.name)
        $snapExisting = Save-RemoteFile $archivePath $localExisting
        if ($snapExisting.bytes -eq $expectedBytes -and $snapExisting.sha256 -eq $expectedHash) {
          $manifest.files.Add([pscustomobject]@{ source_path=$sourcePath; archive_path=$archivePath; status="ALREADY_ARCHIVED_AND_VERIFIED"; bytes=$snapExisting.bytes; sha256=$snapExisting.sha256; readback=$snapExisting }) | Out-Null
          Save-Manifest
          $already = $true
          break
        }
      }
      if (-not $already) { throw "Source package is absent from root and no matching verified archive exists: $sourcePath" }
      continue
    }
    if (-not $sourceListed) { throw "GETSIZE succeeded for a package absent from the captured /data/pkg listing: $sourcePath" }

    $sourceLocal = Join-Path $env:EVIDENCE_DIR ("source-" + $sourceName)
    $sourceSnapshot = Save-RemoteFile $sourcePath $sourceLocal
    if ($sourceSnapshot.bytes -ne $expectedBytes -or $sourceSnapshot.sha256 -ne $expectedHash) { throw "Live source package differs from the offline inspection: $sourcePath" }

    $destinationPath = $null
    for ($n = 0; $n -lt 100; $n++) {
      $archiveName = "$baseName-archived-$($env:SOURCE_OFFLINE_RUN_ID)-$n.pkg"
      if (-not $reservedOldNames.Contains($archiveName)) {
        $destinationPath = "/data/pkg/old/$archiveName"
        [void]$reservedOldNames.Add($archiveName)
        break
      }
    }
    if (-not $destinationPath) { throw "No collision-free archive name available from the captured /data/pkg/old listing for $sourcePath" }

    $entry = [pscustomobject]@{
      source_path=$sourcePath
      archive_path=$destinationPath
      status="SOURCE_VERIFIED_PENDING_RENAME"
      bytes=$expectedBytes
      sha256=$expectedHash
      source_snapshot=$sourceSnapshot
      rename_started_utc=$null
      rename_completed_utc=$null
      rename_status=$null
      archive_readback=$null
    }
    $manifest.files.Add($entry) | Out-Null
    Save-Manifest

    $entry.rename_started_utc = [DateTime]::UtcNow.ToString("o")
    $entry.rename_status = Invoke-FtpRename $sourcePath $destinationPath
    $entry.rename_completed_utc = [DateTime]::UtcNow.ToString("o")
    $entry.status = "RENAMED_PENDING_READBACK"
    Save-Manifest

    if ($null -ne (Get-RemoteSize $sourcePath)) { throw "Source path still exists after FTP rename: $sourcePath" }
    $archiveLocal = Join-Path $env:EVIDENCE_DIR ("archive-" + [IO.Path]::GetFileName($destinationPath))
    $archiveSnapshot = Save-RemoteFile $destinationPath $archiveLocal
    if ($archiveSnapshot.bytes -ne $expectedBytes -or $archiveSnapshot.sha256 -ne $expectedHash) { throw "Archived readback identity mismatch: $destinationPath" }
    $entry.archive_readback = $archiveSnapshot
    $entry.status = "ARCHIVED_AND_READBACK_VERIFIED"
    Save-Manifest
  }

  $canonicalLocal = Join-Path $env:EVIDENCE_DIR "canonical-final.pkg"
  $canonicalSnapshot = Save-RemoteFile "/data/pkg/PS4-Neural-Engine-015.pkg" $canonicalLocal
  if ($canonicalSnapshot.sha256 -ne $installed[0].sha256 -or $canonicalSnapshot.sha256 -ne $canonical[0].sha256) { throw "Canonical PKG 015 changed during archive cleanup." }

  $rootFinalPath = Join-Path $env:EVIDENCE_DIR "pkg-root-after.raw.bin"
  $rootFinalText = Save-RemoteListing "/data/pkg/" $rootFinalPath
  $rootFinalEntries = @(ConvertFrom-FtpListing $rootFinalText)
  $rootFinalNr = @($rootFinalEntries | Where-Object { $_.name -like "PS4-Neural-Engine-*.pkg" })
  $nonNrAfter = @($rootFinalEntries | Where-Object { $_.name -notlike "PS4-Neural-Engine-*.pkg" } | ForEach-Object { $_.name } | Sort-Object)
  if ($rootFinalNr.Count -ne 1 -or $rootFinalNr[0].name -ne "PS4-Neural-Engine-015.pkg") { throw "Final root must contain exactly the canonical NR PKG 015 and no other NR PKG." }
  if (@(Compare-Object -ReferenceObject $nonNrBefore -DifferenceObject $nonNrAfter).Count -gt 0) { throw "A non-NR package or directory entry changed during archive cleanup." }

  $manifest.final_root_nr_packages = @($rootFinalNr | ForEach-Object { $_.name })
  $manifest.third_party_root_names_unchanged = $true
  $manifest.canonical_final_readback = $canonicalSnapshot
  $manifest.completed_utc = [DateTime]::UtcNow.ToString("o")
  $manifest.status = "ARCHIVE_COMPLETE_READBACK_VERIFIED"
  Save-Manifest
  Write-Host "ARCHIVE_COMPLETE_READBACK_VERIFIED"
} catch {
  $manifest.completed_utc = [DateTime]::UtcNow.ToString("o")
  $manifest.status = "ARCHIVE_FAILED_OR_PARTIAL"
  $manifest.error = $_.Exception.GetBaseException().Message
  Save-Manifest
  throw
}
