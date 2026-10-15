// Enterprise Cloudflare Worker API & Admin Dashboard for Wolf-Client Licensing System

const AUTH_AUDIENCE = "wolf-client-v1";
const MAX_JSON_BYTES = 4096;
const MAX_CONFIG_JSON_BYTES = 96 * 1024;
const MAX_CONFIGS_PER_PAGE = 80;
const HEX_64 = /^[a-f0-9]{64}$/i;
const NONCE = /^[a-f0-9]{32}$/i;
const KEY_CODE = /^[A-Za-z0-9-]{8,128}$/;
const MINECRAFT_NAME = /^[A-Za-z0-9_]{3,16}$/;
// The client renews this token in the background.  A longer window protects a
// legitimate session from short Cloudflare/ISP interruptions without changing
// the actual licence expiry or the server-side HWID/epoch checks.
const SESSION_TTL_MS = 45 * 60 * 1000;

function requiredSecret(env, name) {
    const value = env[name];
    if (typeof value !== "string" || value.length < 32) {
        throw new Error(`Server configuration error: missing ${name}`);
    }
    return value;
}

function base64UrlEncode(bytes) {
    let binary = "";
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    for (let i = 0; i < view.length; i++) binary += String.fromCharCode(view[i]);
    return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
}

function base64UrlDecode(value) {
    if (typeof value !== "string" || !/^[A-Za-z0-9_-]+$/.test(value) || value.length > 16384) {
        throw new Error("Invalid base64url data");
    }
    const padded = value.replace(/-/g, "+").replace(/_/g, "/") + "=".repeat((4 - value.length % 4) % 4);
    const binary = atob(padded);
    return Uint8Array.from(binary, (ch) => ch.charCodeAt(0));
}

function constantTimeEqual(left, right) {
    if (typeof left !== "string" || typeof right !== "string" || left.length !== right.length) return false;
    let difference = 0;
    for (let i = 0; i < left.length; i++) difference |= left.charCodeAt(i) ^ right.charCodeAt(i);
    return difference === 0;
}

async function sha256(str) {
    const encoder = new TextEncoder();
    const data = encoder.encode(str);
    const hashBuffer = await crypto.subtle.digest("SHA-256", data);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

// License hashes are still used for authentication.  The encrypted copy exists
// only so the authenticated dashboard can reveal a key the owner created later.
// It is encrypted with a key derived from the Worker-only KEY_PEPPER and is
// never included in the license-list response.
async function licenseCipher(env) {
    const material = await crypto.subtle.digest(
        "SHA-256",
        new TextEncoder().encode(requiredSecret(env, "KEY_PEPPER"))
    );
    return crypto.subtle.importKey("raw", material, { name: "AES-GCM" }, false, ["encrypt", "decrypt"]);
}

async function encryptLicenseKey(key, env) {
    const iv = crypto.getRandomValues(new Uint8Array(12));
    const cipherText = await crypto.subtle.encrypt(
        { name: "AES-GCM", iv },
        await licenseCipher(env),
        new TextEncoder().encode(key)
    );
    const packed = new Uint8Array(iv.length + cipherText.byteLength);
    packed.set(iv, 0);
    packed.set(new Uint8Array(cipherText), iv.length);
    return base64UrlEncode(packed);
}

async function decryptLicenseKey(packedValue, env) {
    const packed = base64UrlDecode(String(packedValue || ""));
    if (packed.length <= 12) throw new Error("Invalid encrypted license key");
    const plainText = await crypto.subtle.decrypt(
        { name: "AES-GCM", iv: packed.slice(0, 12) },
        await licenseCipher(env),
        packed.slice(12)
    );
    const key = new TextDecoder().decode(plainText);
    if (!KEY_CODE.test(key)) throw new Error("Invalid decrypted license key");
    return key;
}

let licenseSchemaReady = false;
let licenseSchemaMigration = null;
let authLogSchemaReady = false;
let authLogSchemaMigration = null;
let configSchemaReady = false;
let configSchemaMigration = null;

// Early Wolf deployments stored the raw key in a NOT NULL key_code column.
// Upgrade the whole table on the Worker, where the pepper is available, so
// new records never need to retain raw license keys.
async function ensureLicenseSchema(env) {
    if (licenseSchemaReady) return;
    if (licenseSchemaMigration) return licenseSchemaMigration;

    licenseSchemaMigration = (async () => {
        const columnInfo = await env.DB.prepare("PRAGMA table_info(license_keys)").all();
        const columns = new Set((columnInfo.results || []).map((column) => String(column.name)));
        if (!columns.has("key_code")) {
            if (!columns.has("key_hash")) throw new Error("license_keys has no key identifier column");
            if (!columns.has("key_encrypted")) {
                await env.DB.prepare("ALTER TABLE license_keys ADD COLUMN key_encrypted TEXT").run();
            }
            if (!columns.has("hwid_hint")) {
                await env.DB.prepare("ALTER TABLE license_keys ADD COLUMN hwid_hint TEXT").run();
            }
            licenseSchemaReady = true;
            return;
        }

        const legacy = await env.DB.prepare("SELECT * FROM license_keys").all();
        const keyPepper = requiredSecret(env, "KEY_PEPPER");
        const hwidPepper = requiredSecret(env, "HWID_PEPPER");
        const statements = [env.DB.prepare(
            "CREATE TABLE license_keys_secure_v2 (" +
            "id INTEGER PRIMARY KEY AUTOINCREMENT, " +
            "key_hash TEXT NOT NULL UNIQUE, " +
            "key_encrypted TEXT, " +
            "hwid_hash TEXT, " +
            "hwid_hint TEXT, " +
            "license_epoch INTEGER DEFAULT 1, " +
            "expires_at DATETIME, " +
            "status TEXT DEFAULT 'unused', " +
            "activated_at DATETIME, " +
            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP)"
        )];

        for (const row of legacy.results || []) {
            const rawKey = String(row.key_code || "");
            const keyHash = HEX_64.test(String(row.key_hash || ""))
                ? String(row.key_hash).toLowerCase()
                : await sha256(rawKey + keyPepper);
            if (!KEY_CODE.test(rawKey) || !HEX_64.test(keyHash)) {
                throw new Error(`Legacy license ${row.id} cannot be migrated safely`);
            }

            const rawHwid = String(row.hwid_hash || row.hwid || "");
            const hwidHash = rawHwid
                ? (HEX_64.test(rawHwid) ? rawHwid.toLowerCase() : await sha256(rawHwid + hwidPepper))
                : null;
            const epoch = Number.isInteger(Number(row.license_epoch)) && Number(row.license_epoch) > 0
                ? Number(row.license_epoch)
                : 1;
            const status = ["unused", "active", "banned", "expired"].includes(String(row.status || "unused").toLowerCase())
                ? String(row.status || "unused").toLowerCase()
                : "unused";

            statements.push(env.DB.prepare(
                "INSERT INTO license_keys_secure_v2 " +
                "(id, key_hash, key_encrypted, hwid_hash, hwid_hint, license_epoch, expires_at, status, activated_at, created_at) " +
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
            ).bind(
                row.id,
                keyHash,
                await encryptLicenseKey(rawKey, env),
                hwidHash,
                rawHwid ? (rawHwid.substring(0, 10) + "..." + rawHwid.substring(Math.max(10, rawHwid.length - 6))) : null,
                epoch,
                row.expires_at || null,
                status,
                row.activated_at || null,
                row.created_at || null
            ));
        }

        statements.push(
            env.DB.prepare("DROP TABLE license_keys"),
            env.DB.prepare("ALTER TABLE license_keys_secure_v2 RENAME TO license_keys"),
            env.DB.prepare("CREATE INDEX idx_license_keys_key_hash ON license_keys(key_hash)")
        );
        await env.DB.batch(statements);

        licenseSchemaReady = true;
    })();

    try {
        await licenseSchemaMigration;
    } finally {
        licenseSchemaMigration = null;
    }
}

async function ensureAuthLogSchema(env) {
    if (authLogSchemaReady) return;
    if (authLogSchemaMigration) return authLogSchemaMigration;

    authLogSchemaMigration = (async () => {
        const columnInfo = await env.DB.prepare("PRAGMA table_info(auth_logs)").all();
        const columns = new Set((columnInfo.results || []).map((column) => String(column.name)));
        const statements = [];
        if (!columns.has("key_hash")) {
            statements.push(env.DB.prepare("ALTER TABLE auth_logs ADD COLUMN key_hash TEXT"));
        }
        if (!columns.has("hwid_hash")) {
            statements.push(env.DB.prepare("ALTER TABLE auth_logs ADD COLUMN hwid_hash TEXT"));
        }
        if (statements.length) await env.DB.batch(statements);
        authLogSchemaReady = true;
    })();

    try {
        await authLogSchemaMigration;
    } finally {
        authLogSchemaMigration = null;
    }
}

// Public configurations deliberately live separately from license material.
// A config is owned by a one-way license-key hash, never by the raw key, HWID,
// or the short display hint exposed in the dashboard.
async function ensureConfigSchema(env) {
    if (configSchemaReady) return;
    if (configSchemaMigration) return configSchemaMigration;

    configSchemaMigration = (async () => {
        await env.DB.prepare(
            "CREATE TABLE IF NOT EXISTS public_configs (" +
            "id INTEGER PRIMARY KEY AUTOINCREMENT, " +
            "owner_key_hash TEXT NOT NULL, " +
            "owner_hwid_hash TEXT, " +
            "author_name TEXT NOT NULL, " +
            "name TEXT NOT NULL, " +
            "description TEXT NOT NULL DEFAULT '', " +
            "payload TEXT NOT NULL, " +
            "module_count INTEGER NOT NULL DEFAULT 0, " +
            "downloads INTEGER NOT NULL DEFAULT 0, " +
            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, " +
            "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP)"
        ).run();

        const tableInfo = await env.DB.prepare("PRAGMA table_info(public_configs)").all();
        const columns = new Set((tableInfo.results || []).map(column => String(column.name || "")));
        if (!columns.has("owner_hwid_hash")) {
            await env.DB.prepare("ALTER TABLE public_configs ADD COLUMN owner_hwid_hash TEXT").run();
        }

        await env.DB.batch([
            env.DB.prepare("CREATE INDEX IF NOT EXISTS idx_public_configs_updated ON public_configs(updated_at DESC, id DESC)"),
            env.DB.prepare("CREATE INDEX IF NOT EXISTS idx_public_configs_owner ON public_configs(owner_key_hash, id DESC)"),
            env.DB.prepare("CREATE INDEX IF NOT EXISTS idx_public_configs_hwid_owner ON public_configs(owner_hwid_hash, id DESC)")
        ]);
        configSchemaReady = true;
    })();

    try {
        await configSchemaMigration;
    } finally {
        configSchemaMigration = null;
    }
}

async function ensureDatabaseSchema(env) {
    await ensureLicenseSchema(env);
    await ensureAuthLogSchema(env);
    await ensureConfigSchema(env);
}

function generateRandomKey() {
    const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const randomIndex = () => {
        // Rejection sampling avoids modulo bias while using Web Crypto instead
        // of predictable Math.random().
        const bytes = new Uint8Array(1);
        do { crypto.getRandomValues(bytes); } while (bytes[0] >= 248);
        return bytes[0] % chars.length;
    };
    const seg = (len) => Array.from({ length: len }, () => chars[randomIndex()]).join('');
    return `WOLF-${seg(4)}-${seg(4)}-${seg(4)}`;
}

function jsonResponse(data, status = 200) {
    return new Response(JSON.stringify(data), {
        status: status,
        headers: {
            "Content-Type": "application/json; charset=utf-8",
            "Cache-Control": "no-store",
            "X-Content-Type-Options": "nosniff",
        },
    });
}

async function verifyAdminAuth(request, env) {
    const authorization = request.headers.get("Authorization") || "";
    if (!authorization.startsWith("Bearer ")) return false;
    const token = authorization.slice(7).trim();
    if (token.length < 32 || token.length > 512) return false;
    const inputHash = await sha256(token);
    const configuredHash = requiredSecret(env, "ADMIN_TOKEN_HASH");
    return HEX_64.test(configuredHash) && constantTimeEqual(inputHash, configuredHash.toLowerCase());
}

function writeAdminAudit(ctx, env, request, action, keyId, reason) {
    try {
        ctx.waitUntil(env.DB.prepare(
            "INSERT INTO admin_audit_logs (admin_id, action, key_hash, previous_epoch, new_epoch, reason, ip_address) " +
            "SELECT ?, ?, key_hash, MAX(1, license_epoch - 1), license_epoch, ?, ? FROM license_keys WHERE id = ?"
        ).bind(
            "dashboard",
            action,
            reason,
            request.headers.get("CF-Connecting-IP") || "0.0.0.0",
            keyId
        ).run());
    } catch (_) { }
}

