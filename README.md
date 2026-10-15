# Onestep

Windows x64 üzerinde Minecraft Fabric 1.21.1 için geliştirilen native C++20 istemci.
OpenGL ve ImGui tabanlı arayüz, HUD düzenleyicisi, modül sistemi ve public config
istemcisi içerir.

## Güncel durum

- Ürün adı **Onestep**.
- Lisans girişi ve lisans heartbeat akışı şu an devre dışı; auth gelecekte eklenecek.
- Public config bağlantısı lisans girişinden ayrı çalışacak şekilde korunuyor.
  Uzak servisin bu ortamda erişilebilirliği doğrulanmadı.
- Vape sayfasında yalnızca Aim Assist (Simple/Adaptive), Left Clicker ve Right Clicker var.
- Eski UI/Qt kaynakları ve arayüz ses sistemi kaldırıldı.

## Gereksinimler

- Windows x64
- Visual Studio C++ Build Tools / MSBuild ve Windows SDK
- PowerShell; altyapı kontrolleri ve proje filtreleri için Python 3.9+
- Oyun içi kullanım için Minecraft Fabric 1.21.1

## Derleme

Komutları depo kökünde çalıştır:

```powershell
# Release x64 DLL
.\scripts\Build-Release.ps1

# Temiz derleme
.\scripts\Build-Release.ps1 -Clean

# İmzasız yerel derleme
.\scripts\Build-Release.ps1 -SkipSign

```

Proje yalnızca DLL olarak derlenir:

| Çıktı | Yol |
| --- | --- |
| İstemci DLL | `build\Onestep.dll` |

İmzalama scriptleri, mevcut kullanıcı sertifika deposundaki
`CN=Thedorius Lite Test` sertifikasını arar. Sertifika adı henüz değiştirilmedi.
GitHub Actions, `main` güncellendiğinde offline testlerden sonra Windows x64
DLL'yi derler ve `Onestep-Windows-x64` artifact'i olarak saklar. Bu çıktı imzasızdır.

## Kaynak yapısı

```text
assets/                   Fontlar, ikonlar ve arka plan
include/                  Üçüncü taraf kütüphaneler
scripts/                  Derleme ve kaynak doğrulama araçları
server/                   Gelecekteki auth için korunan Worker ve veritabanı kaynakları
src/
  main.cpp                Yalnızca DLL girişi ve worker başlatma
  app/                    Uygulama başlangıcı, kapanışı ve modül döngüsü
  auth/                   Korunan auth kodu ve config servis bağlantısı
  cache/                  Oyun nesnesi önbelleği
  configs/                Public config istemcisi
  friends/                Oturumluk arkadaş listesi
  gui/                    ImGui shell, kontroller ve yerel arayüz tercihleri
  ui/                     Çalışma alanı, HUD editörü ve config sayfası
  modules/                Modüller, kayıt sistemi ve config serileştirme
    core/setting_schema.hpp  UI'dan bağımsız ayar tanımları ve doğrulama
    vape/engines/         Korunan Aim Assist ve clicker motorları
    vape/vape_config.hpp  Korunan Vape modüllerinin ortak ayar modeli
    misc/settings/        Utility modüllerinin bağımsız ayar modelleri
    combat/settings/      Combat modüllerinin bağımsız ayar modelleri
    visual/settings/      Görsel modüllerin bağımsız ayar modelleri
    render/settings/      Watermark ve Arraylist tercihleri
    settings_manifest.json  45 stable ID için model ve panel sahipliği
  sdk/minecraft/
    fabric_1_21_1/        Alanlara bölünmüş JNI mapping'leri ve sürüm manifesti
  mappings.hpp            Eski include yolları için ince uyumluluk başlığı
  platform/               Oyun adaptörü, sahipli JVM oturumu ve client-thread iş kuyruğu
  services/               Ortak oyun ve input servisleri
  jvmti/                  JVM araç katmanı
  maths/                  Oyun matematiği
  utils/                  Yardımcı araçlar
tests/                    Offline motor, config ve arayüz test kaynakları
```

## Doğrulama

```powershell
.\scripts\Test-Infrastructure.ps1
python .\scripts\Test-Infrastructure.py
python .\scripts\Test-AimAssistSources.py
python .\scripts\Test-TriggerbotSources.py
python .\scripts\Test-JumpResetSources.py

# Proje girdileri değiştiğinde
python .\scripts\Refresh-ProjectFilters.py
```

