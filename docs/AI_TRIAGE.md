# v0.9.0-beta: optional offline triage

The v0.9 development build adds an optional, deterministic **Mock provider**. It is not a real language model. No API key, external model request, or AI API payment is needed. Core audits work with triage disabled, which is the default.

## Run the local demonstration

Build Windows x64 with Visual Studio 2022's Desktop C++ workload and Windows 10/11 SDK. Both Debug and Release use C++17; the x64 Release executable links the MSVC runtime statically.

```powershell
MSBuild.exe .\RABBITX.sln /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe .\RABBITX.sln /p:Configuration=Release /p:Platform=x64 /m
powershell -ExecutionPolicy Bypass -File .\examples\Start-DemoServer.ps1
```

In another PowerShell window:

```powershell
.\x64\Release\RABBITX.exe --version
.\x64\Release\RABBITX.exe --config .\examples\rabbitx.ai-mock.json
.\x64\Release\RABBITX.exe --config .\examples\rabbitx.ai-mock.json --html --output .\mock-triage.html
.\x64\Release\RABBITX.exe --config .\examples\rabbitx.ai-mock.json --json --output .\mock-triage.audit.json
.\x64\Release\RABBITX.exe --config .\examples\rabbitx.ai-mock.json --new-project .\mock-triage.project.json
.\x64\Release\RABBITX.exe --project .\mock-triage.project.json --replay --html --output .\saved-triage.html
.\x64\Release\RABBITX.exe --project .\mock-triage.project.json --resume --no-ai-triage --max-requests 1
```

CLI activation is `--ai-triage mock`; `--no-ai-triage` overrides config/project defaults. `--ai-timeout-ms` accepts 1–60000 (default 2000). Config keys are `aiTriage` (boolean) and `aiTimeoutMs` (integer). Precedence remains built-ins → saved Project defaults → JSON configuration → CLI. Resume overrides never change saved defaults. Replay reads the saved report and invokes neither target HTTP nor an AI provider.

## Interpretation

- **Recorded facts** are local observations associated with a fact ID and a captured evidence ID.
- **Possible risk**, missing validation and next action are unverified provider interpretations. A reference establishes traceability, not semantic correctness of an interpretation.
- **Candidate** means context/impact needs human validation. **Informational** records an observation. **Confirmed** is reserved for independently verified results; this build's checks and AI provider never assign it.
- `confidence` continues to describe confidence in the observed condition. It does not imply confidence in exploitability.
- Provider priority only sorts the triage list. It does not modify endpoint risk scores, findings, severity, confidence or assessment status.
- The Mock uses existing risk scores, surface classification and recorded rules to generate reproducible review suggestions. It performs no adaptive reasoning or target probing.

## Data and schema

The triage context includes at most 64 subjects, 4 supporting snapshots per subject and 16 parameter types/risk factors per endpoint. Full inventory and evidence remain in the report. Provider JSON is capped at 1 MiB, 12000 nodes and 24 nesting levels; native string limits count UTF-8 bytes and can be stricter than schema character limits. Project JSON has a separate 64 MiB/1000000-node read limit.

Report schema **1.4** adds `productVersion`, `aiTriage`, `evidenceCatalog`, finding `status`, and evidence IDs/fingerprints. Existing fields remain. Project schema stays **4**, with optional `aiTriage`/`aiTimeoutMs` defaults. Projects v1–v4 and old reports remain readable. IDs are local to one run; identify a historical observation by Project + run + evidence ID.

Provider input: [input schema](schemas/ai-triage-input.schema.json). Provider response: [output schema](schemas/ai-triage-output.schema.json). Native validation also enforces relational rules: known unique subjects, subject-specific evidence and facts, each fact's supporting evidence, exact subject URL, bounded arrays/text, known action kinds, and no additional fields. JSON duplicate keys, invalid UTF-8, invalid surrogate pairs, excessive nesting and oversized responses are rejected. Validation is atomic: invalid output yields no accepted triage items.

Raw response bodies are not exported or sent to providers. Complete-body and response SHA-256 fingerprints distinguish snapshots; selected sensitive header values are redacted. Provider input includes structural parameter types and risk factors, not parameter values or raw headers/body. URLs redact recognized sensitive query names, userinfo and fragments. These rules are not a general-purpose secret detector: identifiers, custom query names and public URLs may still be sensitive. A real external provider therefore requires a separate explicit data-disclosure decision and credentials setup.

`--exclude-evidence` suppresses detailed snapshots while preserving catalog IDs and triage facts, so references remain resolvable. Provider output and HTTP content are rendered with HTML `textContent`; embedded report JSON escapes HTML script delimiters.

## Recommendation policy

AI proposals are limited to `manual_review` / `NONE` and `baseline_get` / `GET`, for the exact referenced subject URL. Scope, known endpoint/method and remaining budget are checked. All recommendations have `executed:false` and `requiresApproval:true`.

The v0.9 recommendation path has **no executor**. An otherwise eligible GET is labeled `requires_user_approval_and_send_time_scope_budget_rate_check`. A future executor must obtain explicit approval and use the existing `HttpClient` with the current run's scope, budget and limiter, including every redirect hop. Suggestions do not consume or reset a run's budget and do not reserve a rate slot.

Existing automatic Probe execution remains bounded baseline GET retrieval. Plan names such as identifier variation/search analysis describe review intent; this version does not implement identifier mutation, SQLi/XSS payloads, account switching, or automatic exploit verification. Identical GET plans share one network request.

## Failure and operating limits

AI unavailable, invalid output, exceptions and deadline failures retain core results. A provider exception's raw message is withheld. A background provider receives cancellation on deadline; provider implementations must honor cancellation and enforce their own I/O timeout. C++ cannot safely force-kill an uncooperative provider thread; it may finish after the audit returns. No real/network provider is enabled in this release.

HTTP bodies are limited to 8 MiB. Incomplete or failed responses produce no Findings. Completed redirect snapshots are retained if a later hop is rejected. HTTP timeout is a WinHTTP phase setting, including response headers, not a whole-audit wall-clock deadline. Windows timeout scheduling can delay notification; see [Microsoft's WinHTTP option documentation](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags).

## Tests

To regenerate the committed HTML example using an automatically selected local port:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\create-mock-demo.ps1
```

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\test-client.ps1
powershell -ExecutionPolicy Bypass -File .\tools\test-client.ps1 -Configuration Release
# When MSBuild is not discoverable:
powershell -ExecutionPolicy Bypass -File .\tools\test-client.ps1 -MSBuildPath 'D:\VS\MSBuild\Current\Bin\MSBuild.exe'
```

The runner builds a native test executable and uses an isolated loopback fixture. It checks schema failures, misleading output, evidence binding, provider errors/deadlines, budget/scope/method enforcement, rate gaps, HTTP headers/body integrity, reporting, Project compatibility and optional AI operation. It does not contact third-party security targets or commercial services.

## Deferred

Real OpenAI-compatible/local-model providers, outbound disclosure approval, credential storage, approved-action execution, and commercial entitlement integration remain future work. Do not advertise Mock as real AI or as proof of a paid feature. No Free/Pro boundary is changed here.
