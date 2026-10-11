// Restricted exception for owner-confirmed Live onboarding acceptance, never a
// general zero-payment entitlement or a subscription trial. Defaults OFF.
const epoch = value => typeof value === 'string' && Number.isFinite(Date.parse(value)) ? Math.floor(Date.parse(value) / 1000) : 0;
const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('');
const emailHash = async email => hex(new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(email))));
export async function acceptanceEntitlement(email, customer, sub, txn, env, fetcher, now) {
  if (env.LIVE_ACCEPTANCE_ENABLED !== 'true' || sub.status !== 'active' || txn.status !== 'completed') return null;
  if (await emailHash(email) !== env.AUTH_TEST_EMAIL_SHA256) return null;
  const grant = await env.DB.prepare(`SELECT * FROM live_acceptance_grants
    WHERE email_hash=? AND customer_id=? AND subscription_id=? AND transaction_id=?
      AND approved_at<=? AND expires_at>? AND revoked_at IS NULL AND purpose='paddle_live_acceptance'`)
    .bind(await emailHash(email), customer.id, sub.id, txn.id, now, now).first();
  if (!grant || grant.expires_at > grant.approved_at + 7200 || !grant.approved_by?.trim()) return null;
  if (txn.customer_id !== customer.id || txn.subscription_id !== sub.id || txn.discount_id !== grant.discount_id ||
      txn.currency_code !== 'USD' || txn.details?.totals?.grand_total !== '0') return null;
  const billed = epoch(txn.billed_at);
  if (!billed || billed > grant.approved_at || billed < grant.approved_at - 86400) return null;
  // Exactly one $24 monthly Pro line, fully discounted. No add-ons or paid attempts.
  const item = txn.items?.[0], line = txn.details?.line_items?.[0];
  if (txn.items?.length !== 1 || item.quantity !== 1 || item.price?.id !== grant.price_id ||
      item.price?.unit_price?.amount !== '2400' || item.price?.unit_price?.currency_code !== 'USD' ||
      item.price?.billing_cycle?.interval !== 'month' || item.price?.billing_cycle?.frequency !== 1 ||
      txn.details?.line_items?.length !== 1 || line.price_id !== grant.price_id || line.quantity !== 1 ||
      line.totals?.subtotal !== '2400' || line.totals?.discount !== '2400' || line.totals?.tax !== '0' || line.totals?.total !== '0' ||
      !Array.isArray(txn.payments) || txn.payments.some(p => !/^0+$/.test(p.amount || ''))) return null;
  if (!sub.items?.some(i => i.price?.id === grant.price_id && i.quantity === 1)) return null;
  const result = await fetcher(`https://api.paddle.com/discounts/${encodeURIComponent(grant.discount_id)}`, {
    method: 'GET', redirect: 'error', signal: AbortSignal.timeout(8000),
    headers: { Authorization: `Bearer ${env.PADDLE_API_KEY}`, 'Paddle-Version': '1' }
  });
  if (!result.ok) throw new Error('acceptance billing proof unavailable');
  const discount = (await result.json()).data;
  const created = epoch(discount?.created_at), expires = epoch(discount?.expires_at);
  if (!discount || discount.id !== grant.discount_id || !['active', 'archived'].includes(discount.status) ||
      discount.type !== 'percentage' || Number(discount.amount) !== 100 || discount.recur !== false ||
      discount.usage_limit !== 1 || discount.times_used !== 1 ||
      discount.restrict_to?.length !== 1 || discount.restrict_to[0] !== grant.price_id ||
      !created || !expires || expires <= created || expires > created + 86400 || billed < created || billed >= expires) return null;
  const start = epoch(sub.current_billing_period?.starts_at);
  const end = Math.min(grant.expires_at, epoch(sub.current_billing_period?.ends_at));
  if (!start || start > now || end <= now || billed < start - 300 || billed > start + 300) return null;
  return { tier: 'Pro', reason: 'live_acceptance_only', authorization_mode: 'acceptance',
    valid_until: end, subscription_id: sub.id, customer_id: customer.id };
}
