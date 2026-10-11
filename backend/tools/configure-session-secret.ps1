param([switch]$Apply)
$ErrorActionPreference = 'Stop'
$configPath = Join-Path $PSScriptRoot '..\wrangler.jsonc'
if (-not $Apply) {
    Write-Output 'Local preparation only. No secret created or changed. Run with -Apply only after approving Cloudflare configuration.'
    exit 0
}
# Only creates the independent auth hash secret; never replaces an existing key.
$rawNames = & npx.cmd wrangler secret list --config $configPath
if ($LASTEXITCODE -ne 0) { throw 'Could not check secret names; nothing changed.' }
$names = $rawNames | ConvertFrom-Json
if ($names.name -contains 'AUTH_HASH_SECRET') {
    Write-Output 'AUTH_HASH_SECRET already exists. No value read, printed or replaced.'
    exit 0
}
$bytes = New-Object byte[] 32
$generator = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
    $generator.GetBytes($bytes)
    $newAuthHashSecret = -join ($bytes | ForEach-Object { $_.ToString('x2') })
    # Pipeline input is consumed by Wrangler, without putting the value in argv/files/history.
    $newAuthHashSecret | & npx.cmd wrangler secret put AUTH_HASH_SECRET --config $configPath
    if ($LASTEXITCODE -ne 0) { throw 'Session secret configuration failed; inspect secret names before retrying.' }
    Write-Output 'AUTH_HASH_SECRET configured. No auth interface was enabled by this helper.'
} finally {
    $generator.Dispose()
    [Array]::Clear($bytes, 0, $bytes.Length)
    $newAuthHashSecret = $null
}
