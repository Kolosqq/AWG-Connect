#Requires -Version 7.0
[CmdletBinding(DefaultParameterSetName = 'Sign')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Initialize')][switch]$Initialize,
    [Parameter(Mandatory, ParameterSetName = 'Sign')][string]$Package
)
$ErrorActionPreference = 'Stop'
$keyName = 'AWG-Connect release signing v1'
$provider = [System.Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
$publicFile = Join-Path $PSScriptRoot 'update-public-key.hex'

if ([System.Security.Cryptography.CngKey]::Exists($keyName, $provider)) {
    $key = [System.Security.Cryptography.CngKey]::Open($keyName, $provider)
} elseif ($Initialize -and !(Test-Path -LiteralPath $publicFile)) {
    $creation = [System.Security.Cryptography.CngKeyCreationParameters]::new()
    $creation.Provider = $provider
    $creation.ExportPolicy = [System.Security.Cryptography.CngExportPolicies]::None
    $creation.KeyUsage = [System.Security.Cryptography.CngKeyUsages]::Signing
    $creation.Parameters.Add([System.Security.Cryptography.CngProperty]::new(
        'Length', [BitConverter]::GetBytes([int]3072), [System.Security.Cryptography.CngPropertyOptions]::None))
    $key = [System.Security.Cryptography.CngKey]::Create([System.Security.Cryptography.CngAlgorithm]::Rsa, $keyName, $creation)
} else {
    throw 'Release key missing. Do not replace a pinned key. Use the original Windows profile.'
}

try {
    $public = $key.Export([System.Security.Cryptography.CngKeyBlobFormat]::GenericPublicBlob)
    $hex = [Convert]::ToHexString($public).ToLowerInvariant()
    if ($Initialize -and !(Test-Path -LiteralPath $publicFile)) {
        $stream = [IO.File]::Open($publicFile, [IO.FileMode]::CreateNew)
        try {
            $bytes = [Text.Encoding]::ASCII.GetBytes($hex + [Environment]::NewLine)
            $stream.Write($bytes)
        } finally { $stream.Dispose() }
    }
    if ((Get-Content -LiteralPath $publicFile -Raw).Trim() -cne $hex) {
        throw 'Local signing key does not match the repository public key.'
    }
    if ($Initialize) {
        Write-Output ('Public key SHA-256: ' + [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($public)))
        return
    }
    $msi = Get-Item -LiteralPath $Package
    if ($msi.PSIsContainer -or $msi.Name -cnotmatch '^AWG-Connect_[0-9]+\.[0-9]+\.[0-9]+_windows_x64\.msi$') {
        throw 'Expected an AWG-Connect MSI from the packaging workflow.'
    }
    $rsa = [System.Security.Cryptography.RSACng]::new($key)
    try {
        $inputStream = [IO.File]::Open($msi.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        try {
            $signature = $rsa.SignData($inputStream, [System.Security.Cryptography.HashAlgorithmName]::SHA256,
                [System.Security.Cryptography.RSASignaturePadding]::Pss)
            $inputStream.Position = 0
            if (!$rsa.VerifyData($inputStream, $signature, [System.Security.Cryptography.HashAlgorithmName]::SHA256,
                    [System.Security.Cryptography.RSASignaturePadding]::Pss)) {
                throw 'Signature verification failed.'
            }
            $output = [IO.File]::Open($msi.FullName + '.sig', [IO.FileMode]::CreateNew)
            try { $output.Write($signature) } finally { $output.Dispose() }
        } finally { $inputStream.Dispose() }
        Write-Output ('Signed: ' + $msi.Name)
    } finally { $rsa.Dispose() }
} finally { $key.Dispose() }
