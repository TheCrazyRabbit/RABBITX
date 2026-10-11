# Rollout, validation and rollback

No real Live transaction, paid subscription or $0 order is authorized by this work.
Local fixtures are not evidence of real Paddle checkout/fulfillment success.

## Local verification

Requires Node.js 24+ (node:sqlite), VS2022 v143 and Windows SDK. From repository root:

```powershell
node --check backend/worker.js
node --check backend/entitlements.js
node --test backend/tests/*.test.js
./tools/test-entitlements.ps1
```

The helper builds native tests and the full Release x64 application. In a shared
workspace, unrelated unfinished C++ edits may break the full build; build a clean
checkout of the entitlement commit in that case. Do not revert or include another
task's changes to make an authorization commit build.
The audit machine also lacked a usable MSPDB140 component for generating Release
debug symbols. A clean entitlement-only snapshot built successfully with Link
GenerateDebugInformation=false; no security/compiler settings were weakened.
Repair the VS C++ debug-symbol tooling to build the default project with PDBs.

Tests use actual in-memory SQLite production SQL, real HMAC/crypto verification and
injected read-only Paddle API/email fixtures. Cases include signature tampering,
duplicate/parallel receipts, event order, atomic rollback, mailbox proof, bad/expired
OTP, five guesses, concurrent redemption/replay, forged identity/Paddle IDs, no paid
subscription, active, scheduled/canceled paid time, expiry, pauses, past_due/trialing,
refund/dispute/reversal, zero capture, unrelated prices/customers, first checkout period
proof, API/network/DB/mail failure, limits, session expiry/logout, and repeated checks.
Native tests cover header validation, no saved Pro flag, gate recheck, online failure,
expired/revoked sessions, vault errors and logout recovery.

## Stage 1: additive schema and disabled code

Impact: five new entitlement/auth tables, D1 migration tracking table, a daily cleanup
trigger and `/v1/*` handler that returns 503 while `AUTH_ENABLED=false`. Health and the
Live webhook route/core SQL remain unchanged. No old subscription/event is migrated,
deleted or treated as a formal authorization. There is no public test grant endpoint.

Pre-rollout baseline observed: Worker 09f15d1e-a79c-4e29-9a7a-3e288aaa4868 (100%),
DB binding to rabbitx-entitlements-live, one webhook subscription row. That row is
untrusted for authorization; its actual origin is not assumed from the row count.

Before every rollout record current version, secret names, D1 counts and schema.
Keep `AUTH_ENABLED=false`, then:

```powershell
cd backend
pnpm dlx wrangler deployments list
pnpm dlx wrangler secret list
pnpm dlx wrangler d1 migrations apply rabbitx-entitlements-live --remote
pnpm dlx wrangler deploy --dry-run
pnpm dlx wrangler deploy --keep-vars
```

No migration deletes existing data. Never copy webhook records into verified_paid_periods
or entitlement_snapshots. Do not run tests against the production D1 database. GET `/`
must remain 200; unsigned POST `/webhooks/paddle/live` must remain 400; authenticated
routes must be 503 while disabled. Confirm old records unchanged with read-only SQL.

## Stage 2: required external configuration

1. Create/choose Resend account and verify a sending domain. Review cost/limits before
   purchasing anything. Create a send-only API key. Use a private sender/reply mailbox;
   do not ask customers to post emails/tokens/billing identifiers in GitHub Issues.
2. Create a Paddle **Live server API key**, restricted to customer.read,
   subscription.read, transaction.read, adjustment.read and discount.read (restricted
   acceptance proof). This is separate from the
   website live_ client-side token. No write permissions are needed or used.
3. Put new secrets through `wrangler secret put` interactive prompts or Cloudflare
   Worker Settings → Variables and Secrets. Do not paste their values into chat, files,
   scripts or Git. Existing PADDLE_LIVE_WEBHOOK_SECRET is not changed:

```powershell
pnpm dlx wrangler secret put PADDLE_API_KEY
pnpm dlx wrangler secret put RESEND_API_KEY
pnpm dlx wrangler secret put AUTH_HASH_SECRET
```

AUTH_HASH_SECRET must be independently random, at least 32 characters. Generate it
locally and transfer privately. It is not the Paddle endpoint secret. Add non-secret
vars AUTH_MAIL_FROM (verified sender) and PRO_PRICE_IDS (real Live monthly price ID).
Avoid reusing Resend/Paddle write credentials. Add the same approved non-secret vars
to the deployment config so later deployments do not accidentally remove them.

4. Validate real email deliverability with your own mailbox in a separate, private
   staging Worker/D1 with identical migrations. Missing subscription must display Free.
   Never point a public staging grant/mock Paddle endpoint at production. Tests mock
   billing internally, with no unauthenticated development authorization endpoint.
5. Check Paddle key read access and payload shape through read-only requests; verify
   payment line/capture/first-cycle data. This cannot currently be tested without the
   new API key. Confirm no values/tokens appear in Worker logs.
6. Decide/deliver an actual Pro-only capability and update the offer/privacy disclosures
   for email login/Resend, session storage and canonical billing snapshots before a
   customer-facing release. Do not restrict current Free capabilities without approval.
7. Only then set AUTH_ENABLED="true" in the deployment config and deploy. Keep a
   recorded prior version. Do not enable merely because the schema exists.

## Owner-controlled Live acceptance after approval

Do not execute this checklist until Paddle approval and separate owner authorization.
Do not create a subscription or $0 order automatically. A real paid period is needed
to validate the normal paid entitlement policy; a $0 test does not prove it.
The owner-approved, diagnostic-only exception now has its own
[strict zero-total acceptance plan](LIVE_ZERO_TOTAL_ACCEPTANCE.md). It defaults off,
requires a single verified mailbox and canonical transaction/discount binding, and
does not promote ordinary zero orders into paid rights.

