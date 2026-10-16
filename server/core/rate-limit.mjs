import { jsonResponse } from './http.mjs';

export class RateLimiter {
    constructor(state) {
        this.state = state;
    }

    async fetch(request) {
        const { limit, windowMs } = await request.json();
        return this.state.storage.transaction(async storage => {
            const now = Date.now();
            const existing = await storage.get("bucket");
            const fresh = !existing || existing.windowStart + windowMs <= now;
            const windowStart = fresh ? now : existing.windowStart;
            const count = fresh ? 1 : existing.count + 1;
            await storage.put("bucket", { windowStart, count });
            return jsonResponse({ allowed: count <= limit, retryAfterMs: Math.max(0, windowStart + windowMs - now) });
        });
    }
}

export async function enforceRateLimit(request, env, scope, limit, windowMs) {
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
