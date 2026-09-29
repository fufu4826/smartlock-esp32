param(
  [string]$adapter = 'Wi-Fi 2',
  [string]$testSsid = 'SmartLock-F0A4'
)
$ErrorActionPreference = 'Stop'
$previousProfile = (Get-NetConnectionProfile -InterfaceAlias $adapter -ErrorAction Stop).Name
if (-not $previousProfile -or $previousProfile -eq $testSsid) { throw 'Connect to the normal Wi-Fi network before running this test.' }
$profileFile = Join-Path $env:TEMP "codex-smartlock-phase5-$PID.xml"
$profileAdded = $false
$xml = @'
<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1"><name>SmartLock-F0A4</name><SSIDConfig><SSID><name>SmartLock-F0A4</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM></WLANProfile>
'@
$xml = $xml.Replace('SmartLock-F0A4', $testSsid)
try {
  Set-Content -LiteralPath $profileFile -Value $xml -Encoding utf8
  netsh wlan add profile filename="$profileFile" interface="$adapter" user=current | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'Could not add temporary AP profile' }
  $profileAdded = $true
  netsh wlan connect name="$testSsid" ssid="$testSsid" interface="$adapter" | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'Could not connect to test AP' }
  Start-Sleep -Seconds 8
  $ip = Get-NetIPAddress -InterfaceAlias $adapter -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -like '192.168.4.*' } | Select-Object -First 1 -ExpandProperty IPAddress
  if (-not $ip) { throw 'No AP DHCP address' }
  Write-Output "DHCP: $ip"

  $page = & curl.exe --noproxy '*' --max-time 5 --silent 'http://192.168.4.1/setup'
  if (($page -join "`n") -notmatch 'one-time setup session' -or
      ($page -join "`n") -notmatch 'Complete setup') { throw 'Wizard page missing expected content' }
  Write-Output 'Setup wizard HTML: PASS'

  $missing = & curl.exe --noproxy '*' --max-time 5 --silent --output NUL --write-out '%{http_code}' -X POST 'http://192.168.4.1/api/setup/complete'
  if ($LASTEXITCODE -ne 0 -or $missing -ne '400') { throw "Missing-fields response: $missing" }
  Write-Output 'Missing fields: 400 PASS'

  $form = @(
    ('session=' + ('0' * 64)), 'deviceName=TestLock', 'ownerName=TestOwner',
    'browserName=TestBrowser', 'adminPassphrase=test-passphrase-123',
    'unlockSeconds=5', ('credential=' + ('1' * 64)), ('apPassword=' + ('2' * 32))
  ) -join '&'
  $invalid = & curl.exe --noproxy '*' --max-time 5 --silent --output NUL --write-out '%{http_code}' `
    -H 'Content-Type: application/x-www-form-urlencoded' --data-raw $form 'http://192.168.4.1/api/setup/complete'
  if ($LASTEXITCODE -ne 0 -or $invalid -ne '403') { throw "Invalid-session response: $invalid" }
  Write-Output 'Invalid Setup session: 403 PASS'

  $status = & curl.exe --noproxy '*' --max-time 5 --silent 'http://192.168.4.1/api/status'
  if ($status -notmatch '"configured":false') { throw "Unexpected configured state: $status" }
  Write-Output 'Still unconfigured: PASS'
} finally {
  netsh wlan connect name="$previousProfile" interface="$adapter" | Out-Null
  Start-Sleep -Seconds 3
  if ($profileAdded) { netsh wlan delete profile name="$testSsid" interface="$adapter" | Out-Null }
  if (Test-Path -LiteralPath $profileFile) { Remove-Item -LiteralPath $profileFile }
  Write-Output 'Original Wi-Fi profile restored; temporary AP profile removed.'
}
