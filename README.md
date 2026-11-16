# OneStep

Minecraft Fabric için Windows x64 üzerinde çalışan C++20 istemci. OpenGL/ImGui arayüzü, 60 modül, HUD düzenleyicisi ve sürüme özel Java köprüleri içerir. DLL, JVM ve sürüm kontrollerinin ardından doğrudan başlar.

## Derleme

Windows x64, Visual Studio C++ Build Tools/MSBuild, Windows SDK, PowerShell, Python 3.9+ ve JDK 17+ gereklidir. 26.1 ailesinin oyun ortamı Java 25 gerektirir.

```powershell
./scripts/Build-Dev.ps1
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
./scripts/Build-Release.ps1 -AllMinecraftVersions -SkipSign
./Build-Test.ps1 -MinecraftVersion 1.21.11
```

Solution/proje: `OneStep.slnx` / `OneStep.vcxproj`. 1.21.1 çıktısı `build/Onestep.dll`, diğer hedefler `build/minecraft/<sürüm>/Onestep.dll` yolundadır. Debug test çıktısı `build/local-test/<sürüm>/Onestep-TEST.dll` yolundadır. Her DLL yalnız derlendiği tam Minecraft sürümünü kabul eder.

## Config ve tuş profilleri

Config ekranı profilleri bu bilgisayarda `%APPDATA%/OneStep/configs` altında saklar. Kaydetme, güncelleme, yükleme, silme ve favoriler yerel çalışır. Mevcut modül JSON alanları ve eski config dönüşümleri korunur. Tuş profilleri kendi mevcut yerel dosyasını kullanır.

## Doğrulama

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
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
- [Menü ve HUD](docs/GRIM_UI_LAYOUT.md)
- [Minecraft hedefleri ve derleme](docs/MINECRAFT_TARGETS.md)
- [Hibrit geçiş](docs/HYBRID_MIGRATION.md)
- [26.x SDK denetimi](docs/MINECRAFT_26_PORT.md)
- [Üçüncü taraf bildirimleri](THIRD_PARTY_NOTICES.md)
