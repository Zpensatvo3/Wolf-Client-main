#include "SessionBootstrap.hpp"

#include <QUuid>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace
{
    constexpr quint32 bootstrapMagic = 0x54484452;
    struct BootstrapData
    {
        quint32 magic{};
        wchar_t pipeName[96]{};
        wchar_t nonce[96]{};
    };
}

thedorius::launcher::SessionBootstrap::~SessionBootstrap() { clear(); }

bool thedorius::launcher::SessionBootstrap::prepare(quint32 targetPid, QString& error)
{
    clear();
#ifdef Q_OS_WIN
    pipeName_ = QStringLiteral("Thedorius-%1-%2").arg(targetPid).arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    nonce_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString mappingName = QStringLiteral("Local\\ThedoriusSession-%1").arg(targetPid);
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(BootstrapData), reinterpret_cast<LPCWSTR>(mappingName.utf16()));
    if (!mapping)
    {
        error = QStringLiteral("Session mapping could not be created (%1)").arg(GetLastError());
        return false;
    }
    auto* data = static_cast<BootstrapData*>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(BootstrapData)));
    if (!data)
    {
        error = QStringLiteral("Session mapping could not be opened (%1)").arg(GetLastError());
        CloseHandle(mapping);
        return false;
    }
    *data = {};
    data->magic = bootstrapMagic;
    wcsncpy_s(data->pipeName, pipeName_.toStdWString().c_str(), _TRUNCATE);
    wcsncpy_s(data->nonce, nonce_.toStdWString().c_str(), _TRUNCATE);
    UnmapViewOfFile(data);
    mapping_ = mapping;
    return true;
#else
    Q_UNUSED(targetPid)
    error = QStringLiteral("Session bootstrap is supported on Windows only");
    return false;
#endif
}

void thedorius::launcher::SessionBootstrap::clear()
{
#ifdef Q_OS_WIN
    if (mapping_) CloseHandle(static_cast<HANDLE>(mapping_));
#endif
    mapping_ = nullptr;
    pipeName_.clear();
    nonce_.clear();
}
