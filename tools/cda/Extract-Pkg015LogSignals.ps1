[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $EvidenceDirectory -PathType Container)) { throw "Evidence directory not found: $EvidenceDirectory" }
$manifestPath = Join-Path $EvidenceDirectory 'evidence-manifest.txt'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Evidence manifest is missing; refusing to infer source paths.' }
$manifest = [IO.File]::ReadAllLines($manifestPath)
$targets = @{}
foreach ($entry in $manifest) {
    if ($entry -match '^FILE_SNAPSHOT remote=(\S+) .* local=(\S+)$') {
        $remote = $Matches[1]; $local = $Matches[2]
        if ($remote -match '(?i)(pne\.log|Loader_Logs\.txt|memory\.log|sce_logs/log\.bin)$') { $targets[$local] = $remote }
    }
}
$patterns = @(
    @{ Name='preflight'; Regex='(?i)pre[\s_-]*flight' },
    @{ Name='gcn8_assets'; Regex='(?i)GCN8.{0,80}assets|assets.{0,80}GCN8' },
    @{ Name='gnm_exports'; Regex='(?i)GNM.{0,80}(exports|symbols)|(?:exports|symbols).{0,80}GNM' },
    @{ Name='gnm_calls'; Regex='(?i)no\s+GNM\s+calls|GNM.{0,40}calls?' },
    @{ Name='prx_activation'; Regex='(?i)NR\s+PRX.{0,40}(activated|activation)|PRX.{0,40}(activated|activation)' },
    @{ Name='crash_or_error'; Regex='(?i)CE-34878-0|fatal|error|failed|failure|exception|abort(?:ed)?|crash(?:ed)?|assert(?:ion)?|segmentation|access violation|panic|SIGSEGV|SIGABRT' }
)
$findings = [Collections.Generic.List[object]]::new()
$scanned = [Collections.Generic.List[object]]::new()
function Add-MatchesFromText([string]$Text, [string]$Source, [System.Text.Encoding]$TextEncoding, [long]$BaseOffset) {
    $lines = $Text -split '\r?\n'
    $cursor = 0
    $lineNumber = 0
    foreach ($line in $lines) {
        $lineNumber++
        foreach ($pattern in $patterns) {
            if ([regex]::IsMatch($line, $pattern.Regex)) {
                $excerpt = $line
                if ($excerpt.Length -gt 700) { $excerpt = $excerpt.Substring(0,700) }
                $findings.Add([pscustomobject]@{ source_remote=$Source; encoding=$TextEncoding.WebName; signal=$pattern.Name; offset_bytes=$BaseOffset+$cursor; line_number=$lineNumber; excerpt=$excerpt })
            }
        }
        $cursor += $TextEncoding.GetByteCount($line) + $TextEncoding.GetByteCount([string][char]10)
    }
}
foreach ($localName in ($targets.Keys | Sort-Object)) {
    $path = Join-Path $EvidenceDirectory $localName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    $bytes = [IO.File]::ReadAllBytes($path)
    $remote = $targets[$localName]
    $scanned.Add([pscustomobject]@{ source_remote=$remote; local_file=$localName; bytes=$bytes.Length; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() })
    $isKnownText = $remote -match '(?i)(pne\.log|Loader_Logs\.txt|memory\.log)$'
    if ($isKnownText) {
        $encoding = [Text.Encoding]::UTF8; $offset = 0
        if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $encoding = [Text.Encoding]::UTF8; $offset = 3 }
        elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) { $encoding = [Text.Encoding]::Unicode; $offset = 2 }
        elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) { $encoding = [Text.Encoding]::BigEndianUnicode; $offset = 2 }
        $text = $encoding.GetString($bytes, $offset, $bytes.Length - $offset)
        Add-MatchesFromText $text $remote $encoding $offset
    } else {
        # Preserve log.bin byte-for-byte. This is exploratory string extraction, not a proprietary-format decoder.
        foreach ($mode in @('ASCII','UTF16LE')) {
            $run = [Text.StringBuilder]::new(); $runStart = 0
            $step = if ($mode -eq 'ASCII') { 1 } else { 2 }
            for ($i = 0; $i -lt $bytes.Length; $i += $step) {
                $value = [int]$bytes[$i]
                $printable = (($value -ge 0x20 -and $value -le 0x7E) -or $value -eq 0x09)
                if ($mode -eq 'UTF16LE') {
                    $high = if (($i + 1) -lt $bytes.Length) { [int]$bytes[$i+1] } else { 1 }
                    $printable = $printable -and ($high -eq 0)
                }
                if ($printable) {
                    if ($run.Length -eq 0) { $runStart = $i }
                    [void]$run.Append([char]$value)
                } else {
                    if ($run.Length -ge 4) {
                        $textRun = $run.ToString()
                        foreach ($pattern in $patterns) {
                            if ([regex]::IsMatch($textRun, $pattern.Regex)) {
                                $excerpt = $textRun
                                if ($excerpt.Length -gt 700) { $excerpt = $excerpt.Substring(0,700) }
                                $findings.Add([pscustomobject]@{ source_remote=$remote; encoding=$mode; signal=$pattern.Name; offset_bytes=$runStart; line_number=$null; excerpt=$excerpt })
                            }
                        }
                    }
                    [void]$run.Clear()
                }
            }
            if ($run.Length -ge 4) {
                $textRun = $run.ToString()
                foreach ($pattern in $patterns) {
                    if ([regex]::IsMatch($textRun, $pattern.Regex)) {
                        $excerpt = $textRun
                        if ($excerpt.Length -gt 700) { $excerpt = $excerpt.Substring(0,700) }
                        $findings.Add([pscustomobject]@{ source_remote=$remote; encoding=$mode; signal=$pattern.Name; offset_bytes=$runStart; line_number=$null; excerpt=$excerpt })
                    }
                }
            }
        }
    }
}
$result = [pscustomobject]@{
    schema='CDA-LOG-SIGNALS-1'
    generated_utc=[DateTime]::UtcNow.ToString('o')
    method='text line scan + exploratory ASCII/UTF16LE printable-run extraction'
    limitation='No proprietary log.bin decoding; matches are leads, not proof of a crash or root cause.'
    scanned_sources=@($scanned)
    findings=@($findings)
}
$json = ConvertTo-Json -InputObject $result -Depth 8
[IO.File]::WriteAllText((Join-Path $EvidenceDirectory 'log-signal-findings.json'),$json,[Text.UTF8Encoding]::new($false))
$lines = [Collections.Generic.List[string]]::new()
$lines.Add('CDA PKG015 LOG SIGNAL SEARCH')
$lines.Add("Generated UTC: $($result.generated_utc)")
$lines.Add("Sources scanned: $($scanned.Count)")
$lines.Add("Findings: $($findings.Count)")
$lines.Add('Method: text line scan + exploratory ASCII/UTF16LE printable-run extraction')
$lines.Add('Caution: no proprietary log.bin decoder; absence of strings does not mean absence of an error.')
foreach ($finding in $findings) { $lines.Add(('[{0}] source={1} encoding={2} offset={3} excerpt={4}' -f $finding.signal,$finding.source_remote,$finding.encoding,$finding.offset_bytes,$finding.excerpt)) }
[IO.File]::WriteAllLines((Join-Path $EvidenceDirectory 'log-signal-findings.txt'),$lines,[Text.UTF8Encoding]::new($false))
Write-Host ("CDA_LOG_SCAN_COMPLETE sources={0} findings={1}" -f $scanned.Count,$findings.Count)
