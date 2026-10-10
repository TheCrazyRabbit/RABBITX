import { test } from "node:test";
import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";
import { createHmac } from "node:crypto";
import worker from "../worker.js";

// In-memory SQLite runs the actual production SQL behind a D1-compatible adapter.
// This secret is a test fixture, never a deployed Paddle secret.
const secret = "local-signature-test-fixture";
function database() {
  const sqlite = new DatabaseSync(":memory:");
  sqlite.exec(`
    CREATE TABLE subscriptions (subscription_id TEXT PRIMARY KEY, customer_id TEXT,
      customer_email TEXT, subscription_status TEXT, price_id TEXT, product_id TEXT, updated_at TEXT);
    CREATE TABLE webhook_events (event_id TEXT PRIMARY KEY, event_type TEXT, processed_at TEXT);
  `);
  const db = {
    sqlite, failBatch: false, skipLookup: false,
    prepare(sql) {
      return { sql, args: [], bind(...args) { this.args = args; return this; },
        async first() { return db.skipLookup ? null : sqlite.prepare(sql).get(...this.args) ?? null; }
      };
    },
    async batch(statements) {
      sqlite.exec("BEGIN");
      try {
        for (const [i, statement] of statements.entries()) {
          sqlite.prepare(statement.sql).run(...statement.args);
          if (db.failBatch && i === 0) throw new Error("Injected DB failure");
        }
        sqlite.exec("COMMIT");
        return statements.map(() => ({ success: true }));
      } catch (error) { sqlite.exec("ROLLBACK"); throw error; }
    }
  };
  return db;
}
const signature = (body, ts = Math.floor(Date.now() / 1000), key = secret) =>
  `ts=${ts};h1=${createHmac("sha256", key).update(`${ts}:${body}`).digest("hex")}`;
const event = (id, type = "subscription.created", overrides = {}) => ({
  event_id: id, event_type: type, occurred_at: "2026-10-10T01:00:00Z",
  data: { id: "sub_test", customer_id: "ctm_test", status: "active",
    items: [{ price: { id: "pri_test", product_id: "pro_test" } }] }, ...overrides
});
async function deliver(db, value, options = {}) {
  const body = typeof value === "string" ? value : JSON.stringify(value);
  return worker.fetch(new Request(`https://api.crazyrabbit.dev${options.path ?? "/webhooks/paddle/live"}`, {
    method: "POST", body,
    headers: options.headers ?? { "Paddle-Signature": signature(body) }
  }), { DB: db, PADDLE_LIVE_WEBHOOK_SECRET: secret, ...options.env });
}
const row = (db) => db.sqlite.prepare("SELECT * FROM subscriptions").get();
const count = (db) => db.sqlite.prepare("SELECT count(*) AS n FROM webhook_events").get().n;

test("routes, missing secret/header/binding and signed invalid JSON", async () => {
  const db = database();
  assert.deepEqual(await (await worker.fetch(new Request("https://api.crazyrabbit.dev/"), {})).json(),
    { service: "rabbitx-backend", status: "ok" });
  assert.equal((await worker.fetch(new Request("https://api.crazyrabbit.dev/nope"), {})).status, 404);
  assert.equal((await worker.fetch(new Request("https://api.crazyrabbit.dev/webhooks/paddle"), {})).status, 404);
  assert.equal((await deliver(db, event("evt_1"), { env: { PADDLE_LIVE_WEBHOOK_SECRET: "" } })).status, 500);
  assert.equal((await deliver(db, event("evt_1"), { headers: {} })).status, 400);
  assert.equal((await deliver(db, event("evt_1"), { env: { DB: null } })).status, 500);
  assert.equal((await deliver(db, "not JSON")).status, 400);
  assert.equal((await deliver(db, {})).status, 400);
  assert.equal(count(db), 0);
});

test("real HMAC: tampering, wrong secret, expired/future timestamp, malformed header", async () => {
  const db = database(), body = JSON.stringify(event("evt_sig"));
  const now = Math.floor(Date.now() / 1000);
  for (const header of [signature(body + " "), signature(body, now, "wrong"),
    signature(body, now - 60), signature(body, now + 60), "ts=abc;h1=invalid",
    `${signature(body)};ts=${now}`]) {
    assert.equal((await deliver(db, body, { headers: { "Paddle-Signature": header } })).status, 401);
  }
  assert.equal(count(db), 0);
});

test("raw whitespace/Unicode body and multiple rotation signatures verify", async () => {
  const db = database();
  const body = JSON.stringify(event("evt_raw", "transaction.completed", { data: { note: "兔子" } }), null, 2) + "\n";
  const header = signature(body).replace(";h1=", `;h1=${"0".repeat(64)};h1=`);
  assert.equal((await deliver(db, body, { headers: { "Paddle-Signature": header } })).status, 200);
  assert.equal(count(db), 1);
});

