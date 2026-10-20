import {jsonResponse as reply,readBoundedJson} from '../core/http.mjs';
import {sha256,requiredSecret} from '../core/crypto.mjs';
import {enforceRateLimit} from '../core/rate-limit.mjs';
import {consumeAuthNonce} from '../auth/nonces.mjs';
import {signPayload,verifySessionToken} from '../auth/sessions.mjs';
import {verifyProofV2,LAUNCHER_AUDIENCE,RUNTIME_AUDIENCE,RELEASE_AUDIENCE,LEASE_MS} from '../auth/protocol-v2.mjs';
import {activeRelease,activeLicenseV2,requireRuntimeSession} from '../auth/runtime-v2.mjs';
import {handleAuth} from './auth.mjs';
import {licenseExpiryMs} from '../session-policy.mjs';

const HEX=/^[a-f0-9]{64}$/, NONCE=/^[a-f0-9]{32}$/;
export const targets=new Set(['1.21.1','1.21.4','1.21.8','1.21.11','26.1','26.1.1','26.1.2']);
const routes=new Map(['launcher/login','launcher/manifest','launcher/download','launcher/ticket','runtime/redeem','runtime/renew']
    .map(route=>['/api/v2/'+route,route.split('/')[1]==='login'?'activate':route.split('/')[1]]));

