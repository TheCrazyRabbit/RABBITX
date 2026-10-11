# Subscription entitlement MVP

## Trust boundary

The optional CLI signs in using the customer's Paddle billing mailbox. Every Pro
operation calls a HTTPS Worker gate with an opaque session token. The Worker checks
canonical Paddle Live REST data; neither webhook receipts, browser events nor local
configuration files can assert a paid subscription. No API key is shipped to Windows
or the website. Existing analysis algorithms and Free capabilities are unchanged.

This is an online-only authorization foundation, not a completed commercial product.
See [PRO_ENTITLEMENTS.md](PRO_ENTITLEMENTS.md). External service setup and a paid
feature release still block commercial launch.

## Identity and sessions

`POST /v1/auth/start` accepts exactly `{ "email": "..." }`. The email is normalized
by trimming/lowercasing; no provider-specific plus-address or dot rewriting is used.
A 256-bit random challenge identifies an 8-digit unbiased random OTP. Only its
HMAC-SHA256 digest is stored, with a separate Worker `AUTH_HASH_SECRET` pepper.
The code expires after ten minutes, has five atomic verification attempts and is
consumed exactly once. Concurrent successful attempts cannot create two sessions.

The replaceable `sendCode()` adapter sends through Resend's HTTPS email API. Unit
tests inject a private in-memory mailbox; there is no production mock-mail adapter,
debug code response or manual entitlement grant endpoint. Every syntactically valid
mailbox receives the same sign-in flow, independent of whether it has a subscription.
Errors do not disclose subscription/customer existence.

`POST /v1/auth/finish` accepts exactly `challenge_id` and `code`, and issues a random
256-bit session for 24 hours. D1 stores SHA-256 of the token, not its plaintext.
Sign-in authenticates a mailbox, never a caller-supplied subscription/customer ID.
Authentication and subscription resolution are separate. A new customer-email change
must be made through Paddle, then verified by a fresh mailbox sign-in; the client
cannot relink customers through this API.

For native clients, response headers mirror selected JSON fields:
`X-RABBITX-Challenge`, `X-RABBITX-Session`, `X-RABBITX-Tier`, `X-RABBITX-Valid-Until`.
They are sent only over HTTPS with no-store responses. No CORS headers are emitted;
requests with an Origin header are rejected. Tokens never appear in URLs or logs.

Sensitive bodies are limited to 4096 bytes, including chunked input. D1 atomic rate
buckets impose 100 requests/IP, 10 sign-in starts/IP, 3 starts/mailbox and 60 billing
checks/session per 15 minutes. IP/email buckets use keyed hashes, not plaintext IPs.
Native callers without CF-Connecting-IP share a conservative fallback bucket.

Daily cleanup deletes only expired auth challenges/sessions/limit buckets, not billing
records. Enable the configured cron only with auth enabled. Entitlement snapshots
contain the last canonical decision and verified time for troubleshooting. They are
**not** a cache or source of access: a later failure always denies Pro.

## API

| Route | Auth | Behavior |
|---|---|---|
| POST /v1/auth/start | None | Send mailbox OTP, return challenge |
| POST /v1/auth/finish | OTP | Exchange once for session |
| GET /v1/session | Bearer | Validate current session, no billing data |
| GET /v1/entitlement | Bearer | Refresh canonical billing decision |
| POST /v1/features/pro-foundation/check | Bearer | Empty body; diagnostic gate, 403 if Free |
| POST /v1/auth/logout | Bearer | Revoke this client session |

Auth is disabled unless `AUTH_ENABLED` equals the string `true` and all required
credentials and price allowlist exist. It returns 503 while disabled. Wrong/expired
session is 401, invalid input 400, caller-supplied IDs/state 400, abuse 429, billing,
email or database outage 503. No stale offline Pro allowance exists.

## Canonical subscription decision

Read-only calls use **https://api.paddle.com**, with Worker `PADDLE_API_KEY`, never a
client-side token. Required permissions: `customer.read`, `subscription.read`,
`transaction.read`, `adjustment.read`.

1. List customers by exact verified email; independently check returned email/status.
2. List each customer's subscriptions; independently check customer ID and configured
   `PRO_PRICE_IDS` (comma-separated real Live monthly price IDs).
3. List completed transactions for a matching subscription and its adjustments.
4. Require the transaction's customer/subscription IDs and Pro price to match. Require
   a positive Pro line total and a positive captured payment; missing proof denies access.
5. A paid period must already have started and must not have expired. Active access
   ends at the earlier of the subscription current period end and transaction paid
   period end. Canceled access can continue to the paid transaction period end.
   If the initial web/API checkout has no transaction billing_period, require the
   canonical current cycle start to equal first_billed_at and a captured payment
   within five minutes of that start. Persist that API-observed period in
   verified_paid_periods; after cancellation it can be reused only while the actual
   transaction is still captured and not refunded. No month length is invented.
   A cancellation before any such observation, with no transaction billing period,
   has insufficient proof and remains Free pending support/reconciliation.
