const HEX_64 = /^[a-f0-9]{64}$/i;
const KEY_CODE = /^[A-Za-z0-9-]{8,128}$/;

export function requiredSecret(env, name) {
    const value = env[name];
    if (typeof value !== "string" || value.length < 32) {
        throw new Error(`Server configuration error: missing ${name}`);
    }
    return value;
}

export function base64UrlEncode(bytes) {
    let binary = "";
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    for (let i = 0; i < view.length; i++) binary += String.fromCharCode(view[i]);
    return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
}

export function base64UrlDecode(value) {
    if (typeof value !== "string" || !/^[A-Za-z0-9_-]+$/.test(value) || value.length > 16384) {
        throw new Error("Invalid base64url data");
    }
    const padded = value.replace(/-/g, "+").replace(/_/g, "/") + "=".repeat((4 - value.length % 4) % 4);
    const binary = atob(padded);
    return Uint8Array.from(binary, (ch) => ch.charCodeAt(0));
}

export function constantTimeEqual(left, right) {
    if (typeof left !== "string" || typeof right !== "string" || left.length !== right.length) return false;
    let difference = 0;
    for (let i = 0; i < left.length; i++) difference |= left.charCodeAt(i) ^ right.charCodeAt(i);
    return difference === 0;
}

export async function sha256(str) {
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
export async function licenseCipher(env) {
    const material = await crypto.subtle.digest(
        "SHA-256",
        new TextEncoder().encode(requiredSecret(env, "KEY_PEPPER"))
    );
    return crypto.subtle.importKey("raw", material, { name: "AES-GCM" }, false, ["encrypt", "decrypt"]);
}

export async function encryptLicenseKey(key, env) {
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

export async function decryptLicenseKey(packedValue, env) {
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