function validBinding(f) {
    return Number.isSafeInteger(f.pid) && f.pid>0 && f.pid<=0xffffffff && NONCE.test(f.process_nonce||'') &&
        targets.has(f.minecraft_target) && HEX.test(f.release_sha256||'') &&
        /^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$/.test(f.release_version||'');
}
async function signed(payload,body,proof,env) {
    return reply(await signPayload({...payload,protocol:2,request_hash:proof.requestHash,
        nonce_hash:await sha256(body.nonce)},env));
}
export async function handleLauncherV2(request,env,ctx) {
    const path=new URL(request.url).pathname;
    if (!path.startsWith('/api/v2/')) return null;
    if (!routes.has(path)) return reply({message:'Unknown endpoint.'},404);
    if (request.method!=='POST') return reply({message:'Method not allowed.'},405);
    try {
        const retry=await enforceRateLimit(request,env,'v2-'+routes.get(path),24,60_000);
        if (retry!==null) return reply({message:'Please wait before retrying.'},429);
        const body=await readBoundedJson(request,24576);
        if (body.request_type!==routes.get(path) || !Number.isSafeInteger(body.timestamp) ||
            Math.abs(Date.now()-body.timestamp)>30_000) return reply({message:'Invalid or expired request.'},400);
        const proof=await verifyProofV2(path,body), f=proof.fields;
        if (body.request_type==='activate') {
            // Reuse the existing conditional activation/device-bind transaction.
            // Only this internal call selects a launcher audience and proof verifier.
            const internal=new Request(new URL('/api/v1/auth',request.url),{method:'POST',
                headers:request.headers,body:JSON.stringify({...body,fields:undefined})});
            return handleAuth(internal,env,ctx,{audience:LAUNCHER_AUDIENCE,
                verifyProof:async()=>proof.deviceHash,
                responseClaims:{protocol:2,request_hash:proof.requestHash,kind:'launcher'}});
        }
        const hwidHash=await sha256(body.hwid+requiredSecret(env,'HWID_PEPPER'));
        if (body.request_type==='redeem') {
            if (!HEX.test(body.ticket||'') || !validBinding(f)) throw Error('Invalid ticket');
            const now=Date.now(), ticketHash=await sha256(body.ticket);
            const row=await env.DB.prepare('SELECT * FROM runtime_sessions WHERE ticket_hash = ?').bind(ticketHash).first();
            if (!row || row.ticket_exp<=now || row.hwid_hash!==hwidHash || row.device_key_hash!==proof.deviceHash ||
                ['pid','process_nonce','minecraft_target','release_sha256','release_version'].some(k=>row[k]!==f[k])) throw Error('Invalid ticket binding');
            const license=await activeLicenseV2(env,row);
            await activeRelease(env,row.release_sha256,row.minecraft_target);
            const exp=Math.min(now+LEASE_MS,licenseExpiryMs(license.expires_at)??Infinity);
            // Consume and create the runtime lease in ONE conditional write.
            const session=await env.DB.prepare(`UPDATE runtime_sessions SET ticket_hash=NULL,ticket_exp=0,sequence=0,exp=?
                WHERE ticket_hash=? AND ticket_exp>? AND revoked=0
                AND EXISTS(SELECT 1 FROM approved_releases r WHERE r.sha256=runtime_sessions.release_sha256 AND r.revoked=0)
                AND EXISTS(SELECT 1 FROM license_keys l WHERE l.key_hash=runtime_sessions.key_hash AND l.status='active'
                    AND l.license_epoch=runtime_sessions.license_epoch AND l.device_key_hash=runtime_sessions.device_key_hash
                    AND l.hwid_hash=runtime_sessions.hwid_hash)
                RETURNING *`).bind(exp,ticketHash,now).first();
            if (!session) throw Error('Ticket already consumed');
            return signed({...session,ticket_hash:undefined,ticket_exp:undefined,revoked:undefined,
                aud:RUNTIME_AUDIENCE,kind:'runtime',success:true,iat:now,expires_at:license.expires_at||'lifetime'},body,proof,env);
        }
        const runtime=body.request_type==='renew';
        const claims=runtime?await requireRuntimeSession(body.session_token,env):
            await verifySessionToken(body.session_token,env,LAUNCHER_AUDIENCE);
        if (claims.protocol!==2 || claims.hwid_hash!==hwidHash || claims.device_key_hash!==proof.deviceHash) throw Error('Wrong device session');
        const license=await activeLicenseV2(env,claims);
        if (!await consumeAuthNonce(env,claims.key_hash,proof.deviceHash,path,body.nonce,Date.now())) throw Error('Replayed request');
        if (runtime) {
            if (!validBinding(f) || ['pid','process_nonce','minecraft_target','release_sha256','release_version','session_id','sequence']
                .some(k=>f[k]!==claims[k])) throw Error('Wrong runtime binding');
            const now=Date.now(), exp=Math.min(now+LEASE_MS,licenseExpiryMs(license.expires_at)??Infinity);
            const row=await env.DB.prepare(`UPDATE runtime_sessions SET sequence=sequence+1,exp=?
                WHERE session_id=? AND sequence=? AND exp>? AND revoked=0 AND ticket_hash IS NULL
                AND EXISTS(SELECT 1 FROM approved_releases r WHERE r.sha256=runtime_sessions.release_sha256 AND r.revoked=0)
                AND EXISTS(SELECT 1 FROM license_keys l WHERE l.key_hash=runtime_sessions.key_hash AND l.status='active'
                    AND l.license_epoch=runtime_sessions.license_epoch AND l.device_key_hash=runtime_sessions.device_key_hash
                    AND l.hwid_hash=runtime_sessions.hwid_hash)
                RETURNING sequence`).bind(exp,claims.session_id,claims.sequence,now).first();
            if (!row) throw Error('Renewal conflict');
            return signed({...claims,sequence:row.sequence,iat:now,exp},body,proof,env);
        }
        if (!targets.has(f.minecraft_target)) throw Error('Unsupported target');
        const release=await env.DB.prepare(`SELECT r.* FROM release_channels c JOIN approved_releases r ON r.sha256=c.sha256
            WHERE c.minecraft_target=? AND r.minecraft_target=c.minecraft_target AND r.revoked=0`).bind(f.minecraft_target).first();
        if (!release) throw Error('Release unavailable');
        if (body.request_type==='manifest') return signed({aud:RELEASE_AUDIENCE,kind:'release',
            version:release.version,sha256:release.sha256,bytes:release.bytes,minecraft_target:release.minecraft_target,
            iat:Date.now(),exp:Date.now()+60_000},body,proof,env);
        if (f.release_sha256!==release.sha256 || f.release_version!==release.version) throw Error('Update required');
        if (body.request_type==='download') {
            const stream=await env.RELEASES.get('dll/'+release.sha256,'stream');
            if (!stream) return reply({message:'Release is propagating. Retry shortly.'},503);
            return new Response(stream,{headers:{'Content-Type':'application/octet-stream',
                'Content-Length':String(release.bytes),'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}});
        }
        if (!validBinding(f)) throw Error('Invalid process binding');
        const ticket=Array.from(crypto.getRandomValues(new Uint8Array(32)),v=>v.toString(16).padStart(2,'0')).join('');
        const now=Date.now(), sessionId=crypto.randomUUID();
        await env.DB.batch([
            env.DB.prepare('DELETE FROM runtime_sessions WHERE exp <= ?').bind(now),
            env.DB.prepare(`INSERT INTO runtime_sessions(session_id,key_hash,hwid_hash,device_key_hash,license_epoch,
                release_sha256,release_version,minecraft_target,pid,process_nonce,sequence,exp,ticket_hash,ticket_exp)
                VALUES(?,?,?,?,?,?,?,?,?,?,-1,?,?,?)`).bind(sessionId,claims.key_hash,hwidHash,proof.deviceHash,
                    claims.license_epoch,release.sha256,release.version,release.minecraft_target,f.pid,f.process_nonce,
                    now+60_000,await sha256(ticket),now+60_000)
        ]);
        return signed({aud:LAUNCHER_AUDIENCE,kind:'ticket',ticket,iat:now,exp:now+60_000},body,proof,env);
    } catch (_) {
        return reply({message:'Session verification failed. Start again from the launcher.'},403);
    }
}
