import { test } from 'node:test';
import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFileSync } from 'node:fs';
import { createAuthHandler, digest, pruneAuth, resolveEntitlement, sendCode } from '../entitlements.js';
import worker from '../worker.js';
const NOW = 1800000000;
const iso = t => new Date(t * 1000).toISOString();
function fixture() {
  const sqlite = new DatabaseSync(':memory:');
  sqlite.exec(readFileSync(new URL('../migrations/0001_entitlements.sql', import.meta.url), 'utf8'));
  const db = { sqlite, fail: false, failBatch: false, prepare(sql) {
    return { sql, args: [], bind(...args) { this.args = args; return this; },
      async first() { if (db.fail) throw Error('DB'); return sqlite.prepare(sql).get(...this.args) ?? null; },
      async run() { if (db.fail) throw Error('DB'); return sqlite.prepare(sql).run(...this.args); } };
  }, async batch(statements) {
    sqlite.exec('BEGIN');
    try { for (const [i, s] of statements.entries()) { await s.run(); if (db.failBatch && i === 0) throw Error('batch failure'); } sqlite.exec('COMMIT'); }
    catch (e) { sqlite.exec('ROLLBACK'); throw e; }
  }};
  let clock = NOW, mail, failingMail = false, failingNetwork = false;
  const env = { DB: db, AUTH_ENABLED: 'true', AUTH_HASH_SECRET: 'x'.repeat(32), RESEND_API_KEY: 'mock-only',
    AUTH_MAIL_FROM: 'RABBITX <test@example.com>', PADDLE_API_KEY: 'mock-only', PRO_PRICE_IDS: 'pri_pro' };
  const data = {
    customers: [{ id: 'ctm_owner', status: 'active', email: 'owner@example.com' }],
    subscriptions: [{ id: 'sub_owner', customer_id: 'ctm_owner', status: 'active', items: [{ price: { id: 'pri_pro' }, quantity: 1 }],
      current_billing_period: { starts_at: iso(NOW - 100), ends_at: iso(NOW + 1000) } }],
    transactions: [{ id: 'txn_owner', customer_id: 'ctm_owner', subscription_id: 'sub_owner', status: 'completed',
      billing_period: { starts_at: iso(NOW - 100), ends_at: iso(NOW + 1000) }, items: [{ price: { id: 'pri_pro' }, quantity: 1 }],
      details: { line_items: [{ price_id: 'pri_pro', totals: { total: '2400' } }] }, payments: [{ status: 'captured', amount: '2400' }] }], adjustments: []
  };
  const calls = [];
  const fetcher = async (url, options) => {
    if (failingNetwork) throw Error('Offline');
    const u = new URL(url); calls.push(u); assert.equal(u.origin, 'https://api.paddle.com');
    assert.equal(options.headers.Authorization, 'Bearer mock-only');
    return Response.json({ data: data[u.pathname.slice(1)], meta: { pagination: { has_more: false, next: null } } });
  };
  const handler = createAuthHandler({ fetcher, clock: () => clock,
    mailer: async (_env, email, code, challenge) => { if (failingMail) throw Error('mail'); mail = { email, code, challenge }; } });
  const call = (path, body, token, extra = {}) => handler(new Request(`https://api.crazyrabbit.dev${path}`, {
    method: body === undefined ? 'GET' : 'POST', headers: { 'CF-Connecting-IP': '192.0.2.1', ...(token ? { Authorization: `Bearer ${token}` } : {}), ...extra },
    ...(body === undefined ? {} : { body: typeof body === 'string' ? body : JSON.stringify(body) }) }), env);
  const start = async (email = 'owner@example.com') => { const r = await call('/v1/auth/start', { email }); assert.equal(r.status, 200); return r.json(); };
  const finish = async () => call('/v1/auth/finish', { challenge_id: mail.challenge, code: mail.code });
  const login = async email => { await start(email); const r = await finish(); assert.equal(r.status, 200); return (await r.json()).token; };
  return { db, env, data, call, start, finish, login, fetcher, calls, mail: () => mail, time: t => clock = t,
    failMail: () => failingMail = true, failNetwork: () => failingNetwork = true };
}
test('migration additive/repeatable preserves webhook data', () => {
  const db = new DatabaseSync(':memory:');
  db.exec("CREATE TABLE subscriptions(id TEXT); INSERT INTO subscriptions VALUES('existing'); CREATE TABLE webhook_events(id TEXT); INSERT INTO webhook_events VALUES('existing');");
  const sql = readFileSync(new URL('../migrations/0001_entitlements.sql', import.meta.url), 'utf8'); db.exec(sql); db.exec(sql);
  assert.equal(db.prepare('SELECT COUNT(*) n FROM subscriptions').get().n, 1); assert.equal(db.prepare('SELECT COUNT(*) n FROM webhook_events').get().n, 1);
});
test('service disabled/missing config, HTTPS, CORS, invalid/large input and missing token', async () => {
  assert.equal((await worker.fetch(new Request('https://api.crazyrabbit.dev/v1/entitlement'), {})).status, 503);
  const f = fixture();
  assert.equal((await f.call('/v1/entitlement', undefined, undefined, { Origin: 'https://evil.example' })).status, 403);
  assert.equal((await createAuthHandler()(new Request('http://api.crazyrabbit.dev/v1/session'), f.env)).status, 400);
  assert.equal((await f.call('/v1/auth/start', { email: 'bad' })).status, 400);
  assert.equal((await f.call('/v1/auth/start', 'x'.repeat(5000))).status, 400);
  assert.equal((await f.call('/v1/entitlement')).status, 401); assert.equal((await f.call('/v1/unknown')).status, 404);
  delete f.env.PADDLE_API_KEY; assert.equal((await f.call('/v1/entitlement')).status, 503);
});
test('verified mailbox sign-in stores only credential digests and API-backed snapshot', async () => {
  const f = fixture(), token = await f.login(), session = f.db.sqlite.prepare('SELECT * FROM auth_sessions').get();
  assert.equal(session.token_hash, await digest(token)); assert.notEqual(session.token_hash, token);
  assert.notEqual(f.db.sqlite.prepare('SELECT code_hash FROM auth_challenges').get().code_hash, f.mail().code);
  const r = await f.call('/v1/entitlement', undefined, token);
  assert.equal(r.headers.get('X-RABBITX-Tier'), 'Pro'); assert.equal(r.headers.get('Access-Control-Allow-Origin'), null);
  assert.equal(f.db.sqlite.prepare('SELECT source FROM entitlement_snapshots').get().source, 'paddle_live_api');
});
test('wrong OTP, five guesses, expiry, replay and concurrent redemption', async () => {
  const f = fixture(); await f.start(); const input = { challenge_id: f.mail().challenge, code: f.mail().code === '00000000' ? '00000001' : '00000000' };
  for (let i = 0; i < 5; i++) assert.equal((await f.call('/v1/auth/finish', input)).status, 401);
  assert.equal((await f.finish()).status, 401);
  const expired = fixture(); await expired.start(); expired.time(NOW + 601); assert.equal((await expired.finish()).status, 401);
  const race = fixture(); await race.start(); const results = await Promise.all([race.finish(), race.finish()]);
  assert.deepEqual(results.map(r => r.status).sort(), [200, 401]); assert.equal((await race.finish()).status, 401);
});
test('forged email or Paddle IDs cannot claim another customer; no subscription Free', async () => {
  const f = fixture(); assert.equal((await f.call('/v1/auth/start', { email: 'owner@example.com', subscription_id: 'sub_owner' })).status, 400);
  const token = await f.login('stranger@example.com'); assert.equal((await (await f.call('/v1/entitlement', undefined, token)).json()).tier, 'Free');
  assert.equal((await f.call('/v1/features/pro-foundation/check', { subscription_id: 'sub_owner', status: 'active' }, token)).status, 400);
  assert.equal((await f.call('/v1/entitlement?customer_id=ctm_owner', undefined, token)).status, 400);
});
test('canceled retains paid period; expired, paused, past_due and trialing deny', async () => {
  const f = fixture(), token = await f.login(); f.data.subscriptions[0].status = 'canceled'; f.data.subscriptions[0].current_billing_period = null;
  assert.equal((await (await f.call('/v1/entitlement', undefined, token)).json()).tier, 'Pro'); f.time(NOW + 1000);
  assert.equal((await (await f.call('/v1/entitlement', undefined, token)).json()).tier, 'Free'); f.time(NOW);
  for (const status of ['paused', 'past_due', 'trialing']) { f.data.subscriptions[0].status = status; assert.equal((await (await f.call('/v1/entitlement', undefined, token)).json()).tier, 'Free'); }
});
test('refund/chargeback/$0/wrong price/customer/subscription/missing proof deny', async () => {
  for (const mutate of [
    f => f.data.adjustments.push({ transaction_id: 'txn_owner', action: 'refund', status: 'approved' }),
    f => f.data.adjustments.push({ transaction_id: 'txn_owner', action: 'chargeback', status: 'approved' }),
    f => f.data.transactions[0].payments[0].amount = '0', f => f.data.transactions[0].details.line_items[0].totals.total = '0',
    f => f.data.transactions[0].customer_id = 'ctm_other', f => f.data.transactions[0].subscription_id = 'sub_other',
    f => f.data.transactions[0].items[0].price.id = 'pri_other', f => f.data.subscriptions[0].customer_id = 'ctm_other',
    f => f.data.transactions[0].billing_period = null, f => f.data.subscriptions[0].current_billing_period.ends_at = iso(NOW)
  ]) { const f = fixture(); mutate(f); assert.equal((await resolveEntitlement('owner@example.com', f.env, f.fetcher, NOW)).tier, 'Free'); }
});
test('simulation webhook rows never produce entitlements even with plausible IDs', async () => {
  const f = fixture(); f.db.sqlite.exec("CREATE TABLE subscriptions(id TEXT,status TEXT); INSERT INTO subscriptions VALUES('sub_owner','active');"); f.data.customers = [];
  const token = await f.login(); assert.equal((await f.call('/v1/features/pro-foundation/check', '', token)).status, 403);
});
test('simulated canceled receipts cannot revoke a canonically active paid customer', async () => {
  const f = fixture(); f.db.sqlite.exec("CREATE TABLE subscriptions(id TEXT,status TEXT); INSERT INTO subscriptions VALUES('sub_owner','canceled');");
  const token = await f.login(); assert.equal((await f.call('/v1/features/pro-foundation/check', '', token)).status, 200);
});
test('session exchange rolls back a failed batch and the legitimate code can be retried', async () => {
  const f = fixture(); await f.start(); f.db.failBatch = true;
  assert.equal((await f.finish()).status, 503);
  assert.equal(f.db.sqlite.prepare('SELECT consumed_hash FROM auth_challenges').get().consumed_hash, null);
  assert.equal(f.db.sqlite.prepare('SELECT COUNT(*) n FROM auth_sessions').get().n, 0);
  f.db.failBatch = false; assert.equal((await f.finish()).status, 200);
});
test('initial checkout without billing_period uses canonical first cycle, never guesses an expiry', async () => {
  const f = fixture(), sub = f.data.subscriptions[0], txn = f.data.transactions[0];
  txn.billing_period = null; txn.origin = 'web'; sub.first_billed_at = iso(NOW - 100);
  txn.payments[0].captured_at = iso(NOW - 100);
  assert.equal((await resolveEntitlement('owner@example.com', f.env, f.fetcher, NOW)).tier, 'Pro');
  sub.status = 'canceled'; sub.current_billing_period = null;
  assert.equal((await resolveEntitlement('owner@example.com', f.env, f.fetcher, NOW)).tier, 'Pro');
  f.data.adjustments.push({ transaction_id: txn.id, status: 'approved', action: 'refund' });
  assert.equal((await resolveEntitlement('owner@example.com', f.env, f.fetcher, NOW)).tier, 'Free');
  const g = fixture(); g.data.transactions[0].billing_period = null; g.data.transactions[0].origin = 'subscription_charge';
  assert.equal((await resolveEntitlement('owner@example.com', g.env, g.fetcher, NOW)).tier, 'Free');
});
test('rejected refunds/reversed disputes permit paid access; unrelated refunds do not revoke', async () => {
  const f = fixture(); f.data.adjustments = [
    { transaction_id: 'txn_owner', action: 'refund', status: 'rejected' },
    { transaction_id: 'txn_owner', action: 'chargeback', status: 'reversed' },
    { transaction_id: 'txn_other', action: 'refund', status: 'approved' }
  ];
  assert.equal((await resolveEntitlement('owner@example.com', f.env, f.fetcher, NOW)).tier, 'Pro');
});
test('network/database failure never falls back to prior Pro snapshot', async () => {
  const f = fixture(), token = await f.login(); assert.equal((await f.call('/v1/features/pro-foundation/check', '', token)).status, 200);
  f.failNetwork(); assert.equal((await f.call('/v1/features/pro-foundation/check', '', token)).status, 503);
  f.db.fail = true; assert.equal((await f.call('/v1/entitlement', undefined, token)).status, 503);
});
test('session expiry, server logout, modified token and repeated online checks', async () => {
  const f = fixture(), token = await f.login(); for (let i = 0; i < 3; i++) assert.equal((await f.call('/v1/features/pro-foundation/check', '', token)).status, 200);
  assert.equal(f.calls.filter(u => u.pathname === '/subscriptions').length, 3);
  assert.equal((await f.call('/v1/entitlement', undefined, 'a'.repeat(64))).status, 401);
  assert.equal((await f.call('/v1/auth/logout', '', token)).status, 200); assert.equal((await f.call('/v1/entitlement', undefined, token)).status, 401);
  const g = fixture(), expired = await g.login(); g.time(NOW + 86400); assert.equal((await g.call('/v1/entitlement', undefined, expired)).status, 401);
  await pruneAuth(g.db, NOW + 86400); assert.equal(g.db.sqlite.prepare('SELECT count(*) n FROM auth_sessions').get().n, 0);
});
test('mail failure removes challenge, email/check limits bound abuse', async () => {
  const f = fixture(); f.failMail(); assert.equal((await f.call('/v1/auth/start', { email: 'owner@example.com' })).status, 503);
  assert.equal(f.db.sqlite.prepare('SELECT count(*) n FROM auth_challenges').get().n, 0);
  const g = fixture(); for (let i = 0; i < 3; i++) await g.start(); assert.equal((await g.call('/v1/auth/start', { email: 'owner@example.com' })).status, 429);
  const h = fixture(), token = await h.login(); for (let i = 0; i < 60; i++) assert.equal((await h.call('/v1/entitlement', undefined, token)).status, 200);
  assert.equal((await h.call('/v1/entitlement', undefined, token)).status, 429);
});
test('pagination preserves filters and rejects credential-leaking URL/API failure', async () => {
  const f = fixture(); let n = 0;
  const pages = async url => { n++; assert.equal(new URL(url).searchParams.get('email'), 'owner@example.com'); return Response.json({ data: [], meta: { pagination: { has_more: n === 1, next: 'https://api.paddle.com/customers?after=ctm_next' } } }); };
  assert.equal((await resolveEntitlement('owner@example.com', f.env, pages, NOW)).tier, 'Free'); assert.equal(n, 2);
  await assert.rejects(resolveEntitlement('owner@example.com', f.env, async () => Response.json({ data: [], meta: { pagination: { has_more: true, next: 'https://evil.example/steal' } } }), NOW));
  await assert.rejects(resolveEntitlement('owner@example.com', f.env, async () => new Response('', { status: 401 }), NOW));
});
test('replaceable Resend adapter sends OTP privately, without billing credentials', async () => {
  const f = fixture(); await sendCode(f.env, 'owner@example.com', '12345678', 'challenge', async (url, r) => {
    assert.equal(url, 'https://api.resend.com/emails'); assert.match(JSON.parse(r.body).text, /12345678/); assert.ok(!r.body.includes('PADDLE')); return new Response('', { status: 200 }); });
  await assert.rejects(sendCode(f.env, 'owner@example.com', '12345678', 'challenge', async () => new Response('', { status: 500 })));
});
