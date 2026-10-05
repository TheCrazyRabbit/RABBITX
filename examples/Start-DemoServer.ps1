param(
    [ValidateRange(1024, 65535)]
    [int]$Port = 8787
)

$ErrorActionPreference = 'Stop'
$utf8 = [System.Text.UTF8Encoding]::new($false)

$fixtures = @{
    'case-1' = [pscustomobject]@{
        Body = @'
<!doctype html><html><head><title>RABBITX Local Demo 1</title></head><body>
<h1>API surface prioritization</h1>
<p>Local training fixture. Values and routes are synthetic.</p>
<script>
const user = "/case-1/api/users?id=101";
const search = "/case-1/api/search?q=demo&page=2";
const graph = "/case-1/graphql";
const oauth = "/case-1/oauth/token";
</script></body></html>
'@
        Headers = @('Content-Type: text/html; charset=utf-8')
    }
    'case-2' = [pscustomobject]@{
        Body = @'
<!doctype html><html><head><title>RABBITX Local Demo 2</title></head><body>
<h1>Response header review</h1>
<p>This local fixture intentionally sends weak CORS and cookie settings.</p>
<script>
const account = "/case-2/api/account?account_id=55";
const search = "/case-2/api/search?q=demo&page=1";
const graph = "/case-2/graphql";
</script></body></html>
'@
        Headers = @(
            'Content-Type: text/html; charset=utf-8',
            'Access-Control-Allow-Origin: *',
            'Access-Control-Allow-Credentials: true',
            'Set-Cookie: rabbitx_demo=active; Path=/'
        )
    }
    'case-3' = [pscustomobject]@{
        Body = @'
<!doctype html><html><head><title>RABBITX Local Demo 3</title></head><body>
<h1>Saved project and repeatable report</h1>
<p>This local fixture sends a stronger baseline for comparison.</p>
<script>
const project = "/case-3/api/projects?id=alpha";
const search = "/case-3/api/search?q=demo&page=1";
</script></body></html>
'@
        Headers = @(
            'Content-Type: text/html; charset=utf-8',
            'Content-Security-Policy: default-src ''self''; frame-ancestors ''none''',
            'X-Content-Type-Options: nosniff',
            'X-Frame-Options: DENY',
            'Referrer-Policy: no-referrer',
            'Set-Cookie: rabbitx_demo=active; Path=/; Secure; HttpOnly; SameSite=Strict'
        )
    }
}

$listener = [System.Net.Sockets.TcpListener]::new(
    [System.Net.IPAddress]::Loopback,
    $Port)

try {
    $listener.Start()
    Write-Host "RABBITX local demo server: http://127.0.0.1:$Port/"
    Write-Host 'Cases: /case-1, /case-2, /case-3. Press Ctrl+C to stop.'

    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $stream = $client.GetStream()
            $reader = [System.IO.StreamReader]::new(
                $stream,
                [System.Text.Encoding]::ASCII,
                $false,
                1024,
                $true)

            try {
                $requestLine = $reader.ReadLine()
                if ($null -eq $requestLine) {
                    continue
                }

                while ($true) {
                    $headerLine = $reader.ReadLine()
                    if ($null -eq $headerLine -or $headerLine.Length -eq 0) {
                        break
                    }
                }
            }
            finally {
                $reader.Dispose()
            }

            $parts = $requestLine.Split(' ')
            $requestTarget = if ($parts.Length -ge 2) { $parts[1] } else { '/' }
            $path = $requestTarget.Split('?')[0]
            $caseKey = $null

            foreach ($candidate in $fixtures.Keys) {
                if ($path -eq "/$candidate" -or $path.StartsWith("/$candidate/")) {
                    $caseKey = $candidate
                    break
                }
            }

            if ($null -eq $caseKey) {
                $status = '404 Not Found'
                $body = '<!doctype html><title>Not found</title><p>Use /case-1, /case-2, or /case-3.</p>'
                $responseHeaders = @('Content-Type: text/html; charset=utf-8')
            }
            else {
                $status = '200 OK'
                $body = $fixtures[$caseKey].Body
                $responseHeaders = $fixtures[$caseKey].Headers
            }

            $bodyBytes = $utf8.GetBytes($body)
            $headerLines = [System.Collections.Generic.List[string]]::new()
            $headerLines.Add("HTTP/1.1 $status")
            foreach ($responseHeader in $responseHeaders) {
                $headerLines.Add($responseHeader)
            }
            $headerLines.Add("Content-Length: $($bodyBytes.Length)")
            $headerLines.Add('Connection: close')
            $headerText = ($headerLines -join "`r`n") + "`r`n`r`n"
            $headerBytes = [System.Text.Encoding]::ASCII.GetBytes($headerText)
            $stream.Write($headerBytes, 0, $headerBytes.Length)
            $stream.Write($bodyBytes, 0, $bodyBytes.Length)
            $stream.Flush()
        }
        catch {
            Write-Warning $_.Exception.Message
        }
        finally {
            $client.Dispose()
        }
    }
}
finally {
    $listener.Stop()
}
