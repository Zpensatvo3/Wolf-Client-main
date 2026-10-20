import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
const require = createRequire(new URL('../../server/package.json',import.meta.url));
const {SignJWT,exportJWK} = await import(pathToFileURL(require.resolve('jose')).href);
const fixtures = new Map();
const originalFetch = globalThis.fetch;
// Serve only locally generated test JWKS. No production Access tokens or keys.
globalThis.fetch = async (input,options) => {
    const url=String(input instanceof Request ? input.url : input);
    if (fixtures.has(url)) return Response.json(fixtures.get(url));
    return originalFetch(input,options);
};
export async function accessFixture() {
    const pair=await crypto.subtle.generateKey({name:'RSASSA-PKCS1-v1_5',modulusLength:2048,
        publicExponent:new Uint8Array([1,0,1]),hash:'SHA-256'},true,['sign','verify']);
    const issuer='https://test-'+crypto.randomUUID()+'.cloudflareaccess.com';
    const audience='a'.repeat(64), email='operator@example.com', kid=crypto.randomUUID();
    const jwks={keys:[{...await exportJWK(pair.publicKey),kid,alg:'RS256',use:'sig'}]};
    fixtures.set(issuer+'/cdn-cgi/access/certs',jwks);
    const token=async (claims={})=> new SignJWT({email,iss:issuer,aud:audience,sub:'local-test-operator',
        iat:Math.floor(Date.now()/1000),exp:Math.floor(Date.now()/1000)+300,...claims})
        .setProtectedHeader({alg:'RS256',kid}).sign(pair.privateKey);
    return { env:{ADMIN_ACCESS_TEAM_DOMAIN:issuer,ADMIN_ACCESS_AUD:audience,ADMIN_ACCESS_EMAIL:email},
        header:{'Cf-Access-Jwt-Assertion':await token()},token,jwks };
}
