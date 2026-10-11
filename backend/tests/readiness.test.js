import { test } from 'node:test';
import assert from 'node:assert/strict';
import { checkReadiness } from '../tools/check-readiness.js';
const env = () => ({ DB: {}, AUTH_ENABLED: 'false', LIVE_ACCEPTANCE_ENABLED: 'false', AUTH_HASH_SECRET: 'x'.repeat(32),
  PADDLE_API_KEY: `pdl_live_apikey_${'a'.repeat(26)}_${'b'.repeat(22)}_ccc`, RESEND_API_KEY: 're_mock_only',
  AUTH_MAIL_FROM: 'Crazy Rabbit <auth@auth.example.com>', PRO_PRICE_IDS: 'pri_pro' });

test('readiness defaults to offline, reports absent config without secret values', async () => {
  const f = env(); let calls = 0;
  const report = await checkReadiness(f, { fetcher: () => { calls++; throw Error('unexpected'); } });
  assert.equal(calls, 0); assert.equal(report.auth_enabled, false); assert.equal(report.acceptance_enabled, false);
  assert.equal(report.resend_delivery_verified, false); assert.deepEqual(report.configuration_issues, []);
  assert.ok(!JSON.stringify(report).includes(f.PADDLE_API_KEY)); assert.ok(!JSON.stringify(report).includes(f.RESEND_API_KEY));
  assert.ok((await checkReadiness({})).configuration_issues.length);
});

test('explicit Paddle readiness calls only Live GETs with version and no customer payload output', async () => {
  const f = env(), calls = [];
  const report = await checkReadiness(f, { paddleRead: true, fetcher: async (url, req) => {
    calls.push(url); assert.equal(new URL(url).origin, 'https://api.paddle.com'); assert.equal(req.method, 'GET');
    assert.equal(req.redirect, 'error'); assert.equal(req.headers.Authorization, `Bearer ${f.PADDLE_API_KEY}`);
    assert.equal(req.headers['Paddle-Version'], '1'); assert.equal(req.body, undefined);
    return Response.json({ data: [{ email: 'private@example.com', id: 'private_customer' }] });
  } });
  assert.equal(calls.length, 5); assert.ok(report.paddle_read_checks.every(c => c.ok));
  assert.deepEqual(report.paddle_read_checks.map(c => c.permission), ['customer.read', 'subscription.read', 'transaction.read', 'adjustment.read', 'discount.read']);
  assert.ok(!JSON.stringify(report).includes('private')); assert.equal(report.live_transaction_verified, false);
});

test('readiness rejects browser credentials and reports revoked/missing permission/network/schema failures', async () => {
  let calls = 0;
  await checkReadiness({ ...env(), PADDLE_API_KEY: 'live_browser' }, { paddleRead: true, fetcher: () => calls++ });
  assert.equal(calls, 0);
  for (const fetcher of [async () => new Response('', { status: 403 }), async () => { throw Error('do-not-echo'); }, async () => Response.json({ data: null })]) {
    const report = await checkReadiness(env(), { paddleRead: true, fetcher });
    assert.ok(report.paddle_read_checks.every(c => !c.ok)); assert.ok(!JSON.stringify(report).includes('do-not-echo'));
  }
});
