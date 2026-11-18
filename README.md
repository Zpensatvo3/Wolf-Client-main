# OneStep

Minecraft Fabric için Windows x64 üzerinde çalışan C++20 istemci. OpenGL/ImGui arayüzü, 55 modül, HUD düzenleyicisi ve sürüme özel Java köprüleri içerir. DLL, JVM ve sürüm kontrollerinin ardından doğrudan başlar.

## Güncel kaynakları bilgisayara alma

Geliştirmelerin tamamı varsayılan **`main`** dalında birleştirilir. GitHub'da `Code > Download ZIP` ile güncel kaynakları indirebilir veya Git ile mevcut kopyanı güncelleyebilirsin:

```sh
git switch main
git pull --ff-only origin main
```

Yerel dosyalarını değiştirdiysen güncellemeden önce `git stash push -u` ile sakla. Güncellemeden sonra `git stash pop` ile geri al; aynı satırlar değiştiyse çakışmaları çöz. İlk kez indiriyorsan:

```sh
git clone https://github.com/Zpensatvo3/Wolf-Client-main.git
cd Wolf-Client-main
```

ZIP ile güncellerken yeni arşivi temiz bir klasöre aç. Üzerine dosya kopyalamak, kaldırılan eski modül ve test dosyalarını diskte bırakabilir. Kaynak dosyalarını güncellemek DLL'yi güncellemez; aşağıdaki komutlarla derle veya [Actions](https://github.com/Zpensatvo3/Wolf-Client-main/actions) içindeki başarılı çalıştırmanın sürümüne uygun artifact'ini indir.

## Son geliştirmeler

- ClickGUI varsayılan tuşu **Insert**. Vape Aim Assist, Combat kategorisinde; dört eski Left/Right Clicker modülü ve ilişkili kod/testleri kaldırıldı.
- Görsel modüllerin animasyonları, Sky geçişleri/sisleri ve çizim akışı iyileştirildi. Uzun oturumlar için tarama bütçeleri, medya tamponları, oturum geçişleri ve renderer temizliği düzenlendi.
- Triggerbot hedefe tekrar girişte saldırı zamanlamasını korur; Acele etkisi cooldown hesabına katılır. Shield Breaker oyun mantığı Java'ya taşındı ve Triggerbot'un saldırı/slot sahipliğiyle birleştirildi.
- Son tutarlılık düzeltmeleri ve doğrulama sınırları [yeniden kontrol raporunda](docs/CLIENT_RECHECK_2026-11-18.md), Shield Breaker ayrıntıları [Java geçiş belgesinde](docs/SHIELD_BREAKER_JAVA.md) bulunur.

## Derleme

Windows x64, Visual Studio C++ Build Tools/MSBuild, Windows SDK, PowerShell, Python 3.9+ ve JDK 17+ gereklidir. 26.1 ailesinin oyun ortamı Java 25 gerektirir.

```powershell
./scripts/Build-Dev.ps1
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
./scripts/Build-Release.ps1 -AllMinecraftVersions -SkipSign
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -Clean -SkipSign
./Build-Test.ps1 -MinecraftVersion 1.21.11
```

Solution/proje: `OneStep.slnx` / `OneStep.vcxproj`. 1.21.1 çıktısı `build/Onestep.dll`, diğer hedefler `build/minecraft/<sürüm>/Onestep.dll` yolundadır. Debug test çıktısı `build/local-test/<sürüm>/Onestep-TEST.dll` yolundadır. Her DLL yalnız derlendiği tam Minecraft sürümünü kabul eder.

Normal yerel derleme artımlıdır; MSBuild değişen kaynakları ve bunlardan etkilenen dosyaları derler. `-Clean` yalnız tam yeniden derleme gerektiğinde kullanılır. `Build-Dev.ps1` 1.21.11 hedefini derler ve önceki DLL'yi yedekler. 1.21.1 dahil yedi hedef genel GitHub workflow'unda ayrı üretilir; 1.21.11 DLL'si 1.21.1 üzerinde çalışmaz.

GitHub Actions, **sccache** ile derleme sonuçlarını sonraki çalıştırmalarda kullanır. İlk çalıştırma önbelleği doldurur; sonraki çalıştırmaların kazancı değişen dosyalara ve cache erişimine bağlıdır. Yalnız Markdown değişiklikleri otomatik DLL derlemesi başlatmaz; Actions'tan elle başlatılabilir. Loglarda cache hit/miss sayıları ve MSBuild görev süreleri görünür. Kurulum ve ölçüm yöntemi: [derleme ve önbellek](docs/BUILD_PERFORMANCE.md).

