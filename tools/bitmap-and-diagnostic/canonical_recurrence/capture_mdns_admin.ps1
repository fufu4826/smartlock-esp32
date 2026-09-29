# Bounded administrator-run capture for the SmartLock canonical mDNS name.
# No self-elevation. Capture and evidence are limited to IPv4 UDP/5353.
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Output 'Run this script as Administrator.'
    exit 1
}

$hostName = 'smartlock-04225a0ff0a4.local'
$multicastAddress = '224.0.0.251'
$mdnsPort = 5353
$boardAddress = '192.168.1.179' # Existing LAN diagnostic evidence; no secrets are read.
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent $scriptRoot)
$evidenceDirectory = Join-Path $projectRoot 'evidence\canonical_recurrence\admin-mdns-capture'
$runDirectory = Join-Path $evidenceDirectory ('capture-' + [DateTimeOffset]::Now.ToString('yyyyMMdd-HHmmss-fff'))
$etlPath = Join-Path $runDirectory 'mdns-capture.etl'
$pcapPath = Join-Path $runDirectory 'mdns-capture.pcapng'
$summaryPath = Join-Path $runDirectory 'summary.json'
$filterNames = @('SmartLockMdnsPcGroup', 'SmartLockMdnsBoardGroup', 'SmartLockMdnsPcBoard')
$captureStarted = $false
$filtersAdded = [System.Collections.Generic.List[string]]::new()
$queriesSent = 0
$captureStopped = $false
$filtersRemoved = $false
$stage = 'preflight'
$failureCode = $null
$udp = $null
$wifi = $null
$summary = $null
$baselineTimestamp = $null

function Invoke-PktMon {
    param([Parameter(Mandatory)][string[]]$Arguments)
    $output = @(& "$env:SystemRoot\System32\pktmon.exe" @Arguments 2>&1 | ForEach-Object { [string]$_ })
    $exitCode = $LASTEXITCODE
    return [pscustomobject]@{ ExitCode = $exitCode; Output = ($output -join "`n") }
}

function Get-IdlePktMonState {
    $status = Invoke-PktMon -Arguments @('status')
    if ($status.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($status.Output)) {
        throw 'pktmon_status_unavailable'
    }
    if ($status.Output -match '(?i)access is denied|failed|error') {
        throw 'pktmon_status_unavailable'
    }
    if ($status.Output -match '(?i)not\s+running|stopped') {
        $idle = $true
    } elseif ($status.Output -match '(?i)running|started|active') {
        throw 'pktmon_capture_already_active'
    } else {
        throw 'pktmon_status_unrecognized'
    }

    $filters = Invoke-PktMon -Arguments @('filter', 'list')
    if ($filters.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($filters.Output)) {
        throw 'pktmon_filter_state_unavailable'
    }
    if ($filters.Output -match '(?i)access is denied|failed|error') {
        throw 'pktmon_filter_state_unavailable'
    }
    if ($filters.Output -match '(?i)no\s+(packet\s+)?filters?(\s+(are|were|is)\s+)?(configured|active|set)?') {
        return
    }
    throw 'pktmon_filters_present_or_unrecognized'
}

function Get-WifiBaseline {
    $adapters = @(Get-NetAdapter -ErrorAction Stop | Where-Object {
        $_.Status -eq 'Up' -and ($_.NdisPhysicalMedium -eq 9 -or $_.Name -match '(?i)wi-?fi|wireless')
    })
    if ($adapters.Count -ne 1) { throw 'active_wifi_adapter_ambiguous' }
    $adapter = $adapters[0]
    $configuration = Get-NetIPConfiguration -InterfaceIndex $adapter.ifIndex -ErrorAction Stop
    $ipv4 = @($configuration.IPv4Address | Where-Object { $_.IPAddress -and $_.IPAddress -notlike '169.254.*' })
    $gateway = @($configuration.IPv4DefaultGateway | Where-Object { $_.NextHop })
    if ($ipv4.Count -ne 1 -or $gateway.Count -lt 1) { throw 'wifi_ipv4_or_gateway_unavailable' }

    $band = $null
    $channel = $null
    # Parse only the band/channel fields. Do not retain or print SSID, BSSID, or raw netsh output.
    $wlanOutput = @(& "$env:SystemRoot\System32\netsh.exe" wlan show interfaces 2>$null | ForEach-Object { [string]$_ })
    foreach ($line in $wlanOutput) {
        if (-not $band -and $line -match '^\s*Band\s*:\s*(.+?)\s*$') { $band = $Matches[1] }
        if (-not $channel -and $line -match '^\s*Channel\s*:\s*(\d+)\s*$') { $channel = [int]$Matches[1] }
    }
    return [pscustomobject]@{
        adapter = [string]$adapter.Name
        ipv4 = [string]$ipv4[0].IPAddress
        defaultGateway = [string]$gateway[0].NextHop
        band = $band
        channel = $channel
    }
}

