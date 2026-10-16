#pragma once
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace services::privacy
{
    struct identity { std::string name, alias; };
    inline std::mutex identity_mutex;
    inline std::shared_ptr<const identity> current_identity;

    inline bool name_character(unsigned char c)
    {
        return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_';
    }
    inline bool valid_alias(std::string_view value)
    {
        return !value.empty() && value.size()<=16 && std::all_of(value.begin(),value.end(),name_character);
    }
    inline void publish(std::string name={},std::string alias={})
    {
        std::lock_guard lock(identity_mutex);
        if (name.empty()) { current_identity.reset(); return; }
        if (current_identity && current_identity->name==name && current_identity->alias==alias) return;
        current_identity=std::make_shared<identity>(identity{std::move(name),std::move(alias)});
    }
    inline std::string display_name(std::string_view text)
    {
        std::shared_ptr<const identity> state;
        { std::lock_guard lock(identity_mutex); state=current_identity; }
        if (!state) return std::string(text);
        const auto lower=[](unsigned char c) { return c>='A' && c<='Z' ? c+('a'-'A') : c; };
        std::string result;
        for (std::size_t i=0;i<text.size();)
        {
            const bool matches=i+state->name.size()<=text.size() &&
                (i==0 || !name_character(static_cast<unsigned char>(text[i-1]))) &&
                (i+state->name.size()==text.size() || !name_character(static_cast<unsigned char>(text[i+state->name.size()]))) &&
                std::equal(state->name.begin(),state->name.end(),text.begin()+i,
                    [&](unsigned char a,unsigned char b) { return lower(a)==lower(b); });
            if (matches) { result+=state->alias; i+=state->name.size(); }
            else result+=text[i++];
        }
        return result;
    }
}
