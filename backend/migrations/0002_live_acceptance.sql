-- Local/staging preparation only. Apply to Live only with rollout confirmation.
-- No public creation route. An owner must approve an exact existing transaction.
CREATE TABLE IF NOT EXISTS live_acceptance_grants (
  grant_id TEXT PRIMARY KEY,
  email_hash TEXT NOT NULL CHECK(length(email_hash)=64),
  customer_id TEXT NOT NULL,
  subscription_id TEXT NOT NULL,
  transaction_id TEXT NOT NULL UNIQUE,
  price_id TEXT NOT NULL,
  discount_id TEXT NOT NULL,
  approved_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL CHECK(expires_at>approved_at AND expires_at<=approved_at+7200),
  revoked_at INTEGER,
  purpose TEXT NOT NULL CHECK(purpose='paddle_live_acceptance'),
  approved_by TEXT NOT NULL,
  UNIQUE(customer_id, subscription_id)
);
