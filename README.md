# Onestep

Windows x64 üzerinde Minecraft Fabric 1.21.1 için geliştirilen native C++20 istemci.
OpenGL ve ImGui tabanlı arayüz, HUD düzenleyicisi, modül sistemi ve public config
istemcisi içerir.

## Güncel durum

- Auth kaynakları modüllere ayrıldı; koyu temalı yeni yönetim paneli, sayfalı lisans
  listesi, ayrıntı çekmecesi, işlem geçmişi ve sistem durumu hazırlandı. Bu kaynak
  değişikliği yereldir; canlı servise deploy edilmedi. Mimari, test ve güvenli yayın
  adımları: [server/README.md](server/README.md).
- Terminal tarzı native **Onestep Launcher** hazır: key girişi, imzalı sürüm
  kontrolü, lisanslı internetten indirme ve seçilen açık Minecraft'a başlatma.
  EXE: `build/launcher/Onestep-Launcher.exe`. DLL launcher keyini almaz; hedef
  işlem/cihaz/sürüme bağlı 120 saniyelik tek kullanımlık izni sunucuda doğrular.
  Kullanım/derleme ve sınırlar: [launcher/README.md](launcher/README.md).
- Ürün adı **Onestep**.
- Auth servisi `https://onestep-auth.gogogamesas.workers.dev` adresinde yayında.
  Worker, ayrı `onestep-licenses` D1 veritabanı ve rate limiter kullanır.
  `server/AuthDeployment.props` normal x64 derlemelerde lisans girişini açar;
  doğrulamadan önce modüller/HUD/config işleri bekler. Native HTTPS giriş,
  imza/nonce kontrolü, hatalı anahtar reddi, lisanslı config listesi ve heartbeat
  gerçek serviste doğrulandı; oyun içi giriş ekranı henüz denenmedi.
- `Onestep-Auth.cmd`, yönetici parolasını ve ilk client lisansını kullanıcı
  isteğiyle panoya kopyalayan küçük yerel yardımcıyı açar (PowerShell 7 gerekir).
  Kaynakta parola bulunmaz; yedek `server/.credentials` altında Windows hesabına
  bağlı şifreli saklanır ve git dışında tutulur. Tarihsel Wolf operasyon notları
  `server/archive/wolf/` altındadır; güncel servis bu kaynaklara bağlı değildir.
- Yeni serviste public config çağrıları da lisanslı oturum ister; eski Wolf
  servisinin ayrı lisanssız config yolu değiştirilmedi.
- HWID'ye ek olarak Windows CNG cihaz anahtarıyla imzalı giriş/heartbeat vardır.
  TPM destekleniyorsa kullanılır; bu bilgisayarda TPM yolu doğrulandı. İlk yeni
  sürüm girişi lisansı bu public key'e bağlar; bundan sonra eski/imzasız DLL
  kabul edilmez. HWID Reset cihaz anahtarı bağını da sıfırlar. Eski HWID özeti
  değiştirilmedi. VMProtect kurulu değil; gerçek `.vmp` projesi ve console sürümü
  için ayrı korumalı çıktı/sonradan imzalama akışı hazırdır, henüz uygulanmadı.
  Ayrıntılar: [server/PROTECTION.md](server/PROTECTION.md).
- Vape sayfasında yalnızca Aim Assist (Simple/Adaptive), Left Clicker ve Right Clicker var.
- Anchor Macro artık tek modüldür: Legit ve Blatant arasında seçim yapılır; Safe Anchor
  ayrı kalır. İki modun slot geçişi, anchor koyma, glowstone basma, patlatma, geri dönüş,
  rastgeleleştirme ve kamera tercihleri ayrı saklanır. Slot ve işlem gecikmeleri toplanır.
  Rastgeleleştirme kapalıyken sabit gecikme; açıkken ayarlanan ± sapma kullanılır.
  Her gecikme bileşeni 0–500 ms ile sınırlıdır; Blatant da ayarlanan süreleri uygular.
  Sıfır gecikmede bile kare başına en fazla bir etkileşim gönderilir. Makro her açılışta
  tek döngü yapar; dolum gözlenmeden patlatma aşamasına geçmez. Dünya/oyuncu değişimi,
  odak/menü engeli ve elle slot değişimi bekleyen işi iptal eder; slot yalnızca hâlâ
  modülün seçtiği slotta kalmışsa geri alınır. Rastgeleleştirme sunucu kabulü garantisi değildir.
  Blok/dolum kontrolleri güncel istemci durumunu okur; tek başına sunucu onayı değildir.
  Eski `anchor_macro` ve `blatant_anchor_macro` configleri tek kayda aktarılır; iki
  profilin ayarları korunur. İkisi de açıksa Legit, yalnızca Blatant açıksa Blatant
  seçilir; seçilen eski modülün tuşu alınır. Yeni birleşik kayıt varsa önceliklidir.