18 offline testi tek komutla derleyip çalıştırmak için:

```sh
python scripts/Test-Offline.py

# Yalnızca ayar modeli ve yayınlama testleri
python scripts/Test-Offline.py --test utility_settings --test remaining_settings --test settings_store --test settings_renderer
```

Araç Linux'ta g++, Windows'ta Developer PowerShell içindeki MSVC derleyicisini
kullanır; farklı derleyici yolu `--compiler` ile verilebilir. Testler assertion
kontrolleri açık şekilde geçici klasörde derlenir. Modül tuş/lifecycle akışı,
Vape motorları, Wolf aim matematiği, Triggerbot ve Jump Reset karar/input
modelleri, ortak config serileştirme ve dört modül grubunun config testleri
çalışır. Ek utility testi on modülün eski config alanlarını, varsayılanlarını,
tamsayı sınırlarını ve gizli alanlarını doğrular. Ayar deposu testi iki eşzamanlı
yazıcı ve bir okuyucuyla tutarlı snapshot ve işlem geri alma davranışını sınar.
Kalan ayar modellerinin testi eski alan adlarını, renk/dizi/liste türlerini,
seyrek enum değerlerini, EXP/Anchor uyumluluğunu ve geçersiz içe aktarmaların
atomik reddini doğrular. Renderer testi gerçek çizim yapmadan kontrol
bağlantılarını, koşullu görünürlüğü, yüzde dönüşümünü ve UI/import limitlerini sınar.
SDK testi 127 sınıf adını, temel JNI imzalarını ve dokuz domain başlığının
tek başına derlenmesini kontrol eder; JVM başlatmaz veya oyuna bağlanmaz.
ESP oturum testi ani bağlantı kopmasını, hızlı dünya/sunucu değişimini,
oyuncu nesnesinin yenilenmesini ve eski worker sonuçlarının reddini doğrular.
Player ESP canlı dünya/kamera/oyuncu verisi okunamadığında eski kutulara dönmez.
Block/Storage ESP, Nametag ve Radar önbellekleri dünya ve oyuncu kimliğine
bağlıdır; geçersiz oturumda temizlenir, önceki oturumun taraması yeniden yazılamaz.
Windows'a bağlı UI tercihleri ve görsel arayüz testleri bu pakete dahil değildir.

Kaynak kontrolleri ve offline testler tek başına oyun içi doğrulama değildir.
Windows Release x64 DLL derlemesi de GitHub Actions üzerinde başarıyla tamamlandı.
Oyun içi doğrulama henüz yapılmadı; gerçek bir Minecraft oturumunda ayrıca denenmelidir.

## Uygulama yaşam döngüsü

`main.cpp` worker'ı başlatır ve `app::run()` tamamlandıktan sonra DLL'yi boşaltır.
`app/runtime.cpp` başlangıç/kapanış sırasını, `app/module_loop.cpp` ise modül
tuşlarını ve worker tick akışını yönetir. Devre dışı eski auth konsol ekranı
giriş dosyasından kaldırıldı; gelecekteki auth kaynakları korunuyor.

Modüller render hook kurulmadan önce kaydedilir. Kapanışta config worker ve
render akışı durur; ardından modüller, oyun önbelleği, JNI metadata ve JVM
bağlantısı temizlenir. Aynı kaynak sahipliği, yarıda kalan başlangıçta da geçerlidir.
Panic isteği thread'ler arasında atomik olarak aktarılır.

Tuşlar ekran/focus engeli sırasında da örneklenir; basılı tutulan bir tuşun
focus dönüşünde veya yeniden atanırken modülü yanlışlıkla açması önlenir.
Kendini kapatan modüllerin lifecycle geçişleri aynı tick içinde tamamlanır.
Diğer paylaşılan modül/UI durumları henüz bütünüyle yeni bir thread modeline
taşınmadı; registry ve ayar yayınlama sahipliği sonraki aşamadır.

## Ayarlar ve SDK sınırları

`setting_schema.hpp` bool, tamsayı, sayısal değer, aralık, renk, metin listesi ve
seyrek enum değerlerini de destekleyen seçenek listeleri için
üye bağlarını, config anahtarlarını, sınırları ve dil etiketlerini tanımlar.
`ui/settings_renderer.hpp` bu tanımları mevcut arayüz kontrolleriyle çizer.
İçe aktarma önce yerel aday ayarları doğrular; geçersiz değerler canlı ayarlara
uygulanmaz. Gizli kontrollerin değerleri config içinde korunur.

