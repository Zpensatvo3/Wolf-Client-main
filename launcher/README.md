# OneStep Launcher

Windows x64 native başlatıcı. Güncel kayıtlı dağıtım Fabric **1.21.11** için
launcher-only v2'dir. Kullanım bu belgededir; signed audience, ticket, lease ve
IPC sözleşmesi [LAUNCHER_V2](../docs/LAUNCHER_V2.md) içindedir.

## Kullanım

1. Minecraft Fabric 1.21.11'i normal aç.
2. Onaylı paketteki `Onestep-Launcher.exe` dosyasını çalıştır, client lisansını gir.
3. İmzalı manifest/özeti doğrulanmış DLL indirmesini bekle; doğru Minecraft
   penceresini seçip Launch Client kullan.

DLL-only lisans girişi yoktur. Yönetici yetkisi, browser runtime, driver,
VMProtect veya gizli/manual loader gerekmez. Minecraft kapatılmaz/yeniden
başlatılmaz. Launcher başarılı yüklemeden sonra kapanabilir; yenilemeyi DLL yapar.
İlk başarısız lease yenilemesi client erişimini durdurur; yeniden launcher gerekir.
Yavaş indirme dahil ağ watchdog sınırları protokol belgesindedir.

Client key diske kaydedilmez. Kalıcı non-exportable Windows CNG device key
proof üretir; public doğrulama anahtarı sır değildir. Cache yolu
`%LOCALAPPDATA%/Onestep/releases/<sha256>/Onestep.dll`'dir.
Cache, geçerli launcher session/release onayı/taze one-use ticket'ın yerine geçmez.
İstemci hash beyanı donanım attestation değildir.

## Derleme ve paketleme

Depo kökünde Windows C++ Build Tools ile:

```powershell
./scripts/Build-Launcher.ps1
```

Bu yardımcı 1.21.11 DLL'yi `build/launcher-client/Onestep.dll`,
EXE'yi `build/launcher/Onestep-Launcher.exe` yoluna üretir.
Yayın kaydındaki izole v2 çıktıları `build/launcher-v2/` altındadır;
standart build komutunun otomatik olarak bu kayıtlı hash'leri ürettiğini varsayma.

`scripts/Package-LauncherRelease.ps1` yalnızca izole v2 EXE+DLL çiftini alır.
Var olan output'u ezmez ve ZIP'in tam iki whitelist dosyasını içerdiğini doğrular.
Kaynak, PDB/MAP, test probe'u, vault ve özel yedek dağıtılmaz. Her iki binary
birlikte güncellenir; bir sürümün EXE'siyle başka sürümün DLL'si karıştırılmaz.

## Yayın ve doğrulama sınırı

Yayın, MFA-korumalı admin DLL upload formunda ayrı operatör işlemidir.
GitHub'ın verified-release workflow'u başarılı run/artifact ve onaylı hash
seçerek yalnız binary artifact hazırlar; public unauthenticated indirme veya
otomatik deploy açmaz.

Mevcut binary hash/boyutları [deployment.json](../server/deployment.json),
açık olay işleri [güvenlik kaydı](../server/AUTH_HARDENING.md) içindedir.
Kaydedilmiş v2 yayını canlı lisanslı launcher→oyun round-trip testi değildir.
V1'e veya doğrudan DLL girişine rollback yapılmaz.
