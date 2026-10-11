# RABBITX Live webhook backend

Live-only Cloudflare module Worker for rabbitx-backend. Deploy worker.js. No build or production npm dependencies are required. No C++ or frontend changes, schema migrations, entitlement API, license, login, JWT or client logic are included.

## Routes and configuration

- GET / → 200, {"service":"rabbitx-backend","status":"ok"}. This does not verify DB or secret configuration.
- POST /webhooks/paddle/live → verify Paddle-Signature using env.PADDLE_LIVE_WEBHOOK_SECRET, then write using env.DB.
- Other paths/methods return 404. The original and Sandbox webhook routes are removed.

Required Cloudflare settings:

- Worker: rabbitx-backend; custom domain: api.crazyrabbit.dev.
- D1 binding DB → existing rabbitx-entitlements-live, with the existing subscriptions and webhook_events tables.
- Secret PADDLE_LIVE_WEBHOOK_SECRET → endpoint secret for the existing Paddle Live notification destination. Never place its value in source, Git, logs or the website. It is neither a client-side token nor an API key.

## Verified deployment (11 October 2026)

The Live Worker has been deployed to the existing rabbitx-backend and api.crazyrabbit.dev.

- Active version: 09f15d1e-a79c-4e29-9a7a-3e288aaa4868 (100%).
- Actual D1 binding verified after deploy: DB → rabbitx-entitlements-live, ID 2b0518a0-f1b4-4e98-baa0-6001128f67f4. The previous binding name was DB_LIVE; deployment corrected its name without changing the database or data.
- PADDLE_LIVE_WEBHOOK_SECRET verified present by metadata only. No secret value was retrieved or stored. An older unused Secret remains untouched.
- D1 read-only table check confirmed subscriptions and webhook_events exist.
- GET / returned 200 with health JSON.
- Unsigned POST /webhooks/paddle/live returned 400 with Paddle-Signature header is required.
- Removed routes returned 404.
- Official tail invocation logs confirmed GET / (200) and POST /webhooks/paddle/live (400), scriptName rabbitx-backend, outcome ok. Wrangler's Node WebSocket timed out, so the same official tail stream was verified through browser networking and the temporary tail was deleted. No request headers, signatures or bodies were logged in the verification output.
- A signed Paddle Live notification has not been sent by this deployment task. This is the remaining end-to-end validation step; no paid transaction was created.

## Repeat deployment using Wrangler

backend/wrangler.jsonc now contains the verified existing account, Worker, Live D1 ID, existing custom domain, observability, workers.dev and preview settings. It contains no Secret values. No dashboard code editor is required.

From repository root:

```powershell
npx wrangler whoami
node --test backend/tests/worker.test.js
npx wrangler deploy --config backend/wrangler.jsonc --dry-run
npx wrangler deploy --config backend/wrangler.jsonc --keep-vars
npx wrangler deployments status --config backend/wrangler.jsonc
npx wrangler secret list --config backend/wrangler.jsonc
```

Only run wrangler login if authentication has expired. --keep-vars preserves dashboard variables; existing Secrets are preserved by deployment. The explicit D1 and domain configuration must continue to match the existing resources. Do not deploy to a temporary account, create a new Worker or database, or run migrations. runtime .wrangler files and credential files must not be committed.

## Paddle Live destination and verification

Use the existing destination; do not create a duplicate. In Paddle Live → Events → Notifications confirm URL https://api.crazyrabbit.dev/webhooks/paddle/live and these events:

- transaction.completed
- subscription.created
- subscription.updated
- subscription.canceled

If simulations are supported for this Live account/destination, use usage type Both and Events → Simulations, select the existing destination, then run subscription.created/updated/canceled and transaction.completed single events or an appropriate scenario. Simulations may create sample rows in the Live D1; identify them explicitly and do not treat them as actual customer purchases. Do not make a real paid purchase solely to test this task.

After deployment check:

```powershell
Invoke-RestMethod https://api.crazyrabbit.dev/
```

GET must return the health JSON. POST https://api.crazyrabbit.dev/webhooks/paddle/live with body {} and no Paddle-Signature must return 400. 404 indicates the route is not deployed; 500 may indicate missing Secret. A successful signed delivery returns HTTP 200 and {"ok":true}; duplicate event IDs return the same result without repeating writes.

Paddle delivery log should show 200 and delivered/success. In Cloudflare D1 → rabbitx-entitlements-live → Console:

```sql
SELECT * FROM subscriptions;
SELECT * FROM webhook_events;
```

Subscription lifecycle events create/update the subscription row, including status and first item price/product. Cancellation preserves the row and stores the payload's status. customer_email remains NULL unless explicitly provided. transaction.completed records only the event; it does not grant entitlements. Unknown signed event types are recorded and acknowledged.

## Preserved safety and processing behavior

Verification uses the original request.text() body, ts + ':' + rawBody, HMAC-SHA256 via Web Crypto verify, before JSON parsing. Multiple h1 signatures are supported. Timestamp tolerance remains 5 seconds. Missing header returns 400; invalid/tampered/stale signatures return 401. Use Paddle resend rather than reusing old captured signatures.

Subscription upsert and webhook_events receipt share an atomic D1 batch. A SQL NOT EXISTS guard covers concurrent duplicate deliveries. Failed batches roll back and return 500 for retry. Older occurred_at events are recorded but do not overwrite newer subscription state. updated_at stores occurred_at normalized to UTC milliseconds; processed_at stores receipt processing time. Equal timestamps apply in arrival order. Existing single-price schema records the first item only. No customer email is guessed or fetched with an API key.

## Local checks

Requires Node.js 24+ for the built-in SQLite test adapter. From repository root:

```powershell
node --check backend/worker.js
node --test backend/tests/worker.test.js
```

11 tests cover health, removed routes, missing header/secret/DB, correct signatures, tampering, raw Unicode/whitespace, timestamp expiry, subscription lifecycle/items, cancellation retention, transaction receipt-only behavior, idempotency, out-of-order delivery and atomic rollback. Tests use local fixture secrets and in-memory SQLite; they do not prove Cloudflare deployment.

## References

https://developers.cloudflare.com/workers/wrangler/commands/workers/
https://developers.cloudflare.com/workers/wrangler/configuration/
https://developer.paddle.com/webhooks/about/signature-verification/
https://developer.paddle.com/webhooks/about/notification-destinations/
https://developer.paddle.com/webhooks/simulator/test-webhooks/
