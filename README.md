# OneStep

Minecraft Fabric için Windows x64 üzerinde çalışan C++20 istemci.
OpenGL/ImGui arayüzü, 61 kayıtlı modül, HUD düzenleyicisi, public config servisi
ve sürüme özel Java köprüleri aynı kaynak ağacında geliştirilir.

## Başlangıç

1. Güncel dağıtım hedefi Fabric **1.21.11**'dir; istemci launcher üzerinden açılır.
2. Kullanım ve paketleme: [Launcher rehberi](launcher/README.md).
3. Derleme hedefleri: [Minecraft sürümleri](docs/MINECRAFT_TARGETS.md).
4. Modüllerin kapsam ve sınırları: [Modül notları](docs/MODULES.md).

Üretim API'si: `https://onestep-auth.gogogamesas.workers.dev`.
Depodaki [yayın kaydı](server/deployment.json) 20 Ekim 2026 launcher-only v2
geçişini kaydeder. DLL'den doğrudan lisans girişi ve eski v1 launcher yolları
kapalıdır. Bu belge düzenlemesi canlı durumun yeniden doğrulandığı anlamına gelmez.

Kaynak/SDK testlerinin geçmesi, tüm sürümlerde oyun içi desteğin doğrulandığı
anlamına gelmez. Her DLL yalnızca derlendiği tam Minecraft sürümünü kabul eder.
1.21.11 hibrit geçişinin ilk üç grubunu kullanıcı oyun içinde doğrulamıştır;
diğer grupların ve hedeflerin kabul testleri kendi belgelerinde belirtilir.

## Gereksinimler ve derleme

- Windows x64, Visual Studio C++ Build Tools/MSBuild ve Windows SDK.
- PowerShell, Python 3.9+; bazı SDK denetimleri Python 3.11+ ister.
- Köprü kaynakları değişirse JDK 17+; 26.1 ailesinin çalışma/denetim ortamı Java 25.
- Hedefle tam eşleşen Minecraft Fabric; Sky entegrasyonu için Fabric API.
- Worker geliştirmesi için Node 24 ve kilitli npm bağımlılıkları.

Depo kökünde, Windows Developer PowerShell içinde:

Visual Studio solution/proje adı `OneStep.slnx` / `OneStep.vcxproj`'dir.
Proje adı değişikliği DLL/launcher çıktı adlarını veya sürüm yollarını değiştirmez.

```powershell
./scripts/Build-Dev.ps1
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
./scripts/Build-Release.ps1 -AllMinecraftVersions -SkipSign
./scripts/Build-Launcher.ps1
```

