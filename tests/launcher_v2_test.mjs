import {test} from 'node:test';
import assert from 'node:assert/strict';
import {harness} from './helpers/auth-harness.mjs';
import {base64UrlEncode,base64UrlDecode,sha256} from '../server/core/crypto.mjs';
import {proofMessage} from '../server/auth/protocol-v2.mjs';

async function fixture() {
    const h=await harness();h.env.AUTH_V2_ONLY='true';h.env.AUTH_V2_RELEASES='true';
    const pair=await crypto.subtle.generateKey({name:'ECDSA',namedCurve:'P-256'},true,['sign','verify']);
    const publicKey=base64UrlEncode(await crypto.subtle.exportKey('raw',pair.publicKey));
    const hwid=await sha256(crypto.randomUUID());
    async function proof(path,type,credential,fields={}) {
        const b={request_type:type,hwid,nonce:crypto.randomUUID().replaceAll('-',''),timestamp:Date.now(),
            fields:JSON.stringify(fields),device_key:publicKey,
            [type==='activate'?'key':type==='redeem'?'ticket':'session_token']:credential};
        b.device_signature=base64UrlEncode(await crypto.subtle.sign({name:'ECDSA',hash:'SHA-256'},pair.privateKey,
            new TextEncoder().encode(await proofMessage(path,b))));return b;
    }
    async function call(path,type,credential,fields={}) {return h.request(path,await proof(path,type,credential,fields));}
    async function envelope(response) {
        assert.equal(response.status,200,await response.clone().text());const e=await response.json();
        const key=await crypto.subtle.importKey('spki',base64UrlDecode(h.env.SIGNING_PUBLIC_KEY_SPKI),
            {name:'ECDSA',namedCurve:'P-256'},false,['verify']);
        assert.ok(await crypto.subtle.verify({name:'ECDSA',hash:'SHA-256'},key,base64UrlDecode(e.signature),base64UrlDecode(e.payload)));
        return {...e,claims:JSON.parse(new TextDecoder().decode(base64UrlDecode(e.payload)))};
    }
    const created=await (await h.request('/api/v1/admin/create-key',{days:'7'})).json();assert.ok(created.key);
    const login=await envelope(await call('/api/v2/launcher/login','activate',created.key));
    const hash='a'.repeat(64),version='test-v2';
    await h.env.DB.prepare('INSERT INTO approved_releases(sha256,version,minecraft_target,bytes,created_at) VALUES(?,?,?,?,?)')
        .bind(hash,version,'1.21.11',4096,Date.now()).run();
    await h.env.DB.prepare('INSERT INTO release_channels(minecraft_target,sha256) VALUES(?,?)').bind('1.21.11',hash).run();
    h.releases.set('dll/'+hash,new Uint8Array(4096));
    const fields={pid:1234,process_nonce:'b'.repeat(32),minecraft_target:'1.21.11',release_sha256:hash,release_version:version};
    return {...h,proof,call,envelope,login,fields,created};
}

test('v2: separated audiences, one-use concurrent redeem, bound rotating lease and replay rejection',async()=>{
    const h=await fixture();try {
        assert.equal(h.login.claims.aud,'onestep-launcher-v2');
        assert.equal((await h.request('/api/v1/configs/list',{session_token:h.login.session_token})).status,403);
        const manifest=await h.envelope(await h.call('/api/v2/launcher/manifest','manifest',h.login.session_token,{minecraft_target:'1.21.11'}));
        assert.equal(manifest.claims.aud,'onestep-release-v2');
        const ticket=await h.envelope(await h.call('/api/v2/launcher/ticket','ticket',h.login.session_token,h.fields));
        const redeem=await h.proof('/api/v2/runtime/redeem','redeem',ticket.claims.ticket,h.fields);
        const attempts=await Promise.all([h.request('/api/v2/runtime/redeem',redeem),h.request('/api/v2/runtime/redeem',redeem)]);
        assert.deepEqual(attempts.map(r=>r.status).sort(),[200,403]);
        const runtime=await h.envelope(attempts.find(r=>r.status===200));
        assert.equal(runtime.claims.aud,'onestep-runtime-v2');assert.equal(runtime.claims.sequence,0);
        assert.ok(runtime.claims.exp-runtime.claims.iat<=60000);
        assert.equal((await h.request('/api/v1/configs/list',{session_token:runtime.session_token})).status,200);
        assert.equal((await h.call('/api/v2/launcher/ticket','ticket',runtime.session_token,h.fields)).status,403);
        const fields={...h.fields,session_id:runtime.claims.session_id,sequence:0};
        const wrong=await h.call('/api/v2/runtime/renew','renew',runtime.session_token,{...fields,pid:5678});assert.equal(wrong.status,403);
        const renew=await h.proof('/api/v2/runtime/renew','renew',runtime.session_token,fields);
        const next=await h.envelope(await h.request('/api/v2/runtime/renew',renew));
        assert.equal(next.claims.sequence,1);assert.equal(next.claims.process_nonce,h.fields.process_nonce);
        assert.equal(next.claims.release_sha256,h.fields.release_sha256);
        assert.equal((await h.request('/api/v2/runtime/renew',renew)).status,403);
        assert.equal((await h.request('/api/v1/configs/list',{session_token:runtime.session_token})).status,403);
        assert.equal((await h.request('/api/v1/configs/list',{session_token:next.session_token})).status,200);
    }finally{await h.close();}
});

test('v2: tampered fields/path, revoked releases and all retired entrypoints fail closed',async()=>{
    const h=await fixture();try {
        const proof=await h.proof('/api/v2/launcher/ticket','ticket',h.login.session_token,h.fields);
        proof.fields=JSON.stringify({...h.fields,pid:9999});
        assert.equal((await h.request('/api/v2/launcher/ticket',proof)).status,403);
        const valid=await h.proof('/api/v2/launcher/manifest','manifest',h.login.session_token,{minecraft_target:'1.21.11'});
        valid.request_type='ticket';assert.equal((await h.request('/api/v2/launcher/ticket',valid)).status,403);
        const ticket=await h.envelope(await h.call('/api/v2/launcher/ticket','ticket',h.login.session_token,h.fields));
        assert.equal((await h.request('/api/v1/admin/revoke-release',{sha256:h.fields.release_sha256})).status,200);
        assert.equal((await h.call('/api/v2/runtime/redeem','redeem',ticket.claims.ticket,h.fields)).status,403);
        assert.equal((await h.call('/api/v2/launcher/ticket','ticket',h.login.session_token,h.fields)).status,403);
        assert.equal((await h.call('/api/v2/launcher/download','download',h.login.session_token,h.fields)).status,403);
        for(const path of ['/api/v1/auth','/api/v1/launcher/ticket','/api/v1/launcher/redeem','/api/v1/launcher/manifest'])
            assert.equal((await h.request(path,{})).status,410);
    }finally{await h.close();}
});
