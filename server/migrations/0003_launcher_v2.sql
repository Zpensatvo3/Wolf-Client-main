-- Additive only. No license hashes, bindings, keys or customer configs are changed.
CREATE TABLE IF NOT EXISTS approved_releases (
 sha256 TEXT PRIMARY KEY, version TEXT NOT NULL, minecraft_target TEXT NOT NULL,
 bytes INTEGER NOT NULL CHECK(bytes >= 4096 AND bytes <= 25165824),
 revoked INTEGER NOT NULL DEFAULT 0 CHECK(revoked IN (0,1)),
 created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS release_channels (
 minecraft_target TEXT PRIMARY KEY, sha256 TEXT NOT NULL REFERENCES approved_releases(sha256)
);
CREATE TABLE IF NOT EXISTS runtime_sessions (
 session_id TEXT PRIMARY KEY, key_hash TEXT NOT NULL, hwid_hash TEXT NOT NULL,
 device_key_hash TEXT NOT NULL, license_epoch INTEGER NOT NULL,
 release_sha256 TEXT NOT NULL, release_version TEXT NOT NULL, minecraft_target TEXT NOT NULL,
 pid INTEGER NOT NULL, process_nonce TEXT NOT NULL, sequence INTEGER NOT NULL DEFAULT 0,
 exp INTEGER NOT NULL, revoked INTEGER NOT NULL DEFAULT 0,
 ticket_hash TEXT UNIQUE, ticket_exp INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_runtime_sessions_exp ON runtime_sessions(exp);
CREATE INDEX IF NOT EXISTS idx_runtime_sessions_license ON runtime_sessions(key_hash);
