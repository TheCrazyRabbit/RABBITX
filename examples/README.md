# Local Beta demos

All three examples use a tiny HTTP server bound only to `127.0.0.1`. The routes, identifiers, headers, and cookies are synthetic. No public site is contacted.

## Start the fixture server

Open PowerShell in the extracted RABBITX folder and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\examples\Start-DemoServer.ps1
```

Leave that window open. Use a second PowerShell window for the commands below. The server has only the three documented demo routes and returns local fixture responses.

## Case 1 — API surface prioritization

```powershell
.\RABBITX.exe --config .\examples\rabbitx.example.json
```

RABBITX extracts a resource endpoint with an identifier, a search endpoint with pagination, a GraphQL path, and an OAuth path. Review the endpoint classes, risk factors, and generated probe plans. These are review priorities, not confirmed vulnerabilities.

Expected route shapes include:

```text
/case-1/api/users?id=101
/case-1/api/search?q=demo&page=2
/case-1/graphql
/case-1/oauth/token
```

## Case 2 — security headers, CORS, and cookie flags

```powershell
.\RABBITX.exe --html --output .\case-2-report.html `
  http://127.0.0.1:8787/case-2 `
  --scope-host 127.0.0.1:8787 --scope-path /case-2 `
  --max-requests 8 --min-interval-ms 0
```

The fixture deliberately returns wildcard CORS with credentials, a cookie without `Secure`, `HttpOnly`, or `SameSite`, and several missing browser security headers. RABBITX should capture those observations in the report. The rules report configuration signals; a human still needs to judge application impact.

An independently generated sample is included at [reports/demo-security-posture.html](reports/demo-security-posture.html). Its target is the local synthetic fixture.

## Case 3 — project history and replay

```powershell
.\RABBITX.exe --new-project .\case-3-project.json `
  http://127.0.0.1:8787/case-3 --name "Local project demo" `
  --scope-host 127.0.0.1:8787 --scope-path /case-3 `
  --max-requests 8 --min-interval-ms 0

.\RABBITX.exe --project .\case-3-project.json --replay
.\RABBITX.exe --project .\case-3-project.json --html --output .\case-3-report.html
```

The first command saves an audit run. Replay reads the saved report without sending requests. HTML export creates a standalone deliverable from that saved run.

## Cleanup

Press `Ctrl+C` in the server window. You can delete the generated `case-3-project.json`, `case-2-report.html`, and `case-3-report.html` files after the walkthrough.
