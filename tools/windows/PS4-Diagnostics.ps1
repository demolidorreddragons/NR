param(
  [string]$Ps4Host = "192.168.0.17",
  [int]$FtpPort = 2121,
  [int]$IntervalSeconds = 15,
  [string]$OutputRoot = "$env:USERPROFILE\PS4NeuralEngine-Diagnostics",
  [switch]$Once
)
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
Start-Transcript -Path (Join-Path $OutputRoot "collector-session.log") -Append | Out-Null
function Log([string]$Message) { Write-Host ("[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message) }
function FtpRequest([string]$Path, [string]$Method) {
  $uri = "ftp://{0}:{1}{2}" -f $Ps4Host, $FtpPort, $Path
  $r = [System.Net.FtpWebRequest]::Create($uri)
  $r.Method = $Method
  $r.Credentials = [System.Net.NetworkCredential]::new("anonymous", "ps4diag@localhost")
  $r.UsePassive = $true; $r.UseBinary = $true; $r.KeepAlive = $false
  $r.Timeout = 8000; $r.ReadWriteTimeout = 8000
  return $r
}
function GetListing([string]$Path) {
  $r = FtpRequest $Path ([System.Net.WebRequestMethods+Ftp]::ListDirectoryDetails)
  $response = $r.GetResponse()
  try {
    $reader = [System.IO.StreamReader]::new($response.GetResponseStream())
    try { return @($reader.ReadToEnd() -split '\r?\n' | Where-Object { $_.Trim() }) }
    finally { $reader.Dispose() }
  } finally { $response.Dispose() }
}
function Download([string]$RemotePath, [string]$LocalPath) {
  $r = FtpRequest $RemotePath ([System.Net.WebRequestMethods+Ftp]::DownloadFile)
  $response = $r.GetResponse()
  try {
    $input = $response.GetResponseStream()
    $output = [System.IO.File]::Create($LocalPath)
    try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
  } finally { $response.Dispose() }
}
function CollectDirectory([string]$RemoteDir) {
  try { $entries = GetListing $RemoteDir }
  catch { Log ("Diretório indisponível {0}: {1}" -f $RemoteDir, $_.Exception.Message); return }
  $leaf = $RemoteDir.Trim("/") -replace "/", "_"
  $localDir = Join-Path $OutputRoot $leaf
  New-Item -ItemType Directory -Force -Path $localDir | Out-Null
  foreach ($entry in $entries) {
    $name = (($entry -split '\s+')[-1]).Trim()
    if (!$name -or $name -in @(".", "..")) { continue }
    if ($entry -match "^d") { continue }
    $remote = $RemoteDir.TrimEnd("/") + "/" + $name
    $local = Join-Path $localDir $name
    try {
      $temp = $local + ".part"
      Download $remote $temp
      Move-Item -Force $temp $local
      Log ("Baixado: {0}" -f $remote)
    } catch { Log ("Falha no arquivo {0}: {1}" -f $remote, $_.Exception.Message) }
  }
}
Log "Coletor somente leitura iniciado. Nenhum arquivo do PS4 será alterado."
Log ("Destino local: {0}" -f $OutputRoot)
try {
  do {
    try {
      $probe = FtpRequest "/" ([System.Net.WebRequestMethods+Ftp]::ListDirectory)
      $response = $probe.GetResponse(); $response.Dispose()
      Log ("FTP acessível em {0}:{1}" -f $Ps4Host, $FtpPort)
      CollectDirectory "/data/PS4NeuralEngine"
      CollectDirectory "/data/PS4NeuralEngine/logs"
      CollectDirectory "/data/PS4NeuralEngine/captures"
      [ordered]@{ collected_at=(Get-Date).ToUniversalTime().ToString("o"); ps4_host=$Ps4Host; ftp_port=$FtpPort; note="Somente leitura; ausência dos diretórios pode indicar que o plugin ainda não os criou." } |
        ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $OutputRoot "collection-status.json")
    } catch { Log ("FTP indisponível: {0}" -f $_.Exception.Message) }
    if ($Once) { break }
    Start-Sleep -Seconds $IntervalSeconds
  } while ($true)
} finally { Stop-Transcript | Out-Null }