function New-MdnsAQuery {
    param([Parameter(Mandatory)][string]$Name)
    $stream = [System.IO.MemoryStream]::new()
    $writer = [System.IO.BinaryWriter]::new($stream)
    # Standard mDNS query header: ID=0, flags=0, one question, no records.
    $writer.Write([byte[]](0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0))
    foreach ($label in $Name.TrimEnd('.').Split('.')) {
        $labelBytes = [System.Text.Encoding]::ASCII.GetBytes($label)
        if ($labelBytes.Length -lt 1 -or $labelBytes.Length -gt 63) { throw 'invalid_query_name' }
        $writer.Write([byte]$labelBytes.Length)
        $writer.Write($labelBytes)
    }
    $writer.Write([byte]0)
    $writer.Write([byte[]](0, 1, 0, 1)) # QTYPE=A, QCLASS=IN; QM query.
    $writer.Flush()
    $bytes = $stream.ToArray()
    $writer.Dispose()
    $stream.Dispose()
    return ,$bytes
}

function Test-OnlyOurFiltersPresent {
    $result = Invoke-PktMon -Arguments @('filter', 'list')
    if ($result.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($result.Output)) { return $false }
    if ($result.Output -match '(?i)access is denied|failed|error') { return $false }
    foreach ($name in $filtersAdded) {
        if ($result.Output -notmatch [regex]::Escape($name)) { return $false }
    }
    # Require each active filter row to identify one of our names before using the
    # global remove command, since this PktMon version cannot remove one filter.
    $rows = @($result.Output -split "`r?`n" | Where-Object { $_ -match '^\s*\d+\s*[:.)]?\s+\S' })
    if ($rows.Count -ne $filtersAdded.Count) { return $false }
    foreach ($row in $rows) {
        $matched = $false
        foreach ($name in $filtersAdded) {
            if ($row -match [regex]::Escape($name)) { $matched = $true; break }
        }
        if (-not $matched) { return $false }
    }
    return $true
}

