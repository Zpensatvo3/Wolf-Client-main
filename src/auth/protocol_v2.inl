// Included by auth.cpp: transport/crypto helpers and the authority constructor
// remain private to this translation unit. No public setter can grant access.
namespace {
    struct verified_session {
        std::string token,domain,hwid,fields,audience,session_id,device_hash,expires;
        int epoch=0; unsigned capabilities=0; unsigned long long generation=0,deadline=0;
        long long sequence=0;
        ~verified_session(){secure_clear(token);secure_clear(hwid);}
    };
    std::atomic<std::shared_ptr<const verified_session>> authority;
    std::atomic<unsigned long long> authority_generation{1};
    std::mutex authority_commit_mutex;
    // Config operations and renewals must not race the server's strict sequence.
    // Only background service threads take this lock, never a module/render tick.
    std::mutex runtime_request_mutex;
    constexpr unsigned runtime_capabilities=1|2|4|8|16;
    constexpr std::string_view launcher_aud="onestep-launcher-v2", runtime_aud="onestep-runtime-v2";
    struct request_proof {
        std::string body,nonce,hash,device_hash;
        unsigned long long started_tick=GetTickCount64();
        ~request_proof(){secure_clear(body);}
    };

    std::string json_quote(std::string_view value){return "\""+json_escape(std::string(value))+"\"";}
    request_proof make_v2(const std::string& path,const std::string& type,const std::string& credential,
        const std::string& hwid,const std::string& fields) {
        request_proof out; out.nonce=auth::generate_nonce();
        const auto timestamp=unix_time_ms();
        if(out.nonce.empty()||credential.empty()||hwid.size()!=64||fields.size()>8192) return {};
        const auto message="onestep-device-v2\nPOST\n"+path+"\n"+type+"\n"+hwid+"\n"+
            out.nonce+"\n"+std::to_string(timestamp)+"\n"+sha256_hex(credential)+"\n"+sha256_hex(fields);
        const auto proof=auth::device::sign_message(message);
        if(!proof.success) return {};
        out.hash=sha256_hex(message);out.device_hash=sha256_hex(proof.public_key);
        const auto key=type=="activate"?"key":type=="redeem"?"ticket":"session_token";
        out.body="{\"request_type\":"+json_quote(type)+",\"hwid\":"+json_quote(hwid)+",\"nonce\":"+json_quote(out.nonce)+
            ",\"timestamp\":"+std::to_string(timestamp)+",\""+key+"\":"+json_quote(credential)+",\"fields\":"+json_quote(fields)+
            ",\"device_key\":"+json_quote(proof.public_key)+",\"device_signature\":"+json_quote(proof.signature)+"}";
        return out;
    }
    std::string validated_payload(const std::string& envelope,const std::string& audience,
        const request_proof* request=nullptr) {
        if(envelope.size()>32768)return {};
        const auto payload64=parse_json_value(envelope,"payload"), signature=parse_json_value(envelope,"signature");
        std::vector<UCHAR> bytes;
        if(!payload64||!signature||!base64url_decode(*payload64,bytes))return {};
        std::string text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        const auto iat=parse_integer<long long>(parse_json_value(text,"iat"));
        const auto exp=parse_integer<long long>(parse_json_value(text,"exp"));
        if(!verify_signed_payload(text,*signature)||parse_json_value(text,"aud").value_or("")!=audience||
            parse_json_value(text,"protocol").value_or("")!="2"||!iat||!exp||
            !auth::policy::valid_times(*iat,*exp,unix_time_ms()))return {};
        if(request&&(!constant_time_equal(parse_json_value(text,"request_hash").value_or(""),request->hash)||
            !constant_time_equal(parse_json_value(text,"nonce_hash").value_or(""),sha256_hex(request->nonce))))return {};
        return text;
    }
    bool live(const std::shared_ptr<const verified_session>& state,unsigned mask) {
        return state&&(state->capabilities&mask)==mask&&state->generation==authority_generation.load(std::memory_order_acquire)&&
            auth::policy::before_deadline(GetTickCount64(),state->deadline);
    }
    bool commit_v2(const std::string& envelope,const request_proof& request,const std::string& domain,
        const std::string& hwid,const std::string& audience,const std::string& fields,
        unsigned long long generation,auth::auth_result& result,const verified_session* previous=nullptr) {
        const auto payload=validated_payload(envelope,audience,&request);
        if(payload.empty())return false;
        const auto epoch=parse_integer<int>(parse_json_value(payload,"license_epoch"));
        const auto iat=parse_integer<long long>(parse_json_value(payload,"iat"));
        const auto exp=parse_integer<long long>(parse_json_value(payload,"exp"));
        const auto device_hash=parse_json_value(payload,"device_key_hash").value_or("");
        if(!epoch||*epoch<=0||!iat||!exp||device_hash!=request.device_hash||
            parse_json_value(payload,"success").value_or("")!="true")return false;
        auto state=std::make_shared<verified_session>();
        state->audience=audience;state->domain=domain;state->hwid=hwid;state->epoch=*epoch;
        state->session_id=parse_json_value(payload,"session_id").value_or("");state->device_hash=device_hash;
        state->expires=parse_json_value(payload,"expires_at").value_or("");state->generation=generation;
        if(state->session_id.empty())return false;
        if(audience==runtime_aud) {
            if(*exp-*iat>60000||parse_json_value(payload,"kind").value_or("")!="runtime")return false;
            for(const char* name:{"pid","process_nonce","minecraft_target","release_sha256","release_version"})
                if(parse_json_value(fields,name)!=parse_json_value(payload,name))return false;
            const auto pid=parse_integer<DWORD>(parse_json_value(payload,"pid"));
            if(!pid||*pid!=GetCurrentProcessId())return false;
            const auto sequence=parse_integer<long long>(parse_json_value(payload,"sequence"));
            if(!sequence||*sequence!=(previous?previous->sequence+1:0)||
                (previous&&(state->session_id!=previous->session_id||*epoch!=previous->epoch||device_hash!=previous->device_hash)))return false;
            state->sequence=*sequence;state->capabilities=runtime_capabilities;
            state->fields=fields;
        } else {
            if(audience!=launcher_aud||parse_json_value(payload,"kind").value_or("")!="launcher")return false;
            state->capabilities=unsigned(auth::scope::launcher);state->fields="{}";
        }
        state->token=parse_json_value(envelope,"session_token").value_or("");
        if(state->token.empty()||state->token!=parse_json_value(envelope,"payload").value_or("")+"."+
            parse_json_value(envelope,"signature").value_or(""))return false;
        const auto remaining=*exp-unix_time_ms();
        if(remaining<=0)return false;
        // A slow local clock must not add the allowed UTC skew to the lease.
        // Anchor to request start and signed TTL as well as the absolute expiry.
        state->deadline=(std::min)(GetTickCount64()+static_cast<ULONGLONG>(remaining),
            request.started_tick+static_cast<ULONGLONG>(*exp-*iat));
        if(!auth::policy::before_deadline(GetTickCount64(),state->deadline))return false;
        std::lock_guard lock(authority_commit_mutex);
        if(generation!=authority_generation.load(std::memory_order_acquire))return false;
        if(previous) {
            const auto current=authority.load();
            if(current.get()!=previous || !live(current,runtime_capabilities))return false;
        }
        authority.store(state,std::memory_order_release);
        result.success=true;result.exp=*exp;result.license_epoch=*epoch;result.expires_at=state->expires;
        return true;
    }
    std::string runtime_fields(const verified_session& state) {
        // Preserve the original bindings, replacing only the monotonic renewal cursor.
        std::string result="{";
        for(const char* name:{"pid","process_nonce","minecraft_target","release_sha256","release_version"}) {
            if(result.size()>1)result+=",";
            result+=json_quote(name)+":";
            const auto value=parse_json_value(state.fields,name).value_or("");
            result+=std::string(name)=="pid"?value:json_quote(value);
        }
        return result+",\"session_id\":"+json_quote(state.session_id)+",\"sequence\":"+std::to_string(state.sequence)+"}";
    }
    void revoke_authority() {
        std::lock_guard lock(authority_commit_mutex);
        authority_generation.fetch_add(1,std::memory_order_acq_rel);
        authority.store({},std::memory_order_release);
    }
}

