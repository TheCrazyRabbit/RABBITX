param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [string]$MSBuildPath = '',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $MSBuildPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products '*' -property installationPath
        if ($install) { $MSBuildPath = Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe' }
    }
    if (-not $MSBuildPath -and (Get-Command msbuild.exe -ErrorAction SilentlyContinue)) { $MSBuildPath = (Get-Command msbuild.exe).Source }
}
if (-not $SkipBuild) {
    if (-not $MSBuildPath) { throw 'MSBuild not found; provide -MSBuildPath or install VS C++ Build Tools.' }
    & $MSBuildPath (Join-Path $root 'RABBITX.vcxproj') "/p:Configuration=$Configuration" /p:Platform=x64 /p:RabbitxTests=false /m /v:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Client build failed.' }
    & $MSBuildPath (Join-Path $root 'RABBITX.vcxproj') "/p:Configuration=$Configuration" /p:Platform=x64 /p:RabbitxTests=true /m /v:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Native test build failed.' }
}
$testExe = Join-Path $root "x64\$Configuration\RABBITX-tests.exe"
$probe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$probe.Start(); $port = $probe.LocalEndpoint.Port; $probe.Stop()
$runId = [Guid]::NewGuid().ToString('N')
$ready = Join-Path $root "x64\client-test-$runId.ready"
$serverScript = Join-Path $PSScriptRoot 'tests\Start-TestServer.ps1'
$server = Start-Process powershell.exe -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File', ('"' + $serverScript + '"'),'-Port', $port, '-ReadyFile', ('"' + $ready + '"'))
try {
    for ($i = 0; $i -lt 100 -and -not (Test-Path -LiteralPath $ready); $i++) { Start-Sleep -Milliseconds 50 }
    if (-not (Test-Path -LiteralPath $ready)) { throw 'Local fixture server failed to start.' }
    Push-Location $root
    try {
        & $testExe "http://127.0.0.1:$port"
        if ($LASTEXITCODE -ne 0) { throw 'Native client tests failed.' }
        & (Join-Path $PSScriptRoot 'tests\Test-Cli.ps1') -Executable (Join-Path $root "x64\$Configuration\RABBITX.exe") -BaseUrl "http://127.0.0.1:$port"
    }
    finally { Pop-Location }
} finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force }
    if (Test-Path -LiteralPath $ready) { Remove-Item -LiteralPath $ready -Force }
}
