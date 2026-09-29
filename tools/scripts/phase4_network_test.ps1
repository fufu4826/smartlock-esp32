param(
  [string]$adapter = 'Wi-Fi 2',
  [string]$testSsid = 'SmartLock-F0A4'
)
$ErrorActionPreference = 'Stop'
$previousProfile = (Get-NetConnectionProfile -InterfaceAlias $adapter -ErrorAction Stop).Name
if (-not $previousProfile -or $previousProfile -eq $testSsid) { throw 'Connect to the normal Wi-Fi network before running this test.' }
$profileFile = Join-Path $env:TEMP "codex-smartlock-phase4-$PID.xml"
$profileAdded = $false

$xml = @'
<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
  <name>SmartLock-F0A4</name>
  <SSIDConfig><SSID><name>SmartLock-F0A4</name></SSID></SSIDConfig>
  <connectionType>ESS</connectionType><connectionMode>manual</connectionMode>
  <MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM>
</WLANProfile>
'@
$xml = $xml.Replace('SmartLock-F0A4', $testSsid)

try {
  Set-Content -LiteralPath $profileFile -Value $xml -Encoding utf8
  $added = netsh wlan add profile filename="$profileFile" interface="$adapter" user=current
  if ($LASTEXITCODE -ne 0) { throw "Failed to add temporary AP profile: $added" }
  $profileAdded = $true
  $connected = netsh wlan connect name="$testSsid" ssid="$testSsid" interface="$adapter"
  if ($LASTEXITCODE -ne 0) { throw "Failed to connect to AP: $connected" }
  Start-Sleep -Seconds 8
  $ip = Get-NetIPAddress -InterfaceAlias $adapter -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -like '192.168.4.*' } | Select-Object -First 1 -ExpandProperty IPAddress
  if (-not $ip) { throw 'No AP DHCP address received' }
  Write-Output "DHCP: $ip"

  foreach ($path in @('/', '/setup', '/api/status', '/health', '/generate_204', '/hotspot-detect.html')) {
    $url = "http://192.168.4.1$path"
    $status = & curl.exe --noproxy '*' --max-time 5 --silent --output NUL --write-out '%{http_code}' $url
    if ($LASTEXITCODE -ne 0) { throw "HTTP failed: $path" }
    Write-Output "HTTP $path`: $status"
  }
  $body = & curl.exe --noproxy '*' --max-time 5 --silent 'http://192.168.4.1/api/status'
  if ($body -notmatch '"configured":false' -or $body -match 'session|token') { throw "Unexpected status JSON: $body" }
  $setup = & curl.exe --noproxy '*' --max-time 5 --silent 'http://192.168.4.1/setup'
  if (($setup -join "`n") -notmatch 'Setup information') { throw 'Setup page missing expected content' }
  $postStatus = & curl.exe --noproxy '*' --max-time 5 --silent --output NUL --write-out '%{http_code}' -X POST 'http://192.168.4.1/setup'
  if ($LASTEXITCODE -ne 0 -or $postStatus -ne '404') { throw "Unexpected setup POST response: $postStatus" }
  Write-Output 'POST /setup: 404 (read-only)'
  $queryPage = & curl.exe --noproxy '*' --max-time 5 --silent 'http://192.168.4.1/setup?session=phase4-test'
  if (($queryPage -join "`n") -match 'phase4-test') { throw 'Setup page echoed a session query' }
  Write-Output 'Setup query token not echoed: PASS'
  $dns = Resolve-DnsName -Server 192.168.4.1 -Name 'phase4-test.invalid' -Type A -ErrorAction Stop
  if ($dns.IPAddress -notcontains '192.168.4.1') { throw 'Captive DNS did not return AP IP' }
  Write-Output 'DNS wildcard: PASS'

  for ($i = 1; $i -le 50; $i++) {
    $status = & curl.exe --noproxy '*' --max-time 5 --silent --output NUL --write-out '%{http_code}' 'http://192.168.4.1/health'
    if ($LASTEXITCODE -ne 0 -or $status -ne '200') { throw "Health stress failed at request $i ($status)" }
  }
  Write-Output 'Sequential /health requests: 50/50 PASS'
} finally {
  netsh wlan connect name="$previousProfile" interface="$adapter" | Out-Null
  Start-Sleep -Seconds 3
  if ($profileAdded) { netsh wlan delete profile name="$testSsid" interface="$adapter" | Out-Null }
  if (Test-Path -LiteralPath $profileFile) { Remove-Item -LiteralPath $profileFile }
  Write-Output 'Original Wi-Fi profile restored; temporary AP profile removed.'
}