- Anti Bot yeni kullanımda varsayılan açıktır; config'teki açık/kapalı tercihi korunur.
  UUID tabanlı sunucu oyuncu kaydını kontrol eder. Varsayılan olarak ilk 40 tick'i
  bekler, ardından 40 tick boyunca kayıtsız kalan oyuncu nesnelerini aim/trigger,
  otomatik hedef yardımcıları ve oyuncu görsellerinden süzer. Görünmezlik, isim
  biçimi veya hareketsizlik tek başına bot ölçütü değildir. Liste güvenilir değilse
  oyuncuyu elemez; sunucu/dünya değişiminde karar geçmişi temizlenir.
- Combat > Crystal Aura, en yakın uygun oyuncuya kilitlenir; arkadaşları, izleyicileri
  ve Anti Bot'un elediği nesneleri hedeflemez. Kılıçla vurur, hedef havalanınca uygun
  noktada obsidyen/kristal yerleştirip kristali kırar. Mevcut obsidyen/bedrock zemini
  yeniden kullanabilir; hedef ölünce veya geçersizleşince sıradakini seçer.
  Hedefler menüsünde Oyuncular (varsayılan), Oyuncular + Moblar ve Yalnızca Moblar
  (Test) seçilebilir. Golem, hayvan ve diğer MobEntity türleri desteklenir; zırh
  askıları ve eşyalar hedef değildir. Moblarda Havalanma Bekleme varsayılan açıktır:
  golem gibi geri tepmeye dirençli moblarda kılıç vuruşundan sonra yerdeyken de
  kristal döngüsü denenebilir. Bu seçenek oyuncuların havalanma şartını değiştirmez.
  Kılıç, obsidyen ve kristal hotbar'dan otomatik seçilir. Normal oyun erişimi, görüş,
  saldırı cooldown'u ve yerleştirme boşluğu kontrol edilir. Kamera görünür şekilde
  döner; otomatik yürüme, havaya blok koyma veya erişim artırma uygulanmaz.
  **Kendine hasar/ölüm koruması yoktur.** Arkadaşlar hedeflenmese de patlama alanında
  zarar görebilirler. Menü, odak/oturum kaybı ve kapatma işlemleri döngüyü iptal eder.
  Eşyalar bitince bekler; son yerleştirilen kristali kırma aşaması tamamlanabilir.
  Blok durumu istemciden yeniden okunur, bu tek başına sunucu onayı değildir;
  kristal oluşması/kaybolması ayrıca izlenir. Sonuç gelmezse zaman aşımı ve gecikmeyle
  kılıç aşamasına döner; sınırsız paket serisi göndermez. Oyun içi doğrulama yapılmadı.
  Native/Vape clicker'ların araya saldırı sokması ortak sahiplikle engellenir;
  Triggerbot önceliklidir. Diğer blok/slot makrolarıyla birlikte kullanım doğrulanmadı.
