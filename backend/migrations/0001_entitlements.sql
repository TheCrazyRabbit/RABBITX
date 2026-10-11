-- Additive only: webhook receipts are NEVER promoted to trusted entitlements.
CREATE TABLE IF NOT EXISTS auth_challenges (
  challenge_id TEXT PRIMARY KEY, email TEXT NOT NULL, code_hash TEXT NOT NULL,
  expires_at INTEGER NOT NULL, attempts INTEGER NOT NULL DEFAULT 0,
  consumed_hash TEXT
);
CREATE TABLE IF NOT EXISTS auth_sessions (
  token_hash TEXT PRIMARY KEY, email TEXT NOT NULL, expires_at INTEGER NOT NULL,
  revoked_at INTEGER, created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS auth_limits (
  bucket TEXT PRIMARY KEY, hits INTEGER NOT NULL, expires_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS entitlement_snapshots (
  email TEXT PRIMARY KEY, tier TEXT NOT NULL, reason TEXT NOT NULL,
  subscription_id TEXT, customer_id TEXT, valid_until INTEGER,
  verified_at INTEGER NOT NULL, source TEXT NOT NULL CHECK (source = 'paddle_live_api')
);
CREATE INDEX IF NOT EXISTS auth_sessions_expiry ON auth_sessions(expires_at);
CREATE INDEX IF NOT EXISTS auth_challenges_expiry ON auth_challenges(expires_at);
CREATE INDEX IF NOT EXISTS auth_limits_expiry ON auth_limits(expires_at);
-- Initial checkout transactions may have no billing_period. Preserve only a
-- period independently observed in the canonical subscription API, not a webhook.
CREATE TABLE IF NOT EXISTS verified_paid_periods (
  transaction_id TEXT PRIMARY KEY, subscription_id TEXT NOT NULL, customer_id TEXT NOT NULL,
  starts_at INTEGER NOT NULL, ends_at INTEGER NOT NULL, verified_at INTEGER NOT NULL
);