#if !ONESTEP_AUTH_REQUIRED
namespace { std::atomic_bool local_test_active{false}; }
#endif
bool auth::permitted(scope capability){
#if !ONESTEP_AUTH_REQUIRED
    constexpr unsigned local_scopes=unsigned(scope::modules)|unsigned(scope::input)|unsigned(scope::bridge)|unsigned(scope::render);
    const auto mask=unsigned(capability);
    return local_test_active.load(std::memory_order_acquire)&&mask!=0&&(mask&local_scopes)==mask;
#else
    return live(authority.load(std::memory_order_acquire),unsigned(capability));
#endif
}
bool auth::is_authenticated(){auto state=authority.load();return state&&live(state,state->capabilities);}
int auth::get_active_epoch(){auto state=authority.load();return state?state->epoch:0;}
void auth::configure_config_service(const std::string&) {
#if !ONESTEP_AUTH_REQUIRED
    local_test_active.store(true,std::memory_order_release);
#endif
    // Remote config access still requires a verified runtime lease.
}

void auth::stop_heartbeat_monitor() {
    {std::lock_guard lock(g_heartbeat_mutex);
        if(std::this_thread::get_id()==g_heartbeat_thread_id){g_heartbeat_running=false;g_heartbeat_cv.notify_all();return;}}
    std::lock_guard owner(g_heartbeat_owner_mutex);
    g_heartbeat_running=false;g_heartbeat_cv.notify_all();
    if(g_heartbeat_thread.joinable())g_heartbeat_thread.join();
}
void auth::controlled_shutdown(){
#if !ONESTEP_AUTH_REQUIRED
    local_test_active.store(false,std::memory_order_release);
#endif
    revoke_authority();stop_heartbeat_monitor();
}

