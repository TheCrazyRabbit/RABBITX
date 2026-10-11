// Values are deliberately never returned; safe for local/operator diagnostics.
export function configurationIssues(env) {
  const issues = [];
  if (!env.DB) issues.push('DB binding missing');
  if (typeof env.AUTH_HASH_SECRET !== 'string' || env.AUTH_HASH_SECRET.length < 32 || /PASTE|REPLACE/.test(env.AUTH_HASH_SECRET)) issues.push('AUTH_HASH_SECRET missing or invalid');
  // Client-side live_ tokens and Sandbox API keys are never server credentials.
  if (!/^pdl_live_apikey_[a-z0-9]{26}_[A-Za-z0-9]{22}_[A-Za-z0-9]{3}$/.test(env.PADDLE_API_KEY || '')) issues.push('PADDLE_API_KEY must be a scoped Live server API key');
  if (!/^re_[A-Za-z0-9_-]+$/.test(env.RESEND_API_KEY || '') || /PASTE|REPLACE/.test(env.RESEND_API_KEY)) issues.push('RESEND_API_KEY missing or invalid');
  const sender = env.AUTH_MAIL_FROM || '';
  const mailbox = sender.match(/^[^<>\r\n]+ <([^<>\r\n]+)>$/)?.[1] || sender;
  if (sender.length > 320 || !/^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$/.test(mailbox)) issues.push('AUTH_MAIL_FROM missing or invalid');
  if (!/^pri_[a-z0-9]+(\s*,\s*pri_[a-z0-9]+)*$/i.test(env.PRO_PRICE_IDS || '') || /PASTE|REPLACE/.test(env.PRO_PRICE_IDS)) issues.push('PRO_PRICE_IDS missing or invalid');
  if (env.LIVE_ACCEPTANCE_ENABLED && !['true', 'false'].includes(env.LIVE_ACCEPTANCE_ENABLED)) issues.push('LIVE_ACCEPTANCE_ENABLED must be true or false');
  if ((env.AUTH_TEST_EMAIL_SHA256 || env.LIVE_ACCEPTANCE_ENABLED === 'true') && !/^[a-f0-9]{64}$/.test(env.AUTH_TEST_EMAIL_SHA256 || '')) issues.push('AUTH_TEST_EMAIL_SHA256 must pin one verified test mailbox');
  return issues;
}
