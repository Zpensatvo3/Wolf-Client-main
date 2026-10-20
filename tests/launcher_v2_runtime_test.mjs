import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {harness} from './helpers/auth-harness.mjs';
import {proofMessage} from '../server/auth/protocol-v2.mjs';
import {base64UrlEncode,base64UrlDecode} from '../server/core/crypto.mjs';
const require=createRequire(new URL('../server/package.json',import.meta.url));
const {Miniflare,convertV4MiniflareOptions}=require('miniflare');
const {buildSync}=require('esbuild');
test('workerd v2: real D1 concurrent redemption, rotating runtime lease and revocation',{timeout:60000},async()=>{
    const h=await harness();
    const script=buildSync({entryPoints:[fileURLToPath(new URL('../server/worker.js',import.meta.url))],bundle:true,
        format:'esm',platform:'neutral',target:'es2022',conditions:['workerd','worker','browser'],
        external:['node:*','cloudflare:*'],minify:true,keepNames:false,write:false}).outputFiles[0].text;
    const bindings=Object.fromEntries(['KEY_PEPPER','HWID_PEPPER','SIGNING_PRIVATE_KEY_PKCS8','SIGNING_PUBLIC_KEY_SPKI',
        'ADMIN_ACCESS_TEAM_DOMAIN','ADMIN_ACCESS_AUD','ADMIN_ACCESS_EMAIL'].map(k=>[k,h.env[k]]));
    const runtime=new Miniflare(convertV4MiniflareOptions({modules:true,script,compatibilityDate:'2026-10-16',
        compatibilityFlags:['nodejs_compat'],bindings:{...bindings,AUTH_V2_ONLY:'true',AUTH_V2_RELEASES:'true'},
        d1Databases:['DB'],kvNamespaces:['RELEASES'],
        durableObjects:{RATE_LIMITER:{className:'RateLimiter',useSQLite:true}},
        outboundService:async request=>request.url===h.env.ADMIN_ACCESS_TEAM_DOMAIN+'/cdn-cgi/access/certs'
            ?Response.json(h.access.jwks):new Response('Denied',{status:403})}));
    try {
        const db=await runtime.getD1Database('DB');
        const schema=readFileSync(new URL('../server/schema.sql',import.meta.url),'utf8').replace(/^--.*$/gm,'');
        await db.batch(schema.split(';').map(s=>s.trim()).filter(Boolean).map(sql=>db.prepare(sql)));
        const request=(path,body)=>runtime.dispatchFetch('https://local.test'+path,{method:'POST',
            headers:{...h.access.header,Origin:'https://local.test','Content-Type':'application/json'},body:JSON.stringify(body)});
        const created=await (await request('/api/v1/admin/create-key',{days:'1'})).json();assert.ok(created.key);
        const key=await crypto.subtle.generateKey({name:'ECDSA',namedCurve:'P-256'},true,['sign','verify']);
        const device_key=base64UrlEncode(await crypto.subtle.exportKey('raw',key.publicKey));
        async function call(path,type,credential,fields={}) {
            const body={request_type:type,hwid:'a'.repeat(64),nonce:crypto.randomUUID().replaceAll('-',''),
                timestamp:Date.now(),fields:JSON.stringify(fields),device_key,
                [type==='activate'?'key':type==='redeem'?'ticket':'session_token']:credential};
            body.device_signature=base64UrlEncode(await crypto.subtle.sign({name:'ECDSA',hash:'SHA-256'},key.privateKey,
                new TextEncoder().encode(await proofMessage(path,body))));return request(path,body);
        }
        const login=await (await call('/api/v2/launcher/login','activate',created.key)).json();assert.ok(login.session_token);
        const f={pid:321,process_nonce:'b'.repeat(32),minecraft_target:'1.21.11',release_version:'runtime-test',release_sha256:'c'.repeat(64)};
        await db.prepare('INSERT INTO approved_releases(sha256,version,minecraft_target,bytes,created_at) VALUES(?,?,?,?,?)')
            .bind(f.release_sha256,f.release_version,f.minecraft_target,4096,Date.now()).run();
        await db.prepare('INSERT INTO release_channels(minecraft_target,sha256) VALUES(?,?)').bind(f.minecraft_target,f.release_sha256).run();
        const envelope=await (await call('/api/v2/launcher/ticket','ticket',login.session_token,f)).json();
        const ticket=JSON.parse(new TextDecoder().decode(base64UrlDecode(envelope.payload))).ticket;
        const redemptions=await Promise.all([call('/api/v2/runtime/redeem','redeem',ticket,f),call('/api/v2/runtime/redeem','redeem',ticket,f)]);
        assert.deepEqual(redemptions.map(r=>r.status).sort(),[200,403]);
        const active=await redemptions.find(r=>r.status===200).json();
        const claims=JSON.parse(new TextDecoder().decode(base64UrlDecode(active.payload)));
        const renewFields={...f,session_id:claims.session_id,sequence:0};
        const renewals=await Promise.all([call('/api/v2/runtime/renew','renew',active.session_token,renewFields),
            call('/api/v2/runtime/renew','renew',active.session_token,renewFields)]);
        assert.deepEqual(renewals.map(r=>r.status).sort(),[200,403]);
        const next=await renewals.find(r=>r.status===200).json();
        await db.prepare('UPDATE approved_releases SET revoked=1 WHERE sha256=?').bind(f.release_sha256).run();
        assert.equal((await call('/api/v2/runtime/renew','renew',next.session_token,{...renewFields,sequence:1})).status,403);
        assert.equal((await request('/api/v1/auth',{})).status,410);
    } finally {await runtime.dispose();await h.close();}
});
