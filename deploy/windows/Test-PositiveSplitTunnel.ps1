[CmdletBinding()]
param(
    [ValidateSet('Prepare', 'Probe')]
    [string]$Action = 'Prepare',
    [string]$ExpectedDirectIp
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$probeDirectory = Join-Path $repository '.cache/split-tunnel-probes'
$selectedProbe = Join-Path $probeDirectory 'vpn-probe.exe'
$directProbe = Join-Path $probeDirectory 'direct-probe.exe'

if ($Action -eq 'Prepare') {
    $systemCurl = Join-Path $env:SystemRoot 'System32/curl.exe'
    if (-not (Test-Path -LiteralPath $systemCurl)) {
        throw 'Windows curl.exe was not found.'
    }
    New-Item -ItemType Directory -Path $probeDirectory -Force | Out-Null
    Copy-Item -LiteralPath $systemCurl -Destination $selectedProbe -Force
    Copy-Item -LiteralPath $systemCurl -Destination $directProbe -Force
    Write-Output "Add only this executable to the Via VPN list: $selectedProbe"
    Write-Output "Leave this executable out of the list: $directProbe"
    Write-Output 'No VPN, routes, services or firewall settings were changed.'
    exit 0
}

foreach ($probe in @($selectedProbe, $directProbe)) {
    if (-not (Test-Path -LiteralPath $probe)) {
        throw 'Run this script with -Action Prepare first.'
    }
}

$selectedIp = (& $selectedProbe -4 --silent --show-error --fail --max-time 20 --noproxy '*' https://api.ipify.org)
if ($LASTEXITCODE -ne 0) {
    throw 'The selected application could not complete its HTTPS request.'
}
$directIp = (& $directProbe -4 --silent --show-error --fail --max-time 20 --noproxy '*' https://api.ipify.org)
if ($LASTEXITCODE -ne 0) {
    throw 'The application outside the list could not complete its HTTPS request.'
}

$selectedAddress = [System.Net.IPAddress]::Parse(($selectedIp -join '').Trim())
$directAddress = [System.Net.IPAddress]::Parse(($directIp -join '').Trim())
$expectedMatches = $null
if ($ExpectedDirectIp) {
    $expectedMatches = $directAddress.Equals([System.Net.IPAddress]::Parse($ExpectedDirectIp))
}

[pscustomobject]@{
    DifferentEgress = -not $selectedAddress.Equals($directAddress)
    DirectMatchesReference = $expectedMatches
}

if ($selectedAddress.Equals($directAddress)) {
    throw 'Both applications used the same exit. Positive split tunneling is not demonstrated.'
}
if ($expectedMatches -eq $false) {
    throw 'The unselected application did not use the expected direct exit.'
}
if (-not $ExpectedDirectIp) {
    Write-Warning 'Different exits alone do not prove direct routing. Compare with a known direct reference.'
}
