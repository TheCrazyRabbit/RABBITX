param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$directory = Join-Path $root ('x64\integration-cli-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$checks = 0
function Assert-Result([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "Integration FAIL: $Name" }
    $script:checks++
    Write-Host "Integration PASS: $Name"
}
function Run-Client([string[]]$Arguments, [int]$ExpectedExit = 0) {
    # No shell: capture stderr without PowerShell native error preference differences.
    # These test arguments contain no literal quotes or trailing backslashes.
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = [IO.Path]::GetFullPath($Executable)
    $info.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.RedirectStandardInput = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        [void]$process.Start()
        $process.StandardInput.Close()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) { $process.Kill(); throw 'Integrated CLI timed out.' }
        $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne $ExpectedExit) { throw "Unexpected exit $($process.ExitCode): $output" }
        return $output
    } finally { $process.Dispose() }
}
try {
    Assert-Result ((Run-Client @('--version')) -eq "RABBITX v0.9.0-beta`r`n") 'single binary version'
    $help = Run-Client @('--help')
    Assert-Result ($help.Contains('--auth login|status|check|logout') -and $help.Contains('--ai-triage mock')) 'help exposes both modules'
    Assert-Result ((Run-Client @('--auth') 1).Contains('Usage: RABBITX --auth')) 'missing auth command reaches auth dispatch'
    Assert-Result ((Run-Client @('--auth','invalid') 1).Contains('Usage: RABBITX --auth')) 'unknown auth command rejected'
    foreach ($command in @('login','status','check','logout')) {
        # argc is rejected before credential store access or identity HTTP transport.
        $result = Run-Client @('--auth',$command,'--ai-triage','mock') 1
        Assert-Result ($result.Contains('Usage: RABBITX --auth') -and -not $result.Contains('Auditing')) "auth $command rejects mixed audit options before side effects"
    }
    $target = 'http://127.0.0.1:1/discovery'
    $project = Join-Path $directory 'free.project.json'
    $report = Join-Path $directory 'free.json'
    Run-Client @($target,'--new-project',$project,'--json','--output',$report,'--max-requests','0','--no-ai-triage') | Out-Null
    $free = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    Assert-Result ($free.requestBudget.attemptedRequests -eq 0 -and $free.aiTriage.status -eq 'disabled') 'Free audit and JSON without sign-in or account network'
    $saved = Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
    Assert-Result ($saved.runCount -eq 1 -and -not $saved.defaultConfiguration.aiTriage) 'Free project persistence'
    Run-Client @('--project',$project,'--resume','--ai-triage','mock','--max-requests','0','--json','--output',$report) | Out-Null
    $mock = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    $saved = Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
    Assert-Result ($mock.aiTriage.isMock -and $mock.aiTriage.status -eq 'no_evidence' -and $mock.requestBudget.attemptedRequests -eq 0) 'Mock triage reports no evidence without sign-in or budget bypass'
    Assert-Result ($saved.runCount -eq 2 -and -not $saved.defaultConfiguration.aiTriage) 'resume override leaves Free project defaults unchanged'
    $html = Join-Path $directory 'free.html'
    Run-Client @('--project',$project,'--replay','--html','--output',$html) | Out-Null
    Assert-Result ((Get-Content -LiteralPath $html -Raw).Contains('AI-assisted security triage')) 'Free offline HTML replay with Mock result'
    Write-Host "Integration RESULT: $checks passed, 0 failed"
} finally {
    $allowed = [IO.Path]::GetFullPath((Join-Path $root 'x64')).TrimEnd('\') + '\'
    $resolved = [IO.Path]::GetFullPath($directory)
    if ($resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $resolved) -match '^integration-cli-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
