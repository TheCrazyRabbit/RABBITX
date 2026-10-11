// Operator-only, local diagnostic. Never sends mail or mutates Paddle/Cloudflare.
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { configurationIssues } from '../configuration.js';

export async function checkReadiness(env, { paddleRead = false, fetcher = fetch } = {}) {
  const report = {
    auth_enabled: env.AUTH_ENABLED === 'true',
    acceptance_enabled: env.LIVE_ACCEPTANCE_ENABLED === 'true',
    configuration_issues: configurationIssues(env), paddle_read_checks: [],
    resend_delivery_verified: false, live_transaction_verified: false
  };
  if (!paddleRead || report.configuration_issues.some(s => s.startsWith('PADDLE_API_KEY'))) return report;
  for (const resource of ['customers', 'subscriptions', 'transactions', 'adjustments', 'discounts']) {
    try {
      const result = await fetcher(`https://api.paddle.com/${resource}?per_page=1`, {
        method: 'GET', redirect: 'error', signal: AbortSignal.timeout(8000),
        headers: { Authorization: `Bearer ${env.PADDLE_API_KEY}`, 'Paddle-Version': '1' }
      });
      const valid = result.ok && Array.isArray((await result.json()).data);
      report.paddle_read_checks.push({ permission: resource.replace(/s$/, '') + '.read', status: result.status, ok: valid });
    } catch {
      report.paddle_read_checks.push({ permission: resource.replace(/s$/, '') + '.read', ok: false, error: 'Request or response unavailable' });
    }
  }
  return report;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    if (process.argv.slice(2).some(a => a !== '--paddle-read')) throw Error('Unsupported option');
    const text = readFileSync(new URL('../wrangler.jsonc', import.meta.url), 'utf8').replace(/^\s*\/\/.*$/gm, '');
    const config = JSON.parse(text);
    const binding = config.d1_databases?.find(db => db.binding === 'DB');
    // This checks a declared binding only, not its existence or tables remotely.
    const env = { ...config.vars, ...process.env, DB: binding || null };
    const report = await checkReadiness(env, { paddleRead: process.argv.includes('--paddle-read') });
    console.log(JSON.stringify({ ...report, db_binding_declared: Boolean(binding), remote_schema_verified: false }, null, 2));
    process.exitCode = report.configuration_issues.length || report.paddle_read_checks.some(c => !c.ok) ? 1 : 0;
  } catch {
    console.error('Readiness check could not run. Check local configuration and command options.'); process.exitCode = 1;
  }
}
