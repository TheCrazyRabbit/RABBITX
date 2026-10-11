param([int]$Port, [string]$ReadyFile)
$ErrorActionPreference = 'Stop'
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $Port)
$listener.Start()
[IO.File]::WriteAllText($ReadyFile, 'ready')
try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $stream = $client.GetStream()
            $reader = [IO.StreamReader]::new($stream)
            $line = $reader.ReadLine()
            if (-not $line) { continue }
            $route = ($line -split ' ')[1]
            while ($reader.ReadLine()) {}
            $status = '200 OK'
            $headers = [Collections.Generic.List[string]]::new()
            $headers.Add('Content-Type: text/html')
            $headers.Add("X-Test-Route: $route")
            $body = '<html>Local fixture</html>'
            if ($route -eq '/discovery') { $body = '<script>const a="/api/users?id=1";const b="/api/search?q=test&page=2";</script>' }
            if ($route -eq '/redirect-1') { $status = '302 Found'; $headers.Add('Location: /redirect-2') }
            if ($route -eq '/redirect-2') { $status = '302 Found'; $headers.Add('Location: /landing') }
            if ($route -eq '/scope-start') { $status = '302 Found'; $headers.Add('Location: https://outside.invalid/never-send') }
            if ($route -eq '/large') { $body = 'x' * (8 * 1024 * 1024 + 1024) }
            if ($route -eq '/auth') { $status = '401 Unauthorized' }
            if ($route -eq '/cookies') { $headers.Add('Set-Cookie: first=one; Secure; HttpOnly; SameSite=Lax'); $headers.Add('Set-Cookie: second=two') }
            if ($route -eq '/slow') { Start-Sleep -Milliseconds 5000 }
            $bytes = [Text.Encoding]::UTF8.GetBytes($body)
            $length = $bytes.Length
            if ($route -eq '/partial') { $length += 100 }
            $headers.Add("Content-Length: $length")
            $headers.Add('Connection: close')
            $head = [Text.Encoding]::ASCII.GetBytes("HTTP/1.1 $status`r`n" + ($headers -join "`r`n") + "`r`n`r`n")
            $stream.Write($head, 0, $head.Length)
            $stream.Write($bytes, 0, $bytes.Length)
            $stream.Flush()
        } catch { } finally { $client.Dispose() }
    }
} finally { $listener.Stop() }