Katalogdaki 45 stable ID'nin tamamı `settings_manifest.json` ile izlenir.
Modüle özel ayarları bulunan 41 kayıt ortak şemaya bağlandı. Friends Manager,
Click GUI, Panic ve Full Bright için modüle özel config alanı yoktur; arkadaş
listesi ve genel UI tercihleri kendi mevcut sahiplerinde kalır.

27 combat/utility/visual modülü `settings_store.hpp` üzerinden kilitli yayın
ve kopya snapshot kullanır. Vape ve Chams mevcut kilitlerini, Jump Reset
revision/iptal mekanizmasını korur. Aim/Trigger/Anchor modelleri ortak şemaya
geçerken mevcut runtime sahipliklerini korur; bu taşıma bütün paylaşılan
oyun durumlarının thread modelini yenilemez. HUD/Arraylist tercihleri render
katmanında tutulur; HUD editörüyle mevcut referanslar korunur.

Block ESP blok seçicisi, Spotify'ın ekran boyutuna bağlı konum kontrolleri ve
Jump Reset hazır ayarları özel panel olarak kalır; standart kontroller aynı
renderer'ı kullanır. Önceden planlanmış oyun eylemlerinin genel iptali bu
katmanın sorumluluğu değildir. Stable ID ve mevcut ayar alan adları korunur.
Sayısal içe aktarmalar modülün mevcut runtime sınırlarına normalize edilir;
yanlış tür, geçersiz enum ve aşırı büyük değerler reddedilir. Chest Stealer'ın
yanında Auto Totem, Legit Auto Totem ve Fast EXP'nin de UI ve import limitleri
ayrı tanımlanır; Auto Tool'un görünmeyen eski config alanı da korunur.
Radar konumları geniş ekranlarda slider sınırlarıyla kırpılmaz.

Mapping tanımları `sdk/minecraft/fabric_1_21_1/` altında Java tipleri, geometri,
blok/world, inventory/network, entity tipleri, entity, client entity, input ve
client başlıklarına ayrılır. Domain bağımlılıkları açık include zincirleriyle
kurulur; JNI sınıf isimleri `manifest.json` ile denetlenir. `mappings.hpp` eski
kullanıcılar için tüm SDK'yı sunar; yeni kod ihtiyaç duyduğu domain'i kullanmalı.

## Config dosyaları

Yeni config'ler sürüm etiketi taşımaz: kökte `modules` listesi; her kayıtta
`id`, `enabled`, `keybind` ve `settings` bulunur. Eski sürüm alanı bulunan
dosyalar da okunur; yeniden kaydedilen çıktı sade biçimdedir.
Yükleyici JSON'un tamamını ve tüm modül ayarlarını değişiklik yapmadan önce
doğrular. Beklenmeyen uygulama hatasında önceki ayarları geri yüklemeyi dener;
etkinlik ve tuş atamaları ancak ayarlar uygulandıktan sonra değiştirilir.
Bu işlem worker thread'lerine karşı modüller arası tek bir atomik yayın değildir.
Dosya boyutu 96 KB, modül sayısı 128 ve JSON derinliği 32 ile sınırlıdır.
Bozuk JSON, yinelenen alan/ID, geçersiz tuş kodu ve yanlış türler reddedilir;
artık bulunmayan modüller atlanır. Genel config testi geri uyumluluğu,
Unicode metinleri, tüm belge doğrulamasını ve hata durumunda geri almayı sınar.

## Geliştirme kuralları

- Proje başlıklarını precompiled header içine ekleme.
- Modüllerde input işlemleri için `input_service`, hotbar işlemleri için
  `hotbar_service` kullan.
- JNI thread ve exception işlemlerini `jni_context` üzerinden yürüt.
- Client/render thread gerektiren işleri `client_dispatcher` üzerinden sıraya al.
- Yeni modülleri ilgili kategoriye ekle ve `module_catalog.cpp` içinde kaydet.
- Düz JSON config biçimini ve mevcut modüllerin stable ID değerlerini koru.

Üçüncü taraf kaynakları: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Gelecekteki auth kurulumu: [server/SECURITY_SETUP.md](server/SECURITY_SETUP.md).