Yalnız 1.21.11 DLL'si gerektiğinde [Build Fabric 1.21.11 DLL](https://github.com/Zpensatvo3/Wolf-Client-main/actions/workflows/build-1.21.11.yml) sayfasından `Run workflow > main` seç. [Build Onestep](https://github.com/Zpensatvo3/Wolf-Client-main/actions/workflows/build.yml) tüm hedefleri ve daha geniş test paketini çalıştırır; iki workflow'un toplam süreleri doğrudan karşılaştırılmamalıdır.

18 Kasım doğrulamasında aynı kaynakla 1.21.11 DLL adımı ilk cache derlemesinde **13 dk 15 sn**, sonraki çalıştırmada **1 dk 53 sn** sürdü; 128 isteğin 127'si cache'ten karşılandı. [Yedi hedefin başarılı derlemesi ve DLL'leri](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44390600356), [son 1.21.11 DLL'si](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44394889306/artifacts/14471370175). Ayrıntılı sayaçlar ve ölçüm sınırları derleme belgesindedir.

## Config ve tuş profilleri

Config ekranı profilleri bu bilgisayarda `%APPDATA%/OneStep/configs` altında saklar. Kaydetme, güncelleme, yükleme, silme ve favoriler yerel çalışır. Config dosyaları keybind saklamaz; eski yerel config'lerdeki keybind alanları açılışta sıfırlanır. Config yüklemek oturumun mevcut tuş atamalarını değiştirmez.

İlk menü açılışında keybind'leri bu bilgisayarda saklamak için onay sorulur. Kabul edilirse `%LOCALAPPDATA%/onestep/keybinds.json` kaydedilir ve sonraki açılışta yüklenir. Reddedilirse eski keybind dosyası silinir; atamalar yalnız oturumda kullanılır. Karar ayrı bir `.consent` dosyasında tutulur. Keybinds sayfasındaki “Save keybinds on this computer” seçeneğiyle karar değiştirilebilir. Onay verilmeden mevcut keybind dosyası yüklenmez veya yeni atamalar diske yazılmaz.

## Doğrulama

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
python -m unittest discover -s tests -p 'test_build_cache.py' -v
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-HybridBridge.py
git diff --check
```

Offline başarı Windows DLL derlemesi, gerçek OpenGL veya oyun içi kabul yerine geçmez. Hibrit geçişin ilk üç grubu için önceki kullanıcı kabulü kayıtlıdır; diğer gruplar ve sürümler kendi oyun kabulünü gerektirir.

## Kaynak düzeni

| Dizin | Sorumluluk |
| --- | --- |
| `src/app/` | Başlangıç, kapanış ve modül döngüsü |
| `src/modules/` | Modül kataloğu, stable ID ve ayar şemaları |
| `src/platform/`, `src/sdk/minecraft/` | Sürüm adaptörleri ve JNI/JVMTI |
| `src/services/`, `src/bridge/` | Oyun/input servisleri ve Java köprüleri |
| `src/gui/`, `src/ui/` | Platform/render, menü, HUD ve sayfalar |
| `src/configs/`, `src/friends/` | Yerel profiller ve arkadaş listesi |
| `assets/`, `include/`, `tests/`, `scripts/` | Kaynaklar, bağımlılıklar ve doğrulama araçları |

Mevcut stable ID, config alanları ve gizli uyumluluk verilerini koruyun. Yeni modülleri `src/modules/module_catalog.cpp` ve `src/modules/settings_manifest.json` üzerinden kaydedin. JNI için sahipli context/dispatcher, input ve hotbar için mevcut servisleri kullanın. Sürüm farklarını adaptörde tutun; proje başlıklarını PCH içine taşımayın.

## Belgeler

- [Modüller](docs/MODULES.md)
- [Aim Assist V2 karşılaştırması ve doğrulama](docs/AIM_ASSIST_V2.md)
- [Click Aim Assist modları ve eski profil aktarımı](docs/CLICK_AIM_ASSIST.md)
- [Menü ve HUD](docs/GRIM_UI_LAYOUT.md)
- [Minecraft hedefleri ve derleme](docs/MINECRAFT_TARGETS.md)
- [Derleme performansı ve önbellek](docs/BUILD_PERFORMANCE.md)
- [Uygulama ve teslim kaydı](docs/IMPLEMENTATION_HANDOFF_2026-11-17.md)
- [Performans denetimi](docs/PERFORMANCE_AUDIT_2026-11-17.md)
- [Hibrit geçiş](docs/HYBRID_MIGRATION.md)
- [26.x SDK denetimi](docs/MINECRAFT_26_PORT.md)
- [Üçüncü taraf bildirimleri](THIRD_PARTY_NOTICES.md)
