param(
    [ValidatePattern('^[0-9A-Za-z.-]+$')]
    [string]$Version = 'v0.8.0-beta'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $repositoryRoot 'RABBITX.sln'
$releaseBinary = Join-Path $repositoryRoot 'x64\Release\RABBITX.exe'
$distDirectory = Join-Path $repositoryRoot 'dist'
$packageName = "RABBITX-$Version-windows-x64"
$archivePath = Join-Path $distDirectory "$packageName.zip"

$msbuildPath = $null
$vswhereCandidates = @()
if (${env:ProgramFiles(x86)}) {
    $vswhereCandidates += Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if ($env:ProgramFiles) {
    $vswhereCandidates += Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
}

foreach ($vswherePath in $vswhereCandidates) {
    if (Test-Path -LiteralPath $vswherePath) {
        $installPath = & $vswherePath -latest -products '*' -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) {
                $msbuildPath = $candidate
                break
            }
        }
    }
}

if (-not $msbuildPath -and (Get-Command msbuild.exe -ErrorAction SilentlyContinue)) {
    $msbuildPath = (Get-Command msbuild.exe).Source
}

if (-not $msbuildPath) {
    throw 'MSBuild was not found. Install Visual Studio Build Tools with the C++ desktop workload.'
}

& $msbuildPath $solutionPath /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE."
}
if (-not (Test-Path -LiteralPath $releaseBinary)) {
    throw "Release executable was not produced: $releaseBinary"
}

New-Item -ItemType Directory -Path $distDirectory -Force | Out-Null
$stagePath = Join-Path ([System.IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stagePath | Out-Null

try {
    Copy-Item -LiteralPath $releaseBinary -Destination (Join-Path $stagePath 'RABBITX.exe')
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'README.md') -Destination $stagePath

    $packageDocs = Join-Path $stagePath 'docs'
    New-Item -ItemType Directory -Path $packageDocs | Out-Null
    foreach ($file in @('CONFIGURATION.md', 'PRICING.md', 'pricing.html')) {
        Copy-Item -LiteralPath (Join-Path $repositoryRoot "docs\$file") -Destination $packageDocs
    }

    $packageExamples = Join-Path $stagePath 'examples'
    $packageReports = Join-Path $packageExamples 'reports'
    New-Item -ItemType Directory -Path $packageReports -Force | Out-Null
    foreach ($file in @('README.md', 'rabbitx.example.json', 'Start-DemoServer.ps1')) {
        Copy-Item -LiteralPath (Join-Path $repositoryRoot "examples\$file") -Destination $packageExamples
    }
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'examples\reports\demo-security-posture.html') -Destination $packageReports

    [System.IO.File]::WriteAllText(
        (Join-Path $stagePath 'VERSION.txt'),
        "RABBITX $Version`r`nWindows x64`r`n",
        [System.Text.UTF8Encoding]::new($false))

    $exeHash = (Get-FileHash -LiteralPath (Join-Path $stagePath 'RABBITX.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    [System.IO.File]::WriteAllText(
        (Join-Path $stagePath 'SHA256SUMS.txt'),
        "$exeHash  RABBITX.exe`r`n",
        [System.Text.UTF8Encoding]::new($false))

    if (Test-Path -LiteralPath $archivePath) {
        $resolvedDist = (Resolve-Path -LiteralPath $distDirectory).Path.TrimEnd('\') + '\'
        $resolvedArchive = (Resolve-Path -LiteralPath $archivePath).Path
        if (-not $resolvedArchive.StartsWith($resolvedDist, [System.StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path -Leaf $resolvedArchive) -ne "$packageName.zip") {
            throw 'Refusing to replace an archive outside the expected dist package path.'
        }
        Remove-Item -LiteralPath $resolvedArchive -Force
    }

    Compress-Archive -Path (Join-Path $stagePath '*') -DestinationPath $archivePath -CompressionLevel Optimal
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $archiveChecksumPath = "$archivePath.sha256"
    [System.IO.File]::WriteAllText(
        $archiveChecksumPath,
        "$archiveHash  $(Split-Path -Leaf $archivePath)`r`n",
        [System.Text.UTF8Encoding]::new($false))
    Write-Host "Package created: $archivePath"
    Write-Host "SHA-256: $archiveHash"
    Write-Host "Checksum: $archiveChecksumPath"
}
finally {
    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $resolvedStage = [System.IO.Path]::GetFullPath($stagePath)
    if ($resolvedStage.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedStage) -match '^[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force -ErrorAction SilentlyContinue
    }
}
