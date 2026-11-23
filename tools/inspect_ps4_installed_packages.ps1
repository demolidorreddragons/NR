$ErrorActionPreference = "Stop"
if (-not $env:EVIDENCE_DIR -or -not (Test-Path -LiteralPath $env:EVIDENCE_DIR -PathType Container)) { throw "EVIDENCE_DIR is missing." }
if (-not $env:PKGTOOL -or -not (Test-Path -LiteralPath $env:PKGTOOL -PathType Leaf)) { throw "Pinned OrbisPkgTool DLL is missing." }
if (-not $env:RUNNER_TEMP) { throw "RUNNER_TEMP is missing." }
$controlPath = Join-Path $env:EVIDENCE_DIR "control-plane-preflight.json"
if (-not (Test-Path -LiteralPath $controlPath -PathType Leaf)) { throw "Control-plane evidence manifest is missing." }
$control = Get-Content -LiteralPath $controlPath -Raw | ConvertFrom-Json
if ($control.schema -ne "nr.ps4.control-plane-preflight.v3") { throw "Unexpected control-plane evidence schema." }
if (-not $env:EXPECTED_PREFLIGHT_RUN_ID -or [string]$control.preflight_run_id -ne $env:EXPECTED_PREFLIGHT_RUN_ID) { throw "Evidence artifact does not belong to the triggering preflight run." }
if ([string]$control.collector_commit -notmatch "^[a-fA-F0-9]{40}$") { throw "Collector source commit is missing from evidence manifest." }
if ($env:EXPECTED_COLLECTOR_COMMIT -and [string]$control.collector_commit -ne $env:EXPECTED_COLLECTOR_COMMIT) { throw "Inspector source revision does not match the collector commit in the evidence manifest." }
$entries = @($control.ftp_evidence)
$output = Join-Path $env:GITHUB_WORKSPACE "installed-package-inspection"
New-Item -ItemType Directory -Path $output -ErrorAction Stop | Out-Null
$results = [System.Collections.Generic.List[object]]::new()

function Get-EvidenceEntry([string]$remotePath) {
  return $entries | Where-Object { $_.operation -eq "GET" -and $_.path -eq $remotePath } | Select-Object -First 1
}
function Get-ListingEntry([string]$remotePath) {
  return $entries | Where-Object { $_.operation -eq "LIST" -and $_.path -eq $remotePath } | Select-Object -First 1
}