async function readJson(request) {
    const contentType = request.headers.get("Content-Type") || "";
    if (!contentType.toLowerCase().includes("application/json")) {
        throw new Error("Content-Type must be application/json");
    }
    const contentLength = Number(request.headers.get("Content-Length") || "0");
    if (contentLength > MAX_JSON_BYTES) throw new Error("Request body too large");
    const text = await request.text();
    if (text.length === 0 || text.length > MAX_JSON_BYTES) throw new Error("Request body too large");
    return JSON.parse(text);
}

async function readConfigJson(request) {
    const contentType = request.headers.get("Content-Type") || "";
    if (!contentType.toLowerCase().includes("application/json")) {
        throw new Error("Content-Type must be application/json");
    }
    const contentLength = Number(request.headers.get("Content-Length") || "0");
    if (contentLength > MAX_CONFIG_JSON_BYTES) throw new Error("Request body too large");
    const text = await request.text();
    if (text.length === 0 || text.length > MAX_CONFIG_JSON_BYTES) throw new Error("Request body too large");
    return JSON.parse(text);
}

async function importSigningPrivateKey(env) {
    const der = base64UrlDecode(requiredSecret(env, "SIGNING_PRIVATE_KEY_PKCS8"));
    return crypto.subtle.importKey("pkcs8", der, { name: "ECDSA", namedCurve: "P-256" }, false, ["sign"]);
}

async function importSigningPublicKey(env) {
    const der = base64UrlDecode(requiredSecret(env, "SIGNING_PUBLIC_KEY_SPKI"));
    return crypto.subtle.importKey("spki", der, { name: "ECDSA", namedCurve: "P-256" }, false, ["verify"]);
}

async function signPayload(payload, env) {
    const payloadText = JSON.stringify(payload);
    const payloadBytes = new TextEncoder().encode(payloadText);
    const signature = await crypto.subtle.sign(
        { name: "ECDSA", hash: "SHA-256" },
        await importSigningPrivateKey(env),
        payloadBytes
    );
    const encodedPayload = base64UrlEncode(payloadBytes);
    return {
        payload: encodedPayload,
        signature: base64UrlEncode(signature),
        session_token: `${encodedPayload}.${base64UrlEncode(signature)}`,
    };
}

async function verifySessionToken(token, env) {
    if (typeof token !== "string" || token.length > 16384) throw new Error("Invalid session token");
    const parts = token.split(".");
    if (parts.length !== 2) throw new Error("Invalid session token");
    const payloadBytes = base64UrlDecode(parts[0]);
    const signature = base64UrlDecode(parts[1]);
    const valid = await crypto.subtle.verify(
        { name: "ECDSA", hash: "SHA-256" },
        await importSigningPublicKey(env),
        signature,
        payloadBytes
    );
    if (!valid) throw new Error("Invalid session signature");
    const payload = JSON.parse(new TextDecoder().decode(payloadBytes));
    if (!payload || payload.aud !== AUTH_AUDIENCE || !HEX_64.test(String(payload.key_hash || "")) ||
        !HEX_64.test(String(payload.hwid_hash || "")) || !Number.isInteger(payload.license_epoch) ||
        !Number.isFinite(payload.exp) || payload.exp < Date.now()) {
        throw new Error("Expired or malformed session token");
    }
    return payload;
}

async function requireActiveConfigSession(body, env) {
    // During the temporary no-license-client build, configs retain a stable
    // owner identity derived on the server from the device fingerprint. It is
    // intentionally scoped to public config ownership and cannot authenticate
    // a licence or access the admin API.
    const suppliedToken = String(body && body.session_token || "");
    if (!suppliedToken) {
        const deviceId = String(body && body.device_id || "");
        if (!HEX_64.test(deviceId)) {
            throw new Error("Missing config device identity");
        }
        const hwidHash = await sha256(deviceId + requiredSecret(env, "HWID_PEPPER"));
        return {
            key_hash: await sha256("public-config-owner:" + hwidHash + requiredSecret(env, "KEY_PEPPER")),
            hwid_hash: hwidHash,
            license_epoch: 1,
            config_only: true,
        };
    }

    const claims = await verifySessionToken(suppliedToken, env);
    const license = await env.DB.prepare(
        "SELECT key_hash, hwid_hash, license_epoch, status, expires_at FROM license_keys WHERE key_hash = ? LIMIT 1"
    ).bind(claims.key_hash).first();
    if (!license || String(license.status || "").toLowerCase() !== "active") {
        throw new Error("License session is not active");
    }
    if (Number(license.license_epoch || 1) !== claims.license_epoch ||
        !constantTimeEqual(String(license.hwid_hash || ""), claims.hwid_hash)) {
        throw new Error("License session was revoked");
    }
    if (license.expires_at && Date.now() > new Date(license.expires_at).getTime()) {
        throw new Error("License session expired");
    }
    return claims;
}

function validateConfigInput(body) {
    const name = String(body && body.name || "").trim();
    const description = String(body && body.description || "").trim();
    const authorName = String(body && body.author_name || "").trim();
    const payload = String(body && body.payload || "");
    const moduleCount = Number(body && body.module_count || 0);
    if (name.length < 2 || name.length > 48 || description.length > 180 ||
        !MINECRAFT_NAME.test(authorName) || payload.length < 8 ||
        payload.length > MAX_CONFIG_JSON_BYTES || !Number.isSafeInteger(moduleCount) ||
        moduleCount < 0 || moduleCount > 128) {
        throw new Error("Invalid config fields");
    }
    const parsedPayload = JSON.parse(payload);
    if (!parsedPayload || typeof parsedPayload !== "object" || Array.isArray(parsedPayload) ||
        !Array.isArray(parsedPayload.modules) || parsedPayload.modules.length > 128) {
        throw new Error("Invalid config payload");
    }
    return { name, description, authorName, payload, moduleCount };
}

// Bind this class as RATE_LIMITER in wrangler.toml. A Durable Object gives one
// serialized counter per client IP and route, unlike per-isolate Maps that can
// be bypassed by hitting a different Cloudflare edge.
export class RateLimiter {
    constructor(state) {
        this.state = state;
    }

    async fetch(request) {
        const { limit, windowMs } = await request.json();
        const now = Date.now();
        const existing = await this.state.storage.get("bucket");
        const windowStart = !existing || existing.windowStart + windowMs <= now
            ? now
            : existing.windowStart;
        const count = windowStart === now ? 1 : existing.count + 1;
        await this.state.storage.put("bucket", { windowStart, count });
        return jsonResponse({ allowed: count <= limit, retryAfterMs: Math.max(0, windowStart + windowMs - now) });
    }
}

async function enforceRateLimit(request, env, scope, limit, windowMs) {
    if (!env.RATE_LIMITER) {
        throw new Error("Server configuration error: missing RATE_LIMITER binding");
    }
    const ip = request.headers.get("CF-Connecting-IP") || "unknown";
    const id = env.RATE_LIMITER.idFromName(`${scope}:${ip}`);
    const response = await env.RATE_LIMITER.get(id).fetch("https://rate-limit/check", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ limit, windowMs }),
    });
    const result = await response.json();
    return result.allowed ? null : Math.ceil(result.retryAfterMs / 1000);
}