auth::auth_result auth::authenticate(const std::string& key,const std::string& domain) {
    auth_result result;
#ifndef ONESTEP_LAUNCHER_BUILD
    result.message="Start OneStep from the current launcher.";
#else
    controlled_shutdown();
    const auto generation=authority_generation.load();
    if(!policy::valid_license(key)||domain!=config::server_domain){result.message="Invalid launcher login.";return result;}
    const auto hwid=generate_hwid();
    const auto proof=make_v2("/api/v2/launcher/login","activate",key,hwid,"{}");
    if(proof.body.empty()){result.message="Device verification failed.";return result;}
    auto response=post_json_to_worker(domain,"/api/v2/launcher/login",proof.body);
    if(!response.success||!commit_v2(response.body,proof,domain,hwid,std::string(launcher_aud),"{}",generation,result)) {
        revoke_authority();result.message=response.message.empty()?"Launcher session could not be verified.":response.message;
    }
    secure_clear(response.body);
#endif
    return result;
}

auth::auth_result auth::redeem_launcher_ticket(const std::string& handoff,const std::string& domain,
    const std::string& hash,const std::string& target) {
    controlled_shutdown();auth_result result;const auto generation=authority_generation.load();
    const auto ticket=service_field(handoff,"ticket"), nonce=service_field(handoff,"process_nonce");
    const auto version=service_field(handoff,"release_version");
    if(!launch::hex64(ticket)||nonce.size()!=32||nonce.find_first_not_of("0123456789abcdef")!=std::string::npos||
        !launch::hex64(hash)||version.empty()||version.size()>64||service_field(handoff,"minecraft_target")!=target||domain!=config::server_domain) {
        result.message="Start OneStep using the current launcher.";return result;
    }
    const auto fields="{\"pid\":"+std::to_string(GetCurrentProcessId())+",\"process_nonce\":"+json_quote(nonce)+
        ",\"minecraft_target\":"+json_quote(target)+",\"release_sha256\":"+json_quote(hash)+",\"release_version\":"+json_quote(version)+"}";
    const auto hwid=generate_hwid();const auto proof=make_v2("/api/v2/runtime/redeem","redeem",ticket,hwid,fields);
    if(proof.body.empty()){result.message="Device verification failed.";return result;}
    auto response=post_json_to_worker(domain,"/api/v2/runtime/redeem",proof.body);
    if(!response.success||!commit_v2(response.body,proof,domain,hwid,std::string(runtime_aud),fields,generation,result)) {
        revoke_authority();result.message=response.message.empty()?"Runtime session could not be verified.":response.message;
    }
    secure_clear(response.body);return result;
}

