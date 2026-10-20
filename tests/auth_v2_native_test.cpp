// Publicly disposable TEST signing key. Never used by a production deployment.
#define ONESTEP_AUTH_SERVER_DOMAIN "test.invalid"
#define ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64 "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEKVRpk7e6UndcrWL_OX3rsO3-WIqDdTL_-rWC8pgxi4DaFOyT9kStMYNQASezaAJZSnZ4i4vWa4aP7T9YwBSWzQ"
#include "../src/auth/auth.cpp"
#include "../src/auth/client.cpp"
#include <cassert>
#include <iostream>

static std::string encode_test(const unsigned char* bytes,DWORD size) {
    DWORD count=0;CryptBinaryToStringA(bytes,size,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&count);
    std::string out(count,'\0');CryptBinaryToStringA(bytes,size,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,out.data(),&count);
    while(!out.empty()&&(out.back()==0||out.back()=='='))out.pop_back();
    for(auto& c:out){if(c=='+')c='-';if(c=='/')c='_';}return out;
}
static std::string signed_fixture(const std::string& payload) {
    std::vector<UCHAR> blob;assert(base64url_decode("RUNTMiAAAAApVGmTt7pSd1ytYv85feuw7f5YioN1Mv_6tYLymDGLgNoU7JP2RK0xg1ABJ7NoAllKdniLi9Zrho_tP1jAFJbNTlPdSMJpUkjd7cZrhml6C7M-0_6PHpwT0taqyc-V8zc",blob));
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_KEY_HANDLE key{};
    assert(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0)>=0);
    assert(BCryptImportKeyPair(algorithm,nullptr,BCRYPT_ECCPRIVATE_BLOB,&key,blob.data(),DWORD(blob.size()),0)>=0);
    std::array<UCHAR,32> hash{};assert(sha256_digest(payload,hash));
    std::array<UCHAR,64> signature{};DWORD size{};
    assert(BCryptSignHash(key,nullptr,hash.data(),DWORD(hash.size()),signature.data(),DWORD(signature.size()),&size,0)>=0);
    BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(algorithm,0);SecureZeroMemory(blob.data(),blob.size());
    auto p=encode_test(reinterpret_cast<const unsigned char*>(payload.data()),DWORD(payload.size()));
    auto s=encode_test(signature.data(),size);
    return "{\"payload\":"+json_quote(p)+",\"signature\":"+json_quote(s)+",\"session_token\":"+json_quote(p+"."+s)+"}";
}
int main() {
    using auth::scope;
    auth::controlled_shutdown();
    { std::lock_guard lock(state_mutex); state.status=auth::client::phase::authorized; }
    assert(!auth::client::access_allowed()); // Spoofed UI state cannot create authority.
    for(auto cap:{scope::modules,scope::input,scope::bridge,scope::configs,scope::render,scope::launcher})
        assert(!auth::permitted(cap));
    request_proof request;request.nonce=std::string(32,'b');request.hash=std::string(64,'c');request.device_hash=std::string(64,'d');
    const auto fields="{\"pid\":"+std::to_string(GetCurrentProcessId())+
        ",\"process_nonce\":\""+std::string(32,'e')+"\",\"minecraft_target\":\"1.21.11\",\"release_sha256\":\""+
        std::string(64,'a')+"\",\"release_version\":\"test-v2\"}";
    auto payload=[&](const std::string& audience,int sequence,int duration=60000) {
        const auto now=unix_time_ms();
        return "{\"protocol\":2,\"success\":true,\"aud\":"+json_quote(audience)+
            ",\"kind\":\"runtime\",\"session_id\":\"test-session\",\"license_epoch\":1,\"sequence\":"+std::to_string(sequence)+
            ",\"iat\":"+std::to_string(now)+",\"exp\":"+std::to_string(now+duration)+
            ",\"device_key_hash\":"+json_quote(request.device_hash)+",\"nonce_hash\":"+json_quote(sha256_hex(request.nonce))+
            ",\"request_hash\":"+json_quote(request.hash)+","+fields.substr(1);
    };
    auth::auth_result result;const auto generation=authority_generation.load();
    auto signedRuntime=signed_fixture(payload(std::string(runtime_aud),0));
    assert(!commit_v2(signed_fixture(payload(std::string(runtime_aud),0,-1)),request,"test.invalid",std::string(64,'f'),
        std::string(runtime_aud),fields,generation,result));
    assert(commit_v2(signedRuntime,request,"test.invalid",std::string(64,'f'),std::string(runtime_aud),fields,generation,result));
    assert(auth::permitted(scope::modules)&&auth::permitted(scope::input)&&!auth::permitted(scope::launcher));
    auto previous=authority.load();
    assert(previous->deadline<=request.started_tick+60000);
    assert(!commit_v2(signed_fixture(payload(std::string(launcher_aud),0)),request,"test.invalid",std::string(64,'f'),
        std::string(runtime_aud),fields,generation,result));
    assert(!commit_v2(signed_fixture(payload(std::string(runtime_aud),1,61000)),request,"test.invalid",std::string(64,'f'),
        std::string(runtime_aud),fields,generation,result,previous.get()));
    const auto renewed=signed_fixture(payload(std::string(runtime_aud),1));
    assert(commit_v2(renewed,request,"test.invalid",std::string(64,'f'),std::string(runtime_aud),fields,generation,result,previous.get()));
    assert(!commit_v2(renewed,request,"test.invalid",std::string(64,'f'),std::string(runtime_aud),fields,generation,result,previous.get()));
    auth::controlled_shutdown();
    assert(!auth::permitted(scope::modules));
    assert(!commit_v2(signedRuntime,request,"test.invalid",std::string(64,'f'),std::string(runtime_aud),fields,generation,result));
    // Direct license login is compiled out of DLLs, even for a plausible key.
    assert(!auth::authenticate("TEST-KEY-1234","test.invalid").success);
    std::cout<<"PASS signed lease, role isolation, duration, replay, cancellation, DLL login denial\n";
}