- Temalar sayfasında menü arka planı seçilebilir: Klasik (eski görünüm), Hisar,
  Pamuk Alacakaranlığı, Nebula ve Aurora. Hazır görseller DLL'e gömülüdür; indirme
  gerektirmez. **Görsel ekle** ile PNG/JPG seçilebilir (16 MB, kenar başına 8192
  piksel ve toplam 16 megapiksel sınırı). Görsel oranı korunarak ortadan kırpılır;
  yoğunluk ve karartma ayarlanır. Özel görselin yerel kopyası
  `%LOCALAPPDATA%\onestep\menu-background.image` içinde saklanır; kaynak dosya
  değiştirilmez. Seçim UI tercihlerine kaydedilir, paylaşılan oyun configlerine
  eklenmez. Eski tercihler klasik görünümü korur; kayıp görselde klasik görünüm
  kullanılır. Windows dosya seçicisi ve oyun içi görünüm ayrıca denenmelidir.
- Eski UI/Qt kaynakları ve arayüz ses sistemi kaldırıldı.
- Player > Auto Totem artık tek modüldür: Normal / Legit seçimi aynı paneldedir.
  Normal otomatik envanter açmayı, Legit yalnızca açık envanterde çalışmayı ve
  imleç hareketini korur. İki profilin ayarları ayrı saklanır; mod veya ayar
  değişiminde bekleyen işlem sıfırlanır. Eski `auto_totem` / `legit_auto_totem`
  config kayıtları tek modüle taşınır. Yalnızca Legit açıksa Legit seçilir;
  ikisi de açıksa Normal önceliklidir. Seçilen eski modülün tuşu korunur;
  yeni birleşik config kaydı varsa önceliklidir.
- Client > Notifications, modül açma/kapatma ve işlem sonuçlarını menü kapalıyken
  de gösterir. Kartlar aktif temanın geçişli renklerini, giriş/çıkış ve dizilme
  animasyonlarını kullanır; Azaltılmış Hareket tercihini izler ve ses üretmez.
  Konum, süre, kart sayısı ve bildirim türleri config'e kaydedilir. Hızlı tekrarlar
  aynı kartı günceller; başlangıçta bildirim yağmuru ve Panic animasyonu oluşturulmaz.
- Movement kategorisindeki No Jump Delay, tekrar zıplama beklemesini kaldırır;
  otomatik zıplama girdisi üretmez. Menü açıkken veya oyun odakta değilken çalışmaz.
- Movement > Fast Stop, yerde hareket girdisi kesildiğinde yatay kaymayı tek seferde
  durdurur; düşey hızı değiştirmez ve tuş basmaz. Havadayken, zıplarken, su/lavda,
  tırmanırken, uçarken, binekte ve `hurtTime` etkinken çalışmaz. Sürekli hız sıfırlama
  yapmaz; dururken gelen dış itmelere müdahale etmez. Güncel oyun tuş atamalarını
  izler; menü, odak kaybı, kapatma ve dünya/oyuncu değişiminde hareket geçmişi silinir.
- Inv Walk, envanter ve isteğe bağlı sandık/eşya ekranlarında gerçek hareket tuşlarını
  izler. Zıplama açık, eğilme varsayılan olarak kapalıdır. Sohbet, kitap, tabela,
  duraklatma ve odaktaki arama/yazı alanlarında çalışmaz. Tuş atamalarını izler;
  kapatma, odak kaybı ve dünya değişiminde yalnızca kendi bastığı tuşları temizler.
- Player > Pot Refill, yalnızca boş hotbar slotlarına Can I/II iksirlerini taşır.
  Sabit veya aralıklı gecikme ile soldan sağa, sağdan sola ya da rastgele slot sırası
  seçilebilir; eski `delay_ms` config alanı korunur. Creative envanterinde çalışmaz.
  Her taşımanın ardından eşya türü/bileşenleri ve adet yeniden okunur; bu kontrol
  sunucu onayı değildir. Sonuç 1,5 saniyede gözlenmezse tekrar tıklamak yerine durur.
  Envanter yeniden açıldığında sıfırlanır; dünya/oyuncu/ekran değişimi, odak kaybı,
  modülün kapanması veya elle envanter işlemi bekleyen işi iptal eder.
- Item ESP, yere düşmüş yüklenmiş eşyaları kutu, isim, adet ve mesafe etiketiyle gösterir.
  Her karede güncel dünyayı okur; sunucudan çıkıldığında eski eşya görüntüsü tutulmaz.