std::string auth::service_field(const std::string& json,const std::string& key){return parse_json_value(json,key).value_or("");}
std::string auth::decode_service_payload(const std::string& envelope,const std::string& audience){return validated_payload(envelope,audience);}
std::string auth::make_launcher_request(const std::string& path,const std::string& fields) {
    auto state=authority.load();if(!live(state,unsigned(scope::launcher)))return {};
    const auto type=path.substr(path.find_last_of('/')+1);
    if(path!="/api/v2/launcher/download")return {}; // Streaming download owns its transport.
    return make_v2(path,type,state->token,state->hwid,"{"+fields+"}").body;
}
auth::service_result auth::post_authenticated_json(const std::string& path,const std::string& fields) {
    std::unique_lock operation(runtime_request_mutex);
    auto state=authority.load();service_result response;
    if(path.starts_with("/api/v2/launcher/")) {
        if(!live(state,unsigned(scope::launcher)))return {false,0,"","Launcher session ended."};
        const auto type=path.substr(path.find_last_of('/')+1);
        if(type!="manifest"&&type!="ticket")return {false,0,"","Invalid launcher operation."};
        const auto proof=make_v2(path,type,state->token,state->hwid,"{"+fields+"}");
        if(proof.body.empty())return {false,0,"","Device verification failed."};
        response=post_json_to_worker(state->domain,path,proof.body);
        const auto audience=type=="manifest"?"onestep-release-v2":"onestep-launcher-v2";
        if(!response.success||validated_payload(response.body,audience,&proof).empty()||!live(state,unsigned(scope::launcher))) {
            revoke_authority();response.success=false;response.message="Launcher request could not be verified. Sign in again.";
        }
        return response;
    }
    if(!path.starts_with("/api/v1/configs/")||!live(state,unsigned(scope::configs)))return {false,0,"","Runtime permission required."};
    auto body="{\"session_token\":"+json_quote(state->token);
    if(!fields.empty())body+=","+fields;
    response=post_json_to_worker(state->domain,path,body+"}");
    if(response.status_code==0||response.status_code==401||response.status_code==403)revoke_authority();
    return response;
}

void auth::start_heartbeat_monitor(const std::string&) {
    std::lock_guard owner(g_heartbeat_owner_mutex);
    auto initial=authority.load();if(!live(initial,unsigned(scope::modules))||g_heartbeat_running)return;
    if(g_heartbeat_thread.joinable())g_heartbeat_thread.join();
    g_heartbeat_running=true;
    try {g_heartbeat_thread=std::thread([]{
        {std::lock_guard lock(g_heartbeat_mutex);g_heartbeat_thread_id=std::this_thread::get_id();}
        while(g_heartbeat_running.load()) {
            auto state=authority.load();if(!live(state,unsigned(auth::scope::modules)))break;
            unsigned char random=0;
            if(BCryptGenRandom(nullptr,&random,1,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0){revoke_authority();break;}
            {std::unique_lock lock(g_heartbeat_mutex);if(g_heartbeat_cv.wait_for(lock,std::chrono::seconds(18+random%5),
                []{return !g_heartbeat_running.load();}))break;}
            std::unique_lock operation(runtime_request_mutex);
            state=authority.load();
            if(!g_heartbeat_running.load()||!live(state,unsigned(auth::scope::modules)))break;
            const auto fields=runtime_fields(*state);
            const auto proof=make_v2("/api/v2/runtime/renew","renew",state->token,state->hwid,fields);
            if(proof.body.empty()){revoke_authority();break;}
            try {
                auto response=post_json_to_worker(state->domain,"/api/v2/runtime/renew",proof.body);auth::auth_result result;
                const bool ok=response.success&&g_heartbeat_running.load()&&
                    commit_v2(response.body,proof,state->domain,state->hwid,std::string(runtime_aud),fields,state->generation,result,state.get());
                secure_clear(response.body);
                if(!ok){revoke_authority();break;}
            } catch(...){revoke_authority();break;}
        }
        {std::lock_guard lock(g_heartbeat_mutex);g_heartbeat_thread_id={};g_heartbeat_running=false;}
        g_heartbeat_cv.notify_all();
    });}catch(...){g_heartbeat_running=false;revoke_authority();}
}
