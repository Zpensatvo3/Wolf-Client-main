import assert from "node:assert/strict";
import { test } from "node:test";
import { deviceMessage, verifyDeviceProof } from "../server/device-proof.mjs";

test("device proof binds the key, credential, nonce, HWID, time and operation", async () => {
    const pair = await crypto.subtle.generateKey({ name: "ECDSA", namedCurve: "P-256" }, true, ["sign", "verify"]);
    const key = Buffer.from(await crypto.subtle.exportKey("raw", pair.publicKey)).toString("base64url");
    const body = { key: "TEST-LICENSE-1234", hwid: "a".repeat(64), nonce: "b".repeat(32),
        timestamp: 1800000000000, request_type: "activate", device_key: key };
    body.device_signature = Buffer.from(await crypto.subtle.sign({ name: "ECDSA", hash: "SHA-256" },
        pair.privateKey, new TextEncoder().encode(await deviceMessage(body)))).toString("base64url");
    assert.match(await verifyDeviceProof(body), /^[a-f0-9]{64}$/);
    for (const change of [{ key: "OTHER-LICENSE-1234" }, { hwid: "c".repeat(64) },
        { nonce: "d".repeat(32) }, { timestamp: body.timestamp + 1 },
        { request_type: "heartbeat", session_token: "different-token" },
        { device_signature: "A".repeat(86) }, { device_key: key + "=" }])
        await assert.rejects(verifyDeviceProof({ ...body, ...change }));
});
