import { sha256, base64UrlDecode, base64UrlEncode } from '../core/crypto.mjs';

export const LAUNCHER_AUDIENCE = 'onestep-launcher-v2';
export const RUNTIME_AUDIENCE = 'onestep-runtime-v2';
export const RELEASE_AUDIENCE = 'onestep-release-v2';
export const LEASE_MS = 60_000;
const HEX = /^[a-f0-9]{64}$/;
const NONCE = /^[a-f0-9]{32}$/;
const TYPES = new Set(['activate','manifest','download','ticket','redeem','renew']);

// Fields is the exact bounded UTF-8 JSON string sent by both implementations.
// Its digest binds every operation-specific value, without JSON canonicalization
// ambiguities between C++ and JavaScript. No unsigned field affects decisions.
export async function proofMessage(path, body) {
    const credential = body.request_type === 'activate' ? body.key :
        body.request_type === 'redeem' ? body.ticket : body.session_token;
    if (!/^\/api\/v2\/(launcher|runtime)\/[a-z]+$/.test(path) || !TYPES.has(body.request_type) ||
        !HEX.test(body.hwid || '') || !NONCE.test(body.nonce || '') ||
        !Number.isSafeInteger(body.timestamp) || body.timestamp <= 0 ||
        typeof credential !== 'string' || !credential.length || credential.length > 16384 ||
        typeof body.fields !== 'string' || body.fields.length > 8192) throw Error('Invalid proof');
    return ['onestep-device-v2','POST',path,body.request_type,body.hwid,body.nonce,
        String(body.timestamp),await sha256(credential),await sha256(body.fields)].join('\n');
}

export async function verifyProofV2(path, body) {
    const message = await proofMessage(path, body);
    const point = base64UrlDecode(body.device_key), signature = base64UrlDecode(body.device_signature);
    if (point.length !== 65 || point[0] !== 4 || signature.length !== 64 ||
        base64UrlEncode(point) !== body.device_key || base64UrlEncode(signature) !== body.device_signature)
        throw Error('Invalid proof encoding');
    const key = await crypto.subtle.importKey('raw', point, {name:'ECDSA',namedCurve:'P-256'}, false, ['verify']);
    if (!await crypto.subtle.verify({name:'ECDSA',hash:'SHA-256'}, key, signature,
        new TextEncoder().encode(message))) throw Error('Invalid device signature');
    const fields = JSON.parse(body.fields);
    if (!fields || typeof fields !== 'object' || Array.isArray(fields)) throw Error('Invalid fields');
    return {deviceHash:await sha256(body.device_key),requestHash:await sha256(message),fields};
}
