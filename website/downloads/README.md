# RABBITX v0.9.0-beta — early trial delivery

Delivery ID: v0.9.0-beta-trial.20261020. This is a repackaged, unchanged executable from the v0.9.0-beta-qa.20261019 Windows QA build. It is an early evaluation build, not a formal commercial release or a claim of real-world vulnerability detection.

Free evaluation: no payment, card or account login is required for the core review workflow. Paid subscriptions are currently unavailable. Three days refers to the agreed supported evaluation, not automatic software expiry. Read LICENSE.txt and TRIAL_GUIDE.md first.

Use only on owned or explicitly authorized test assets. Start with the local demo. Configuration observations and unverified risk candidates are not confirmed vulnerabilities. AI triage is an offline Mock, not a live AI service.

Verify the outer ZIP against the hash on https://crazyrabbit.dev/trial/ and its adjacent .sha256 file. SHA256SUMS.txt inside this package covers the extracted files. DELIVERY_NOTES.md records provenance and limits. A matching hash shows file integrity, not a security certification or code signature.

The original QA README follows for technical reference. Its references to the original QA archive identify the source build; this delivery has its own filename and checksum. The public GitHub main README and older v0.8 release may differ.

---
# RABBITX v0.9.0-beta development

This source targets **v0.9.0-beta**. The supplied v0.9 Windows archive is the QA baseline; the updated test build is named `RABBITX-v0.9.0-beta-qa.20261019-windows-x64.zip`. It is not a new formal GitHub Release. AI triage is an optional **offline Mock** demonstration; no real model API is enabled. See [the v0.9 guide](docs/AI_TRIAGE.md) and [the report counting guide](docs/REPORT_COUNTS.md).

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

The v0.9 optional triage list summarizes recorded facts, prioritizes human review and explains missing validation. Each item references evidence IDs. It is explicitly labeled Mock, preserves the existing finding certainty, and executes no recommendations.

## Windows download

For this QA build, use `dist/RABBITX-v0.9.0-beta-qa.20261019-windows-x64.zip` and its `.sha256` sidecar supplied with the delivery. The [older v0.8.0 Beta archive](https://github.com/TheCrazyRabbit/RABBITX/releases/download/v0.8.0-beta/RABBITX-v0.8.0-beta-windows-x64.zip) does not contain the v0.9 Mock or report aggregation changes.

Extract the archive and open PowerShell in the extracted folder. The archive includes `RABBITX.exe`, this guide, the example configuration, the local demo server, and a sample HTML report.

The v0.9 release build targets Windows 10/11 x64 and statically links the MSVC runtime. It uses Windows WinHTTP for network requests and does not bundle third-party scanning libraries.

Maintainers can rebuild the archive from source with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\package-windows.ps1 -Version v0.9.0-beta-qa.20261019
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
.\RABBITX.exe --config .\examples\rabbitx.example.json
```

That sample is deliberately scoped to the local demo server. For an authorized external target, edit both `targetUrl` and the allowlisted host/domain/path values. See [the configuration guide](docs/CONFIGURATION.md).

## Try the three local demos

The demos only bind to `127.0.0.1`; they do not contact external hosts. Open two PowerShell windows. In the first, start the demo server:

```powershell
powershell -ExecutionPolicy Bypass -File .\examples\Start-DemoServer.ps1
```

In the second, run the walkthroughs in [examples/README.md](examples/README.md). They cover endpoint prioritization, intentionally weak response headers, and saved project/report replay. The report in [examples/reports/demo-security-posture.html](examples/reports/demo-security-posture.html) was generated from the local lab and is sample data, not a real-world vulnerability report.

## Reading the report

Case 1 produces **20 observations in 4 rule groups**; Case 2 produces **32 observations in 8 rule groups**. Those are configuration checks repeated across URLs, not 20 or 32 verified vulnerabilities. Case 3 produces zero configuration findings, which does not establish that the application has no vulnerabilities. Expand each HTML group for all affected response URLs and original evidence.

JSON schema 1.5 keeps `findings` and `findingCount` as the original records and adds `findingSummary` and `findingGroups`. Endpoint/parameter risks are unverified review candidates. [Counting definitions and compatibility](docs/REPORT_COUNTS.md).

## Beta pricing and feedback

The client supports optional `--auth login|status|check|logout` account commands. They contact the account service; `check` is a diagnostic Pro gate. Core audits, projects, JSON/HTML and offline Mock triage work without sign-in. This build does not deliver a distinct paid analysis feature or real AI service. US$24/month is a proposed positioning to validate with users, not evidence of delivered Pro value; do not describe Mock as paid model analysis. Commercial checkout and deployment are maintained separately from this QA work. Please do not send payment or payment details through this repository.

Feedback that is most useful:

1. Did the endpoint ranking change what you chose to inspect?
2. How many minutes did the report save, and what still needed manual work?
3. Would you pay US$24/month for the proposed Pro workflow? Why or why not?

## Current limits

- Discovery is based on the responses RABBITX fetches. It is not a full browser crawler and does not execute application JavaScript.
- Endpoint risk is prioritization, not a vulnerability verdict.
- Probe plans are bounded and evidence-focused; run them only within explicit authorization.
- The product development branch includes Mock triage. Real AI providers and approved recommendation execution are deferred; commercial authorization is owned by a separate work stream.

## Responsible use

Use RABBITX only on systems you own or are explicitly authorized to assess, including targets covered by an applicable bug bounty or security testing policy. Respect program scope, request limits, rate limits, and disclosure rules. Stop if an owner or program asks you to stop. Do not use the demo cases against public targets.

## Project status

The v0.9 QA build provides grouped reporting, an offline evidence-to-triage flow and bounded HTTP testing. See `RABBITX_QA_REPORT.md` for actual test results, limits and delivery readiness. A formal GitHub Release needs the owner's release decision.