6. Approved refunds/credits and unresolved chargeback/warning adjustments on that
   transaction deny that period. This conservative MVP denies even partial approved
   refunds for the affected period; it does not alter the refund policy or issue refunds.

| Canonical status | MVP access |
|---|---|
| active | Pro only with matching, captured, non-refunded paid period |
| scheduled cancel while active | Same, until the paid period ends |
| canceled | Keep remaining eligible paid time; no renewal or unlimited extension |
| past_due / paused | Free; no grace period or paid-pause promise was configured |
| trialing | Free; the existing offer does not configure an authorized free trial |
| expired / missing proof / unsupported status | Free |
| provider/network/database unavailable | Pro unavailable; existing Free CLI remains usable |

No $0/trial entitlement policy was invented. A future owner-authorized $0 onboarding
test may verify checkout, webhook and email/session flow but will **not** receive paid
Pro under the current nonzero-capture rule. If $0 orders should exercise Pro, explicitly
decide that policy first; do not whitelist arbitrary $0 transactions in production.

Pagination preserves filters, accepts cursors only from the same API origin/path,
and is capped at ten pages per list and forty calls/fifteen seconds per check;
incomplete or unauthorized reads fail closed. Each network request has an eight-second timeout.
Every check re-reads API state, so email login and billing checks need functioning
Paddle/Resend connectivity. This favors security and low operational complexity over
offline use. Accounts with very long histories can exceed the cap and need a future
reviewed optimization. It is not intended for polling on every HTTP audit request.

## Webhooks and simulation isolation

The original HMAC raw-body verifier, five-second signature tolerance, D1 atomic batch,
event ID deduplication, monotonic occurred_at UPSERT and canceled-row preservation
are retained. Existing subscription fields are webhook receipts only. No old row is
backfilled into an entitlement. The newly added tables are disjoint.

Paddle simulations can contain real entity IDs. Filtering by an invented ID prefix
cannot distinguish them. Canonical API reads prevent a simulated active webhook
from granting rights, or a simulated canceled webhook from revoking real paid rights.
Only the actual entity and paid transaction/adjustments affect the decision.

No Live Notification Destination was edited. Current events remain supported.
For future operational visibility, the owner can additionally subscribe to
subscription.paused/resumed/activated/past_due and adjustment.created/updated. The
Worker already records unknown event receipts and returns 200, but does not derive
entitlements from them. These extra events are unnecessary for correctness of online
canonical checks; confirm the destination traffic_source before changing it.

## Windows client and threat model

`Infrastructure/Security/entitlement_client.*` is a small testable state machine;
`entitlement_windows.cpp` supplies WinHTTP and Windows Credential Manager. Identity
traffic uses a separate, fixed HTTPS host, without altering audit scope/budget logic.
Redirects are disabled and normal Windows certificate validation remains enabled.
No hardware serials, fingerprints, email-password storage or device lock is introduced.

The credential vault is per Windows user. Only the session token is stored. No Pro
flag/expiry file is trusted. 401 removes the local token; a network error retains it
for retry but denies Pro. Logout revokes server state before clearing the vault; an
offline logout reports failure and keeps the token so revocation can be retried.
Compromised Windows accounts can steal bearer credentials until expiry/revocation.
This does not defend against a locally patched native binary: fully local algorithms
cannot be made piracy-proof with a client boolean. Future server-hosted paid services
must enforce the same gate server-side. Never treat a client tier as server authority.

Future paid operations call `Client::Check(true)` before executing, do not persist
its result, and recheck before later operations. `--auth check` exercises this
boundary but does not execute a paid analysis function or run a transaction.

## Deployment and rollback

See [LIVE_ENTITLEMENT_TEST_PLAN.md](LIVE_ENTITLEMENT_TEST_PLAN.md) and backend README.
The migration is additive, uses IF NOT EXISTS, never deletes or updates old billing
records, and is safe to keep after rolling back code. AUTH_ENABLED defaults to false.
Production enabling is a distinct step after service setup, isolated tests and product
readiness. Existing public v0.8 release artifacts are not automatically replaced.

Sources used to verify payloads and behavior:
[customers](https://developer.paddle.com/api-reference/customers/list-customers/),
[subscriptions](https://developer.paddle.com/api-reference/subscriptions/get-subscription/),
[transactions](https://developer.paddle.com/api-reference/transactions/list-transactions/),
[adjustments](https://developer.paddle.com/api-reference/adjustments/list-adjustments/),
[simulator](https://developer.paddle.com/webhooks/simulator/),
[Resend send API](https://resend.com/docs/api-reference/emails/send-email).
