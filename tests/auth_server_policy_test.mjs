import assert from "node:assert/strict";
import { test } from "node:test";
import { SESSION_TTL_MS, licenseExpiryMs, sessionExpiryMs, validSessionTimes } from "../server/session-policy.mjs";

test("session expiry is bounded by both license and token lifetime", () => {
    const now = 1_800_000_000_000;
    assert.equal(sessionExpiryMs(now, null), now + SESSION_TTL_MS);
    assert.equal(sessionExpiryMs(now, now + 500), now + 500);
    assert.equal(sessionExpiryMs(now, now + SESSION_TTL_MS * 2), now + SESSION_TTL_MS);
    for (const invalid of [now, now - 1, NaN, Infinity, "lifetime"])
        assert.equal(sessionExpiryMs(now, invalid), null);
    assert.ok(validSessionTimes({ iat: now, exp: now + SESSION_TTL_MS }, now));
    for (const claims of [
        { iat: now, exp: now }, { iat: now, exp: now + SESSION_TTL_MS + 1 },
        { iat: now + 30_001, exp: now + SESSION_TTL_MS },
        { iat: 0, exp: now + 1 }, { iat: now, exp: Infinity },
    ]) assert.ok(!validSessionTimes(claims, now));
});

test("historical D1 dates are UTC and invalid dates fail closed", () => {
    assert.equal(licenseExpiryMs(null), null);
    assert.equal(licenseExpiryMs("2026-10-16 12:00:00"), Date.parse("2026-10-16T12:00:00Z"));
    assert.equal(licenseExpiryMs("2026-10-16T12:00:00Z"), Date.parse("2026-10-16T12:00:00Z"));
    assert.ok(Number.isNaN(licenseExpiryMs("broken")));
    assert.ok(Number.isNaN(licenseExpiryMs(12)));
});
