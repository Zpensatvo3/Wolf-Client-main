// The legacy API remains independent. Only browser navigation moves to Onestep.
// Used as an additional module alongside the original deployed legacy worker.
export function redirectLegacyAdmin(request) {
    const path = new URL(request.url).pathname;
    if ((request.method === 'GET' || request.method === 'HEAD') &&
        (path === '/' || path === '/admin' || path === '/admin/')) {
        // Never forward query strings, fragments or credentials to the new host.
        return new Response(null, {
            status: 302,
            headers: {
                Location: 'https://onestep-auth.gogogamesas.workers.dev/admin',
                'Cache-Control': 'no-store',
                'Referrer-Policy': 'no-referrer'
            }
        });
    }
    return null;
}