$targets = @(
  [pscustomobject]@{path="/user/app/PNEU00001/app.pkg";role="installed_app";required=$true},
  [pscustomobject]@{path="/user/app/PNEU00004/app.pkg";role="legacy_app";required=$false},
  [pscustomobject]@{path="/user/data/pkg/PS4-Neural-Engine-015.pkg";role="app_json_source";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-015.pkg";role="staged_canonical_015";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-015-7609b5d2b536.pkg";role="staged_015_7609";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-015-089684400316.pkg";role="staged_015_089684";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-006-dfda3a974df8.pkg";role="staged_006_dfda";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-006-eb68c632848b.pkg";role="staged_006_eb68";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-006-a3c9eb2b185e.pkg";role="staged_006_a3c9";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-006-0f0e7060fc3d.pkg";role="staged_006_0f0e";required=$false},
  [pscustomobject]@{path="/data/pkg/PS4-Neural-Engine-006-8d95e10d45e9.pkg";role="staged_006_8d95";required=$false}
)

foreach ($target in $targets) {
  $entry = Get-EvidenceEntry $target.path
  if (-not $entry -or -not $entry.local_file -or $entry.result -notin @("ok","size_or_mutability_mismatch","partial")) {
    $results.Add([pscustomobject]@{remote_path=$target.path;role=$target.role;status="not_collected";required=$target.required})
    if ($target.required) { throw "Required installed package evidence is unavailable: $($target.path)" }
    continue
  }
  $pkg = Join-Path $env:EVIDENCE_DIR $entry.local_file
  if (-not (Test-Path -LiteralPath $pkg -PathType Leaf)) { throw "Evidence file missing: $($entry.local_file)" }
  $size = (Get-Item -LiteralPath $pkg).Length
  $hash = (Get-FileHash -LiteralPath $pkg -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($size -ne [int64]$entry.local_bytes -or $hash -ne [string]$entry.sha256) { throw "Local artifact identity differs from collector manifest: $($target.path)" }
  if ($entry.result -ne "ok") {
    $results.Add([pscustomobject]@{remote_path=$target.path;role=$target.role;status=$entry.result;bytes=$size;sha256=$hash;required=$target.required})
    if ($target.required) { throw "Required installed app.pkg snapshot is not size-stable." }
    continue
  }

  $stem = [IO.Path]::GetFileNameWithoutExtension($target.path)
  $validationPath = Join-Path $output ($stem + ".validation.txt")
  $validation = dotnet $env:PKGTOOL validate $pkg 2>&1
  $validation | Out-File -LiteralPath $validationPath -Encoding utf8
  if ($LASTEXITCODE -ne 0) {
    $results.Add([pscustomobject]@{remote_path=$target.path;role=$target.role;status="pkg_validation_failed";bytes=$size;sha256=$hash})
    if ($target.required) { throw "Installed app.pkg failed independent PKG validation." }
    continue
  }

  $extract = Join-Path $env:RUNNER_TEMP ("nr-pkg-inspect-" + [guid]::NewGuid().ToString("N"))
  dotnet $env:PKGTOOL extract $pkg $extract --verbose 2>&1 | Out-File -LiteralPath (Join-Path $output ($stem + ".extract.txt")) -Encoding utf8
  if ($LASTEXITCODE -ne 0) {
    $results.Add([pscustomobject]@{remote_path=$target.path;role=$target.role;status="pkg_extract_failed";bytes=$size;sha256=$hash})
    if ($target.required) { throw "Installed app.pkg extraction failed." }
    continue
  }
  $sfo = Get-ChildItem -LiteralPath $extract -Recurse -File -Filter "param.sfo" | Select-Object -First 1
  if (-not $sfo) {
    $results.Add([pscustomobject]@{remote_path=$target.path;role=$target.role;status="param_sfo_missing";bytes=$size;sha256=$hash})
    if ($target.required) { throw "Installed app.pkg has no embedded PARAM.SFO." }
    continue
  }
  $sfoText = (dotnet $env:PKGTOOL sfo read $sfo.FullName 2>&1) -join [Environment]::NewLine
  if ($LASTEXITCODE -ne 0) { throw "PARAM.SFO parser failed for $($target.path)." }
  $sfoText | Out-File -LiteralPath (Join-Path $output ($stem + ".param-sfo.txt")) -Encoding utf8
  $appVerMatch = [regex]::Match($sfoText, '(?m)^APP_VER\s*=\s*([^\r\n]+)')
  $versionMatch = [regex]::Match($sfoText, '(?m)^VERSION\s*=\s*([^\r\n]+)')
  $appVer = if ($appVerMatch.Success) { $appVerMatch.Groups[1].Value.Trim() } else { "" }
  $version = if ($versionMatch.Success) { $versionMatch.Groups[1].Value.Trim() } else { "" }
  $expectedTitle = if ($target.role -eq "legacy_app") { "PNEU00004" } else { "PNEU00001" }
  $titleMatch = [regex]::Match($sfoText, '(?m)^TITLE_ID\s*=\s*([^\r\n]+)')
  $contentMatch = [regex]::Match($sfoText, '(?m)^CONTENT_ID\s*=\s*([^\r\n]+)')
  $categoryMatch = [regex]::Match($sfoText, '(?m)^CATEGORY\s*=\s*([^\r\n]+)')
  $storageMatch = [regex]::Match($sfoText, 'st_type=([^,\s]+)')
  $storageType = if ($storageMatch.Success) { $storageMatch.Groups[1].Value.Trim() } else { "" }
  $titleId = if ($titleMatch.Success) { $titleMatch.Groups[1].Value.Trim() } else { "" }
  $contentId = if ($contentMatch.Success) { $contentMatch.Groups[1].Value.Trim() } else { "" }
  $category = if ($categoryMatch.Success) { $categoryMatch.Groups[1].Value.Trim() } else { "" }
  $expectedContent = if ($expectedTitle -eq "PNEU00004") { $contentId -match '^IV0000-PNEU00004_' } else { $contentId -eq "IV0000-PNEU00001_00-PS4NEURALENG0000" }
  $semantic = ($category -eq "gde") -and ($titleId -eq $expectedTitle) -and $expectedContent -and ($storageType -eq 'digital50') -and ($appVer -ne "") -and ($appVer -eq $version)
  $packagedPrx = Get-ChildItem -LiteralPath $extract -Recurse -File | Where-Object { $_.Name -eq "ps4_neural_engine.prx" } | Select-Object -First 1
  $prxHash = $null
  if ($packagedPrx) { $prxHash = (Get-FileHash -LiteralPath $packagedPrx.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
  $results.Add([pscustomobject]@{
    remote_path=$target.path;role=$target.role;status=$(if($semantic){"PASS_PC_ONLY"}else{"SFO_SEMANTIC_MISMATCH"})
    bytes=$size;sha256=$hash;category=$category;title_id=$titleId;app_ver=$appVer;version=$version
    content_id=$contentId;storage_type=$storageType;packaged_prx_sha256=$prxHash
  })
  if ($target.required -and -not $semantic) { throw "Installed app.pkg PARAM.SFO semantic fields do not match the NR base application contract." }
}

$installed = $results | Where-Object { $_.role -eq "installed_app" } | Select-Object -First 1
$source = $results | Where-Object { $_.role -eq "app_json_source" } | Select-Object -First 1
$staged = @($results | Where-Object { $_.role -like "staged_*" })
$comparisons = [ordered]@{
  installed_app_sha256=$installed.sha256
  app_json_source_sha256=$source.sha256
  installed_matches_app_json_source=($installed.sha256 -and $source.sha256 -and $installed.sha256 -eq $source.sha256)
  staged_packages=@($staged | ForEach-Object { [pscustomobject]@{role=$_.role;sha256=$_.sha256;matches_installed=($_.sha256 -and $_.sha256 -eq $installed.sha256)} })
  app_json_package_digest_semantics="not_assumed_to_be_raw_file_sha256"
}
$currentPrxEntry = Get-EvidenceEntry "/data/GoldHEN/plugins/ps4_neural_engine.prx"
$prxSnapshotPaths = @(
  "/data/GoldHEN/plugins/ps4_neural_engine.prx",
  "/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-tmp",
  "/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-backup",
  "/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-backup.001"
)
$prxSnapshots = [System.Collections.Generic.List[object]]::new()
$pluginListingEntry = Get-ListingEntry "/data/GoldHEN/plugins/"
$pluginListingLines = @()
if ($pluginListingEntry -and $pluginListingEntry.local_file) {
  $pluginListingPath = Join-Path $env:EVIDENCE_DIR $pluginListingEntry.local_file
  if (Test-Path -LiteralPath $pluginListingPath -PathType Leaf) {
    $pluginListingLines = Get-Content -LiteralPath $pluginListingPath
  }
}
function Get-PluginListingSize([string]$fileName) {
  $escaped = [regex]::Escape($fileName)
  $line = $pluginListingLines | Where-Object { $_ -match ("\s" + $escaped + "\z") } | Select-Object -First 1
  if (-not $line) { return $null }
  $match = [regex]::Match($line, '^\\S+\\s+\\d+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+')
  if ($match.Success) { return [int64]$match.Groups[1].Value }
  return $null
}
foreach ($path in $prxSnapshotPaths) {
  $entry = Get-EvidenceEntry $path
  if (-not $entry -or -not $entry.local_file -or $entry.result -notin @("ok","size_or_mutability_mismatch","partial")) {
    $prxSnapshots.Add([pscustomobject]@{remote_path=$path;status="not_collected"})
    continue
  }
  $file = Join-Path $env:EVIDENCE_DIR $entry.local_file
  if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "PRX evidence file is missing: $($entry.local_file)" }
  $bytes = (Get-Item -LiteralPath $file).Length
  $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($bytes -ne [int64]$entry.local_bytes -or $hash -ne [string]$entry.sha256) { throw "PRX evidence differs from collector manifest: $path" }
  $fileName = [IO.Path]::GetFileName($path)
  $listingSize = Get-PluginListingSize $fileName
  $listGetMatch = if ($null -ne $listingSize) { $listingSize -eq $bytes } else { $null }
  $prxSnapshots.Add([pscustomobject]@{
    remote_path=$path;status=$entry.result;bytes=$bytes;sha256=$hash
    listing_bytes=$listingSize;list_get_size_match=$listGetMatch
    stable_get=($entry.result -eq "ok")
  })
}
$comparisons["goldhen_prx_snapshots"] = @($prxSnapshots)
$releaseManifestEntry = Get-EvidenceEntry "/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-release.ini"
$releasePrxHash = $null
if ($releaseManifestEntry -and $releaseManifestEntry.result -eq "ok" -and $releaseManifestEntry.local_file) {
  $releasePath = Join-Path $env:EVIDENCE_DIR $releaseManifestEntry.local_file
  $releaseText = Get-Content -LiteralPath $releasePath -Raw
  $releaseHashMatch = [regex]::Match($releaseText, '(?m)^prx_sha256=([a-fA-F0-9]{64})\\z')
  if ($releaseHashMatch.Success) { $releasePrxHash = $releaseHashMatch.Groups[1].Value.ToLowerInvariant() }
  foreach ($key in @("release_number","app_version","title_id","content_id","abi_status","hardware_status")) {
    $valueMatch = [regex]::Match($releaseText, "(?m)^" + [regex]::Escape($key) + "=([^\\r\\n]+)")
    if ($valueMatch.Success) { $comparisons["release_manifest_" + $key] = $valueMatch.Groups[1].Value.Trim() }
  }
  $comparisons["release_manifest_prx_sha256"] = $releasePrxHash
}
$pluginsIniEntry = Get-EvidenceEntry "/data/GoldHEN/plugins.ini"
$pluginsIniRegistersPrx = $false
if ($pluginsIniEntry -and $pluginsIniEntry.result -eq "ok" -and $pluginsIniEntry.local_file) {
  $pluginsIniPath = Join-Path $env:EVIDENCE_DIR $pluginsIniEntry.local_file
  $pluginsIniText = Get-Content -LiteralPath $pluginsIniPath -Raw
  $pluginsIniRegistersPrx = [regex]::IsMatch($pluginsIniText, '(?im)^\\s*/data/GoldHEN/plugins/ps4_neural_engine\\.prx\\s*\\z')
}
$comparisons["plugins_ini_registers_nr_prx"] = $pluginsIniRegistersPrx
$currentPrxHash = $null
$currentPrxTrusted = $false
$currentPrxSnapshot = $prxSnapshots | Where-Object { $_.remote_path -eq "/data/GoldHEN/plugins/ps4_neural_engine.prx" } | Select-Object -First 1
if ($currentPrxSnapshot) {
  $currentPrxHash = $currentPrxSnapshot.sha256
  $comparisons["current_goldhen_prx_sha256"] = $currentPrxHash
  $comparisons["current_goldhen_prx_snapshot_result"] = $currentPrxSnapshot.status
  $comparisons["current_goldhen_prx_get_bytes"] = $currentPrxSnapshot.bytes
  $comparisons["current_goldhen_prx_listing_bytes"] = $currentPrxSnapshot.listing_bytes
  $comparisons["current_goldhen_prx_list_get_size_match"] = $currentPrxSnapshot.list_get_size_match
  $manifestMatches = ($releasePrxHash -and $currentPrxHash -and $releasePrxHash -eq $currentPrxHash)
  $comparisons["release_manifest_matches_current_prx"] = [bool]$manifestMatches
  $currentPrxTrusted = ($currentPrxSnapshot.status -eq "ok" -and $currentPrxSnapshot.list_get_size_match -eq $true -and $manifestMatches)
  $comparisons["current_goldhen_prx_identity_trusted"] = [bool]$currentPrxTrusted
  $comparisons["installed_app_prx_matches_goldhen_prx"] = [bool]($currentPrxTrusted -and $installed.packaged_prx_sha256 -and $installed.packaged_prx_sha256 -eq $currentPrxHash)
}
foreach ($snapshot in $prxSnapshots) {
  $snapshot | Add-Member -NotePropertyName matches_release_manifest -NotePropertyValue ([bool]($releasePrxHash -and $snapshot.sha256 -eq $releasePrxHash)) -Force
  $snapshot | Add-Member -NotePropertyName matches_installed_app_prx -NotePropertyValue ([bool]($installed.packaged_prx_sha256 -and $snapshot.sha256 -eq $installed.packaged_prx_sha256)) -Force
}
$comparisons["goldhen_prx_snapshots"] = @($prxSnapshots)
$appJsonEntry = Get-EvidenceEntry "/user/app/PNEU00001/app.json"
if ($appJsonEntry -and $appJsonEntry.result -eq "ok") {
  $appJsonPath = Join-Path $env:EVIDENCE_DIR $appJsonEntry.local_file
  $appJson = Get-Content -LiteralPath $appJsonPath -Raw | ConvertFrom-Json
  $comparisons["app_json_package_digest"] = [string]$appJson.packageDigest
  $comparisons["app_json_source_url"] = [string]$appJson.pieces[0].url
}
$report = [ordered]@{
  schema="nr.ps4.installed-package-inspection.v1"
  inspected_utc=[DateTime]::UtcNow.ToString("o")
  preflight_run_id=$control.preflight_run_id
  build_run_id=$control.build_run_id
  build_commit=$control.build_commit
  collector_commit=$control.collector_commit
  results=$results
  comparisons=$comparisons
  hardware_validation="not_performed"
}
$json = $report | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText((Join-Path $output "installed-package-inspection.json"),$json,[Text.UTF8Encoding]::new($false))
$json | Set-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8
Write-Host $json