1. Confirm Live Website approval and final price/product in official checkout.
2. With explicit owner authorization only, run Paddle's approved end-to-end procedure.
3. Inspect actual transaction/subscription in Paddle, original webhook delivery 2xx,
   and D1 event/subscription receipts. Simulation alone cannot satisfy this step.
4. On the new Windows build use `--auth login` with the actual billing mailbox and
   the mailed OTP; `--auth status` must independently resolve canonical billing.
5. `--auth check` succeeds only for a captured eligible paid period. It is a diagnostic
   gate, not a commercial feature delivery. A $0 order or unpaid mailbox remains Free.
6. Check wrong code, replay and another mailbox cannot claim the customer.
7. An owner-authorized scheduled cancellation retains eligible paid time, then Free;
   paused/past_due/expired/refunded periods deny. Do not mutate a real customer's
   subscription simply to test these states; use local fixtures until approved.
8. Disconnect networking: Pro check fails, ordinary Free commands still work. Restore
   connectivity and retry. Logout must revoke the bearer session; replay yields 401.

Read-only SQL (never publish emails/token digests/customer IDs):

```sql
SELECT COUNT(*) FROM subscriptions;
SELECT COUNT(*) FROM webhook_events;
SELECT tier, reason, valid_until, verified_at, source FROM entitlement_snapshots;
SELECT COUNT(*) FROM auth_sessions WHERE revoked_at IS NULL;
SELECT COUNT(*) FROM verified_paid_periods;
```

## Rollback

Fast stop: set AUTH_ENABLED=false and deploy; existing Free CLI and webhooks continue.
If auth was enabled and sessions must be invalidated, execute the narrowly scoped
`UPDATE auth_sessions SET revoked_at=unixepoch() WHERE revoked_at IS NULL` only with
owner incident-response authorization. Never reset webhook tables or billing records.

Code rollback: `pnpm dlx wrangler rollback <recorded-worker-version-id>`. The pre-change
version above is a fallback baseline, not a substitute for recording the immediately
preceding version at deployment time. Confirm env.DB and the live secret NAME remain
correct, GET /200 and unsigned live webhook/400. Older code simply ignores additive
auth tables; leave them in place. Do not run a DROP/DOWN migration in Live.
Wrangler code rollback does not undo D1 schema or secret/trigger/config changes.
Preserve secrets and ensure auth stays disabled. Remove the new cleanup trigger via
the documented configuration deploy if required; its handler only touches auth TTL data.

No updated executable is automatically uploaded to GitHub Releases. Ship a reviewed
new build, with version/release notes, only after the commercial/product blockers are resolved.

## Additional local readiness stage (2026-10-11)

37 backend tests now pass: the original 11 webhook regressions, authentication/state
tests, restricted zero-total grant matching/expiry/revocation and read-only diagnostic checks.
Migration 0002 was tested only in local SQLite, including repeat application preserving
existing receipt rows. It has not been applied to Live. No new code was deployed,
no production secret created/overwritten, no mail sent and no order/subscription created.
Current local config retains AUTH_ENABLED=false and adds LIVE_ACCEPTANCE_ENABLED=false.

See [ENTITLEMENT_CONFIGURATION](ENTITLEMENT_CONFIGURATION.md) for Resend registration,
provider DNS and private key configuration. The operator readiness tool defaults offline;
five real Paddle read checks require explicit --paddle-read and privately supplied credentials.
The user chose local preparation because no Resend account exists yet.
Public readiness checks are distinct from real email/transaction tests.

Wrangler 4.149.0 local deploy --dry-run bundled the new module successfully (no upload).
It declared the existing DB and both flags=false. A read-only public verification of
the unchanged deployed service returned GET /200, unsigned live webhook/400 and
GET /v1/entitlement/503. No signed notification or enabled-auth request was made.

Before any later deployment, record the actual immediately preceding Worker version,
preserve flag/DB/secret metadata, and obtain confirmation for migration/authorization rollout.
An AUTH_ENABLED=false rollout remains mandatory until isolated delivery/security checks pass.
The restricted acceptance flag and grant must be separately revoked at the end of acceptance.
Code rollback leaves additive D1 tables in place; do not delete legacy customer/event data.

## Observed delivery verification (2026-10-11)

- 28 backend automated tests passed, including all 11 original webhook regressions.
- 8 native authorization tests passed. A clean committed-baseline + entitlement-only
  snapshot compiled as Release x64 with debug-symbol generation disabled for the local
  MSPDB installation issue. Native login against the disabled public service correctly
  reported unavailable; a missing session remained Free. No email or session was created.
- Additive D1 migration 0001_entitlements.sql applied to the existing Live database.
  The original subscription row count remained 1; new auth sessions/snapshots remained 0.
- Disabled code deployed to existing rabbitx-backend, version
  a324d369-5706-4bee-82dd-031dc5712f65. Verified AUTH_ENABLED=false,
  DB=2b0518a0-f1b4-4e98-baa0-6001128f67f4, existing live secret NAME preserved.
- Public GET / returned 200; unsigned POST /webhooks/paddle/live returned 400;
  GET /v1/entitlement returned 503. Both custom domain and workers.dev matched.
  Python's default User-Agent initially got Cloudflare 403/1010; the actual native
  RABBITX-Entitlement/1 agent and browser agents reached the Worker. No WAF rule changed.
- Real email delivery, canonical Paddle API reads with the new read-only key, enabled
  production authentication and real paid fulfillment have not been verified. No real
  transaction or $0 order was created. No new secret was acquired, overwritten or logged.
- Unrelated AI working-tree edits were left in place and excluded from this delivery.