- Visuals > China Hat ve Wings, kendi karakterine yerel kozmetik şapka ve
  kanatlar ekler. China Hat tasarımları Klasik, Geniş Kenar ve Katmanlı;
  Wings tasarımları Ejderha, Melek ve Kelebek olarak seçilebilir. Eski configler
  tasarım alanı olmadan ilk yüklendiğinde Klasik/Ejderha kullanır.
  Üçüncü şahısta görünür; diğer oyunculara gönderilmez. Şapka başı,
  kanatlar gövdeyi takip eder. Boyut, opaklık, tema/özel renk ve glow ayarları vardır;
  Wings çırpınması ve hızı ayarlanabilir, azaltılmış hareket tercihine uyar.
  Kamera yakın düzlemi ve görünüm sınırları kırpılır; karakter için gövde kutuları,
  yakındaki tam opak bloklar için kamera gölgeleri kullanılır. Kısmi/şeffaf bloklar
  ve hareketli uzuvların tam şekli bu çizim maskesine dahil değildir.
  Uçuş, yüzme, yatma ve binme pozlarında çizilmez. Oyun içi doğrulama yapılmadı.
- Visuals > Cape, üçüncü şahısta kendi karakterine hareketli pelerin ekler.
  Kumaş omuzdan sabitlenir; yürüyüş, zıplama ve dönüşlerde yumuşakça savrulur,
  dururken hafif dalgalanır. Düz, Şeritli ve Amblemli tasarımları; uzunluk,
  genişlik, hareket miktarı, opaklık, tema/özel renk ve glow ayarları vardır.
  Animasyon kapatılabilir ve azaltılmış hareket tercihine uyar. Hareket yalnızca
  çizimden hesaplanır; ilk şahıs, desteklenmeyen pozlar, teleport ve kapanışta
  animasyon sıfırlanır. China Hat/Wings ile aynı yerel çizim ve blok maskesi
  sınırlamaları geçerlidir; diğer oyunculara gönderilmez. Oyun içinde denenmedi.
- Client > NameProtect, kendi hesap adını yerel ekranda takma adla gösterir.
  Varsayılan `Protected` değiştirilebilir; takma ad 1–16 harf, rakam veya alt çizgi
  içerir ve config'e kaydedilir. Tam isimleri büyük/küçük harf farkı olmadan eşler;
  başka bir oyuncunun daha uzun adının içindeki benzer parçayı değiştirmez.
  Vanilla font çizimi ve genişlik ölçümü aynı görünür adı kullanır; normal sohbet,
  TAB ve scoreboard yazıları bu yoldan geçer. Renkli OrderedText isimleri farklı
  stil parçalarına ayrılsa da tek ad olarak ele alınır. Target HUD, Nametag, ESP
  ve Radar'ın kendi etiketleri de aynı filtreye bağlıdır. Hesap profili, UUID,
  gönderilen mesajlar ve sunucudaki oyuncu adı değişmez; ham yazı verisi korunur.
  Uzun sohbet satırlarında önceden iki satıra bölünmüş isim parçaları ve vanilla
  fontunu kullanmayan üçüncü taraf çizimler bu filtrenin kapsamı dışındadır.
  `NameProtectTransformer.java`, yalnızca Fabric 1.21.1 TextRenderer sınıfının
  dört çizim ve üç genişlik metoduna JVMTI retransformation ile filtre ekler.
  Java filtresi render sırasında native callback kullanmaz. Kapanışta filtre
  pasifleştirilir, bu JVMTI ortamına ait dönüşüm geri alınır ve ortam bırakılır;
  Java yardımcı sınıfları pasif olarak oyun kapanana kadar kalabilir.
  Hazır sınıflar `assets/nameprotect` içine gömülüdür. Yeniden üretim:
  `scripts/Build-NameProtectBridge.ps1` (JDK 17+, ASM/ASM Tree 9.x; Fabric'in
  mevcut ASM kütüphanelerini kullanır). Bağlantı başarısızsa ayarda hata gösterilir.
  Oyun içinde henüz denenmedi.
