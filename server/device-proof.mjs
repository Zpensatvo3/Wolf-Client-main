const encoder = new TextEncoder();
const HEX64 = /^[a-f0-9]{64}$/i;
const NONCE = /^[a-f0-9]{32}$/i;
const URL64 = /^[A-Za-z0-9_-]+$/;

export async function hashText(text) {
    return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", encoder.encode(text))),
        byte => byte.toString(16).padStart(2, "0")).join("");
}

export async function deviceMessage(body) {
    const type = body.request_type;
    const credential = type === "activate" ? body.key : type === "redeem" ? body.ticket : body.session_token;
    if (!["activate", "heartbeat", "redeem"].includes(type) || !HEX64.test(body.hwid || "") ||
        !NONCE.test(body.nonce || "") || !Number.isSafeInteger(body.timestamp) || body.timestamp <= 0 ||
        typeof credential !== "string" || !credential.length || credential.length > 16384)
        throw new Error("Invalid device proof fields");
    return "onestep-device-v1\n" + type + "\n" + body.hwid + "\n" + body.nonce + "\n" +
        body.timestamp + "\n" + await hashText(credential);
}

function decode(value, size) {
    if (typeof value !== "string" || !URL64.test(value) || value.length !== Math.ceil(size * 8 / 6))
        throw new Error("Invalid device proof encoding");
    const data = Uint8Array.from(atob(value.replace(/-/g, "+").replace(/_/g, "/") +
        "=".repeat((4 - value.length % 4) % 4)), c => c.charCodeAt(0));
    if (data.length !== size) throw new Error("Invalid device proof size");
    // Require a canonical key encoding so equivalent points cannot get different hashes.
    let binary = "";
    for (const byte of data) binary += String.fromCharCode(byte);
    const canonical = btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
    if (canonical !== value) throw new Error("Noncanonical device key");
    return data;
}

export async function verifyDeviceProof(body) {
    const point = decode(body.device_key, 65);
    const signature = decode(body.device_signature, 64);
    if (point[0] !== 4) throw new Error("Invalid public point");
    const key = await crypto.subtle.importKey("raw", point, { name: "ECDSA", namedCurve: "P-256" }, false, ["verify"]);
    if (!await crypto.subtle.verify({ name: "ECDSA", hash: "SHA-256" }, key, signature,
        encoder.encode(await deviceMessage(body)))) throw new Error("Invalid device signature");
    return hashText(body.device_key);
}
