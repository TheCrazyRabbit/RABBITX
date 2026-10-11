import { configurationIssues } from './configuration.js';
import { acceptanceEntitlement } from './live-acceptance.js';
// No webhook payload or caller-supplied Paddle ID is an entitlement authority.
const enc = new TextEncoder();
const nowSeconds = () => Math.floor(Date.now() / 1000);
const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('');
export const digest = async value => hex(new Uint8Array(await crypto.subtle.digest('SHA-256', enc.encode(value))));
const random = () => hex(crypto.getRandomValues(new Uint8Array(32)));
const emailOf = value => typeof value === 'string' ? value.trim().toLowerCase() : '';
const validEmail = value => value.length <= 254 && /^[a-z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-z0-9.-]+\.[a-z]{2,}$/i.test(value);
const response = (status, body, headers = {}) => Response.json(body, {
  status, headers: { 'Cache-Control': 'no-store', 'Pragma': 'no-cache', ...headers }
});
async function keyed(secret, value) {
  const key = await crypto.subtle.importKey('raw', enc.encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return hex(new Uint8Array(await crypto.subtle.sign('HMAC', key, enc.encode(value))));
}
async function codeMatches(secret, challenge, code, expected) {
  const key = await crypto.subtle.importKey('raw', enc.encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['verify']);
  return crypto.subtle.verify('HMAC', key, Uint8Array.from(expected.match(/../g), h => parseInt(h, 16)), enc.encode(`${challenge}:${code}`));
}
function otp() {
  let value;
  do { value = crypto.getRandomValues(new Uint32Array(1))[0]; } while (value >= 4200000000);
  return String(value % 100000000).padStart(8, '0');
}
async function limited(db, bucket, max, now) {
  const row = await db.prepare(`INSERT INTO auth_limits(bucket,hits,expires_at) VALUES (?,1,?)
    ON CONFLICT(bucket) DO UPDATE SET hits=CASE WHEN expires_at<=? THEN 1 ELSE hits+1 END,
    expires_at=CASE WHEN expires_at<=? THEN excluded.expires_at ELSE expires_at END RETURNING hits`)
    .bind(bucket, now + 900, now, now).first();
  return row.hits > max;
}
async function jsonBody(request) {
  // Bound actual bytes, including chunked requests without Content-Length.
  const reader = request.body?.getReader();
  if (!reader) throw new Error('input');
  const chunks = []; let size = 0;
  try {
    while (true) {
      const { value, done } = await reader.read(); if (done) break;
      size += value.length;
      if (size > 4096) { await reader.cancel(); throw new Error('input'); }
      chunks.push(value);
    }
  } finally { reader.releaseLock(); }
  const bytes = new Uint8Array(size); let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
  const value = JSON.parse(new TextDecoder().decode(bytes));
  if (!value || Array.isArray(value) || typeof value !== 'object') throw new Error('input');
  return value;
}
async function hasBody(request) {
  const reader = request.body?.getReader(); if (!reader) return false;
  try {
    while (true) {
      const { value, done } = await reader.read(); if (done) return false;
      if (value.length) { await reader.cancel(); return true; }
    }
  } finally { reader.releaseLock(); }
}

// Replaceable adapter. No test-mail endpoint or code is exposed in production.
export async function sendCode(env, email, code, challenge, fetcher = fetch) {
  const result = await fetcher('https://api.resend.com/emails', {
    method: 'POST', redirect: 'error', signal: AbortSignal.timeout(8000),
    headers: { Authorization: `Bearer ${env.RESEND_API_KEY}`, 'Content-Type': 'application/json', 'Idempotency-Key': challenge },
    body: JSON.stringify({ from: env.AUTH_MAIL_FROM, to: [email], subject: 'Your RABBITX sign-in code',
      text: `Your RABBITX sign-in code is ${code}. It expires in 10 minutes. Do not share it. If you did not request this, ignore this email.` })
  });
  if (!result.ok) throw new Error('mail unavailable');
}

async function listPaddle(path, query, env, fetcher) {
  let url = new URL(`https://api.paddle.com/${path}`);
  for (const [key, value] of Object.entries(query)) url.searchParams.set(key, value);
  url.searchParams.set('per_page', '50');
  const output = [];
  for (let page = 0; page < 10; page++) {
    const result = await fetcher(url.href, { headers: { Authorization: `Bearer ${env.PADDLE_API_KEY}`, 'Paddle-Version': '1' },
      redirect: 'error', signal: AbortSignal.timeout(8000) });
    if (!result.ok) throw new Error('billing unavailable');
    const body = await result.json();
    if (!Array.isArray(body.data) || !body.meta?.pagination) throw new Error('billing schema');
    output.push(...body.data);
    if (!body.meta.pagination.has_more) return output;
    const next = new URL(body.meta.pagination.next);
    if (next.origin !== 'https://api.paddle.com' || next.pathname !== `/${path}`) throw new Error('pagination');
    // Only accept the cursor; preserve filters to prevent unrelated pages granting access.
    const after = next.searchParams.get('after'); if (!after) throw new Error('pagination');
    url.searchParams.set('after', after);
  }
  throw new Error('billing pagination limit');
}
const epoch = value => typeof value === 'string' && Number.isFinite(Date.parse(value)) ? Math.floor(Date.parse(value) / 1000) : 0;
const approvedPrice = (items, prices) => Array.isArray(items) && items.some(i => prices.has(i.price?.id) && i.quantity > 0);

export async function resolveEntitlement(email, env, fetcher = fetch, now = nowSeconds()) {
  const remote = fetcher, deadline = AbortSignal.timeout(15000); let requests = 0;
  fetcher = (url, options) => {
    if (++requests > 40 || deadline.aborted) throw new Error('billing budget');
    return remote(url, { ...options, signal: AbortSignal.any([options.signal, deadline]) });
  };
  const free = { tier: 'Free', reason: 'no_paid_subscription', valid_until: null, subscription_id: null, customer_id: null };
  const prices = new Set(env.PRO_PRICE_IDS.split(',').map(s => s.trim()));
  const customers = await listPaddle('customers', { email }, env, fetcher);
  let best = free;
  for (const customer of customers) {
    if (emailOf(customer.email) !== email || customer.status !== 'active') continue;
    const subs = await listPaddle('subscriptions', { customer_id: customer.id }, env, fetcher);
    for (const sub of subs) {
      if (sub.customer_id !== customer.id || !approvedPrice(sub.items, prices)) continue;
      if (!['active', 'canceled'].includes(sub.status)) {
        if (best.tier !== 'Pro') best = { ...free, reason: sub.status }; continue;
      }
      const transactions = await listPaddle('transactions', { subscription_id: sub.id, status: 'completed' }, env, fetcher);
      const adjustments = await listPaddle('adjustments', { subscription_id: sub.id }, env, fetcher);
      for (const transaction of transactions) {
        if (transaction.status !== 'completed' || transaction.subscription_id !== sub.id ||
            transaction.customer_id !== customer.id || !approvedPrice(transaction.items, prices)) continue;
        // Require a nonzero, actually captured Pro line, not merely a completed $0 invoice.
        const paidLine = transaction.details?.line_items?.some(line => prices.has(line.price_id) && /^\d+$/.test(line.totals?.total) && BigInt(line.totals.total) > 0n);
        const captured = transaction.payments?.some(p => p.status === 'captured' && /^\d+$/.test(p.amount) && BigInt(p.amount) > 0n);
        // Conservative: any approved refund/credit or unresolved dispute on this paid period denies it.
        if (adjustments.some(a => a.transaction_id === transaction.id &&
          (a.status === 'approved' && ['refund', 'credit'].includes(a.action) ||
           ['chargeback', 'chargeback_warning'].includes(a.action) && a.status !== 'reversed'))) continue;
        if (!paidLine || !captured) {
          const acceptance = await acceptanceEntitlement(email, customer, sub, transaction, env, fetcher, now);
          if (acceptance && best.tier !== 'Pro') best = acceptance;
          continue;
        }
        let paidEnd = epoch(transaction.billing_period?.ends_at);
        let paidStart = epoch(transaction.billing_period?.starts_at);
        if (!paidStart || !paidEnd) {
          const start = epoch(sub.current_billing_period?.starts_at);
          const first = epoch(sub.first_billed_at);
          const initialPayment = ['web', 'api'].includes(transaction.origin) && first > 0 && first === start &&
            transaction.payments.some(p => p.status === 'captured' && epoch(p.captured_at) > 0 && Math.abs(epoch(p.captured_at) - first) <= 300);
          if (initialPayment && epoch(sub.current_billing_period?.ends_at) > start) {
            paidStart = start; paidEnd = epoch(sub.current_billing_period.ends_at);
            await env.DB.prepare(`INSERT INTO verified_paid_periods(transaction_id,subscription_id,customer_id,starts_at,ends_at,verified_at)
              VALUES (?,?,?,?,?,?) ON CONFLICT(transaction_id) DO NOTHING`)
              .bind(transaction.id, sub.id, customer.id, paidStart, paidEnd, now).run();
          } else {
            const prior = await env.DB.prepare('SELECT starts_at,ends_at FROM verified_paid_periods WHERE transaction_id=? AND subscription_id=? AND customer_id=?')
              .bind(transaction.id, sub.id, customer.id).first();
            paidStart = prior?.starts_at || 0; paidEnd = prior?.ends_at || 0;
          }
        }
        const end = sub.status === 'active' ? Math.min(paidEnd, epoch(sub.current_billing_period?.ends_at)) : paidEnd;
        if (!paidStart || paidStart > now || end <= now) { if (best.tier !== 'Pro') best = { ...free, reason: 'expired' }; continue; }
        if (best.authorization_mode === 'acceptance' || end > (best.valid_until || 0)) best = { tier: 'Pro', reason: 'verified_paid_period', authorization_mode: 'paid', valid_until: end,
          subscription_id: sub.id, customer_id: customer.id };
      }
    }
  }
  return best;
}

export function createAuthHandler({ fetcher = fetch, mailer = sendCode, clock = nowSeconds } = {}) {
  return async function handleAuth(request, env) {
    const url = new URL(request.url), path = url.pathname;
    if (!path.startsWith('/v1/')) return null;
    if (url.protocol !== 'https:') return response(400, { error: 'HTTPS required' });
    // Native CLI only: browsers cannot invoke sensitive routes cross-origin.
    if (request.headers.has('Origin')) return response(403, { error: 'Browser origin not allowed' });
    if (env.AUTH_ENABLED !== 'true' || configurationIssues(env).length) {
      return response(503, { error: 'Account service is not enabled' });
    }
    const allowed = { '/v1/auth/start': 'POST', '/v1/auth/finish': 'POST', '/v1/session': 'GET',
      '/v1/entitlement': 'GET', '/v1/features/pro-foundation/check': 'POST', '/v1/auth/logout': 'POST' };
    if (allowed[path] !== request.method) return response(404, { error: 'Not found' });
    const db = env.DB, now = clock();
    const permittedEmail = async email => !env.AUTH_TEST_EMAIL_SHA256 || await digest(email) === env.AUTH_TEST_EMAIL_SHA256;
    try {
      const ip = request.headers.get('CF-Connecting-IP') || 'unknown';
      const ipBucket = await keyed(env.AUTH_HASH_SECRET, `ip:${ip}`);
      if (await limited(db, ipBucket, 100, now)) return response(429, { error: 'Try again later' });
      if (path === '/v1/auth/start') {
        let input; try { input = await jsonBody(request); } catch { return response(400, { error: 'Invalid request' }); }
        if (Object.keys(input).some(k => k !== 'email')) return response(400, { error: 'Invalid request' });
        const email = emailOf(input.email);
        if (!validEmail(email)) return response(400, { error: 'Enter a valid email' });
        const emailBucket = await keyed(env.AUTH_HASH_SECRET, `email:${email}`);
        const startBucket = await keyed(env.AUTH_HASH_SECRET, `start:${ip}`);
        if (await limited(db, emailBucket, 3, now) || await limited(db, startBucket, 10, now)) return response(429, { error: 'Try again later' });
        const challenge = random(), code = otp(), hash = await keyed(env.AUTH_HASH_SECRET, `${challenge}:${code}`);
        // Restricted rollout: same response shape for other mailboxes, no mail or redeemable challenge.
        if (!await permittedEmail(email)) return response(200, { challenge_id: challenge, message: 'Check your email for a sign-in code' }, { 'X-RABBITX-Challenge': challenge });
        await db.prepare('INSERT INTO auth_challenges(challenge_id,email,code_hash,expires_at) VALUES (?,?,?,?)')
          .bind(challenge, email, hash, now + 600).run();
        try { await mailer(env, email, code, challenge, fetcher); }
        catch { await db.prepare('DELETE FROM auth_challenges WHERE challenge_id=?').bind(challenge).run(); throw new Error('mail'); }
        return response(200, { challenge_id: challenge, message: 'Check your email for a sign-in code' }, { 'X-RABBITX-Challenge': challenge });
      }
      if (path === '/v1/auth/finish') {
        let input; try { input = await jsonBody(request); } catch { return response(400, { error: 'Invalid request' }); }
        if (Object.keys(input).some(k => !['challenge_id', 'code'].includes(k)) ||
            !/^[a-f0-9]{64}$/.test(input.challenge_id || '') || !/^\d{8}$/.test(input.code || '')) return response(400, { error: 'Invalid request' });
        // Atomically claim an attempt; five guesses maximum even under concurrent requests.
        const challenge = await db.prepare(`UPDATE auth_challenges SET attempts=attempts+1
          WHERE challenge_id=? AND expires_at>? AND consumed_hash IS NULL AND attempts<5 RETURNING *`)
          .bind(input.challenge_id, now).first();
        if (!challenge || !await permittedEmail(challenge.email) || !await codeMatches(env.AUTH_HASH_SECRET, input.challenge_id, input.code, challenge.code_hash)) return response(401, { error: 'Code invalid or expired' });
        const token = random(), hash = await digest(token);
        await db.batch([
          db.prepare('UPDATE auth_challenges SET consumed_hash=? WHERE challenge_id=? AND consumed_hash IS NULL AND expires_at>?')
            .bind(hash, input.challenge_id, now),
          db.prepare(`INSERT INTO auth_sessions(token_hash,email,expires_at,created_at)
            SELECT ?,email,?,? FROM auth_challenges WHERE challenge_id=? AND consumed_hash=?`)
            .bind(hash, now + 86400, now, input.challenge_id, hash)
        ]);
        if (!await db.prepare('SELECT token_hash FROM auth_sessions WHERE token_hash=?').bind(hash).first()) return response(401, { error: 'Code already used' });
        return response(200, { token, expires_at: now + 86400 }, { 'X-RABBITX-Session': token });
      }
      const token = request.headers.get('Authorization')?.match(/^Bearer ([a-f0-9]{64})$/)?.[1];
      if (!token) return response(401, { error: 'Sign in required' });
      const hash = await digest(token);
      const session = await db.prepare('SELECT email FROM auth_sessions WHERE token_hash=? AND expires_at>? AND revoked_at IS NULL').bind(hash, now).first();
      if (!session) return response(401, { error: 'Session expired or revoked; sign in again' });
      if (path === '/v1/auth/logout') {
        await db.prepare('UPDATE auth_sessions SET revoked_at=? WHERE token_hash=?').bind(now, hash).run();
        return response(200, { ok: true });
      }
      if (!await permittedEmail(session.email)) return response(401, { error: 'Session expired or revoked; sign in again' });
      if (url.search || request.method === 'POST' && await hasBody(request)) return response(400, { error: 'Caller-supplied subscription data is not accepted' });
      if (path === '/v1/session') return response(200, { signed_in: true });
      if (await limited(db, `check:${hash}`, 60, now)) return response(429, { error: 'Try again later' });
      // Every gate checks canonical Live data anew: no stale offline grants or webhook simulation promotion.
      const entitlement = await resolveEntitlement(session.email, env, fetcher, now);
      await db.prepare(`INSERT INTO entitlement_snapshots(email,tier,reason,subscription_id,customer_id,valid_until,verified_at,source)
        VALUES (?,?,?,?,?,?,?,'paddle_live_api') ON CONFLICT(email) DO UPDATE SET tier=excluded.tier,reason=excluded.reason,
        subscription_id=excluded.subscription_id,customer_id=excluded.customer_id,valid_until=excluded.valid_until,verified_at=excluded.verified_at`)
        .bind(session.email, entitlement.tier, entitlement.reason, entitlement.subscription_id, entitlement.customer_id, entitlement.valid_until, now).run();
      const gate = path === '/v1/features/pro-foundation/check';
      return response(gate && entitlement.tier !== 'Pro' ? 403 : 200,
        { tier: entitlement.tier, reason: entitlement.reason, valid_until: entitlement.valid_until,
          authorization_mode: entitlement.authorization_mode || 'none',
          ...(gate ? { allowed: entitlement.tier === 'Pro', feature: 'pro-foundation', commercial_feature: false } : {}) },
        { 'X-RABBITX-Tier': entitlement.tier, 'X-RABBITX-Valid-Until': String(entitlement.valid_until || 0),
          'X-RABBITX-Authorization-Mode': entitlement.authorization_mode || 'none' });
    } catch {
      console.error('Account service unavailable');
      return response(503, { error: 'Account service unavailable; Pro access cannot be verified' });
    }
  };
}
export const handleAuth = createAuthHandler();

export async function pruneAuth(db, now = nowSeconds()) {
  await db.batch([
    db.prepare('DELETE FROM auth_challenges WHERE expires_at<=?').bind(now),
    db.prepare('DELETE FROM auth_sessions WHERE expires_at<=? OR revoked_at IS NOT NULL').bind(now),
    db.prepare('DELETE FROM auth_limits WHERE expires_at<=?').bind(now)
  ]);
}
