// The supported OneStep schema is installed explicitly from schema.sql.
// Request handling validates it without rewriting historical tables or rows.
const requiredColumns = {
    auth_request_nonces: ['nonce_hash','exp'],
    license_keys: ['id', 'key_hash', 'key_encrypted', 'hwid_hash', 'hwid_hint',
        'device_key_hash', 'license_epoch', 'expires_at', 'status', 'activated_at', 'created_at'],
    auth_logs: ['id', 'key_hash', 'hwid_hash', 'ip_address', 'status', 'timestamp'],
    admin_audit_logs: ['id', 'admin_id', 'action', 'key_hash', 'previous_epoch',
        'new_epoch', 'reason', 'ip_address', 'timestamp'],
    public_configs: ['id', 'owner_key_hash', 'owner_hwid_hash', 'author_name', 'name',
        'description', 'payload', 'module_count', 'downloads', 'created_at', 'updated_at'],
    launcher_tickets: ['ticket_hash', 'key_hash', 'hwid_hash', 'device_key_hash',
        'license_epoch', 'pid', 'release_sha256', 'exp']
};

export async function ensureDatabaseSchema(env) {
    // Only fixed table names enter PRAGMA; no request input is interpolated.
    await Promise.all(Object.entries(requiredColumns).map(async ([table, required]) => {
        const { results } = await env.DB.prepare(`PRAGMA table_info(${table})`).all();
        const columns = new Set((results || []).map(column => column.name));
        if (required.some(column => !columns.has(column)) ||
            (table === 'license_keys' && columns.has('key_code'))) {
            throw new Error('Unsupported database schema; operator review required');
        }
    }));
}
