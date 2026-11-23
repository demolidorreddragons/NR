$ErrorActionPreference = "Stop"
$module = Join-Path (Split-Path $PSScriptRoot -Parent) "ps4-evidence-parser.psm1"
Import-Module $module -Force

$manifest = @'
schema=1
release_number=010
installer_version=1
app_version=01.06
title_id=PNEU00001
content_id=IV0000-PNEU00001_00-PS4NEURALENG0000
prx_file=ps4_neural_engine.prx
prx_sha256=54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277
abi_status=unverified
hardware_status=not_tested
'@
$values = ConvertFrom-NrReleaseManifest -Text $manifest
if ($values["release_number"] -ne "010") { throw "Release number parser failed." }
if ($values["app_version"] -ne "01.06") { throw "App version parser failed." }
if ($values["prx_sha256"] -ne "54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277") { throw "Manifest PRX SHA-256 parser failed." }

$listing = @(
  "-rw------- 1 root root 224128 Nov 21 20:33 ps4_neural_engine.prx",
  "-rw------- 1 root root 206528 Nov 21 20:33 ps4_neural_engine.prx.pne-backup"
)
$size = Get-NrFtpListingSize -Lines $listing -FileName "ps4_neural_engine.prx"
if ($size -ne 224128) { throw "FTP LIST size parser failed: $size" }
if ($null -ne (Get-NrFtpListingSize -Lines $listing -FileName "missing.prx")) { throw "Missing listing entry must return null." }

$nl = [char]13 + [char]10
$ini = "[default]" + $nl + "/data/GoldHEN/plugins/ps4_neural_engine.prx" + $nl
if (-not (Test-NrPluginsIniRegistration -Text $ini)) { throw "plugins.ini registration parser failed." }
if (Test-NrPluginsIniRegistration -Text ("[default]" + $nl + "/other/plugin.prx" + $nl)) { throw "plugins.ini parser accepted an unrelated path." }

Write-Host "PS4 release manifest, FTP LIST size, and plugins.ini parser tests: PASS"
