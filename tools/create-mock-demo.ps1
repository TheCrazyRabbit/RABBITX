param([string]$Executable = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Executable) { $Executable = Join-Path $root 'x64\Release\RABBITX.exe' }
if (-not (Test-Path -LiteralPath $Executable)) { throw 'Build the Release client first, or supply -Executable.' }
$probe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$probe.Start(); $port = $probe.LocalEndpoint.Port; $probe.Stop()
$scriptPath = Join-Path $root 'examples\Start-DemoServer.ps1'
$server = Start-Process powershell.exe -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$scriptPath+'"'),'-Port',$port)
try {
    $ready = $false
    for ($i = 0; $i -lt 100 -and -not $ready; $i++) {
        $connection = [Net.Sockets.TcpClient]::new()
        try { $connection.Connect([Net.IPAddress]::Loopback,$port); $ready = $true }
        catch { Start-Sleep -Milliseconds 50 }
        finally { $connection.Dispose() }
    }
    if (-not $ready) { throw 'Local demonstration server did not start.' }
    $target = "http://127.0.0.1:$port/case-1"
    $config = Join-Path $root 'examples\rabbitx.ai-mock.json'
    $output = Join-Path $root 'examples\reports\demo-ai-triage.html'
    Push-Location $root
    try {
        & $Executable --config $config --url $target --scope-host "127.0.0.1:$port" --html --output examples/reports/demo-ai-triage.html
        if ($LASTEXITCODE -ne 0) { throw 'Mock report generation failed.' }
    } finally { Pop-Location }
    $html = Get-Content -LiteralPath $output -Raw
    if (-not $html.Contains('"isMock":true') -or -not $html.Contains('"status":"completed"')) { throw 'Report has no completed Mock triage.' }
    Write-Host "Generated synthetic offline Mock example: $output"
} finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force }
}
