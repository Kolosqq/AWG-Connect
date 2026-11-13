param([Parameter(Mandatory)][string]$BuildDirectory)

$ErrorActionPreference = 'Stop'
$packages = @(Get-ChildItem -LiteralPath $BuildDirectory -File -Filter 'AWG-Connect_*_windows_x64.msi')
if ($packages.Count -ne 1) { throw 'Expected one AWG-Connect MSI' }
$package = $packages[0]
if ($package.Name -notmatch '^AWG-Connect_([0-9]+\.[0-9]+\.[0-9]+)_windows_x64\.msi$') {
    throw 'Unexpected MSI name'
}
$version = $Matches[1]
$installer = New-Object -ComObject WindowsInstaller.Installer
$db = $installer.GetType().InvokeMember('OpenDatabase', 'InvokeMethod', $null, $installer, @($package.FullName, 0))
function Get-MsiValue([string]$Query) {
    $view = $db.GetType().InvokeMember('OpenView', 'InvokeMethod', $null, $db, @($Query))
    $null = $view.GetType().InvokeMember('Execute', 'InvokeMethod', $null, $view, $null)
    $row = $view.GetType().InvokeMember('Fetch', 'InvokeMethod', $null, $view, $null)
    if ($null -eq $row) { return $null }
    $row.GetType().InvokeMember('StringData', 'GetProperty', $null, $row, @(1))
}
if ((Get-MsiValue "SELECT ``Value`` FROM ``Property`` WHERE ``Property`` = 'ProductVersion'") -ne $version) {
    throw 'MSI version does not match its filename'
}
if ((Get-MsiValue "SELECT ``Value`` FROM ``Property`` WHERE ``Property`` = 'UpgradeCode'") -ne '{A7E8403F-F7E7-46BB-A631-6D5518DB7C05}') {
    throw 'Unexpected MSI upgrade family'
}
$initialize = [int](Get-MsiValue "SELECT ``Sequence`` FROM ``InstallExecuteSequence`` WHERE ``Action`` = 'InstallInitialize'")
$remove = [int](Get-MsiValue "SELECT ``Sequence`` FROM ``InstallExecuteSequence`` WHERE ``Action`` = 'RemoveExistingProducts'")
$finalize = [int](Get-MsiValue "SELECT ``Sequence`` FROM ``InstallExecuteSequence`` WHERE ``Action`` = 'InstallFinalize'")
if ($initialize -le 0 -or $remove -le $initialize -or $remove -ge $finalize) {
    throw 'Previous version removal must be inside the rollback transaction'
}
if ((Get-MsiValue "SELECT ``Name`` FROM ``ServiceInstall`` WHERE ``Name`` = 'AmneziaVPN-service'") -ne 'AmneziaVPN-service') {
    throw 'MSI service installation is missing'
}
if ((Get-MsiValue "SELECT ``Name`` FROM ``ServiceControl`` WHERE ``Name`` = 'AmneziaVPN-service'") -ne 'AmneziaVPN-service') {
    throw 'MSI service lifecycle control is missing'
}
if ((Get-MsiValue 'SELECT `Cabinet` FROM `Media`') -notlike '#*') { throw 'Expected an embedded cabinet' }
Write-Output "Verified MSI version $version, service registration, embedded payload and rollback sequence"
