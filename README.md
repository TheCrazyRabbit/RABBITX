# RABBITX v0.8 Beta

**Prioritize risky API endpoints and produce reproducible evidence with fewer requests.**

RABBITX is a Windows command-line assistant for authorized web security reviews. It extracts endpoints from a target response, classifies endpoints and parameters, ranks review candidates, plans bounded probes, and saves evidence in JSON or standalone HTML reports.

RABBITX helps a researcher decide where to look next. Its risk scores and findings are triage signals; they do not prove exploitability or replace manual verification.

## What is in this beta

- Endpoint and parameter classification with explainable risk factors.
- Probe planning based on endpoint characteristics.
- A host, domain, and path scope gate on HTTP requests.
- Per-run request budgets and minimum request intervals, including redirect hops.
- Saved projects with audit history, resume, and offline replay.
- JSON and standalone HTML reports with scope, budget, effective configuration, and evidence.
- A local-only demo server and three reproducible walkthroughs.

This is a CLI beta, not a hosted service. AI triage and account or license management are not included in v0.8.

## Windows download

Download the [RABBITX v0.8.0 Beta Windows x64 archive](https://github.com/TheCrazyRabbit/RABBITX/releases/download/v0.8.0-beta/RABBITX-v0.8.0-beta-windows-x64.zip). The SHA-256 sidecar is available from the same GitHub Release.

Extract the archive and open PowerShell in the extracted folder. The archive includes `RABBITX.exe`, this guide, the example configuration, the local demo server, and a sample HTML report.

The release binary targets Windows x64 and statically links the MSVC runtime. It uses Windows WinHTTP for network requests and does not bundle third-party scanning libraries.

Maintainers can rebuild the archive from source with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\package-windows.ps1
```

## Quick start

Only audit assets that you own or are explicitly authorized to test. Confirm the program scope and testing policy first.

```powershell
.\RABBITX.exe --help
.\RABBITX.exe https://authorized.example --max-requests 50 --min-interval-ms 200
```

Save a standalone report:

```powershell
.\RABBITX.exe --html --output .\audit.html https://authorized.example
.\RABBITX.exe --json --output .\audit.json https://authorized.example
```

Create and resume a project:

```powershell
.\RABBITX.exe --new-project .\review.json https://authorized.example --name "Authorized review"
.\RABBITX.exe --project .\review.json --resume
.\RABBITX.exe --project .\review.json --replay
```

Load a JSON configuration. Project defaults are applied first, then the file, then CLI options. A resume uses those values for that run without changing the saved defaults.

```powershell
.\RABBITX.exe --config .\examples\rabbitx.example.json https://authorized.example
```

That sample is deliberately scoped to the local demo server. For an authorized external target, edit both `targetUrl` and the allowlisted host/domain/path values. See [the configuration guide](docs/CONFIGURATION.md).

## Try the three local demos

The demos only bind to `127.0.0.1`; they do not contact external hosts. Open two PowerShell windows. In the first, start the demo server:

```powershell
powershell -ExecutionPolicy Bypass -File .\examples\Start-DemoServer.ps1
```

In the second, run the walkthroughs in [examples/README.md](examples/README.md). They cover endpoint prioritization, intentionally weak response headers, and saved project/report replay. The report in [examples/reports/demo-security-posture.html](examples/reports/demo-security-posture.html) was generated from the local lab and is sample data, not a real-world vulnerability report.

## Beta pricing and feedback

The current beta has no checkout or license enforcement. The proposed Free and Pro plans are a pricing experiment, not an active offer. See the [pricing page](docs/pricing.html) and [recruitment drafts](docs/BETA_OUTREACH.md). Please do not send payment or payment details through this repository.

Feedback that is most useful:

1. Did the endpoint ranking change what you chose to inspect?
2. How many minutes did the report save, and what still needed manual work?
3. Would you pay US$24/month for the proposed Pro workflow? Why or why not?

## Current limits

- Discovery is based on the responses RABBITX fetches. It is not a full browser crawler and does not execute application JavaScript.
- Endpoint risk is prioritization, not a vulnerability verdict.
- Probe plans are bounded and evidence-focused; run them only within explicit authorization.
- The beta does not implement AI triage, cloud accounts, team collaboration, or paid feature gates.

## Responsible use

Use RABBITX only on systems you own or are explicitly authorized to assess, including targets covered by an applicable bug bounty or security testing policy. Respect program scope, request limits, rate limits, and disclosure rules. Stop if an owner or program asks you to stop. Do not use the demo cases against public targets.

## Project status

RABBITX v0.8 is the Product Beta baseline. The next development slice is v0.9 AI Security Triage: explainable endpoint/finding prioritization and a small, reviewable next action. It will not run unbounded scans.
