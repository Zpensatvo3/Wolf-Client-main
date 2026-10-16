export const SESSION_TTL_MS = 45 * 60 * 1000;

export function licenseExpiryMs(value) {
    if (value === null || value === undefined || value === "") return null;
    if (typeof value !== "string") return NaN;
    // D1's historical DATETIME rows are UTC, without an ISO timezone suffix.
    const date = /^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(value)
        ? value.replace(" ", "T") + "Z" : value;
    return Date.parse(date);
}

export function sessionExpiryMs(now, licenseExpiry) {
    if (!Number.isSafeInteger(now) || now <= 0 ||
        (licenseExpiry !== null && (!Number.isSafeInteger(licenseExpiry) || licenseExpiry <= now)))
        return null;
    return Math.min(now + SESSION_TTL_MS, licenseExpiry ?? Infinity);
}

export function validSessionTimes(payload, now) {
    return Number.isSafeInteger(payload.iat) && Number.isSafeInteger(payload.exp) &&
        payload.iat > 0 && payload.iat <= now + 30_000 && payload.exp > now &&
        payload.exp > payload.iat && payload.exp - payload.iat <= SESSION_TTL_MS;
}
