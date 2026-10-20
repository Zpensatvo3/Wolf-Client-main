import { createRemoteJWKSet, jwtVerify } from 'jose';

// Cache public verification material only, never an identity or authorization decision.
const resolvers = new Map();
export async function verifyAdminAccess(request, env) {
    const issuer = String(env.ADMIN_ACCESS_TEAM_DOMAIN || '');
    const audience = String(env.ADMIN_ACCESS_AUD || '');
    const email = String(env.ADMIN_ACCESS_EMAIL || '').trim().toLowerCase();
    if (!/^https:\/\/[a-z0-9](?:[a-z0-9-]{0,62})\.cloudflareaccess\.com$/.test(issuer) ||
        !/^[a-f0-9]{64}$/i.test(audience) || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email))
        throw new Error('Admin Access is not configured');
    const token = request.headers.get('Cf-Access-Jwt-Assertion');
    if (!token || token.length > 16384) return false;
    try {
        let keys = resolvers.get(issuer);
        if (!keys) {
            keys = createRemoteJWKSet(new URL(issuer+'/cdn-cgi/access/certs'), { timeoutDuration:3000 });
            if (resolvers.size >= 8) resolvers.delete(resolvers.keys().next().value);
            resolvers.set(issuer,keys);
        }
        const {payload} = await jwtVerify(token,keys,{
            issuer,audience,algorithms:['RS256'],clockTolerance:5,maxTokenAge:'1h',
            requiredClaims:['exp','iat','sub','email']
        });
        return typeof payload.sub==='string' && payload.sub.length>0 &&
            typeof payload.email==='string' && payload.email.toLowerCase()===email;
    } catch (_) { return false; }
}
