-- Apply with the hardened Worker during an approved deployment. Does not change
-- key_hash, license IDs, config ownership, device bindings or license expiry.
CREATE TABLE IF NOT EXISTS auth_request_nonces (nonce_hash TEXT PRIMARY KEY, exp INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS idx_auth_request_nonces_exp ON auth_request_nonces(exp);
-- Intentionally irreversible after backup: old licenses remain valid but their
-- recoverable copies are removed. Newly issued keys are shown exactly once.
UPDATE license_keys SET key_encrypted = NULL WHERE key_encrypted IS NOT NULL;
