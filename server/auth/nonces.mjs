import { sha256 } from '../core/crypto.mjs';

// Timestamp validation is necessary but not sufficient: consume the signed
// request's nonce atomically in strongly consistent D1, not eventually consistent KV.
export async function consumeAuthNonce(env, keyHash, deviceHash, type, nonce, now) {
    const hash = await sha256([type,keyHash,deviceHash,nonce].join(':'));
    const result = await env.DB.batch([
        env.DB.prepare('DELETE FROM auth_request_nonces WHERE exp <= ?').bind(now),
        env.DB.prepare('INSERT INTO auth_request_nonces (nonce_hash,exp) VALUES (?,?) ON CONFLICT(nonce_hash) DO NOTHING')
            .bind(hash,now + 5*60*1000)
    ]);
    return result[1]?.meta?.changes === 1;
}
