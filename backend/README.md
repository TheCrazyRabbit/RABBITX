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

## Deploy using Wrangler (read-only dashboard editor is not needed)

The agent checked Wrangler authentication on 11 October 2026: not authenticated. These instructions require logging in as a user authorized to deploy the existing Worker. Do not deploy to a temporary preview account; it would not update api.crazyrabbit.dev.

Use a terminal with Node.js/npm installed:

```powershell
npx wrangler login
npx wrangler whoami
```

Export the existing Worker's configuration into an empty scratch directory outside the repository, preserving its actual account, D1 database ID, compatibility settings and domain configuration:

```powershell
$deployRoot = Join-Path $env:TEMP ("rabbitx-live-deploy-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $deployRoot | Out-Null
Set-Location -LiteralPath $deployRoot
npx wrangler init --from-dash rabbitx-backend
Set-Location -LiteralPath (Join-Path $deployRoot 'rabbitx-backend')
```

Inspect the generated wrangler.jsonc or wrangler.toml. Confirm name is rabbitx-backend; binding is DB; database_name is rabbitx-entitlements-live; database_id matches that database. Confirm existing custom domain api.crazyrabbit.dev is preserved. Do not create a new database or run migrations. Use `npx wrangler secret list --name rabbitx-backend` to confirm PADDLE_LIVE_WEBHOOK_SECRET exists (lists metadata only). The already configured secret does not need to be re-entered.

Deploy the repository's source as the entrypoint, using the exported configuration in the current directory:

```powershell
npx wrangler deploy "D:\幻灵末士\C and C++\RABBITX\RABBITX\backend\worker.js" --name rabbitx-backend --keep-vars
```

The positional file overrides the exported main entrypoint. --keep-vars preserves dashboard variables; existing Secrets are preserved by Wrangler deployment. This flag does not replace the need to verify D1/domain bindings in the exported configuration. Only proceed with the existing Worker/account/database confirmed. Exported settings, credentials and deployment scratch files must not be committed.

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
