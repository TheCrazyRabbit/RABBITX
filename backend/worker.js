// Cloudflare module Worker. Deploy this file to rabbitx-backend.
// Live-only webhook: PADDLE_LIVE_WEBHOOK_SECRET + existing D1 binding DB.
const SIGNATURE_TOLERANCE_SECONDS = 5;
const SUBSCRIPTION_EVENTS = new Set([
  "subscription.created", "subscription.updated", "subscription.canceled"
]);
const encoder = new TextEncoder();
const reply = (status, body) => Response.json(body, {
  status, headers: { "Cache-Control": "no-store" }
});
const text = (value) => typeof value === "string" && value.length > 0 ? value : null;

async function verifySignature(header, rawBody, secret) {
  const timestamps = [];
  const signatures = [];
  for (const part of header.split(";")) {
    const [name, ...rest] = part.trim().split("=");
    const value = rest.join("=").trim();
    if (name === "ts") timestamps.push(value);
    if (name === "h1" && /^[a-fA-F0-9]{64}$/.test(value)) signatures.push(value);
  }
  if (timestamps.length !== 1 || !/^\d+$/.test(timestamps[0]) || !signatures.length) return false;
  const timestamp = Number(timestamps[0]);
  if (!Number.isSafeInteger(timestamp) ||
      Math.abs(Math.floor(Date.now() / 1000) - timestamp) > SIGNATURE_TOLERANCE_SECONDS) return false;
  const key = await crypto.subtle.importKey(
    "raw", encoder.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["verify"]
  );
  const payload = encoder.encode(`${timestamps[0]}:${rawBody}`);
  // Web Crypto verifies HMAC without a JavaScript string comparison.
  // Multiple h1 values are supported for Paddle secret rotation.
  for (const signature of signatures) {
    const bytes = Uint8Array.from(signature.match(/../g), (hex) => parseInt(hex, 16));
    if (await crypto.subtle.verify("HMAC", key, bytes, payload)) return true;
  }
  return false;
}

function subscriptionStatement(db, event) {
  const data = event.data;
  const id = text(data?.id) || text(data?.subscription_id);
  const status = text(data?.status);
  const time = Date.parse(event.occurred_at);
  if (!id || !status || !Number.isFinite(time)) return null;
  if (data.items !== undefined && !Array.isArray(data.items)) return null;
  const hasItems = Array.isArray(data.items);
  const item = data.items?.[0];
  const priceId = text(item?.price?.id) || text(item?.price_id);
  const productId = text(item?.price?.product_id) || text(item?.price?.product?.id) || text(item?.product_id);
  // Subscription notifications normally have no email; never infer one.
  const email = text(data.customer_email) || text(data.customer?.email);
  const occurredAt = new Date(time).toISOString();
  return db.prepare(`
    INSERT INTO subscriptions
      (subscription_id, customer_id, customer_email, subscription_status, price_id, product_id, updated_at)
    SELECT ?, ?, ?, ?, ?, ?, ?
    WHERE NOT EXISTS (SELECT 1 FROM webhook_events WHERE event_id = ?)
    ON CONFLICT(subscription_id) DO UPDATE SET
      customer_id = COALESCE(excluded.customer_id, subscriptions.customer_id),
      customer_email = COALESCE(excluded.customer_email, subscriptions.customer_email),
      subscription_status = excluded.subscription_status,
      price_id = CASE WHEN ? THEN excluded.price_id ELSE subscriptions.price_id END,
      product_id = CASE WHEN ? THEN excluded.product_id ELSE subscriptions.product_id END,
      updated_at = excluded.updated_at
    WHERE subscriptions.updated_at IS NULL
       OR julianday(subscriptions.updated_at) IS NULL
       OR julianday(excluded.updated_at) >= julianday(subscriptions.updated_at)
  `).bind(id, text(data.customer_id), email, status, priceId, productId, occurredAt,
    event.event_id, hasItems ? 1 : 0, hasItems ? 1 : 0);
}

export default {
  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (request.method === "GET" && path === "/") {
      return reply(200, { service: "rabbitx-backend", status: "ok" });
    }
    if (request.method !== "POST" || path !== "/webhooks/paddle/live") {
      return reply(404, { error: "Not found" });
    }
    const secret = env.PADDLE_LIVE_WEBHOOK_SECRET;
    const db = env.DB;
    if (!text(secret)) {
      console.error("Paddle webhook secret is not configured");
      return reply(500, { error: "Webhook secret is not configured" });
    }
    const header = request.headers.get("Paddle-Signature");
    if (!header) return reply(400, { error: "Paddle-Signature header is required" });

    let rawBody;
    try {
      rawBody = await request.text();
      if (!await verifySignature(header, rawBody, secret)) {
        return reply(401, { error: "Invalid webhook signature" });
      }
    } catch {
      console.error("Webhook signature verification failed");
      return reply(500, { error: "Signature verification failed" });
    }

    let event;
    try { event = JSON.parse(rawBody); }
    catch { return reply(400, { error: "Invalid JSON" }); }
    if (!text(event?.event_id) || !text(event?.event_type)) {
      return reply(400, { error: "event_id and event_type are required" });
    }
    if (!db) {
      console.error("D1 DB binding is missing");
      return reply(500, { error: "Database is not configured" });
    }

    try {
      const existing = await db.prepare("SELECT event_id FROM webhook_events WHERE event_id = ?")
        .bind(event.event_id).first();
      if (existing) return reply(200, { ok: true });

      const statements = [];
      if (SUBSCRIPTION_EVENTS.has(event.event_type)) {
        const statement = subscriptionStatement(db, event);
        if (!statement) return reply(400, { error: "Invalid subscription event data" });
        statements.push(statement);
      }
      // transaction.completed (including missing data fields) and unknown events
      // are acknowledged and recorded without granting product access.
      statements.push(db.prepare(`
        INSERT INTO webhook_events (event_id, event_type, processed_at)
        VALUES (?, ?, ?) ON CONFLICT(event_id) DO NOTHING
      `).bind(event.event_id, event.event_type, new Date().toISOString()));

      // Atomic D1 batch: subscription change and event receipt commit together.
      // The SQL NOT EXISTS guard also covers concurrent deliveries that both
      // passed the preliminary SELECT before either batch committed.
      await db.batch(statements);
      console.log("Paddle webhook processed", event.event_id, event.event_type);
      return reply(200, { ok: true });
    } catch {
      // Do not log body, signature, secret, customer email or DB error details.
      console.error("Paddle webhook D1 processing failed", event.event_id, event.event_type);
      return reply(500, { error: "Webhook processing failed; retry delivery" });
    }
  }
};
