# Configuration

Configuration precedence is fixed:

```text
Built-in defaults → Project defaults → JSON config → CLI options
```

CLI options always win. On `--resume`, JSON and CLI values apply to that audit run. They do not rewrite the project's saved defaults. `persistProject` decides whether the run is appended to project history.

## Supported JSON fields

```json
{
  "targetUrl": "https://authorized.example",
  "projectName": "Quarterly API review",
  "maxRequests": 50,
  "minIntervalMs": 200,
  "timeoutMs": 15000,
  "followRedirects": true,
  "scope": {
    "hosts": ["authorized.example:443"],
    "domains": [],
    "paths": ["/api"]
  },
  "outputFormat": "html",
  "outputPath": "reports/audit.html",
  "reportOptions": {
    "includeEvidence": true,
    "includeProbeHistory": true
  },
  "persistProject": true
}
```

`outputFormat` accepts `console`, `json`, or `html`. HTML output requires `outputPath`. `timeoutMs` must be between 1 and 600000. `minIntervalMs` must be between 0 and 86400000. A scope must allow at least one host or domain. Paths are optional; an empty path list allows every path on the permitted host/domain.

Unknown JSON fields are ignored. Duplicate fields and invalid types are rejected. UTF-8 JSON with or without a byte-order mark is supported.

## Command-line overrides

```powershell
.\RABBITX.exe --config .\rabbitx.json --max-requests 20 --min-interval-ms 500
```

Supported run overrides include `--timeout-ms`, `--follow-redirects`, `--no-follow-redirects`, `--scope-host`, `--scope-domain`, `--scope-path`, `--include-evidence`, `--exclude-evidence`, `--include-probe-history`, `--exclude-probe-history`, `--save-project`, and `--no-save-project`.

## Safe project resume

```powershell
.\RABBITX.exe --project .\review.json --resume --config .\run-overrides.json --max-requests 20
```

Every resume starts a new request budget. The effective settings are recorded in the JSON report and audit history. `--no-save-project` or `persistProject: false` keeps the run out of the saved project history.
