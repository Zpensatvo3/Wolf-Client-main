import assert from 'node:assert/strict';
import {test} from 'node:test';
import {harness} from './helpers/auth-harness.mjs';
import {verifyAdminAccess} from '../server/auth/admin-access.mjs';
import {signPayload,verifySessionToken} from '../server/auth/sessions.mjs';

test('Access-only admin rejects bearer bypass, forged identities, invalid sessions and cross-site mutations',async()=>{
    const h=await harness();
    try {
        const path='/api/v1/admin/list-keys';
        assert.equal((await h.request(path,undefined,{admin:true,headers:{'Cf-Access-Jwt-Assertion':''}})).status,403);
        for (const claims of [{email:'attacker@example.com'},{exp:1},{aud:'b'.repeat(64)},{iss:'https://other.cloudflareaccess.com'}]) {
            assert.equal((await h.request(path,undefined,{admin:true,headers:{'Cf-Access-Jwt-Assertion':await h.access.token(claims)}})).status,403);
        }
        assert.equal((await h.request(path,undefined,{admin:true,headers:{'Cf-Access-Jwt-Assertion':'e30.e30.invalid','Cf-Access-Authenticated-User-Email':h.env.ADMIN_ACCESS_EMAIL}})).status,403);
        assert.equal((await h.request(path,undefined,{admin:true})).status,200);
        delete h.env.ADMIN_TOKEN_HASH;
        assert.equal((await h.request(path)).status,200);
        assert.equal((await h.request('/health')).status,200);
        for(const headers of [{Origin:''},{Origin:'https://attacker.test'},{'Sec-Fetch-Site':'cross-site'}])
            assert.equal((await h.request('/api/v1/admin/create-key',{}, {headers})).status,401);
        assert.equal((await h.request('/api/v1/admin/create-key',{})).status,200);
        await assert.rejects(verifyAdminAccess(new Request('https://local.test'),{}));
    } finally {await h.close();}
});

test('licenses are random, shown once and stored only as hashes; encryption/weak-key routes are gone',async()=>{
    const h=await harness();
    try {
        assert.equal((await h.request('/api/v1/admin/create-key',{key:'wolf2026'},{admin:true})).status,400);
        const created=await (await h.request('/api/v1/admin/create-key',{days:'7'},{admin:true})).json();
        assert.match(created.key,/^ONESTEP-(?:[A-F0-9]{8}-){3}[A-F0-9]{8}$/);
        const row=h.env.DB.sqlite.prepare('SELECT * FROM license_keys WHERE id=?').get(created.id);
        assert.equal(row.key_encrypted,null); assert.ok(!JSON.stringify(row).includes(created.key));
        const reveal=await h.request('/api/v1/admin/reveal-key',{id:created.id},{admin:true});
        assert.equal(reveal.status,410); assert.ok(!(await reveal.text()).includes(created.key));
    } finally {await h.close();}
});

test('signed request replay is atomically rejected; device proof and licensed config sessions cannot be disabled',async()=>{
    const h=await harness();
    try {
        const created=await (await h.request('/api/v1/admin/create-key',{}, {admin:true})).json();
        const device=await h.device(), body=await device.proof('activate',created.key);
        const responses=await Promise.all([h.request('/api/v1/auth',body),h.request('/api/v1/auth',body)]);
        assert.deepEqual(responses.map(x=>x.status).sort(),[200,409]);
        const accepted=await responses.find(x=>x.status===200).json();
        const heartbeat=await device.proof('heartbeat',accepted.session_token);
        assert.equal((await h.request('/api/v1/auth',heartbeat)).status,200);
        assert.equal((await h.request('/api/v1/auth',heartbeat)).status,409);
        const unsigned=await device.proof('activate',created.key); delete unsigned.device_signature;
        assert.equal((await h.request('/api/v1/auth',unsigned)).status,403);
        h.env.ALLOW_DEVICE_CONFIGS='true';
        assert.notEqual((await h.request('/api/v1/configs/publish',{device_id:'a'.repeat(64),name:'Unlicensed',author_name:'Player_1',payload:'{"modules":[]}'})).status,200);
        assert.equal((await h.request('/api/v1/configs/list',{session_token:accepted.session_token})).status,200);
    } finally {await h.close();}
});

test('key cutover rejects retired IDs and unknown license errors match wrong-device errors',async()=>{
    const h=await harness();
    try {
        const created=await (await h.request('/api/v1/admin/create-key',{}, {admin:true})).json();
        const device=await h.device(), accepted=await (await h.request('/api/v1/auth',await device.proof('activate',created.key))).json();
        const claims=await verifySessionToken(accepted.session_token,h.env);
        assert.equal(claims.kid,'primary');
        h.env.SIGNING_KEY_ID='rotation-2';
        await assert.rejects(verifySessionToken(accepted.session_token,h.env));
        const fresh=await signPayload(claims,h.env);
        assert.equal((await verifySessionToken(fresh.session_token,h.env)).kid,'rotation-2');
        const stranger=await h.device();
        const invalid=await h.request('/api/v1/auth',await stranger.proof('activate','UNKNOWN-KEY-1234'));
        const mismatch=await h.request('/api/v1/auth',await stranger.proof('activate',created.key));
        assert.equal(invalid.status,mismatch.status); assert.deepEqual(await invalid.json(),await mismatch.json());
    } finally {await h.close();}
});
