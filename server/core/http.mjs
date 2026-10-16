export const MAX_JSON_BYTES = 4096;
export const MAX_CONFIG_JSON_BYTES = 96 * 1024;

export function jsonResponse(data, status = 200) {
    return new Response(status === 204 ? null : JSON.stringify(data), {
        status: status,
        headers: {
            "Content-Type": "application/json; charset=utf-8",
            "Cache-Control": "no-store",
            "X-Content-Type-Options": "nosniff",
        },
    });
}

export async function readBoundedJson(request, maximum) {
    const contentType = request.headers.get("Content-Type") || "";
    if (!contentType.toLowerCase().includes("application/json")) {
        throw new Error("Content-Type must be application/json");
    }
    const contentLength = Number(request.headers.get("Content-Length") || "0");
    if (contentLength > maximum || !request.body) throw new Error("Request body too large or empty");
    const reader = request.body.getReader();
    const chunks = [];
    let length = 0;
    try {
        for (;;) {
            const { value, done } = await reader.read();
            if (done) break;
            length += value.byteLength;
            if (length > maximum) {
                await reader.cancel();
                throw new Error("Request body too large");
            }
            chunks.push(value);
        }
    } finally { reader.releaseLock(); }
    if (!length) throw new Error("Request body empty");
    const body = new Uint8Array(length);
    let offset = 0;
    for (const chunk of chunks) { body.set(chunk, offset); offset += chunk.byteLength; }
    const parsed = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(body));
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) throw new Error("Invalid request object");
    return parsed;
}

export async function readJson(request) { return readBoundedJson(request, MAX_JSON_BYTES); }
export async function readConfigJson(request) { return readBoundedJson(request, MAX_CONFIG_JSON_BYTES); }