try {
    Get-IdlePktMonState
    $baselineTimestamp = [DateTimeOffset]::Now.ToString('o')
    $wifi = Get-WifiBaseline
    $boardOctets = [System.Net.IPAddress]::Parse($boardAddress).GetAddressBytes()
    $pcOctets = [System.Net.IPAddress]::Parse($wifi.ipv4).GetAddressBytes()
    $groupOctets = [System.Net.IPAddress]::Parse($multicastAddress).GetAddressBytes()
    if ($pcOctets.Length -ne 4 -or $boardOctets.Length -ne 4 -or $groupOctets.Length -ne 4) {
        throw 'ipv4_address_invalid'
    }

    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    $stage = 'filter_setup'
    $filterSpecs = @(
        @($filterNames[0], $wifi.ipv4, $multicastAddress),
        @($filterNames[1], $boardAddress, $multicastAddress),
        @($filterNames[2], $wifi.ipv4, $boardAddress)
    )
    foreach ($spec in $filterSpecs) {
        $add = Invoke-PktMon -Arguments @('filter', 'add', $spec[0], '-d', 'IPv4', '-t', 'UDP', '-i', $spec[1], $spec[2], '-p', '5353')
        if ($add.ExitCode -ne 0) { throw 'pktmon_filter_add_failed' }
        $filtersAdded.Add($spec[0])
    }

    $stage = 'capture_start'
    $start = Invoke-PktMon -Arguments @('start', '--capture', '--comp', 'nics', '--pkt-size', '128', '--file-name', $etlPath, '--file-size', '8', '--log-mode', 'circular')
    if ($start.ExitCode -ne 0) { throw 'pktmon_capture_start_failed' }
    $captureStarted = $true

    $stage = 'queries'
    $udp = [System.Net.Sockets.UdpClient]::new([System.Net.Sockets.AddressFamily]::InterNetwork)
    $udp.Client.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
    $udp.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Parse($wifi.ipv4), $mdnsPort))
    $udp.JoinMulticastGroup([System.Net.IPAddress]::Parse($multicastAddress), [System.Net.IPAddress]::Parse($wifi.ipv4))
    $udp.Ttl = 255
    $queryBytes = New-MdnsAQuery -Name $hostName
    for ($index = 0; $index -lt 3; $index++) {
        [void]$udp.Send($queryBytes, $queryBytes.Length, [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Parse($multicastAddress), $mdnsPort))
        $queriesSent++
        if ($index -lt 2) { Start-Sleep -Milliseconds 250 }
    }

    # Allow a short, fixed reply window after the third query.
    $stage = 'reply_window'
    Start-Sleep -Seconds 2
} catch {
    # Keep exception text out of console and evidence; it can contain machine-specific details.
    if (-not $failureCode) { $failureCode = 'operation_failed' }
} finally {
    if ($udp) { $udp.Dispose(); $udp = $null }
    if ($captureStarted) {
        try {
            $stop = Invoke-PktMon -Arguments @('stop')
            $captureStopped = ($stop.ExitCode -eq 0)
        } catch { $captureStopped = $false }
        if (-not $captureStopped -and -not $failureCode) { $failureCode = 'pktmon_capture_stop_failed' }
    }
    if ($filtersAdded.Count -gt 0) {
        try {
            if (Test-OnlyOurFiltersPresent) {
                $remove = Invoke-PktMon -Arguments @('filter', 'remove')
                $filtersRemoved = ($remove.ExitCode -eq 0)
            }
        } catch { $filtersRemoved = $false }
        if (-not $filtersRemoved -and -not $failureCode) { $failureCode = 'pktmon_filter_cleanup_skipped_or_failed' }
    }
}

if ($captureStarted -and $captureStopped -and (Test-Path -LiteralPath $etlPath)) {
    $stage = 'export'
    try {
        $export = Invoke-PktMon -Arguments @('etl2pcap', $etlPath, '--out', $pcapPath)
        if ($export.ExitCode -ne 0 -and -not $failureCode) { $failureCode = 'pktmon_export_failed' }
        elseif (-not (Test-Path -LiteralPath $pcapPath) -and -not $failureCode) { $failureCode = 'pktmon_export_file_missing' }
    } catch { if (-not $failureCode) { $failureCode = 'pktmon_export_failed' } }
} elseif ($captureStarted -and -not $failureCode) {
    $failureCode = 'capture_evidence_unavailable'
}

$generatedFiles = @()
if (Test-Path -LiteralPath $etlPath) { $generatedFiles += 'mdns-capture.etl' }
if (Test-Path -LiteralPath $pcapPath) { $generatedFiles += 'mdns-capture.pcapng' }
$timestamp = if ($baselineTimestamp) { $baselineTimestamp } else { [DateTimeOffset]::Now.ToString('o') }
$baseline = $null
if ($wifi) {
    $baseline = [ordered]@{
        activeWifiAdapter = $wifi.adapter
        pcIPv4 = $wifi.ipv4
        defaultGateway = $wifi.defaultGateway
        wifiBand = $wifi.band
        wifiChannel = $wifi.channel
        canonicalHostname = $hostName
        esp32DiagnosticStaIPv4 = $boardAddress
    }
}
$summary = [ordered]@{
    timestamp = $timestamp
    baseline = $baseline
    captureStarted = [bool]$captureStarted
    threeQueriesSent = [bool]($queriesSent -eq 3)
    queriesSent = $queriesSent
    generatedEvidenceFiles = $generatedFiles
    captureStopped = [bool]$captureStopped
    temporaryFiltersRemoved = [bool]$filtersRemoved
    failureStage = if ($failureCode) { $stage } else { $null }
    failureCode = $failureCode
}

if (-not (Test-Path -LiteralPath $runDirectory)) {
    # A non-admin exit and failed preflight leave no filesystem changes.
    if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    }
}
if (Test-Path -LiteralPath $runDirectory) {
    $summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
}

if ($failureCode) {
    Write-Output "Capture did not complete cleanly. Evidence summary: $summaryPath"
    exit 2
}
Write-Output "Capture complete. Evidence summary: $summaryPath"
exit 0
