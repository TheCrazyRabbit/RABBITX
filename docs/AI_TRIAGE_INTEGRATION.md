# Work 2 integration boundary

For the combined Windows client and its verification, see
[v0.9 client integration](V0.9_INTEGRATION_REPORT.md). Both modules coexist;
AI subscription enforcement remains a future product decision.

The optional product module is independent of payments/subscriptions.

Stable client entry points:

- `Core/Context/audit_configuration.h`: `aiTriage` (default false), `aiTimeoutMs`.
- `Engine/Intelligence/ai_triage_engine.h`: `AITriageEngine::Run(audit, shared_ptr<IAIProvider>, timeoutMs)`.
- `Infrastructure/AI/ai_provider.h`: replaceable sanitized-context provider contract.
- `Core/Models/ai_triage.h`: report/result models; output schema version 1.0.
- `Report/json_reporter.cpp`: report schema 1.4 `aiTriage` envelope.

If the commercial owner later designates AI as Pro, evaluate the completed entitlement system before calling `AITriageEngine::Run`. Return a product-facing unavailable/disabled state when denied and keep the core audit/report path available according to the owner's approved feature policy. Do not put authorization checks into the provider or silently charge for a model call.

This task does not modify authorization implementation files, Paddle, subscription database, backend or website code. The development commit excludes concurrent Work 2 CLI dispatch and project registration hunks. Integrate branches by merging both sets of changes to the shared CLI/project files, then run product and entitlement suites independently. No outbound model API, commercial API or target probe can be initiated by the Mock.
