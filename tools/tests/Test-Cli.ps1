param([string]$Executable, [string]$BaseUrl)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testDirectory = Join-Path $root ('x64\cli-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$checks = 0
function Assert-Result([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "CLI FAIL: $Name" }
    $script:checks++
    Write-Host "CLI PASS: $Name"
}
function Run-Cli([string[]]$Arguments, [int]$ExpectedExit = 0) {
    $output = & $Executable @Arguments 2>&1
    if ($LASTEXITCODE -ne $ExpectedExit) { throw "Unexpected CLI exit $LASTEXITCODE : $output" }
    return ($output -join "`n")
}
try {
    $reportPath = Join-Path $testDirectory 'audit.json'
    $projectPath = Join-Path $testDirectory 'audit.project.json'
    $configPath = Join-Path $testDirectory 'config.json'
    $config = @{targetUrl="$BaseUrl/discovery"; maxRequests=5; minIntervalMs=0; timeoutMs=1500; aiTriage=$true; aiTimeoutMs=500; outputFormat='json'; outputPath=$reportPath}
    [IO.File]::WriteAllText($configPath, ($config | ConvertTo-Json))
    Assert-Result ((Run-Cli @('--version')) -eq 'RABBITX v0.9.0-beta') 'version'
    Run-Cli @('--config', $configPath, '--new-project', $projectPath, '--no-ai-triage') | Out-Null
    $initial = Get-Content -LiteralPath $projectPath -Raw | ConvertFrom-Json
    Assert-Result (-not $initial.defaultConfiguration.aiTriage -and $initial.defaultConfiguration.maxRequests -eq 5) 'CLI overrides file in saved initial defaults'
    $config.maxRequests = 3
    $config.aiTriage = $false
    [IO.File]::WriteAllText($configPath, ($config | ConvertTo-Json))
    Run-Cli @('--project', $projectPath, '--resume', '--config', $configPath, '--ai-triage', 'mock', '--max-requests', '1') | Out-Null
    $resumed = Get-Content -LiteralPath $projectPath -Raw | ConvertFrom-Json
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    Assert-Result ($report.effectiveConfiguration.aiTriage -and $report.effectiveConfiguration.maxRequests -eq 1 -and $report.effectiveConfiguration.aiTimeoutMs -eq 500) 'Project then config then CLI priority'
    Assert-Result ($resumed.runCount -eq 2 -and $report.requestBudget.attemptedRequests -eq 1) 'resume has new run budget'
    Assert-Result (-not $resumed.defaultConfiguration.aiTriage -and $resumed.defaultConfiguration.maxRequests -eq 5) 'resume never mutates defaults'
    $catalog = @($report.evidenceCatalog | ForEach-Object { $_.id })
    Assert-Result ($report.aiTriage.isMock -and $report.aiTriage.status -eq 'completed') 'report explicitly labels Mock'
    foreach ($item in $report.aiTriage.items) {
        foreach ($id in $item.evidenceIds) { if ($catalog -notcontains $id) { throw 'Unresolvable report evidence ID' } }
        if ($item.nextAction.executed) { throw 'AI action was executed' }
    }
    Assert-Result ($report.aiTriage.items.Count -gt 0) 'actual evidence references and no executed action'
    $before = Get-Content -LiteralPath $projectPath -Raw
    $replay = Join-Path $testDirectory 'replay.json'
    Run-Cli @('--project',$projectPath,'--replay','--json','--output',$replay) | Out-Null
    $saved = Get-Content -LiteralPath $replay -Raw | ConvertFrom-Json
    Assert-Result ($saved.aiTriage.status -eq 'completed' -and $saved.requestBudget.attemptedRequests -eq 1 -and (Get-Content -LiteralPath $projectPath -Raw) -eq $before) 'offline replay retains exact saved run'
    $html = Join-Path $testDirectory 'replay.html'
    Run-Cli @('--project',$projectPath,'--replay','--html','--output',$html) | Out-Null
    Assert-Result ((Get-Content -LiteralPath $html -Raw).Contains('AI-assisted security triage')) 'HTML exports triage'
    Run-Cli @('--config',$configPath,'--json','--max-requests','0') | Out-Null
    $zero = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    Assert-Result ($zero.requestBudget.attemptedRequests -eq 0 -and $zero.aiTriage.status -eq 'disabled') 'no-AI zero-budget operation'
    Run-Cli @('--config',$configPath,'--json','--ai-triage','mock','--exclude-evidence','--max-requests','1') | Out-Null
    $omitted = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    Assert-Result ($omitted.evidenceCatalog[0].detailsOmitted -and $omitted.aiTriage.items.Count -gt 0) 'evidence option preserves IDs'
    $invalid = Join-Path $testDirectory 'invalid.json'
    [IO.File]::WriteAllText($invalid,'{"aiTriage":"yes"}')
    Assert-Result ((Run-Cli @('--config',$invalid) 1).Contains('aiTriage must be a boolean')) 'invalid configuration message'
    Assert-Result ((Run-Cli @('--ai-triage','openai',"$BaseUrl/discovery") 1).Contains('real providers are not enabled')) 'real provider cannot be accidentally enabled'
    Write-Host "CLI RESULT: $checks passed, 0 failed"
} finally {
    $allowed = [IO.Path]::GetFullPath((Join-Path $root 'x64')).TrimEnd('\') + '\'
    $resolved = [IO.Path]::GetFullPath($testDirectory)
    if ($resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $resolved) -match '^cli-test-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
