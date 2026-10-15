#pragma once

#include <QString>

namespace thedorius::launcher
{
    class SessionBootstrap final
    {
    public:
        SessionBootstrap() = default;
        ~SessionBootstrap();
        SessionBootstrap(const SessionBootstrap&) = delete;
        SessionBootstrap& operator=(const SessionBootstrap&) = delete;

        bool prepare(quint32 targetPid, QString& error);
        void clear();
        QString pipeName() const { return pipeName_; }
        QString nonce() const { return nonce_; }

    private:
        void* mapping_ = nullptr;
        QString pipeName_;
        QString nonce_;
    };
}
