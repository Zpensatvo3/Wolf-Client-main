import { DatabaseSync } from 'node:sqlite';
import { readFileSync } from 'node:fs';
import worker from '../../server/worker.js';
import { base64UrlEncode, sha256 } from '../../server/core/crypto.mjs';
import { deviceMessage } from '../../server/device-proof.mjs';
import { accessFixture } from './access-fixture.mjs';

// A real, isolated SQLite database with the D1 prepared-statement surface.
// No production resource, network call or persistent credential is used.
export class LocalD1 {
    constructor(schema = true) {
        this.sqlite = new DatabaseSync(':memory:');
        if (schema) this.sqlite.exec(readFileSync(new URL('../../server/schema.sql', import.meta.url), 'utf8'));
    }
    prepare(query) {
        const db = this;
        function prepared(values = []) {
            const statement = {
                query, values,
                bind(...next) { return prepared(next); },
                execute() {
                    const sql = db.sqlite.prepare(query);
                    if (/^\s*(SELECT|PRAGMA|WITH)\b/i.test(query) || /\bRETURNING\b/i.test(query)) {
                        const results = sql.all(...values).map(row => ({ ...row }));
                        return { success: true, results, meta: { changes: Number(db.sqlite.prepare('SELECT changes() AS n').get().n) } };
                    }
                    const result = sql.run(...values);
                    return { success: true, results: [], meta: { changes: Number(result.changes), last_row_id: Number(result.lastInsertRowid) } };
                },
                async all() { return statement.execute(); },
                async run() { return statement.execute(); },
                async first(column) { const row = statement.execute().results[0] || null; return column && row ? row[column] : row; }
            };
            return statement;
        }
        return prepared();
    }
    async batch(statements) {
        this.sqlite.exec('BEGIN');
        try {
            const results = statements.map(statement => statement.execute());
            this.sqlite.exec('COMMIT');
            return results;
        } catch (error) { this.sqlite.exec('ROLLBACK'); throw error; }
    }
    close() { this.sqlite.close(); }
}

export async function harness() {
    const access = await accessFixture();
    const pair = await crypto.subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']);
    const admin = base64UrlEncode(crypto.getRandomValues(new Uint8Array(36)));
    const releases = new Map();
    const env = {
        ...access.env,
        DB: new LocalD1(), KEY_PEPPER: base64UrlEncode(crypto.getRandomValues(new Uint8Array(32))),
        HWID_PEPPER: base64UrlEncode(crypto.getRandomValues(new Uint8Array(32))),
        ADMIN_TOKEN_HASH: await sha256(admin),
        SIGNING_PRIVATE_KEY_PKCS8: base64UrlEncode(await crypto.subtle.exportKey('pkcs8', pair.privateKey)),
        SIGNING_PUBLIC_KEY_SPKI: base64UrlEncode(await crypto.subtle.exportKey('spki', pair.publicKey)),
        ALLOW_DEVICE_CONFIGS: 'false',
        RATE_LIMITER: { idFromName: value => value, get: () => ({ fetch: async () => Response.json({ allowed: true }) }) },
        RELEASES: {
            async get(key, type) {
                if (!releases.has(key)) return null;
                const value = releases.get(key);
                if (type === 'json') return JSON.parse(value);
                if (type === 'stream') return new Response(value).body;
                return value;
            },
            async put(key, value) { releases.set(key, typeof value === 'string' ? value : new Uint8Array(await new Response(value).arrayBuffer())); }
        }
    };
    const pending = [];
    const ctx = { waitUntil(promise) { pending.push(promise); } };
    async function drain() { await Promise.all(pending.splice(0)); }
    async function request(path, body, options = {}) {
        const headers = { ...access.header, Origin:'https://local.test', ...(body === undefined ? {} : { 'Content-Type': 'application/json' }),
            ...(options.admin ? { Authorization: 'Bearer ' + admin } : {}), ...options.headers };
        const req = new Request('https://local.test' + path, { method: options.method || (body === undefined ? 'GET' : 'POST'),
            headers, body: body === undefined ? undefined : JSON.stringify(body) });
        const response = await worker.fetch(req, env, ctx); await drain(); return response;
    }
    async function device() {
        const key = await crypto.subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']);
        const publicKey = base64UrlEncode(await crypto.subtle.exportKey('raw', key.publicKey));
        const hwid = await sha256(crypto.randomUUID());
        return { publicKey, hwid, async proof(type, credential, extra = {}) {
            const body = { hwid, nonce: Array.from(crypto.getRandomValues(new Uint8Array(16)), byte => byte.toString(16).padStart(2, '0')).join(''),
                timestamp: Date.now(), request_type: type, device_key: publicKey,
                ...(type === 'activate' ? { key: credential } : type === 'redeem' ? { ticket: credential } : { session_token: credential }), ...extra };
            body.device_signature = base64UrlEncode(await crypto.subtle.sign({ name: 'ECDSA', hash: 'SHA-256' }, key.privateKey,
                new TextEncoder().encode(await deviceMessage(body))));
            return body;
        } };
    }
    return { env, admin, access, releases, ctx, request, drain, device, async close() { await drain(); env.DB.close(); } };
}