- Visuals > Sky, oyunun kendi gökyüzü rengini Tema, Günbatımı, Mor, Buz veya Özel
  renk seçenekleriyle değiştirir. Yoğunluk ve ufuk sisini uyumlama ayarları vardır.
  Panorama modu, DLL'e gömülü Yıldızlı Gece, Samanyolu ve Bulutlu Şafak fotoğrafları
  yanında stilize Pastel Günbatımı, Nebula ve Kutup Işıkları görsellerini kullanır.
  Yeni ayarlarda varsayılan Pastel Günbatımı'dır; kayıtlı seçimler korunur.
  Parlaklık, yön ve yavaş dönüş ayarlanabilir. Ufuk, RenderSystem'in aynı karedeki
  gerçek sis rengine ve başlangıç/bitiş mesafesine uyarlanır; parlaklık ayarı bu sis
  rengini açıp koyulaştırmaz. Yoğun kısa mesafeli siste panorama ayrıntısı azalır.
  Ufkun altında doku satırı uzatmak yerine sis rengi kullanılır. Yatay birleşimde
  açık ve döngüye göre düzeltilmiş doku türevleri yanlış mip seviyesi/çizgi oluşmasını
  önler; stilize görsellerin kenar ve tepe renkleri ayrıca yumuşatılır.
  Kaynak görsellerin yer/yansıma bölümleri gökyüzüne taşınmaz. Üretilen PNG'ler
  ve üretim istemleri `assets/skies` altındadır; görseller ayrıca indirilmez.
  Fabric API BEFORE_ENTITIES olayı ile gerçek dünya framebuffer'ına uzak derinlikte
  çizilir; opak arazi derinliğine uyar, su/varlık/partikül/el/HUD çizimlerinden önce
  çalışır. Nether/End gibi gökyüzü ışığı olmayan boyutlarda ve kamera su/lav/kar
  içindeyken panorama çizilmez ve modülün biome gökyüzü/sis renkleri geri alınır.
  Aktif Iris shader paketi varsa panorama durur;
  hata durumu ayarlarda tek satır gösterilir. Renk modu ayrıca kullanılabilir.
  `src/bridge/SkyBridge.java`, standart Java yansımasıyla Fabric olayına bağlanır;
  hazır bytecode `assets/skybridge` içinde taşınır. Java kaynak değişince
  `scripts/Build-SkyBridge.ps1` ile yeniden hazırlanır (JDK 17+ gerekir).
  Oyun klasörüne mod kurmaz. DLL kapanırken listener önce pasifleştirilir,
  native bağlantı ve GPU kaynakları bırakılır; Fabric'in kayıt silme API'si
  olmadığından küçük, pasif listener oyun kapanana kadar kalır.
  Saat, hava durumu ve ışık seviyesi vanilla kalır; gece/yağmur rengi etkileyebilir.
  BiomeEffects renkleri client/render thread'de saklanıp uygulanır; kapanışta,
  dünyadan çıkarken ve render kancası kaldırılırken sahip olunan değerler geri alınır.
  Başka bir modun sonradan değiştirdiği renklerin üzerine eski değer yazılmaz.
  Şaderlerin kendi gökyüzü renkleri bu vanilla renklerini geçersiz kılabilir.
  Bağlar [Yarn 1.21.1 BiomeEffects](https://maven.fabricmc.net/docs/yarn-1.21.1+build.3/net/minecraft/world/biome/BiomeEffects.html)
  ve DynamicRegistryManager dokümanından doğrulandı; oyun içinde denenmedi.
- Visuals > Trails, Parçacık / Ayak İzi / İkisi Birlikte seçenekleriyle kendi
  karakterinin hareket ettiği konumlarda yerel iz bırakır; birinci/üçüncü şahısta
  görülebilir. Ayak izleri yerde, su/lav dışında ve gerçek blok yüzeyinin üzerinde
  oluşur; slab/merdiven için vanilla collider raycast kullanılır. İz süresi, sıklık,
  boyut, opaklık, tema/özel renk ve glow ayarlanabilir. En fazla 96 parçacık ve 48
  ayak izi tutulur; emission mesafeye bağlıdır, uzun karelerde toplu birikim yapılmaz.
  Dünya/oyuncu değişimi, teleport, modül kapanışı ve geçersiz görüntüde izler silinir.
  Duvar görünürlüğü kare başına en fazla 12 ray ile dönüşümlü güncellenir; yaklaşık
  200 ms önbellek ve parçacık merkezi kullanılır, çok düşük FPS'de güncelleme uzayabilir.
  Azaltılmış hareket tercihinde parçacıklar daha seyrek ve sabit oluşur. Oyun içinde denenmedi.
- Visuals > Target HUD, nişangâhtaki oyuncunun gerçek skin kafasını, canını, mesafesini,
  dört zırh parçasını ve elindeki eşyayı 232×94 yatay kartta gösterir. Ekipman tek
  sırada oyunun yüklü eşya modelleri ve atlas dokularıyla çizilir; renk katmanları ve
  GUI dönüşümleri korunur. Dayanıklılık ince çubukla gösterilir; büyülü eşyanın yuvası
  mor çerçevelenir. Özel Java çizicisi gerektiren built-in modellerde görsel yerine
  soru işareti gösterilir. Hedef nişangâhtan
  çıktıktan sonra varsayılan iki saniye boyunca canlı güncellenir; süre ayarlanabilir.
  Dünya değişimi, ölüm veya yüklenmiş oyuncunun kaybında kart temizlenir.
  HUD Editor'da kendi karakterinle önizleme, taşıma, boyut değiştirme, hizalama,
  geri alma ve otomatik kayıt desteklenir. Oyun içi doğrulama henüz yapılmadı.
- ESP/Chams için ayrı bir çizim mesafesi seçilmez. Storage ESP yüklenmiş chunk'lardaki
  depolama nesnelerini, Block ESP ise yüklenmiş chunk bölümlerini parça parça tarar.
  Chams ve Nametag de mesafe filtresi uygulamaz. Oyunun/sunucunun göndermediği
  nesneler gösterilemez; Block ESP'deki mevcut 10.000 işaret kaynak sınırı korunur.

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
server/                   Lisans Worker'ı, oturum politikası ve veritabanı kaynakları
src/
  main.cpp                Yalnızca DLL girişi ve worker başlatma
  app/                    Uygulama başlangıcı, kapanışı ve modül döngüsü
  auth/                   Lisans girişi, oturum, heartbeat ve config servis bağlantısı
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
    settings_manifest.json  Modül stable ID'leri için model ve panel sahipliği
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

Offline testleri tek komutla derleyip çalıştırmak için:

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
çalışır. PotRefill testi gerçek handler slot kimlikleriyle sıralamayı, dolu hotbar
korumasını, tek bekleyen işlem kuralını, zaman aşımını ve iptal modelini sınar.
FastStop testi tuş bırakma geçişini, zıt yön tuşlarını, düşey hızın korunmasını,
ortam/hasar engellerini, geçersiz sayıları ve hareket geçmişinin sıfırlanmasını sınar.
Crystal Aura testleri hedef kilidi/değişimini, havalanma sonrası aşamaları, gecikmeleri,
zaman aşımını, oturum iptalini ve clicker/Triggerbot saldırı sahipliğini doğrular.
Ek utility testi on bir modülün eski config alanlarını, varsayılanlarını,
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
tuşlarını ve worker tick akışını yönetir. Auth etkin build'lerde oyun içi giriş
paneli gösterilir; tek arka plan işi doğrulamayı yapar ve modül akışı oturumu bekler.

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

Katalogdaki stable ID'lerin tamamı `settings_manifest.json` ile izlenir.
Modüle özel ayarları bulunan kayıtlar ortak şemaya bağlandı. Friends Manager,
Click GUI, Panic, Full Bright, No Jump Delay ve Fast Stop için modüle özel config alanı yoktur; arkadaş
listesi ve genel UI tercihleri kendi mevcut sahiplerinde kalır.

Combat/utility/visual modülleri `settings_store.hpp` üzerinden kilitli yayın
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
Auth kurulumu ve etkin derleme: [server/SECURITY_SETUP.md](server/SECURITY_SETUP.md).
