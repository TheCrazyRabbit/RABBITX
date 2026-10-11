$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { throw 'Node.js 24+ must be on PATH (tests use node:sqlite).' }
Push-Location (Join-Path $root 'backend')
try { & $node.Source --test tests/*.test.js; if ($LASTEXITCODE) { throw 'Backend tests failed.' } }
finally { Pop-Location }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install = & $vswhere -latest -products '*' -property installationPath
$msbuild = Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $root 'tests\entitlement_client_tests.vcxproj') /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE) { throw 'Native tests build failed.' }
& (Join-Path $root 'x64\AuthTests\entitlement_client_tests.exe')
if ($LASTEXITCODE) { throw 'Native tests failed.' }
& $msbuild (Join-Path $root 'RABBITX.sln') /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE) { throw 'RABBITX Release build failed.' }
