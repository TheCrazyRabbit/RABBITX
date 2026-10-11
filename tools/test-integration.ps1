param([string]$MSBuildPath = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
foreach ($configuration in @('Debug','Release')) {
    & (Join-Path $PSScriptRoot 'test-client.ps1') -Configuration $configuration -MSBuildPath $MSBuildPath
    if (-not $?) { throw "$configuration product suite failed." }
    & (Join-Path $PSScriptRoot 'tests\Test-IntegratedCli.ps1') -Executable (Join-Path $root "x64\$configuration\RABBITX.exe")
    if (-not $?) { throw "$configuration CLI integration suite failed." }
}
# Original authorization suite: mocked Paddle/email/D1 and native injected transport.
# No deployment commands and no real account commands are invoked here.
& (Join-Path $PSScriptRoot 'test-entitlements.ps1')
if (-not $?) { throw 'Authorization suite failed.' }
