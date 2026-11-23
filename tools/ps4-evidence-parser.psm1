function ConvertFrom-NrReleaseManifest {
  [CmdletBinding()]
  param([Parameter(Mandatory=$true)][string]$Text)
  return ConvertFrom-StringData $Text
}

function Get-NrFtpListingSize {
  [CmdletBinding()]
  param(
    [Parameter(Mandatory=$true)][string[]]$Lines,
    [Parameter(Mandatory=$true)][string]$FileName
  )
  foreach ($line in $Lines) {
    if ($line.TrimEnd().EndsWith($FileName, [StringComparison]::Ordinal)) {
      $match = [regex]::Match($line, '^\S+\s+\d+\s+\S+\s+\S+\s+(\d+)\s+')
      if ($match.Success) { return [int64]$match.Groups[1].Value }
    }
  }
  return $null
}

function Test-NrPluginsIniRegistration {
  [CmdletBinding()]
  param([Parameter(Mandatory=$true)][string]$Text)
  return $Text.Contains("/data/GoldHEN/plugins/ps4_neural_engine.prx")
}

Export-ModuleMember -Function ConvertFrom-NrReleaseManifest, Get-NrFtpListingSize, Test-NrPluginsIniRegistration
