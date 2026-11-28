param(
    [ValidateRange(0, 600)][int]$Seconds = 90,
    [string]$DirectProcessName = 'msedge'
)

$ErrorActionPreference = 'Stop'
$directory = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'AWGConnect\Diagnostics'
[void][IO.Directory]::CreateDirectory($directory)
$report = Join-Path $directory ("Codex-{0:yyyyMMdd-HHmmss}-{1}.txt" -f (Get-Date), $PID)
function Write-Report([string]$Message) {
    Add-Content -LiteralPath $report -Value $Message -Encoding UTF8
    Write-Host $Message
}

try {
    Write-Report "Отчёт: $report"
    Write-Report "Запись в течение $Seconds с. Отправьте запрос в Codex во время проверки."
    Write-Report 'Ожидаемый интерфейс для внешних соединений Codex при включённом раздельном туннелировании — AmneziaVPN.'
    Write-Report 'Проверка наблюдает TCP-соединения Windows. Сетевые адреса и тексты запросов в отчёт не записываются.'
    $packages = @(Get-AppxPackage -Name 'OpenAI.Codex*' | Where-Object {
        $_.Name -eq 'OpenAI.Codex' -or $_.Name -like 'OpenAI.CodexPrimaryRuntime.v*'
    })
    foreach ($pkg in $packages) { Write-Report "Установлен пакет $($pkg.PackageFullName)" }

    $userKey = 'HKCU:\Software\AmneziaVPN.ORG\AmneziaVPN\Conf'
    $serviceKey = 'HKLM:\Software\AmneziaVPN.ORG\AmneziaVPN'
    $until = (Get-Date).AddSeconds($Seconds)
    $previous = ''
    do {
        $userSettings = Get-Item -LiteralPath $userKey -ErrorAction SilentlyContinue
        $serviceSettings = Get-Item -LiteralPath $serviceKey -ErrorAction SilentlyContinue
        $mode = 'неизвестно'
        if ($userSettings) {
            $mode = if ($userSettings.GetValue('appsSplitTunnelingEnabled') -ne 'true') { 'выключено' }
                    elseif ($userSettings.GetValue('appsRouteMode') -eq 1) { 'Через VPN' } else { 'Напрямую' }
        }
        $policy = if ($serviceSettings) { $serviceSettings.GetValue('positiveSplitTunnelEnabled', '?') } else { '?' }
        $interfaces = @{}
        foreach ($address in Get-NetIPAddress) { $interfaces[$address.IPAddress] = $address.InterfaceAlias }
        $images = @{}
        foreach ($process in Get-Process) {
            $path = $process.Path
            if (!$path) { continue }
            foreach ($pkg in $packages) {
                if ($pkg.InstallLocation -and $path.StartsWith($pkg.InstallLocation + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    $images[$process.Id] = $path.Substring($pkg.InstallLocation.Length + 1)
                    break
                }
            }
            if (!$images.ContainsKey($process.Id)) {
                if ($process.ProcessName -match '^(codex|chatgpt)$') { $images[$process.Id] = "$($process.ProcessName).exe вне пакета" }
                elseif ($process.ProcessName -eq $DirectProcessName) { $images[$process.Id] = "$DirectProcessName (для сравнения)" }
            }
        }
        $core = 'app\resources\codex.exe'
        $connections = @(Get-NetTCPConnection -ErrorAction Stop)
        $observations = foreach ($tcp in $connections) {
            $owner = [int]$tcp.OwningProcess
            if (!$images.ContainsKey($owner) -or $tcp.State -notin 'SynSent','Established','CloseWait') { continue }
            $peer = [ipaddress]$tcp.RemoteAddress
            if ($peer.IsIPv4MappedToIPv6) { $peer = $peer.MapToIPv4() }
            $local = [ipaddress]$tcp.LocalAddress
            if ($local.IsIPv4MappedToIPv6) { $local = $local.MapToIPv4() }
            $via = $interfaces[$local.ToString()]
            if ([ipaddress]::IsLoopback($peer)) { $via = 'внутри компьютера' }
            elseif (!$via) { $via = 'интерфейс не определён' }
            "$($images[$owner]) | $via | $($tcp.State) | $($peer.AddressFamily)"
        }
        $summary = @("Раздельное туннелирование: $mode. Политика службы «Через VPN»: $policy.")
        if ($images.Values -notcontains $core) { $summary += 'Процесс app\resources\codex.exe не найден.' }
        elseif (!($observations -like "$core |*")) { $summary += 'У app\resources\codex.exe в этом снимке нет наблюдаемых TCP-соединений.' }
        $summary += @($observations | Sort-Object -Unique)
        $state = $summary -join [Environment]::NewLine
        if ($state -ne $previous) {
            Write-Report ("{0:HH:mm:ss}  {1}" -f (Get-Date), $state)
            $previous = $state
        }
        if ((Get-Date) -ge $until) { break }
        Start-Sleep -Seconds 2
    } while ($true)
    Write-Report 'Запись завершена. Established означает установленное соединение, SynSent — попытку подключения.'
    Write-Report 'Если Codex потерял связь, верните прежний режим VPN. Отчёт уже сохранён.'
} catch {
    Write-Report ("Сбор завершился с ошибкой на строке {0}: {1}" -f $_.InvocationInfo.ScriptLineNumber, $_.Exception.GetType().Name)
    exit 1
}
