-- Safe bootstrap schema. This file must never drop production license data.
CREATE TABLE IF NOT EXISTS license_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key_hash TEXT NOT NULL UNIQUE,
    key_encrypted TEXT,
    hwid_hash TEXT,
    hwid_hint TEXT,
    device_key_hash TEXT,
    license_epoch INTEGER DEFAULT 1,
    expires_at DATETIME,
    status TEXT DEFAULT 'unused',
    activated_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS auth_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key_hash TEXT,
    hwid_hash TEXT,
    ip_address TEXT,
    status TEXT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS admin_audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id TEXT NOT NULL,
    action TEXT NOT NULL,
    key_hash TEXT,
    previous_epoch INTEGER,
    new_epoch INTEGER,
    reason TEXT NOT NULL,
    ip_address TEXT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_auth_logs_timestamp ON auth_logs(timestamp);
CREATE INDEX IF NOT EXISTS idx_admin_audit_logs_timestamp ON admin_audit_logs(timestamp);

-- One-use launcher grants belong in strongly consistent D1, never KV.
CREATE TABLE IF NOT EXISTS launcher_tickets (
    ticket_hash TEXT PRIMARY KEY, key_hash TEXT NOT NULL, hwid_hash TEXT NOT NULL,
    device_key_hash TEXT NOT NULL, license_epoch INTEGER NOT NULL, pid INTEGER NOT NULL,
    release_sha256 TEXT NOT NULL, exp INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_launcher_tickets_exp ON launcher_tickets(exp);

-- Public, license-owned client configurations. Payloads intentionally contain
-- client preferences only; license keys, HWIDs and session data never enter
-- this table.
CREATE TABLE IF NOT EXISTS public_configs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_key_hash TEXT NOT NULL,
    owner_hwid_hash TEXT,
    author_name TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    payload TEXT NOT NULL,
    module_count INTEGER NOT NULL DEFAULT 0,
    downloads INTEGER NOT NULL DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_public_configs_updated ON public_configs(updated_at DESC, id DESC);
CREATE INDEX IF NOT EXISTS idx_public_configs_owner ON public_configs(owner_key_hash, id DESC);
CREATE INDEX IF NOT EXISTS idx_public_configs_hwid_owner ON public_configs(owner_hwid_hash, id DESC);

-- Do not seed production with a known/demo key. Create the first license from
-- the protected admin panel after configuring all Worker secrets.