export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);

        if (request.method === "OPTIONS") {
            return jsonResponse(null, 204);
        }

        // ----------------------------------------------------
        // 1. ADMIN DASHBOARD HTML WEB UI (/admin)
        // ----------------------------------------------------
        if (url.pathname === "/admin" || url.pathname === "/admin/" || url.pathname === "/") {
            const html = `<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="theme-color" content="#090b10">
    <title>Wolf Client - Admin Panel</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
    <style>
        :root {
            color-scheme: dark;
            --bg: #07090d;
            --bg-soft: #0c0f15;
            --panel: rgba(18, 22, 30, 0.90);
            --panel-strong: #12161e;
            --panel-hover: #171c26;
            --border: rgba(255, 255, 255, 0.075);
            --border-strong: rgba(255, 255, 255, 0.13);
            --text: #f5f7fb;
            --muted: #8d96a8;
            --subtle: #626b7c;
            --accent: #7c6cff;
            --accent-soft: rgba(124, 108, 255, 0.14);
            --accent-hover: #9185ff;
            --success: #39d98a;
            --success-soft: rgba(57, 217, 138, 0.13);
            --warning: #f6bd5a;
            --warning-soft: rgba(246, 189, 90, 0.14);
            --danger: #ff6474;
            --danger-soft: rgba(255, 100, 116, 0.13);
            --info: #5db4ff;
            --info-soft: rgba(93, 180, 255, 0.13);
            --shadow: 0 28px 90px rgba(0, 0, 0, 0.42);
            --radius-xl: 22px;
            --radius-lg: 16px;
            --radius-md: 12px;
            --radius-sm: 9px;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        html { min-width: 320px; background: var(--bg); }
        body { min-height: 100vh; overflow-x: hidden; color: var(--text); background: radial-gradient(circle at 9% 3%, rgba(124, 108, 255, 0.13), transparent 31%), radial-gradient(circle at 88% 92%, rgba(93, 180, 255, 0.08), transparent 32%), var(--bg); font-family: 'Inter', system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; -webkit-font-smoothing: antialiased; }
        button, input, select { font: inherit; }
        button { border: 0; }
        button:focus-visible, input:focus-visible, select:focus-visible { outline: 2px solid rgba(124, 108, 255, 0.85); outline-offset: 2px; }
        .hidden { display: none !important; }
        .icon { width: 18px; height: 18px; flex: 0 0 auto; stroke: currentColor; stroke-width: 1.8; stroke-linecap: round; stroke-linejoin: round; fill: none; }

        .login-shell { min-height: 100vh; display: grid; place-items: center; padding: 28px; position: relative; isolation: isolate; }
        .login-shell::before { content: ''; position: absolute; width: 420px; height: 420px; border-radius: 50%; background: rgba(124, 108, 255, 0.12); filter: blur(90px); z-index: -1; pointer-events: none; }
        .login-card { position: relative; z-index: 1; width: min(100%, 430px); padding: 34px; border: 1px solid var(--border-strong); border-radius: 26px; background: linear-gradient(145deg, rgba(22, 26, 36, 0.96), rgba(12, 15, 21, 0.96)); box-shadow: var(--shadow); backdrop-filter: blur(24px); }
        .brand-mark { width: 52px; height: 52px; display: grid; place-items: center; border: 1px solid rgba(124, 108, 255, 0.35); border-radius: 16px; background: linear-gradient(145deg, rgba(124, 108, 255, 0.24), rgba(124, 108, 255, 0.08)); color: #b9b2ff; box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.08); }
        .brand-mark svg { width: 28px; height: 28px; }
        .login-brand { display: flex; align-items: center; gap: 14px; margin-bottom: 30px; }
        .login-brand h1 { font-size: 1.08rem; font-weight: 800; letter-spacing: -0.02em; }
        .login-brand p { margin-top: 4px; color: var(--muted); font-size: 0.82rem; }
        .login-copy { margin-bottom: 24px; }
        .login-copy h2 { font-size: 1.72rem; letter-spacing: -0.045em; line-height: 1.2; }
        .login-copy p { margin-top: 9px; color: var(--muted); font-size: 0.92rem; line-height: 1.55; }
        .field-label { display: block; margin-bottom: 8px; color: #c9cfda; font-size: 0.78rem; font-weight: 700; letter-spacing: 0.025em; }
        .input-wrap { position: relative; }
        .input-icon { position: absolute; top: 50%; left: 14px; transform: translateY(-50%); color: var(--subtle); pointer-events: none; }
        input, select { width: 100%; min-height: 44px; padding: 0 14px; border: 1px solid var(--border); border-radius: var(--radius-sm); color: var(--text); background: rgba(7, 9, 13, 0.74); outline: none; transition: border-color 160ms ease, background 160ms ease, box-shadow 160ms ease; }
        .input-with-icon { padding-left: 43px; }
        input::placeholder { color: #555e6f; }
        input:hover, select:hover { border-color: var(--border-strong); }
        input:focus, select:focus { border-color: rgba(124, 108, 255, 0.78); background: rgba(10, 12, 18, 0.92); box-shadow: 0 0 0 4px rgba(124, 108, 255, 0.10); }

        .primary-button, .secondary-button, .ghost-button, .danger-button, .warning-button, .success-button, .icon-button { display: inline-flex; align-items: center; justify-content: center; gap: 8px; border-radius: var(--radius-sm); font-weight: 700; cursor: pointer; transition: transform 150ms ease, opacity 150ms ease, background 150ms ease, border-color 150ms ease; user-select: none; }
        .primary-button { min-height: 44px; padding: 0 17px; color: #fff; background: linear-gradient(135deg, var(--accent), #6354f4); box-shadow: 0 10px 28px rgba(124, 108, 255, 0.20); }
        .primary-button:hover { background: linear-gradient(135deg, var(--accent-hover), #7365ff); transform: translateY(-1px); }
        .secondary-button, .ghost-button { min-height: 38px; padding: 0 13px; border: 1px solid var(--border); color: #d8dce5; background: rgba(255, 255, 255, 0.035); }
        .secondary-button:hover, .ghost-button:hover { border-color: var(--border-strong); background: rgba(255, 255, 255, 0.065); }
        .danger-button { min-height: 38px; padding: 0 13px; border: 1px solid rgba(255, 100, 116, 0.18); color: #ffadb6; background: var(--danger-soft); }
        .danger-button:hover { border-color: rgba(255, 100, 116, 0.35); background: rgba(255, 100, 116, 0.19); }
        .warning-button { min-height: 38px; padding: 0 13px; border: 1px solid rgba(246, 189, 90, 0.18); color: #ffd38b; background: var(--warning-soft); }
        .warning-button:hover { border-color: rgba(246, 189, 90, 0.34); background: rgba(246, 189, 90, 0.20); }
        .success-button { min-height: 38px; padding: 0 13px; border: 1px solid rgba(57, 217, 138, 0.18); color: #83edb8; background: var(--success-soft); }
        .success-button:hover { border-color: rgba(57, 217, 138, 0.34); background: rgba(57, 217, 138, 0.20); }
        button:disabled { cursor: not-allowed; opacity: 0.55; transform: none !important; }
        .login-button { position: relative; z-index: 2; width: 100%; margin-top: 16px; pointer-events: auto; }
        .login-footer { display: flex; align-items: center; gap: 8px; margin-top: 20px; color: var(--subtle); font-size: 0.76rem; }
        .status-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--success); box-shadow: 0 0 0 5px rgba(57, 217, 138, 0.08); }

        .app-shell { min-height: 100vh; display: grid; grid-template-columns: 250px minmax(0, 1fr); }
        .sidebar { position: sticky; top: 0; height: 100vh; padding: 24px 18px; border-right: 1px solid var(--border); background: rgba(8, 10, 15, 0.78); backdrop-filter: blur(24px); }
        .sidebar-brand { display: flex; align-items: center; gap: 12px; padding: 0 8px 25px; border-bottom: 1px solid var(--border); }
        .sidebar-brand .brand-mark { width: 42px; height: 42px; border-radius: 13px; }
        .sidebar-brand .brand-mark svg { width: 23px; height: 23px; }
        .sidebar-brand strong { display: block; font-size: 0.95rem; letter-spacing: -0.02em; }
        .sidebar-brand span { display: block; margin-top: 3px; color: var(--muted); font-size: 0.71rem; }
        .nav-label { margin: 27px 10px 10px; color: #505969; font-size: 0.67rem; font-weight: 800; letter-spacing: 0.12em; text-transform: uppercase; }
        .nav-item { width: 100%; min-height: 42px; display: flex; align-items: center; gap: 11px; padding: 0 12px; border: 1px solid transparent; border-radius: 10px; color: var(--muted); background: transparent; font-size: 0.84rem; font-weight: 650; text-align: left; cursor: pointer; }
        .nav-item:hover { color: #d6d9e1; background: rgba(255, 255, 255, 0.035); }
        .nav-item.active { color: #dcd8ff; border-color: rgba(124, 108, 255, 0.18); background: var(--accent-soft); }

        .main-content { min-width: 0; padding: 28px clamp(20px, 3vw, 42px) 45px; }
        .topbar { display: flex; align-items: center; justify-content: space-between; gap: 18px; margin-bottom: 28px; }
        .page-title h1 { font-size: clamp(1.55rem, 2vw, 2rem); letter-spacing: -0.045em; }
        .page-title p { margin-top: 7px; color: var(--muted); font-size: 0.84rem; }
        .topbar-actions { display: flex; align-items: center; gap: 10px; }
        .api-status { min-height: 38px; display: flex; align-items: center; gap: 9px; padding: 0 13px; border: 1px solid var(--border); border-radius: 10px; color: #b9c0cc; background: rgba(255, 255, 255, 0.025); font-size: 0.76rem; font-weight: 650; }

        .stats-grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 14px; margin-bottom: 18px; }
        .stat-card { position: relative; min-height: 126px; padding: 19px; overflow: hidden; border: 1px solid var(--border); border-radius: var(--radius-lg); background: linear-gradient(145deg, rgba(20, 24, 33, 0.92), rgba(14, 17, 24, 0.92)); box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.02); cursor: pointer; transition: transform 160ms ease, border-color 160ms ease, background 160ms ease; }
        .stat-card:hover { transform: translateY(-2px); border-color: var(--border-strong); background: linear-gradient(145deg, rgba(24, 29, 40, 0.94), rgba(15, 18, 25, 0.94)); }
        .stat-card.selected { border-color: rgba(124, 108, 255, 0.50); box-shadow: inset 0 0 0 1px rgba(124, 108, 255, 0.10), 0 12px 34px rgba(0, 0, 0, 0.18); }
        .stat-top { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
        .stat-label { color: var(--muted); font-size: 0.76rem; font-weight: 650; }
        .stat-icon { width: 34px; height: 34px; display: grid; place-items: center; border-radius: 10px; color: #b7afff; background: var(--accent-soft); }
        .stat-card.success .stat-icon { color: #79e9ad; background: var(--success-soft); }
        .stat-card.info .stat-icon { color: #8bcaff; background: var(--info-soft); }
        .stat-card.danger .stat-icon { color: #ff9ba6; background: var(--danger-soft); }
        .stat-value { margin-top: 17px; font-size: 1.85rem; font-weight: 800; letter-spacing: -0.045em; }

        .content-grid { display: grid; grid-template-columns: minmax(280px, 0.72fr) minmax(0, 1.55fr); gap: 18px; align-items: start; }
        .content-grid.licenses-only { grid-template-columns: minmax(0, 1fr); }
        .content-grid.licenses-only .table-panel { grid-column: 1; }
        .view-hidden { display: none !important; }
        .panel { border: 1px solid var(--border); border-radius: var(--radius-lg); background: linear-gradient(145deg, rgba(18, 22, 30, 0.92), rgba(13, 16, 22, 0.92)); box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.025); }
        .panel-header { display: flex; align-items: center; justify-content: space-between; gap: 14px; padding: 18px 20px; border-bottom: 1px solid var(--border); }
        .panel-title { display: flex; align-items: center; gap: 10px; }
        .panel-title-icon { width: 32px; height: 32px; display: grid; place-items: center; border-radius: 9px; color: #b7afff; background: var(--accent-soft); }
        .panel-title h2 { font-size: 0.94rem; letter-spacing: -0.02em; }
        .panel-title p { margin-top: 3px; color: var(--subtle); font-size: 0.69rem; }
        .panel-body { padding: 20px; }
        .form-group + .form-group { margin-top: 17px; }
        .field-help { margin-top: 7px; color: var(--subtle); font-size: 0.68rem; line-height: 1.45; }
        .create-button { width: 100%; margin-top: 20px; }

        .created-key-box { display: none; margin-top: 18px; padding: 14px; border: 1px solid rgba(57, 217, 138, 0.22); border-radius: 12px; background: var(--success-soft); }
        .created-key-box.visible { display: block; animation: slideIn 180ms ease; }
        .created-key-box small { display: block; color: #79dba7; font-weight: 700; }
        .created-key-row { display: flex; align-items: center; gap: 9px; margin-top: 9px; }
        .created-key-value { min-width: 0; flex: 1; overflow: hidden; color: #dff9eb; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 0.78rem; font-weight: 700; text-overflow: ellipsis; white-space: nowrap; }
        .icon-button { width: 36px; height: 36px; flex: 0 0 auto; border: 1px solid var(--border); color: #c7ccd6; background: rgba(255, 255, 255, 0.035); }
        .icon-button:hover { border-color: var(--border-strong); background: rgba(255, 255, 255, 0.07); }
        .table-panel { min-width: 0; }
        .toolbar { display: flex; align-items: center; gap: 9px; }
        .search-box { position: relative; width: min(100%, 250px); }
        .search-box input { height: 38px; min-height: 38px; padding-left: 38px; font-size: 0.78rem; }
        .search-box .input-icon { left: 12px; }
        .status-filter { width: 132px; min-height: 38px; height: 38px; font-size: 0.76rem; }
        .table-wrap { overflow-x: auto; }
        table { width: 100%; min-width: 880px; border-collapse: collapse; }
        th, td { padding: 11px 14px; border-bottom: 1px solid rgba(255, 255, 255, 0.052); text-align: left; vertical-align: middle; white-space: nowrap; }
        th { color: #697284; background: rgba(255, 255, 255, 0.012); font-size: 0.64rem; font-weight: 800; letter-spacing: 0.075em; text-transform: uppercase; }
        td { color: #c7cdd7; font-size: 0.76rem; }
        tbody tr { transition: background 140ms ease; }
        tbody tr:hover { background: rgba(255, 255, 255, 0.026); }
        tbody tr:last-child td { border-bottom: 0; }
        .id-cell { color: var(--subtle); font-weight: 700; }
        .mono-cell { max-width: 176px; overflow: hidden; color: #e8ebf1; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 0.71rem; font-weight: 650; text-overflow: ellipsis; }
        .hwid-cell { color: #8d96a7; }
        .license-cell { min-width: 238px; }
        .license-reveal { width: 100%; display: flex; align-items: center; gap: 10px; padding: 7px 9px; border: 1px solid rgba(124, 108, 255, 0.13); border-radius: 10px; color: inherit; background: rgba(124, 108, 255, 0.045); text-align: left; cursor: pointer; transition: background 150ms ease, border-color 150ms ease, transform 150ms ease; }
        .license-reveal:hover { border-color: rgba(124, 108, 255, 0.42); background: rgba(124, 108, 255, 0.10); transform: translateY(-1px); }
        .license-reveal.revealed { border-color: rgba(57, 217, 138, 0.30); background: var(--success-soft); }
        .license-reveal-mark { width: 28px; height: 28px; display: grid; place-items: center; flex: 0 0 auto; border-radius: 8px; color: #bbb4ff; background: var(--accent-soft); }
        .license-reveal.revealed .license-reveal-mark { color: #7ceaad; background: rgba(57, 217, 138, 0.15); }
        .license-reveal-copy { min-width: 0; display: grid; gap: 2px; }
        .license-reveal-main { overflow: hidden; color: #edf0f6; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 0.73rem; font-weight: 750; text-overflow: ellipsis; white-space: nowrap; }
        .license-reveal-meta { overflow: hidden; color: var(--subtle); font-size: 0.63rem; font-weight: 650; text-overflow: ellipsis; white-space: nowrap; }
        .license-reveal.revealed .license-reveal-meta { color: #73c99a; }
        .epoch-pill { display: inline-flex; align-items: center; min-height: 25px; padding: 0 9px; border: 1px solid var(--border); border-radius: 999px; color: #aeb5c1; background: rgba(255, 255, 255, 0.025); font-size: 0.69rem; font-weight: 750; }
        .badge { display: inline-flex; align-items: center; gap: 6px; min-height: 26px; padding: 0 9px; border: 1px solid transparent; border-radius: 999px; font-size: 0.67rem; font-weight: 800; }
        .badge::before { content: ''; width: 6px; height: 6px; border-radius: 50%; background: currentColor; box-shadow: 0 0 0 4px currentColor; opacity: 0.85; }
        .badge-active { color: #6be5a4; border-color: rgba(57, 217, 138, 0.13); background: var(--success-soft); }
        .badge-unused { color: #89c9ff; border-color: rgba(93, 180, 255, 0.13); background: var(--info-soft); }
        .badge-banned, .badge-expired { color: #ff98a3; border-color: rgba(255, 100, 116, 0.13); background: var(--danger-soft); }
        .actions-cell { display: flex; align-items: center; gap: 7px; }
        .table-action { min-height: 31px; padding: 0 10px; font-size: 0.67rem; }
        .empty-state { display: none; padding: 52px 20px; color: var(--muted); text-align: center; }
        .empty-state.visible { display: block; }
        .empty-state-icon { width: 46px; height: 46px; display: grid; place-items: center; margin: 0 auto 13px; border: 1px solid var(--border); border-radius: 14px; color: var(--subtle); background: rgba(255, 255, 255, 0.025); }
        .empty-state strong { display: block; color: #c7cdd6; font-size: 0.82rem; }
        .empty-state span { display: block; margin-top: 5px; font-size: 0.72rem; }

        .loading-overlay { position: fixed; z-index: 80; inset: 0; display: none; place-items: center; background: rgba(5, 6, 9, 0.68); backdrop-filter: blur(6px); }
        .loading-overlay.visible { display: grid; }
        .loader-card { min-width: 190px; display: flex; align-items: center; gap: 13px; padding: 17px 19px; border: 1px solid var(--border-strong); border-radius: 14px; background: var(--panel-strong); box-shadow: var(--shadow); color: #cdd2dc; font-size: 0.78rem; font-weight: 700; }
        .spinner { width: 18px; height: 18px; border: 2px solid rgba(255, 255, 255, 0.12); border-top-color: var(--accent); border-radius: 50%; animation: spin 700ms linear infinite; }

        .toast-stack { position: fixed; z-index: 120; right: 20px; bottom: 20px; width: min(calc(100vw - 40px), 360px); display: grid; gap: 10px; pointer-events: none; }
        .toast { display: flex; align-items: flex-start; gap: 11px; padding: 14px; border: 1px solid var(--border-strong); border-radius: 13px; background: rgba(17, 21, 29, 0.97); box-shadow: 0 18px 55px rgba(0, 0, 0, 0.34); backdrop-filter: blur(16px); animation: toastIn 220ms ease; pointer-events: auto; }
        .toast.leaving { animation: toastOut 180ms ease forwards; }
        .toast-icon { width: 29px; height: 29px; display: grid; place-items: center; flex: 0 0 auto; border-radius: 9px; color: #b7afff; background: var(--accent-soft); }
        .toast.success .toast-icon { color: #79e9ad; background: var(--success-soft); }
        .toast.error .toast-icon { color: #ff9ba6; background: var(--danger-soft); }
        .toast.warning .toast-icon { color: #ffd185; background: var(--warning-soft); }
        .toast-body { min-width: 0; flex: 1; }
        .toast-body strong { display: block; color: #eef1f6; font-size: 0.77rem; }
        .toast-body p { margin-top: 4px; color: var(--muted); font-size: 0.7rem; line-height: 1.45; overflow-wrap: anywhere; }

        .modal-backdrop { position: fixed; z-index: 100; inset: 0; display: none; place-items: center; padding: 20px; background: rgba(5, 6, 9, 0.76); backdrop-filter: blur(8px); }
        .modal-backdrop.visible { display: grid; }
        .modal { width: min(100%, 430px); padding: 22px; border: 1px solid var(--border-strong); border-radius: 18px; background: linear-gradient(145deg, #171b25, #10131a); box-shadow: var(--shadow); animation: modalIn 180ms ease; }
        .modal-icon { width: 42px; height: 42px; display: grid; place-items: center; border-radius: 12px; color: #ffd185; background: var(--warning-soft); }
        .modal h3 { margin-top: 17px; font-size: 1.04rem; letter-spacing: -0.025em; }
        .modal p { margin-top: 9px; color: var(--muted); font-size: 0.79rem; line-height: 1.55; }
        .modal-actions { display: flex; justify-content: flex-end; gap: 9px; margin-top: 22px; }

        @keyframes spin { to { transform: rotate(360deg); } }
        @keyframes toastIn { from { opacity: 0; transform: translateY(10px) scale(0.98); } to { opacity: 1; transform: translateY(0) scale(1); } }
        @keyframes toastOut { to { opacity: 0; transform: translateY(8px) scale(0.98); } }
        @keyframes modalIn { from { opacity: 0; transform: translateY(10px) scale(0.98); } to { opacity: 1; transform: translateY(0) scale(1); } }
        @keyframes slideIn { from { opacity: 0; transform: translateY(-5px); } to { opacity: 1; transform: translateY(0); } }

        @media (max-width: 1180px) { .stats-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } .content-grid { grid-template-columns: 1fr; } }
        @media (max-width: 860px) { .app-shell { display: block; } .sidebar { position: relative; width: 100%; height: auto; display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 14px 18px; border-right: 0; border-bottom: 1px solid var(--border); } .sidebar-brand { padding: 0; border: 0; } .sidebar .nav-label, .sidebar nav { display: none; } .main-content { padding-top: 22px; } }
        @media (max-width: 680px) { .login-shell { padding: 16px; } .login-card { padding: 26px 22px; } .topbar { align-items: flex-start; flex-direction: column; } .topbar-actions { width: 100%; } .api-status { flex: 1; } .stats-grid { grid-template-columns: 1fr 1fr; gap: 10px; } .stat-card { min-height: 110px; padding: 15px; } .stat-value { font-size: 1.55rem; } .panel-header { align-items: flex-start; flex-direction: column; } .toolbar { width: 100%; flex-wrap: wrap; } .search-box { width: 100%; } .status-filter { flex: 1; } .table-action span { display: none; } }
        @media (max-width: 420px) { .stats-grid { grid-template-columns: 1fr; } .topbar-actions { align-items: stretch; flex-direction: column; } }
    </style>
</head>
<body>
    <!-- LOGIN SCREEN -->
    <div id="login-box" class="login-shell">
        <div class="login-card">
            <div class="login-brand">
                <div class="brand-mark" aria-hidden="true">
                    <svg viewBox="0 0 24 24" fill="none"><path d="M4 4.5 8.2 7l3.8-3 3.8 3L20 4.5l-1.1 8.6a7 7 0 0 1-13.8 0L4 4.5Z" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/><path d="M8.3 12.1 10 11l2 1.2 2-1.2 1.7 1.1M9.2 15.1c1.7 1.2 3.9 1.2 5.6 0" stroke="currentColor" stroke-width="1.55" stroke-linecap="round"/></svg>
                </div>
                <div>
                    <h1>Wolf Client</h1>
                    <p>Licensing Control Center</p>
                </div>
            </div>
            <div class="login-copy">
                <h2>Yonetim paneline giris</h2>
                <p>Lisanslari, donanim kilitlerini ve erisim durumlarini tek panelden yonetin.</p>
            </div>

            <form id="login-form" novalidate>
                <label class="field-label" for="pass-field">ADMIN PAROLASI</label>
                <div class="input-wrap">
                    <svg class="icon input-icon" viewBox="0 0 24 24" aria-hidden="true"><rect x="5" y="10" width="14" height="10" rx="2"></rect><path d="M8 10V7a4 4 0 0 1 8 0v3"></path></svg>
                    <input class="input-with-icon" type="password" id="pass-field" placeholder="Parolanizi girin" autocomplete="current-password">
                </div>
                <button type="submit" id="login-btn" class="primary-button login-button">
                    <svg class="icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M10 17l5-5-5-5"></path><path d="M15 12H3"></path><path d="M14 4h4a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2h-4"></path></svg>
                    <span id="login-btn-text">Giris Yap</span>
                </button>
            </form>

            <div class="login-footer">
                <span class="status-dot"></span>
                <span>Cloudflare Worker API baglantisi hazir</span>
            </div>
        </div>
    </div>

    <!-- MAIN APP SCREEN -->
    <div id="main-panel" class="app-shell" style="display: none;">
        <aside class="sidebar">
            <div>
                <div class="sidebar-brand">
                    <div class="brand-mark" aria-hidden="true">
                        <svg viewBox="0 0 24 24" fill="none"><path d="M4 4.5 8.2 7l3.8-3 3.8 3L20 4.5l-1.1 8.6a7 7 0 0 1-13.8 0L4 4.5Z" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/><path d="M8.3 12.1 10 11l2 1.2 2-1.2 1.7 1.1M9.2 15.1c1.7 1.2 3.9 1.2 5.6 0" stroke="currentColor" stroke-width="1.55" stroke-linecap="round"/></svg>
                    </div>
                    <div>
                        <strong>Wolf Client</strong>
                        <span>Admin Panel</span>
                    </div>
                </div>
                <div class="nav-label">Yonetim</div>
                <nav>
                    <button type="button" id="nav-overview" class="nav-item active" data-view="overview" aria-current="page">
                        <svg class="icon" viewBox="0 0 24 24"><rect x="3" y="3" width="7" height="7" rx="1"></rect><rect x="14" y="3" width="7" height="7" rx="1"></rect><rect x="3" y="14" width="7" height="7" rx="1"></rect><rect x="14" y="14" width="7" height="7" rx="1"></rect></svg>
                        Genel Bakis
                    </button>
                    <button type="button" id="nav-licenses" class="nav-item" data-view="licenses">
                        <svg class="icon" viewBox="0 0 24 24"><circle cx="8" cy="15" r="3"></circle><path d="m10.5 12.5 7-7 2 2-2 2 1.5 1.5-2 2-1.5-1.5-3 3"></path></svg>
                        Lisanslar
                    </button>
                </nav>
            </div>
        </aside>

        <main class="main-content">
            <header class="topbar">
                <div class="page-title">
                    <h1 id="page-title">Genel Bakis</h1>
                    <p id="page-subtitle">Wolf Client lisans altyapisinin guncel durumu.</p>
                </div>
                <div class="topbar-actions">
                    <div class="api-status">
                        <span class="status-dot"></span>
                        <span>API Cevrimici</span>
                    </div>
                    <button type="button" id="logout-btn" class="ghost-button">
                        <svg class="icon" viewBox="0 0 24 24"><path d="M10 17l5-5-5-5"></path><path d="M15 12H3"></path><path d="M14 4h4a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2h-4"></path></svg>
                        Cikis
                    </button>
                </div>
            </header>

            <section id="overview-stats" class="stats-grid" aria-label="Lisans istatistikleri">
                <article class="stat-card selected" data-filter="all">
                    <div class="stat-top">
                        <span class="stat-label">Toplam Lisans</span>
                        <span class="stat-icon"><svg class="icon" viewBox="0 0 24 24"><circle cx="8" cy="15" r="3"></circle><path d="m10.5 12.5 7-7 2 2-2 2 1.5 1.5-2 2-1.5-1.5-3 3"></path></svg></span>
                    </div>
                    <div id="st-total" class="stat-value">0</div>
                </article>
                <article class="stat-card success" data-filter="active">
                    <div class="stat-top">
                        <span class="stat-label">Aktif Kullanımda</span>
                        <span class="stat-icon"><svg class="icon" viewBox="0 0 24 24"><path d="M20 6 9 17l-5-5"></path></svg></span>
                    </div>
                    <div id="st-active" class="stat-value">0</div>
                </article>
                <article class="stat-card info" data-filter="unused">
                    <div class="stat-top">
                        <span class="stat-label">Kullanilmamis</span>
                        <span class="stat-icon"><svg class="icon" viewBox="0 0 24 24"><path d="M12 3v18M3 12h18"></path></svg></span>
                    </div>
                    <div id="st-unused" class="stat-value">0</div>
                </article>
                <article class="stat-card danger" data-filter="blocked">
                    <div class="stat-top">
                        <span class="stat-label">Yasakli / Suresi Dolmus</span>
                        <span class="stat-icon"><svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"></circle><path d="m6 6 12 12"></path></svg></span>
                    </div>
                    <div id="st-banned" class="stat-value">0</div>
                </article>
            </section>

            <section id="content-grid" class="content-grid">
                <!-- CREATE KEY PANEL -->
                <article id="create-panel" class="panel">
                    <div class="panel-header">
                        <div class="panel-title">
                            <span class="panel-title-icon"><svg class="icon" viewBox="0 0 24 24"><path d="M12 3v18M3 12h18"></path></svg></span>
                            <div>
                                <h2>Yeni Lisans Olustur</h2>
                                <p>Key ve kullanim suresini belirleyin.</p>
                            </div>
                        </div>
                    </div>
                    <div class="panel-body">
                        <div class="form-group">
                            <label class="field-label" for="new-key-name">OZEL KEY</label>
                            <input type="text" id="new-key-name" placeholder="Bos birakirsaniz otomatik uretilir" autocomplete="off" spellcheck="false">
                            <p class="field-help">Ornek: WOLF-VIP-1234. Bos alan mevcut otomatik key uretimini kullanir.</p>
                        </div>
                        <div class="form-group">
                            <label class="field-label" for="new-key-expiry">GECERLILIK SURESI</label>
                            <select id="new-key-expiry">
                                <option value="lifetime">Sinirsiz (Lifetime)</option>
                                <option value="30">30 Gun</option>
                                <option value="7">7 Gun</option>
                                <option value="1">1 Gun</option>
                            </select>
                        </div>
                        <button type="button" id="create-key-btn" class="primary-button create-button">
                            <svg class="icon" viewBox="0 0 24 24"><circle cx="8" cy="15" r="3"></circle><path d="m10.5 12.5 7-7 2 2-2 2 1.5 1.5-2 2-1.5-1.5-3 3"></path></svg>
                            <span>Lisans Olustur</span>
                        </button>
                        <div id="created-key-display" class="created-key-box">
                            <small>KEY BASARIYLA OLUSTURULDU</small>
                            <div class="created-key-row">
                                <div id="created-key-value" class="created-key-value"></div>
                                <button type="button" id="copy-key-btn" class="icon-button" title="Kopyala">
                                    <svg class="icon" viewBox="0 0 24 24"><rect x="8" y="8" width="11" height="11" rx="2"></rect><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"></path></svg>
                                </button>
                            </div>
                        </div>
                    </div>
                </article>

                <!-- TABLE PANEL -->
                <article id="licenses-panel" class="panel table-panel">
                    <div class="panel-header">
                        <div class="panel-title">
                            <span class="panel-title-icon"><svg class="icon" viewBox="0 0 24 24"><path d="M4 5h16M4 12h16M4 19h16"></path></svg></span>
                            <div>
                                <h2>Lisans Anahtarlari</h2>
                                <p id="table-summary">0 lisans gosteriliyor</p>
                            </div>
                        </div>
                        <div class="toolbar">
                            <div class="search-box">
                                <svg class="icon input-icon" viewBox="0 0 24 24"><circle cx="11" cy="11" r="7"></circle><path d="m20 20-4-4"></path></svg>
                                <input type="text" id="filter-box" placeholder="Key, HWID veya ID ara..." autocomplete="off">
                            </div>
                            <select id="status-filter" class="status-filter">
                                <option value="all">Tum durumlar</option>
                                <option value="active">Aktif</option>
                                <option value="unused">Kullanilmamis</option>
                                <option value="banned">Yasakli</option>
                                <option value="expired">Suresi dolmus</option>
                                <option value="blocked">Yasakli / Suresi dolmus</option>
                            </select>
                            <button type="button" id="refresh-btn" class="icon-button" title="Yenile">
                                <svg class="icon" viewBox="0 0 24 24"><path d="M20 11a8 8 0 1 0-2.34 5.66"></path><path d="M20 4v7h-7"></path></svg>
                            </button>
                        </div>
                    </div>
                    <div class="table-wrap">
                        <table>
                            <thead>
                                <tr>
                                    <th>ID</th>
                                    <th>Lisans Anahtari</th>
                                    <th>HWID</th>
                                    <th>Epoch</th>
                                    <th>Durum</th>
                                    <th>Islem</th>
                                </tr>
                            </thead>
                            <tbody id="table-body"></tbody>
                        </table>
                        <div id="empty-state" class="empty-state">
                            <div class="empty-state-icon"><svg class="icon" viewBox="0 0 24 24"><circle cx="11" cy="11" r="7"></circle><path d="m20 20-4-4"></path></svg></div>
                            <strong>Eslesen lisans bulunamadi</strong>
                            <span>Arama veya durum filtresini degistirmeyi deneyin.</span>
                        </div>
                    </div>
                </article>
            </section>
        </main>
    </div>

    <!-- LOADING OVERLAY -->
    <div id="loading-overlay" class="loading-overlay" aria-hidden="true">
        <div class="loader-card"><span class="spinner"></span><span id="loading-text">Islem yurutuluyor...</span></div>
    </div>

    <!-- CONFIRM MODAL -->
    <div id="confirm-modal" class="modal-backdrop" role="dialog" aria-modal="true">
        <div class="modal">
            <div class="modal-icon"><svg class="icon" viewBox="0 0 24 24"><path d="M12 9v4"></path><path d="M12 17h.01"></path><path d="M10.3 3.7 2.6 17a2 2 0 0 0 1.7 3h15.4a2 2 0 0 0 1.7-3L13.7 3.7a2 2 0 0 0-3.4 0Z"></path></svg></div>
            <h3 id="confirm-title">Islemi onaylayin</h3>
            <p id="confirm-message">Bu islem geri alinamayabilir.</p>
            <div class="modal-actions">
                <button type="button" id="confirm-cancel" class="secondary-button">Iptal</button>
                <button type="button" id="confirm-accept" class="danger-button">Onayla</button>
            </div>
        </div>
    </div>

    <!-- TOAST STACK -->
    <div id="toast-stack" class="toast-stack" aria-live="polite"></div>

    <script>
        (function () {
            'use strict';

            var adminSecret = '';
            var allKeys = [];
            var createdKey = '';
            var confirmResolver = null;
            var currentView = 'overview';
            var loginInProgress = false;

            var icons = {
                success: '<svg class="icon" viewBox="0 0 24 24"><path d="M20 6 9 17l-5-5"></path></svg>',
                error: '<svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"></circle><path d="M15 9 9 15M9 9l6 6"></path></svg>',
                warning: '<svg class="icon" viewBox="0 0 24 24"><path d="M12 9v4"></path><path d="M12 17h.01"></path><path d="M10.3 3.7 2.6 17a2 2 0 0 0 1.7 3h15.4a2 2 0 0 0 1.7-3L13.7 3.7a2 2 0 0 0-3.4 0Z"></path></svg>',
                info: '<svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"></circle><path d="M12 11v5M12 8h.01"></path></svg>'
            };

            function byId(id) {
                return document.getElementById(id);
            }

            function setView(authenticated) {
                var loginBox = byId('login-box');
                var mainPanel = byId('main-panel');
                if (loginBox) loginBox.style.display = authenticated ? 'none' : 'grid';
                if (mainPanel) mainPanel.style.display = authenticated ? 'grid' : 'none';
            }

            function setLoginBusy(busy) {
                loginInProgress = busy;
                var button = byId('login-btn');
                var text = byId('login-btn-text');
                var field = byId('pass-field');
                if (button) button.disabled = busy;
                if (field) field.disabled = busy;
                if (text) text.textContent = busy ? 'Dogrulaniyor...' : 'Giris Yap';
            }

            function setLoading(visible, text) {
                var overlay = byId('loading-overlay');
                var label = byId('loading-text');
                if (label) label.textContent = text || 'Islem yurutuluyor...';
                if (overlay) overlay.classList.toggle('visible', Boolean(visible));
            }

            function escapeHtml(text) {
                var div = document.createElement('div');
                div.appendChild(document.createTextNode(String(text || '')));
                return div.innerHTML;
            }

            function showToast(type, title, message) {
                var stack = byId('toast-stack');
                if (!stack) return;
                var toast = document.createElement('div');
                toast.className = 'toast ' + (type || 'info');
                toast.innerHTML = '<div class="toast-icon">' + (icons[type] || icons.info) + '</div>' +
                    '<div class="toast-body"><strong>' + escapeHtml(title) + '</strong><p>' + escapeHtml(message || '') + '</p></div>';
                stack.appendChild(toast);
                window.setTimeout(function () {
                    toast.classList.add('leaving');
                    window.setTimeout(function () { toast.remove(); }, 190);
                }, 3600);
            }

            function apiHeaders() {
                return {
                    'Authorization': 'Bearer ' + adminSecret,
                    'Content-Type': 'application/json'
                };
            }

            async function parseJsonResponse(response) {
                var text = await response.text();
                if (!text) return {};
                try { return JSON.parse(text); }
                catch (e) { throw new Error('Sunucu JSON yerine gecersiz bir yanit dondurdu.'); }
            }

            async function requestAdmin(path, options) {
                var requestOptions = options || {};
                requestOptions.headers = Object.assign({}, apiHeaders(), requestOptions.headers || {});
                requestOptions.cache = 'no-store';
                requestOptions.credentials = 'same-origin';

                var response = await fetch(path, requestOptions);
                var data = await parseJsonResponse(response);

                if (response.status === 401) {
                    var authError = new Error(data.message || 'Admin parolasi hatali.');
                    authError.code = 'UNAUTHORIZED';
                    throw authError;
                }
                if (!response.ok || data.success === false) {
                    throw new Error(data.message || ('HTTP ' + response.status));
                }
                return data;
            }

            function clearAuth() {
                adminSecret = '';
                setView(false);
                var field = byId('pass-field');
                if (field) {
                    field.disabled = false;
                    field.value = '';
                    window.setTimeout(function () { field.focus(); }, 0);
                }
            }

            async function authenticate(secret, silent) {
                if (!secret || loginInProgress) return false;
                setLoginBusy(true);
                try {
                    adminSecret = secret;
                    var data = await requestAdmin('/api/v1/admin/list-keys', { method: 'GET' });
                    allKeys = Array.isArray(data.keys) ? data.keys : [];
                    setView(true);
                    renderTable(allKeys);
                    if (!silent) showToast('success', 'Giris Basarili', 'Yonetim paneline hosgeldiniz.');
                    return true;
                } catch (e) {
                    adminSecret = '';
                    setView(false);
                    if (!silent || e.code !== 'UNAUTHORIZED') {
                        showToast('error', 'Giris Basarisiz', e.message || 'Baglanti kurulamadi.');
                    }
                    return false;
                } finally {
                    setLoginBusy(false);
                }
            }

            async function doLogin(event) {
                if (event) event.preventDefault();
                var field = byId('pass-field');
                var value = field ? field.value.trim() : '';
                if (!value) {
                    showToast('warning', 'Parola Gerekli', 'Lutfen admin parolasini girin.');
                    if (field) field.focus();
                    return;
                }
                await authenticate(value, false);
            }

            function doLogout() {
                clearAuth();
                allKeys = [];
                createdKey = '';
                renderTable([]);
                showToast('info', 'Oturum Kapatildi', 'Admin oturumu sonlandirildi.');
            }

            async function loadDashboard(showLoading) {
                if (!adminSecret) {
                    clearAuth();
                    return false;
                }
                if (showLoading) setLoading(true, 'Veriler yukleniyor...');
                try {
                    var data = await requestAdmin('/api/v1/admin/list-keys', { method: 'GET' });
                    allKeys = Array.isArray(data.keys) ? data.keys : [];
                    setView(true);
                    renderTable(allKeys);
                    return true;
                } catch (e) {
                    if (e.code === 'UNAUTHORIZED') clearAuth();
                    showToast('error', 'Veriler Yuklenemedi', e.message);
                    return false;
                } finally {
                    if (showLoading) setLoading(false);
                }
            }

            function normalizeStatus(value) {
                var status = String(value || 'unused').trim().toLowerCase();
                if (status === 'ban' || status === 'blocked' || status === 'disabled') return 'banned';
                if (status === 'expire') return 'expired';
                return status || 'unused';
            }

            function renderTable(keys) {
                var tbody = byId('table-body');
                if (!tbody) return;
                tbody.innerHTML = '';
                var active = 0;
                var unused = 0;
                var blocked = 0;

                keys.forEach(function (key) {
                    var status = normalizeStatus(key.status);
                    if (status === 'active') active++;
                    else if (status === 'unused') unused++;
                    else blocked++;

                    var keyName = String(key.key_hash || key.key_code || ('Key #' + key.id));
                    var keyFingerprint = keyName.length > 14
                        ? keyName.substring(0, 8) + '...' + keyName.substring(keyName.length - 5)
                        : keyName;
                    var hwidValue = String(key.hwid_hint || key.hwid_hash || key.hwid || '');
                    var hwidDisplay = hwidValue ? (hwidValue.length > 20 ? hwidValue.substring(0, 16) + '...' : hwidValue) : 'Kilitlenmedi';

                    var row = document.createElement('tr');
                    row.dataset.status = status;
                    row.dataset.search = (key.id + ' ' + keyName + ' ' + hwidValue).toLowerCase();
                    row.innerHTML = '<td class="id-cell">#' + escapeHtml(key.id) + '</td>' +
                        '<td class="license-cell"><button type="button" class="license-reveal" data-action="reveal-key" data-id="' + escapeHtml(key.id) + '" title="Anahtari goster">' +
                        '<span class="license-reveal-mark"><svg class="icon" viewBox="0 0 24 24"><circle cx="8" cy="15" r="3"></circle><path d="m10.5 12.5 7-7 2 2-2 2 1.5 1.5-2 2-1.5-1.5-3 3"></path></svg></span>' +
                        '<span class="license-reveal-copy"><span class="license-reveal-main">Anahtari goster</span><span class="license-reveal-meta">Parmak izi: ' + escapeHtml(keyFingerprint) + '</span></span>' +
                        '</button></td>' +
                        '<td class="mono-cell hwid-cell">' + escapeHtml(hwidDisplay) + '</td>' +
                        '<td><span class="epoch-pill">v' + escapeHtml(key.license_epoch || 1) + '</span></td>' +
                        '<td><span class="badge badge-' + escapeHtml(status) + '">' + escapeHtml(status.toUpperCase()) + '</span></td>' +
                        '<td><div class="actions-cell">' +
                        '<button type="button" class="warning-button table-action" data-action="reset-hwid" data-id="' + escapeHtml(key.id) + '"><span>Reset HWID</span></button>' +
                        (status === 'banned'
                            ? '<button type="button" class="success-button table-action" data-action="set-status" data-id="' + escapeHtml(key.id) + '" data-status="unused"><span>Unban</span></button>'
                            : '<button type="button" class="danger-button table-action" data-action="set-status" data-id="' + escapeHtml(key.id) + '" data-status="banned"><span>Ban</span></button>') +
                        '<button type="button" class="danger-button table-action" data-action="delete-key" data-id="' + escapeHtml(key.id) + '"><span>Sil</span></button>' +
                        '</div></td>';
                    tbody.appendChild(row);
                });

                byId('st-total').textContent = String(keys.length);
                byId('st-active').textContent = String(active);
                byId('st-unused').textContent = String(unused);
                byId('st-banned').textContent = String(blocked);
                filterTable();
            }

            function filterTable() {
                var filterBox = byId('filter-box');
                var statusFilter = byId('status-filter');
                var query = filterBox ? filterBox.value.toLowerCase() : '';
                var selectedStatus = statusFilter ? statusFilter.value : 'all';
                var rows = document.querySelectorAll('#table-body tr');
                var visible = 0;

                rows.forEach(function (row) {
                    var rowStatus = normalizeStatus(row.dataset.status);
                    var matchesText = !query || (row.dataset.search || '').indexOf(query) !== -1;
                    var matchesStatus = selectedStatus === 'all' || rowStatus === selectedStatus ||
                        (selectedStatus === 'blocked' && (rowStatus === 'banned' || rowStatus === 'expired'));
                    var show = matchesText && matchesStatus;
                    row.style.display = show ? '' : 'none';
                    if (show) visible++;
                });

                var summary = byId('table-summary');
                if (summary) summary.textContent = visible + ' / ' + allKeys.length + ' lisans gosteriliyor';
                var emptyState = byId('empty-state');
                if (emptyState) emptyState.classList.toggle('visible', visible === 0);
            }

            function setPanelView(view, shouldScroll) {
                currentView = view === 'licenses' ? 'licenses' : 'overview';
                var isLicenses = currentView === 'licenses';
                var stats = byId('overview-stats');
                var createPanel = byId('create-panel');
                var contentGrid = byId('content-grid');
                var title = byId('page-title');
                var subtitle = byId('page-subtitle');

                if (stats) stats.classList.toggle('view-hidden', isLicenses);
                if (createPanel) createPanel.classList.toggle('view-hidden', isLicenses);
                if (contentGrid) contentGrid.classList.toggle('licenses-only', isLicenses);
                if (title) title.textContent = isLicenses ? 'Lisanslar' : 'Genel Bakis';
                if (subtitle) subtitle.textContent = isLicenses
                    ? 'Tum lisans anahtarlarini, durumlarini ve HWID kilitlerini yonetin.'
                    : 'Wolf Client lisans altyapisinin guncel durumu.';

                document.querySelectorAll('.nav-item[data-view]').forEach(function (item) {
                    var active = item.dataset.view === currentView;
                    item.classList.toggle('active', active);
                    if (active) item.setAttribute('aria-current', 'page');
                    else item.removeAttribute('aria-current');
                });

                if (shouldScroll !== false) {
                    window.requestAnimationFrame(function () {
                        if (isLicenses) {
                            var panel = byId('licenses-panel');
                            if (panel) panel.scrollIntoView({ behavior: 'smooth', block: 'start' });
                        } else {
                            window.scrollTo({ top: 0, behavior: 'smooth' });
                        }
                    });
                }
            }

            function setStatusFilter(filter, card) {
                var select = byId('status-filter');
                if (select) select.value = filter;
                document.querySelectorAll('.stat-card').forEach(function (item) {
                    item.classList.toggle('selected', item === card);
                });
                filterTable();
            }

            async function createKey() {
                var keyInput = byId('new-key-name');
                var expiryInput = byId('new-key-expiry');
                var keyValue = keyInput ? keyInput.value.trim() : '';
                var expiry = expiryInput ? expiryInput.value : 'lifetime';
                setLoading(true, 'Lisans olusturuluyor...');
                try {
                    var data = await requestAdmin('/api/v1/admin/create-key', {
                        method: 'POST',
                        body: JSON.stringify({ key: keyValue, days: expiry })
                    });
                    createdKey = String(data.key || '');
                    var display = byId('created-key-display');
                    var value = byId('created-key-value');
                    if (value) value.textContent = createdKey;
                    if (display) display.classList.add('visible');
                    if (keyInput) keyInput.value = '';
                    await copyText(createdKey, false);
                    showToast('success', 'Lisans Olusturuldu', createdKey);
                    await loadDashboard(false);
                } catch (e) {
                    if (e.code === 'UNAUTHORIZED') clearAuth();
                    showToast('error', 'Lisans Olusturulamadi', e.message);
                } finally {
                    setLoading(false);
                }
            }

            async function copyText(text, notify) {
                if (!text) return false;
                try {
                    if (navigator.clipboard && window.isSecureContext) {
                        await navigator.clipboard.writeText(text);
                    } else {
                        var area = document.createElement('textarea');
                        area.value = text;
                        area.style.position = 'fixed';
                        area.style.opacity = '0';
                        document.body.appendChild(area);
                        area.select();
                        document.execCommand('copy');
                        area.remove();
                    }
                    if (notify !== false) showToast('success', 'Kopyalandi', 'Key panoya kopyalandi.');
                    return true;
                } catch (e) {
                    if (notify !== false) showToast('warning', 'Kopyalanamadi', 'Keyi elle kopyalayabilirsiniz.');
                    return false;
                }
            }

            async function revealKey(button) {
                if (!button || button.disabled) return;
                var existingKey = String(button.dataset.key || '');
                if (existingKey) {
                    await copyText(existingKey, true);
                    return;
                }

                var label = button.querySelector('.license-reveal-main');
                var hint = button.querySelector('.license-reveal-meta');
                button.disabled = true;
                if (label) label.textContent = 'Anahtar aciliyor...';
                try {
                    var data = await requestAdmin('/api/v1/admin/reveal-key', {
                        method: 'POST',
                        body: JSON.stringify({ id: Number(button.dataset.id) })
                    });
                    var key = String(data.key || '');
                    if (!key) throw new Error('Sunucu anahtari dondurmedi.');
                    button.dataset.key = key;
                    button.classList.add('revealed');
                    button.title = 'Kopyalamak icin tikla';
                    if (label) label.textContent = key;
                    if (hint) hint.textContent = 'Gorunur • Kopyalamak icin tekrar tikla';
                    showToast('success', 'Anahtar Goruntulendi', 'Kopyalamak icin lisans kutusuna tekrar tiklayin.');
                } catch (e) {
                    if (label) label.textContent = 'Anahtar kullanilamiyor';
                    if (hint) hint.textContent = e.message || 'Bu eski lisans geri acilamaz.';
                    showToast('warning', 'Anahtar Açilamadi', e.message || 'Bu lisans anahtari geri acilamiyor.');
                } finally {
                    button.disabled = false;
                }
            }

            function showConfirm(title, message) {
                byId('confirm-title').textContent = title;
                byId('confirm-message').textContent = message;
                byId('confirm-modal').classList.add('visible');
                return new Promise(function (resolve) { confirmResolver = resolve; });
            }

            function resolveConfirm(value) {
                byId('confirm-modal').classList.remove('visible');
                if (confirmResolver) {
                    var resolve = confirmResolver;
                    confirmResolver = null;
                    resolve(value);
                }
            }

            async function resetHWID(id) {
                var accepted = await showConfirm('HWID Sifirlama', 'Bu lisansin HWID kilidi sifirlanacak. Devam edilsin mi?');
                if (!accepted) return;
                setLoading(true, 'HWID sifirlaniyor...');
                try {
                    await requestAdmin('/api/v1/admin/reset-hwid', {
                        method: 'POST',
                        body: JSON.stringify({ id: Number(id) })
                    });
                    showToast('success', 'HWID Sifirlandi', 'Lisans yeniden kullanima acildi.');
                    await loadDashboard(false);
                } catch (e) {
                    if (e.code === 'UNAUTHORIZED') clearAuth();
                    showToast('error', 'HWID Sifirlanamadi', e.message);
                } finally {
                    setLoading(false);
                }
            }

            async function toggleBan(id, newStatus) {
                var title = newStatus === 'banned' ? 'Lisans Yasakla' : 'Yasagi Kaldir';
                var message = newStatus === 'banned'
                    ? 'Bu lisans yasaklanacak ve kullanilamaz hale gelecek.'
                    : 'Bu lisansin yasagi kaldirilacak.';
                var accepted = await showConfirm(title, message);
                if (!accepted) return;
                setLoading(true, 'Durum guncelleniyor...');
                try {
                    await requestAdmin('/api/v1/admin/ban-key', {
                        method: 'POST',
                        body: JSON.stringify({ id: Number(id), status: newStatus })
                    });
                    showToast('success', 'Durum Guncellendi', 'Lisans durumu ' + newStatus + ' olarak degistirildi.');
                    await loadDashboard(false);
                    var statusSelect = byId('status-filter');
                    if (statusSelect) statusSelect.value = newStatus === 'banned' ? 'banned' : 'all';
                    setPanelView('licenses', false);
                    filterTable();
                } catch (e) {
                    if (e.code === 'UNAUTHORIZED') clearAuth();
                    showToast('error', 'Durum Guncellenemedi', e.message);
                } finally {
                    setLoading(false);
                }
            }

            async function deleteKey(id) {
                var accepted = await showConfirm(
                    'Lisansi Tamamen Sil',
                    'Bu lisans, HWID baglantisi ve aktif oturumlariyla birlikte kalici olarak silinecek. Bu islem geri alinamaz.'
                );
                if (!accepted) return;
                setLoading(true, 'Lisans siliniyor...');
                try {
                    await requestAdmin('/api/v1/admin/delete-key', {
                        method: 'POST',
                        body: JSON.stringify({ id: Number(id) })
                    });
                    showToast('success', 'Lisans Silindi', 'Lisans kaydi tamamen kaldirildi.');
                    await loadDashboard(false);
                } catch (e) {
                    if (e.code === 'UNAUTHORIZED') clearAuth();
                    showToast('error', 'Lisans Silinemedi', e.message);
                } finally {
                    setLoading(false);
                }
            }

            function bindEvents() {
                var loginForm = byId('login-form');
                if (loginForm) loginForm.addEventListener('submit', doLogin);

                document.querySelectorAll('.nav-item[data-view]').forEach(function (item) {
                    item.addEventListener('click', function () {
                        setPanelView(item.dataset.view || 'overview', true);
                    });
                });

                var logout = byId('logout-btn');
                if (logout) logout.addEventListener('click', doLogout);

                var createButton = byId('create-key-btn');
                if (createButton) createButton.addEventListener('click', createKey);

                var copyButton = byId('copy-key-btn');
                if (copyButton) copyButton.addEventListener('click', function () { copyText(createdKey, true); });

                var refreshButton = byId('refresh-btn');
                if (refreshButton) refreshButton.addEventListener('click', function () { loadDashboard(true); });

                var filterBox = byId('filter-box');
                if (filterBox) filterBox.addEventListener('input', filterTable);

                var statusFilter = byId('status-filter');
                if (statusFilter) statusFilter.addEventListener('change', filterTable);

                document.querySelectorAll('.stat-card').forEach(function (card) {
                    card.addEventListener('click', function () {
                        setStatusFilter(card.dataset.filter || 'all', card);
                    });
                });

                var tableBody = byId('table-body');
                if (tableBody) {
                    tableBody.addEventListener('click', function (event) {
                        var button = event.target.closest('button[data-action]');
                        if (!button) return;
                        var action = button.dataset.action;
                        if (action === 'reveal-key') revealKey(button);
                        if (action === 'reset-hwid') resetHWID(button.dataset.id);
                        if (action === 'set-status') toggleBan(button.dataset.id, button.dataset.status);
                        if (action === 'delete-key') deleteKey(button.dataset.id);
                    });
                }

                var cancelButton = byId('confirm-cancel');
                if (cancelButton) cancelButton.addEventListener('click', function () { resolveConfirm(false); });

                var acceptButton = byId('confirm-accept');
                if (acceptButton) acceptButton.addEventListener('click', function () { resolveConfirm(true); });

                var modal = byId('confirm-modal');
                if (modal) {
                    modal.addEventListener('click', function (event) {
                        if (event.target === modal) resolveConfirm(false);
                    });
                }

                document.addEventListener('keydown', function (event) {
                    if (event.key === 'Escape' && byId('confirm-modal').classList.contains('visible')) {
                        resolveConfirm(false);
                    }
                });
            }

            async function init() {
                bindEvents();
                setPanelView('overview', false);
                setView(false);
                var passwordField = byId('pass-field');
                if (passwordField) passwordField.focus();
            }

            if (document.readyState === 'loading') {
                document.addEventListener('DOMContentLoaded', init, { once: true });
            } else {
                init();
            }
        }());
    </script>
</body>
</html>`;
            return new Response(html, {
                headers: {
                    "Content-Type": "text/html; charset=utf-8",
                    "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
                    "Pragma": "no-cache",
                    "X-Content-Type-Options": "nosniff",
                    "X-Frame-Options": "DENY",
                    "Referrer-Policy": "no-referrer"
                }
            });
        }

        // ----------------------------------------------------
        // 2. ADMIN API ENDPOINTS (/api/v1/admin/*)
        // ----------------------------------------------------
        if (url.pathname.startsWith("/api/v1/admin/")) {
            let retryAfter;
            try {
                retryAfter = await enforceRateLimit(request, env, "admin", 10, 60 * 1000);
            } catch (_) {
                return jsonResponse({ success: false, message: "Admin service is not configured." }, 503);
            }
            if (retryAfter !== null) {
                return jsonResponse({ success: false, message: "Too many admin requests. Please wait." }, 429);
            }

            let isAuth = false;
            try {
                isAuth = await verifyAdminAuth(request, env);
            } catch (_) {
                return jsonResponse({ success: false, message: "Admin service is not configured." }, 503);
            }
            if (!isAuth) {
                return jsonResponse({ success: false, message: "Unauthorized Admin Access" }, 401);
            }

            try {
                await ensureDatabaseSchema(env);
            } catch (error) {
                console.error("license_schema_migration_failed", { message: error && error.message });
                return jsonResponse({ success: false, message: "License database migration failed." }, 500);
            }

            // LIST KEYS
            if (url.pathname === "/api/v1/admin/list-keys") {
                try {
                    // Do not send encrypted key material to the browser. The
                    // owner can ask to reveal one specific key when needed.
                    const { results } = await env.DB.prepare(
                        "SELECT id, key_hash, hwid_hash, hwid_hint, license_epoch, expires_at, status, activated_at, created_at " +
                        "FROM license_keys ORDER BY id DESC"
                    ).all();
                    return jsonResponse({ success: true, keys: results || [] });
                } catch (_) {
                    return jsonResponse({ success: false, message: "Unable to load licenses." }, 500);
                }
            }

            // CREATE KEY
            if (url.pathname === "/api/v1/admin/create-key" && request.method === "POST") {
                try {
                    const body = await readJson(request);
                    let keyToCreate = body.key || generateRandomKey();
                    const days = String(body.days || "lifetime");
                    if (!KEY_CODE.test(keyToCreate) || !["lifetime", "1", "7", "30"].includes(days)) {
                        return jsonResponse({ success: false, message: "Invalid license request." }, 400);
                    }

                    let expiresAt = null;
                    if (days && days !== "lifetime") {
                        const expDate = new Date();
                        expDate.setDate(expDate.getDate() + parseInt(days));
                        expiresAt = expDate.toISOString().replace("T", " ").substring(0, 19);
                    }

                    const keyHash = await sha256(keyToCreate + requiredSecret(env, "KEY_PEPPER"));

                    const encryptedKey = await encryptLicenseKey(keyToCreate, env);
                    const insertResult = await env.DB.prepare(
                        "INSERT INTO license_keys (key_hash, key_encrypted, expires_at, status) VALUES (?, ?, ?, 'unused')"
                    )
                        .bind(keyHash, encryptedKey, expiresAt)
                        .run();

                    return jsonResponse({
                        success: true,
                        key: keyToCreate,
                        id: insertResult && insertResult.meta ? insertResult.meta.last_row_id : null,
                        expires_at: expiresAt
                    });
                } catch (error) {
                    // Keep client errors generic, but preserve the actionable D1/config
                    // failure in Worker logs for operators.
                    console.error("admin_create_key_failed", {
                        name: error && error.name,
                        message: error && error.message,
                    });
                    return jsonResponse({ success: false, message: "Unable to create license." }, 500);
                }
            }

            // REVEAL KEY
            if (url.pathname === "/api/v1/admin/reveal-key" && request.method === "POST") {
                try {
                    const body = await readJson(request);
                    const keyId = Number(body.id);
                    if (!Number.isSafeInteger(keyId) || keyId <= 0) {
                        return jsonResponse({ success: false, message: "Invalid license id." }, 400);
                    }

                    const license = await env.DB.prepare(
                        "SELECT key_encrypted FROM license_keys WHERE id = ? LIMIT 1"
                    ).bind(keyId).first();
                    if (!license) {
                        return jsonResponse({ success: false, message: "License not found." }, 404);
                    }
                    if (!license.key_encrypted) {
                        return jsonResponse({
                            success: false,
                            message: "This older license was stored as a one-way hash and cannot be revealed."
                        }, 409);
                    }

                    const key = await decryptLicenseKey(license.key_encrypted, env);
                    writeAdminAudit(ctx, env, request, "REVEAL_KEY", keyId, "License key revealed from admin dashboard");
                    return jsonResponse({ success: true, key });
                } catch (error) {
                    console.error("admin_reveal_key_failed", { message: error && error.message });
                    return jsonResponse({ success: false, message: "Unable to reveal license key." }, 500);
                }
            }

            // RESET HWID
            if (url.pathname === "/api/v1/admin/reset-hwid" && request.method === "POST") {
                try {
                    const body = await readJson(request);
                    const keyId = Number(body.id);
                    if (!Number.isSafeInteger(keyId) || keyId <= 0) {
                        return jsonResponse({ success: false, message: "Invalid license id." }, 400);
                    }
                    await env.DB.prepare("UPDATE license_keys SET hwid_hash = NULL, hwid_hint = NULL, status = 'unused', license_epoch = license_epoch + 1 WHERE id = ?")
                        .bind(keyId)
                        .run();

                    writeAdminAudit(ctx, env, request, "RESET_HWID", keyId, "HWID reset from admin dashboard");

                    return jsonResponse({ success: true, message: "HWID Reset Successful" });
                } catch (_) {
                    return jsonResponse({ success: false, message: "Unable to reset HWID." }, 500);
                }
            }

            // DELETE KEY
            if (url.pathname === "/api/v1/admin/delete-key" && request.method === "POST") {
                try {
                    const body = await readJson(request);
                    const keyId = Number(body.id);
                    if (!Number.isSafeInteger(keyId) || keyId <= 0) {
                        return jsonResponse({ success: false, message: "Invalid license id." }, 400);
                    }

                    // Keep an audit record in the same D1 transaction before
                    // deleting the license row, so a deleted key is traceable
                    // without retaining the usable license itself.
                    const results = await env.DB.batch([
                        env.DB.prepare(
                            "INSERT INTO admin_audit_logs (admin_id, action, key_hash, previous_epoch, new_epoch, reason, ip_address) " +
                            "SELECT ?, ?, key_hash, license_epoch, license_epoch, ?, ? FROM license_keys WHERE id = ?"
                        ).bind(
                            "dashboard",
                            "DELETE_KEY",
                            "License permanently deleted from admin dashboard",
                            request.headers.get("CF-Connecting-IP") || "0.0.0.0",
                            keyId
                        ),
                        env.DB.prepare("DELETE FROM license_keys WHERE id = ?").bind(keyId)
                    ]);

                    const deletion = results && results[1];
                    if (!deletion || !deletion.meta || deletion.meta.changes !== 1) {
                        return jsonResponse({ success: false, message: "License not found." }, 404);
                    }

                    return jsonResponse({ success: true, message: "License deleted." });
                } catch (error) {
                    console.error("admin_delete_key_failed", { message: error && error.message });
                    return jsonResponse({ success: false, message: "Unable to delete license." }, 500);
                }
            }

            // BAN KEY
            if (url.pathname === "/api/v1/admin/ban-key" && request.method === "POST") {
                try {
                    const body = await readJson(request);
                    const keyId = Number(body.id);
                    const newStatus = String(body.status || "banned").toLowerCase();
                    if (!Number.isSafeInteger(keyId) || keyId <= 0 || !["active", "banned"].includes(newStatus)) {
                        return jsonResponse({ success: false, message: "Invalid status request." }, 400);
                    }
                    await env.DB.prepare("UPDATE license_keys SET status = ?, license_epoch = license_epoch + 1 WHERE id = ?")
                        .bind(newStatus, keyId)
                        .run();

                    writeAdminAudit(ctx, env, request, "SET_STATUS", keyId, `Status changed to ${newStatus}`);

                    return jsonResponse({ success: true, message: "Status Updated" });
                } catch (_) {
                    return jsonResponse({ success: false, message: "Unable to update license status." }, 500);
                }
            }

            // CONFIG MODERATION
            if (url.pathname === "/api/v1/admin/delete-config" && request.method === "POST") {
                try {
                    const body = await readJson(request);
                    const configId = Number(body.id);
                    if (!Number.isSafeInteger(configId) || configId <= 0) {
                        return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                    }
                    const result = await env.DB.prepare("DELETE FROM public_configs WHERE id = ?").bind(configId).run();
                    if (!result.meta || result.meta.changes !== 1) {
                        return jsonResponse({ success: false, message: "Config not found." }, 404);
                    }
                    return jsonResponse({ success: true, message: "Public config deleted." });
                } catch (error) {
                    console.error("admin_delete_config_failed", { message: error && error.message });
                    return jsonResponse({ success: false, message: "Unable to delete config." }, 500);
                }
            }
        }

        // ----------------------------------------------------
        // 3. PUBLIC CONFIGS (/api/v1/configs/*)
        // ----------------------------------------------------
        if (url.pathname.startsWith("/api/v1/configs/")) {
            try {
                await ensureDatabaseSchema(env);
                const retryAfter = await enforceRateLimit(request, env, "configs", 30, 60 * 1000);
                if (retryAfter !== null) {
                    return jsonResponse({ success: false, message: "Too many config requests. Please wait." }, 429);
                }
                if (request.method !== "POST") {
                    return jsonResponse({ success: false, message: "Method not allowed." }, 405);
                }

                const body = await readConfigJson(request);
                const claims = await requireActiveConfigSession(body, env);

                if (url.pathname === "/api/v1/configs/list") {
                    const { results } = await env.DB.prepare(
                        "SELECT id, author_name, name, description, module_count, downloads, created_at, updated_at, " +
                        "CASE WHEN owner_key_hash = ? OR owner_hwid_hash = ? THEN 1 ELSE 0 END AS mine " +
                        "FROM public_configs ORDER BY updated_at DESC, id DESC LIMIT ?"
                    ).bind(claims.key_hash, claims.hwid_hash, MAX_CONFIGS_PER_PAGE).all();
                    return jsonResponse({ success: true, configs: results || [] });
                }

                if (url.pathname === "/api/v1/configs/publish") {
                    const input = validateConfigInput(body);
                    const requestedId = Number(body.id || 0);
                    if (requestedId > 0) {
                        if (!Number.isSafeInteger(requestedId)) {
                            return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                        }
                        const update = await env.DB.prepare(
                            "UPDATE public_configs SET owner_key_hash = ?, owner_hwid_hash = ?, author_name = ?, name = ?, description = ?, payload = ?, module_count = ?, updated_at = CURRENT_TIMESTAMP " +
                            "WHERE id = ? AND (owner_key_hash = ? OR owner_hwid_hash = ?)"
                        ).bind(claims.key_hash, claims.hwid_hash, input.authorName, input.name, input.description,
                            input.payload, input.moduleCount, requestedId, claims.key_hash, claims.hwid_hash).run();
                        if (!update.meta || update.meta.changes !== 1) {
                            return jsonResponse({ success: false, message: "Only the original publisher can update this config." }, 403);
                        }
                        return jsonResponse({ success: true, id: requestedId, message: "Config updated." });
                    }

                    const insert = await env.DB.prepare(
                        "INSERT INTO public_configs (owner_key_hash, owner_hwid_hash, author_name, name, description, payload, module_count) " +
                        "VALUES (?, ?, ?, ?, ?, ?, ?)"
                    ).bind(claims.key_hash, claims.hwid_hash, input.authorName, input.name, input.description,
                        input.payload, input.moduleCount).run();
                    return jsonResponse({
                        success: true,
                        id: insert.meta ? insert.meta.last_row_id : null,
                        message: "Config published publicly."
                    });
                }

                if (url.pathname === "/api/v1/configs/load") {
                    const configId = Number(body.id);
                    if (!Number.isSafeInteger(configId) || configId <= 0) {
                        return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                    }
                    const config = await env.DB.prepare(
                        "SELECT id, author_name, name, description, payload, module_count, downloads, created_at, updated_at " +
                        "FROM public_configs WHERE id = ? LIMIT 1"
                    ).bind(configId).first();
                    if (!config) return jsonResponse({ success: false, message: "Config not found." }, 404);
                    await env.DB.prepare("UPDATE public_configs SET downloads = downloads + 1 WHERE id = ?").bind(configId).run();
                    return jsonResponse({ success: true, config: { ...config, downloads: Number(config.downloads || 0) + 1 } });
                }

                if (url.pathname === "/api/v1/configs/delete") {
                    const configId = Number(body.id);
                    if (!Number.isSafeInteger(configId) || configId <= 0) {
                        return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                    }
                    const deletion = await env.DB.prepare(
                        "DELETE FROM public_configs WHERE id = ? AND (owner_key_hash = ? OR owner_hwid_hash = ?)"
                    ).bind(configId, claims.key_hash, claims.hwid_hash).run();
                    if (!deletion.meta || deletion.meta.changes !== 1) {
                        return jsonResponse({ success: false, message: "Only the original publisher can delete this config." }, 403);
                    }
                    return jsonResponse({ success: true, message: "Config deleted." });
                }

                return jsonResponse({ success: false, message: "Invalid config endpoint." }, 404);
            } catch (error) {
                const message = error && error.message === "Expired or malformed session token"
                    ? "Config session expired. Please re-open the client."
                    : "Config service is temporarily unavailable.";
                console.error("configs_request_failed", { message: error && error.message });
                return jsonResponse({ success: false, message }, 500);
            }
        }

        // ----------------------------------------------------
        // 4. CLIENT AUTH ENDPOINT (/api/v1/auth)
        // ----------------------------------------------------
        if (url.pathname === "/api/v1/auth" && request.method === "POST") {
            try {
                await ensureDatabaseSchema(env);
                const retryAfter = await enforceRateLimit(request, env, "auth", 12, 60 * 1000);
                if (retryAfter !== null) {
                    return jsonResponse({ success: false, message: "Too many authentication attempts. Please wait." }, 429);
                }

                const body = await readJson(request);
                const {
                    hwid,
                    nonce,
                    timestamp,
                    request_type: requestType,
                } = body;

                if (!hwid || !nonce || !timestamp || !requestType ||
                    !HEX_64.test(String(hwid)) || !NONCE.test(String(nonce))) {
                    return jsonResponse({
                        success: false,
                        message: "Missing Parameters",
                    }, 400);
                }

                if (requestType !== "activate" && requestType !== "heartbeat") {
                    return jsonResponse({
                        success: false,
                        message: "Invalid Request Type",
                    }, 400);
                }

                const requestTime = Number(timestamp);
                const now = Date.now();
                const maxClockSkewMs = 2 * 60 * 1000;

                if (!Number.isFinite(requestTime) ||
                    Math.abs(now - requestTime) > maxClockSkewMs) {
                    return jsonResponse({
                        success: false,
                        message: "Authentication Request Expired",
                    }, 408);
                }

                const hwidHash = await sha256(hwid + requiredSecret(env, "HWID_PEPPER"));
                // Display only: this matches the fingerprint shown by the
                // client without exposing the full value in the dashboard.
                const hwidHint = hwid.substring(0, 10) + "..." + hwid.substring(hwid.length - 6);
                const nonceHash = await sha256(nonce);
                let keyHash;
                let heartbeatClaims = null;
                let license;

                if (requestType === "activate") {
                    const key = String(body.key || "").trim();
                    if (!KEY_CODE.test(key)) {
                        return jsonResponse({ success: false, message: "Invalid License Key" }, 401);
                    }
                    keyHash = await sha256(key + requiredSecret(env, "KEY_PEPPER"));
                    license = await env.DB.prepare(
                        "SELECT * FROM license_keys WHERE key_hash = ? LIMIT 1"
                    ).bind(keyHash).first();
                } else {
                    heartbeatClaims = await verifySessionToken(body.session_token, env);
                    if (!constantTimeEqual(heartbeatClaims.hwid_hash, hwidHash)) {
                        return jsonResponse({ success: false, message: "HWID Mismatch!" }, 403);
                    }
                    keyHash = heartbeatClaims.key_hash;
                    license = await env.DB.prepare(
                        "SELECT * FROM license_keys WHERE key_hash = ? LIMIT 1"
                    ).bind(keyHash).first();
                }

                if (!license) {
                    return jsonResponse({ success: false, message: "Invalid License Key" }, 401);
                }

                const status = String(license.status || "unused").toLowerCase();

                if (status === "banned") {
                    return jsonResponse({
                        success: false,
                        message: "License Key is Banned",
                    }, 403);
                }

                if (status === "expired") {
                    return jsonResponse({
                        success: false,
                        message: "License Expired",
                    }, 403);
                }

                if (license.expires_at) {
                    const expiryTime = new Date(license.expires_at).getTime();
                    if (!Number.isFinite(expiryTime) || now > expiryTime) {
                        try {
                            await env.DB.prepare(
                                "UPDATE license_keys SET status = 'expired', license_epoch = license_epoch + 1 WHERE id = ? AND status != 'expired'"
                            ).bind(license.id).run();
                        } catch (error) {
                            try {
                                await env.DB.prepare(
                                    "UPDATE license_keys SET status = 'expired' WHERE id = ?"
                                ).bind(license.id).run();
                            } catch (_) { }
                        }

                        return jsonResponse({
                            success: false,
                            message: "License Expired",
                        }, 403);
                    }
                }

                const currentBoundHwid =
                    String(license.hwid_hash || license.hwid || "");
                const hwidMatches =
                    currentBoundHwid === hwidHash || currentBoundHwid === hwid;

                if (requestType === "heartbeat") {
                    // Heartbeats are never allowed to activate or re-bind a license.
                    // This prevents an old device from reclaiming a key after an admin reset.
                    if (status !== "active" || !currentBoundHwid) {
                        return jsonResponse({
                            success: false,
                            message: "Session Revoked",
                        }, 403);
                    }

                    if (!hwidMatches) {
                        try {
                            ctx.waitUntil(
                                env.DB.prepare(
                                    "INSERT INTO auth_logs (key_hash, hwid_hash, ip_address, status) VALUES (?, ?, ?, ?)"
                                ).bind(
                                    keyHash,
                                    hwidHash,
                                    request.headers.get("CF-Connecting-IP") || "0.0.0.0",
                                    "HEARTBEAT_HWID_MISMATCH"
                                ).run()
                            );
                        } catch (_) { }

                        return jsonResponse({
                            success: false,
                            message: "HWID Mismatch! Key bound to another device.",
                        }, 403);
                    }
                } else {
                    // Initial activation or normal re-login.
                    if (status === "active") {
                        if (!currentBoundHwid || !hwidMatches) {
                            return jsonResponse({
                                success: false,
                                message: "HWID Mismatch! Key bound to another device.",
                            }, 403);
                        }
                    } else if (status === "unused") {
                        if (currentBoundHwid) {
                            if (!hwidMatches) {
                                return jsonResponse({
                                    success: false,
                                    message: "HWID Mismatch! Key bound to another device.",
                                }, 403);
                            }

                            // Reactivate only if the same HWID is still bound at write time.
                            let reactivateResult;
                            try {
                                reactivateResult = await env.DB.prepare(
                                    "UPDATE license_keys SET status = 'active' WHERE id = ? AND status = 'unused' AND (hwid_hash = ? OR hwid = ?)"
                                ).bind(license.id, hwidHash, hwid).run();
                            } catch (error) {
                                try {
                                    reactivateResult = await env.DB.prepare(
                                        "UPDATE license_keys SET status = 'active' WHERE id = ? AND status = 'unused' AND hwid_hash = ?"
                                    ).bind(license.id, hwidHash).run();
                                } catch (_) {
                                    reactivateResult = await env.DB.prepare(
                                        "UPDATE license_keys SET status = 'active' WHERE id = ? AND status = 'unused' AND hwid = ?"
                                    ).bind(license.id, hwid).run();
                                }
                            }

                            if (!reactivateResult ||
                                !reactivateResult.meta ||
                                reactivateResult.meta.changes !== 1) {
                                return jsonResponse({
                                    success: false,
                                    message: "License State Changed. Please retry.",
                                }, 409);
                            }
                        } else {
                            // First activation is one conditional write. Two machines cannot
                            // both win the initial HWID bind.
                            let bindResult;
                            try {
                                bindResult = await env.DB.prepare(
                                    "UPDATE license_keys SET hwid_hash = ?, hwid_hint = ?, status = 'active', activated_at = COALESCE(activated_at, CURRENT_TIMESTAMP) WHERE id = ? AND status = 'unused' AND (hwid_hash IS NULL OR hwid_hash = '') AND (hwid IS NULL OR hwid = '')"
                                ).bind(hwidHash, hwidHint, license.id).run();
                            } catch (error) {
                                try {
                                    bindResult = await env.DB.prepare(
                                        "UPDATE license_keys SET hwid_hash = ?, hwid_hint = ?, status = 'active', activated_at = COALESCE(activated_at, CURRENT_TIMESTAMP) WHERE id = ? AND status = 'unused' AND (hwid_hash IS NULL OR hwid_hash = '')"
                                    ).bind(hwidHash, hwidHint, license.id).run();
                                } catch (_) {
                                    bindResult = await env.DB.prepare(
                                        "UPDATE license_keys SET hwid = ?, status = 'active' WHERE id = ? AND status = 'unused' AND (hwid IS NULL OR hwid = '')"
                                    ).bind(hwid, license.id).run();
                                }
                            }

                            if (!bindResult ||
                                !bindResult.meta ||
                                bindResult.meta.changes !== 1) {
                                return jsonResponse({
                                    success: false,
                                    message: "License Activation Conflict. Please retry.",
                                }, 409);
                            }
                        }
                    } else {
                        return jsonResponse({
                            success: false,
                            message: "License is not active.",
                        }, 403);
                    }
                }

                // Upgrade a legacy raw binding only after it has matched the
                // current device. The stored value remains a peppered hash;
                // the UI receives only the compact display hint.
                if (hwidMatches &&
                    (String(license.hwid_hash || "") !== hwidHash ||
                        String(license.hwid_hint || "") !== hwidHint)) {
                    try {
                        await env.DB.prepare(
                            "UPDATE license_keys SET hwid_hash = ?, hwid_hint = ? WHERE id = ?"
                        ).bind(hwidHash, hwidHint, license.id).run();
                        license.hwid_hash = hwidHash;
                        license.hwid_hint = hwidHint;
                    } catch (error) {
                        console.error("hwid_binding_migration_failed", { message: error && error.message });
                    }
                }

                // Re-read the epoch after any activation write so the token reflects
                // the current server state instead of the earlier SELECT snapshot.
                let currentEpoch = Number(license.license_epoch || 1);
                try {
                    const epochRow = await env.DB.prepare(
                        "SELECT license_epoch, status FROM license_keys WHERE id = ? LIMIT 1"
                    ).bind(license.id).first();

                    if (!epochRow || String(epochRow.status).toLowerCase() !== "active") {
                        return jsonResponse({
                            success: false,
                            message: "Session Revoked",
                        }, 403);
                    }

                    currentEpoch = Number(epochRow.license_epoch || 1);
                } catch (_) { }

                if (!Number.isInteger(currentEpoch) || currentEpoch <= 0) {
                    return jsonResponse({
                        success: false,
                        message: "Invalid License Epoch",
                    }, 500);
                }

                if (heartbeatClaims && heartbeatClaims.license_epoch !== currentEpoch) {
                    return jsonResponse({
                        success: false,
                        message: "Session Revoked",
                    }, 403);
                }

                const issuedAt = Date.now();
                const sessionExpiresAt = issuedAt + SESSION_TTL_MS;

                const payload = {
                    success: true,
                    session_id: await sha256(
                        keyHash + nonceHash + issuedAt + requestType
                    ),
                    key_hash: keyHash,
                    hwid_hash: hwidHash,
                    nonce_hash: nonceHash,
                    aud: AUTH_AUDIENCE,
                    license_epoch: currentEpoch,
                    iat: issuedAt,
                    exp: sessionExpiresAt,
                    expires_at: license.expires_at || "lifetime",
                };

                try {
                    ctx.waitUntil(
                        env.DB.prepare(
                            "INSERT INTO auth_logs (key_hash, hwid_hash, ip_address, status) VALUES (?, ?, ?, ?)"
                        ).bind(
                            keyHash,
                            hwidHash,
                            request.headers.get("CF-Connecting-IP") || "0.0.0.0",
                            requestType === "heartbeat" ? "HEARTBEAT_SUCCESS" : "SUCCESS"
                        ).run()
                    );
                } catch (_) { }

                return jsonResponse(await signPayload(payload, env));
            } catch (error) {
                // The native client receives only a generic failure. Keep the
                // operator-visible cause in Worker logs without leaking it.
                console.error("client_auth_failed", {
                    name: error && error.name,
                    message: error && error.message,
                });
                return jsonResponse({
                    success: false,
                    message: "Authentication service unavailable.",
                }, 500);
            }
        }

        return jsonResponse({ error: "Invalid Endpoint" }, 404);
    },
};