test("create, update plan/items/status, cancel without deletion, preserve absent email", async () => {
  const db = database();
  assert.deepEqual(await (await deliver(db, event("evt_create"))).json(), { ok: true });
  assert.equal(row(db).customer_email, null);
  assert.equal(row(db).price_id, "pri_test");
  const update = event("evt_update", "subscription.updated", {
    occurred_at: "2026-10-10T02:00:00Z",
    data: { id: "sub_test", status: "paused", customer_email: "test@example.invalid",
      items: [{ price: { id: "pri_upgrade", product_id: "pro_upgrade" } }] }
  });
  assert.equal((await deliver(db, update)).status, 200);
  assert.equal(row(db).price_id, "pri_upgrade");
  assert.equal(row(db).product_id, "pro_upgrade");
  assert.equal(row(db).subscription_status, "paused");
  assert.equal((await deliver(db, event("evt_cancel", "subscription.canceled", {
    occurred_at: "2026-10-10T03:00:00Z", data: { id: "sub_test", status: "canceled" }
  }))).status, 200);
  assert.equal(row(db).subscription_status, "canceled");
  assert.equal(row(db).price_id, "pri_upgrade");
  assert.equal(row(db).customer_email, "test@example.invalid");
  assert.equal(count(db), 3);
});

test("retries are acknowledged; batch guard covers concurrent duplicate lookup", async () => {
  const db = database();
  const value = event("evt_duplicate");
  await deliver(db, value);
  assert.equal((await deliver(db, value)).status, 200);
  db.skipLookup = true; // Simulate both deliveries having observed no event row.
  value.data.status = "paused";
  assert.equal((await deliver(db, value)).status, 200);
  assert.equal(row(db).subscription_status, "active");
  assert.equal(count(db), 1);
});

test("out-of-order events recorded but never overwrite newer subscription state", async () => {
  const db = database();
  await deliver(db, event("evt_latest", "subscription.canceled", {
    occurred_at: "2026-10-10T04:00:00Z", data: { id: "sub_test", status: "canceled" }
  }));
  await deliver(db, event("evt_old"));
  assert.equal(row(db).subscription_status, "canceled");
  assert.equal(count(db), 2);
});

test("transaction missing fields and unknown event recorded without subscription creation", async () => {
  const db = database();
  for (const type of ["transaction.completed", "customer.updated"]) {
    assert.equal((await deliver(db, event(`evt_${type}`, type, { data: {} }))).status, 200);
  }
  assert.equal(count(db), 2);
  assert.equal(row(db), undefined);
});

test("invalid subscription payload not recorded; explicit empty items clears price", async () => {
  const db = database();
  assert.equal((await deliver(db, event("evt_bad", "subscription.created", { data: {} }))).status, 400);
  assert.equal(count(db), 0);
  await deliver(db, event("evt_valid"));
  await deliver(db, event("evt_empty", "subscription.updated", {
    occurred_at: "2026-10-10T02:00:00Z", data: { id: "sub_test", status: "active", items: [] }
  }));
  assert.equal(row(db).price_id, null);
  assert.equal(row(db).product_id, null);
});

test("failed atomic batch rolls back subscription and receipt; retry can succeed", async () => {
  const db = database(); db.failBatch = true;
  assert.equal((await deliver(db, event("evt_retry"))).status, 500);
  assert.equal(row(db), undefined); assert.equal(count(db), 0);
  db.failBatch = false;
  assert.equal((await deliver(db, event("evt_retry"))).status, 200);
  assert.equal(count(db), 1);
});

test("only the Live POST route exists; removed routes cannot write to DB", async () => {
  const db = database();
  for (const path of ["/webhooks/paddle", "/webhooks/paddle/sandbox", "/webhooks/paddle/other"]) {
    assert.equal((await deliver(db, event("evt_removed"), { path })).status, 404);
  }
  assert.equal((await worker.fetch(new Request("https://api.crazyrabbit.dev/webhooks/paddle/live"), {})).status, 404);
  assert.equal(count(db), 0);
});

test("Live secret and DB binding are mandatory; signed Live request succeeds", async () => {
  const db = database();
  assert.equal((await deliver(db, event("evt_live"), { env: { PADDLE_LIVE_WEBHOOK_SECRET: undefined } })).status, 500);
  assert.equal((await deliver(db, event("evt_live"), { env: { DB: undefined } })).status, 500);
  assert.equal(count(db), 0);
  assert.deepEqual(await (await deliver(db, event("evt_live"))).json(), { ok: true });
  assert.equal(count(db), 1);
});