`Build-Dev.ps1` yalnızca 1.21.11 için imzasız Release DLL üretir. Eski DLL'i
yedekleyip yeniden linkler; ara derleme dosyalarını koruduğu için yalnızca
değişen kaynaklar derlenir. Release optimizasyonlarını değiştirmez.
`-Clean` temiz derleme içindir; `-SkipSign` imzasız geliştirme çıktısı üretir.
Varsayılan DLL `build/Onestep.dll`, ek hedefler
`build/minecraft/<sürüm>/Onestep.dll` yolundadır.
Launcher yardımcısı ayrıca `build/launcher-client/Onestep.dll` ve
`build/launcher/Onestep-Launcher.exe` üretir. İmza ve köprü yeniden üretim
ayrıntıları [hedefler rehberindedir](docs/MINECRAFT_TARGETS.md#build).

GitHub Actions, offline kontrollerden sonra yedi DLL hedefini ve launcher'ı
derler. Başarılı artifact üretimi, canlı yayın veya oyun içi doğrulama değildir.
Kimlik doğrulaması üretim derlemesinde zorunludur;
[public build ayarları](server/SECURITY_SETUP.md) eksikse derleme durur.

## Doğrulama

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
python scripts/Test-AuthConfiguration.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-HybridBridge.py
git diff --check
```

Offline C++ testleri Linux'ta GCC, Windows Developer PowerShell'de MSVC kullanır;
`--compiler`, `--test` ve `--minecraft-version` ile sınırlandırılabilir.
Hybrid testi `java` ve `javac` gerektirir. UI testi gerçek ImGui kodunu,
oyun/platform hizmetleri için etkisiz test karşılıklarıyla çalıştırır.
Sunucu testleri ve dry-run: [server/README.md](server/README.md#yerel-doğrulama).
Kaynak kontrolleri DLL bağlama, canlı OpenGL veya Minecraft testinin yerine geçmez.

## Kaynak ve sahiplik

Güvenli temizlik kuralları, isim eşleştirmeleri ve korunan uyumluluk kodları:
[Bakım ve temizlik denetimi](docs/MAINTENANCE.md).

| Dizin | Sorumluluk |
| --- | --- |
| `src/app/` | Başlangıç/kapanış, izin denetimi ve modül döngüsü |
| `src/auth/` | Launcher/runtime izinleri, imza ve config taşıması |
| `src/modules/` | Modüller, katalog, stable ID'ler ve ayar şemaları |
| `src/platform/`, `src/sdk/minecraft/` | Sürüm adaptörleri, JNI/JVMTI ve ortak tipler |
| `src/services/`, `src/bridge/` | Paylaşılan oyun/input servisleri ve Java I/O köprüleri |
| `src/gui/` | Windows/OpenGL entegrasyonu, fontlar, girdiler ve ortak kontroller |
| `src/ui/` | GrimUI sayfaları, HUD editörü ve config/Keybinds görünümü |
| `src/configs/`, `src/friends/` | Public config istemcisi, yerel keybind profili ve arkadaş listesi |
| `launcher/`, `server/` | Native başlatıcı ve lisans/config Worker'ı |
| `assets/`, `include/`, `tests/`, `scripts/` | Kaynaklar, bağımlılıklar, testler ve araçlar |

Registry render hook'tan önce kurulur. Kapanışta ağ/render işleri durur;
modüller, oyun önbelleği ve JNI/JVM sahipliği ardından temizlenir.
Client-thread oyun işleri dispatcher sınırından geçer. Genel modüller arası
tek bir atomik durum yayını veya tüm runtime için tamamlanmış yeni thread modeli
varsayılmamalıdır.

## Geliştirme kuralları

- Mevcut stable ID'leri, config alanlarını ve protokol bağlarını koru.
- Yeni modülü `src/modules/module_catalog.cpp` ve
  `src/modules/settings_manifest.json` üzerinden kaydet.
- Standart ayarları ortak şema/renderer'a bağla; gizli config alanlarını kaybetme.
- Input/hotbar için mevcut servisleri, JNI için sahipli context/dispatcher'ı kullan.
- Hedef sürüm farklarını adaptörde tut; modül/SDK ağacını sürüm başına kopyalama.
- Proje başlıklarını precompiled header'a toplama.
- Sırları, oyun JAR'larını, özel yedekleri ve operatör kasalarını Git'e koyma.
- Push, yayın, migration veya anahtar rotasyonunu otomatik bir temizlik adımı sayma.

## Belgeler

| Konu | Belge |
| --- | --- |
| Modüller, config uyumluluğu ve görsel sınırlar | [MODULES](docs/MODULES.md) |
| Menü, HUD, temalar ve Keybinds | [GRIM_UI_LAYOUT](docs/GRIM_UI_LAYOUT.md) |
| Tam sürüm hedefleri ve derleme | [MINECRAFT_TARGETS](docs/MINECRAFT_TARGETS.md) |
| 26.x SDK/JNI denetimi ve tarihsel keşif | [MINECRAFT_26_PORT](docs/MINECRAFT_26_PORT.md) |
| C++ karar motorları / Java oyun I/O sınırı | [HYBRID_MIGRATION](docs/HYBRID_MIGRATION.md) |
| Launcher kullanımı / v2 protokolü | [Launcher](launcher/README.md) · [LAUNCHER_V2](docs/LAUNCHER_V2.md) |
| Sunucu mimarisi ve testler | [Server](server/README.md) |
| Aktif kurulum / güvenlik ve kalan olay işleri | [SECURITY_SETUP](server/SECURITY_SETUP.md) · [AUTH_HARDENING](server/AUTH_HARDENING.md) |
| Çalıştırılmayan Wolf tarihçesi | [Arşiv](server/archive/wolf/README.md) |
| Lisanslar ve asset kökeni | [Bildirimler](THIRD_PARTY_NOTICES.md) · [Kozmetikler](assets/cosmetics/README.md) · [Gökyüzü](assets/skies/README.md) |
