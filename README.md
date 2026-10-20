# Onestep

Windows x64 üzerinde Minecraft Fabric için geliştirilen native C++20 istemci.
OpenGL ve ImGui tabanlı arayüz, HUD düzenleyicisi, modül sistemi ve public config
istemcisi içerir.

## Güncel durum

- Auth güvenlik sertleştirmesi kaynakta uygulandı: varsayılan zorunlu auth,
  admin API/release yollarında Access JWT + bearer, tek gösterimlik rastgele
  lisans, zorunlu cihaz imzası, D1 nonce tekrar koruması ve lisanslı config.
  Parolalı harici kasa akışı hazır; eski otomatik DPAPI/kopyalama kaldırıldı.
  **Canlı deploy, kasa taşıma, veri temizliği ve secret rotasyonu yapılmadı.**
  Access hesapta henüz etkin değil. Geçiş/geriye uyumluluk ve kalan operatör
  adımları: [Auth hardening](server/AUTH_HARDENING.md). Mevcut DLL korunur.
- ClickGUI alt kısayol yazısı kaldırıldı; sabit/değiştirilebilir keybind'ler taşmayan
  ortak kutu kullanır. Uzun seçenekler dar alanda tam genişlikte ayrı satıra geçer;
  gereksiz değer balonu kaldırıldı. Seçili modülün yuvarlak çerçevesinde tema rengi
  akar (Reduce Motion açıksa sabit). Keybinds tuş balonları dar/satır kaydırmalı;
  sistem tuşlarının atama koruması korunur. Block ESP'de satır listesi yerine gerçek
  oyun item modelleriyle ikon ızgarası, seçili filtresi ve açılır arama vardır.
  Yalnızca görünen ikonlar kare başına iki okuma bütçesiyle hazırlanır; item'i
  olmayan bloklar küp simgesiyle seçilebilir. 1.21.11 x64 Release test DLL'si:
  `build/ui-cosmetics-update-20261020/1.21.11/Onestep.dll` (0 uyarı, 0 hata).
  Oyun içi görünüm/FPS kontrolü henüz yapılmadı; önceki DLL korundu.
- Wings/Cape görsel-fizik güncellemesi: Dragon, Angel ve Butterfly için üç özgün
  saydam PNG kaplama, Cape'e Phoenix ve Astral Dragon desenleri eklendi. Wings'te
  Detailed Art varsayılan açık; kapatıldığında eski sade çizime dönülür. Görselin
  kendi renkleri korunur, Tint Artwork istenirse tema/özel rengi uygular. Eski
  Cape stilleri 0-7 ve kayıtlı ayarlar korunur. Pelerinin 9x13 parçacıklı kumaşı,
  kanatların sönümlü omuz/uç eklemleri hareket ve dönüşe tepki verir; Physics
  şiddeti ayarlanır. Sabit adım/bütçe, gövde çarpışması ve oturum/teleport sıfırlama
  vardır. Oyuncu hareketi, girişler ve paketler değişmez. Reduce Motion/Animation
  kapalıyken poz sabittir. Görseller gömülüdür, ağdan indirilmez. Kaynak/promptlar:
  [Cosmetics artwork](assets/cosmetics/README.md). Yukarıdaki 1.21.11 test DLL'sine
  dahildir; oyun içi görsel/FPS kontrolü henüz yapılmadı.
- Hit Particles saldırı sayacı sıfırlamasını hedef yenilemeden önce okur; kısa
  nişangâh çıkışlarında hedefi, gecikmiş hasarda en fazla 500 ms vuruş takibini korur.
  Hasar/sayaç gözlem sırası farkına 75 ms tolerans vardır; hedef değişince eski hasar
  taşınmaz. Efektler combat arkadaş/AntiBot filtresinden bağımsızdır; saldırı göndermez.
  View Model'e Vanilla, Compact, PvP, Low ve Cinematic hazır profilleri eklendi.
  Elle değişiklikler Custom görünür; config aynı transform alanlarını saklar,
  profil seçimi Both Hands tercihini değiştirmez. Cape'e Split, Inferno,
  Constellation, Circuit ve Wings desenleri eklendi; eski üç stilin config değerleri
  korundu. Desenler aynı deforme kumaşa çizilir, ayrı görsel indirmez.
  Bu değişiklikler için DLL ve oyun içi görsel doğrulama henüz yapılmadı.
- Visuals'a Hit Particles (yıldız/kıvılcım/halka), Jump Circles (zıplama/iniş),
  gerçek blok şekline uyan Block Highlight ve View Model eklendi. Damage Numbers
  eklenmedi. Tema, config ve tuş ataması ortak sistemdedir; yeni modüller varsayılan kapalıdır.
  Parçacıklar 96, halkalar 8 ve seçili blok şekli 24 kutuyla sınırlıdır; görünürlük
  kontrolleri bütçelidir. Eski dünya/oyuncu, menü, bağlantı kaybı ve disable sonrası
  efektler temizlenir. Hit Particles yalnızca yerel saldırı zamanına yakın gözlenen
  hedef hasarında çıkar; bu, sunucuda vuruş kabulünün ayrı bir garantisi değildir.
  View Model yalnızca vanilla el çiziminin kendi push/pop aralığında eşyanın konum,
  boyut ve açısını değiştirir; menzil, kamera, envanter veya saldırı mekaniğini değiştirmez.
  El çizimi sözleşmesi eşleşmezse modül güvenli biçimde durur ve ayar panelinde hata gösterir.
  DLL kapanışında dönüşüm geri alınır; Java bağlantısı native callback tutmaz.
  Derleme öncesinde `scripts/Build-ViewModelBridge.ps1` ile gömülü iki Java sınıfını
  yenileyin. Odaklı kontrol: `scripts/Test-ViewModel.py --client-jar <yerel-jar>
  --target 12111 --asm-classpath <asm-ve-asm-tree>`; Minecraft'ı çalıştırmaz.
  Oyun içi görsel doğrulama ve yeni DLL derlemesi henüz yapılmadı.
- Arayüz stabilizasyonu: modül ayarları açılırken ana panelin konumu/boyutu
  değişmez. Yer varsa ayarlar sağda ayrı açılır; dar ekranda sağ tarafa bindirilir.
  Features/Other, arama ve HUD sayfaları aynı ana panel konumunu kullanır;
  kategori değişince başka bir konuma sıçramaz. Sürükleme ekranda sınırlanır,
  geçersiz fare/odak kaybında durur; küçültülmüş pencere konumu ezmez.
  Modül adları 17 px, açıklamalar 14 px oldu; başlık/açıklama sarılır ve kart
  yüksekliği metne göre hesaplanır. Küçük ekranda sol oyuncu önizlemesi ana
  paneli itmek yerine yalnızca yeterli alan varsa gösterilir.
  Spotify HUD en son kullanıcı ZIP'inden gelen `assets/icons/spotify-2015-seeklogo.png`
  dosyasını değiştirmeden, orijinal saydamlığı ve renkleriyle kullanır. Ayrıca
  siyah zemin çizilmez. Eski SVG, mesh derleyicisi ve eski PNG korunur fakat HUD'da
  kullanılmaz. Yeni PNG, mevcut mipmap/linear filtreli ortak texture yolundadır.
  Panel açıldığı karede vanilla tuş durumları/kuyruktaki basışlar temizlenir,
  hareket girişi nötrlenir ve fare kilidi bırakılır; imleç hemen görünür.
  Panel açıkken oyun girdileri engellenir. Kapanışta fare yalnızca aynı dünya/
  oyuncu hâlâ etkin, oyun odakta ve başka Minecraft ekranı açık değilse geri
  kilitlenir. Fiziksel tuşlara sentetik bırakma gönderilmez; hız/yerçekimi değişmez.
  Config seçimleri ve ortak seçenek listeleri 12 px yumuşak köşeli popup,
  içten yuvarlatılmış seçim satırları ve onay işareti kullanır. Çalışma alanında
  küçük açıklamalar 14 px, kontroller 15 px; config onayı başlığı 19 px,
  butonları 40 px yüksekliğindedir. HUD boyutları/ölçekleri değişmez.
  Keybinds filtresi ana menüyle aynı sunum kategorilerini kullanır: Movement,
  Player ve World, Misc'te birleşir; Friends yönetim sayfası filtreye girmez.
  Klavyede harf/rakamlar 15 px, diğer tuşlar 12.5 px; dar ekranlarda harfleri
  küçültmek yerine yalnızca klavye yatay kayar. Tam tuş adları tooltip'te kalır.
  Lisans girişinde uzun key yapıştırırken oluşan yatay kaydırma hatası giderildi:
  yer tutucu ayrı çizilir; gizli metnin imleç/kaydırma ölçüsü baştan tutarlıdır.
  Giriş kartı okunabilir sabit yazı boyutları, anahtar simgesi, hafif tema vurgusu,
  belirgin key alanı ve öncelikli giriş butonuyla yenilendi. Kapatma ikincil eylemdir;
  doğrulama durumu ve hatalar okunaklıdır, kısa ekranda kart içeriği kaydırılabilir.
  Arayüzde ekran dışında kalan modül kartları çizilmez; filtre sonuçları ve
  kart metin ölçüleri liste/sorgu/genişlik/yazı tipi değişene kadar önbellekte tutulur.
  Ayarlar sürükleme sırasında otomatik kaydedilmez; son değişiklikten sonra beklenir
  ve disk hatalarında her karede tekrar denenmez. Taşan seçim listeleri ve lisans
  kartı, genel gizli scrollbar stilinden bağımsız geçerli kaydırma boyutu kullanır.
  HUD yönetiminde adlar 17 px, ölçek/eylem yazıları 14–15 px ve düğmeler daha
  büyük; dar alanda kontroller küçülmek yerine alt satıra geçer. Modül kartlarının
  üstüne gelince açıklayıcı tooltip açılmaz. Alt+Tab/Win tuşları pencereye iletilir;
  odak kaybında UI fare yakalamasını bırakır, arka plandaki fare/kısayol basışlarını
  işlemez. Dönüşte önceden basılı tuşların yeni tıklama/menü açma sayılması engellenir.
- C++/Java hibrit geçişinde **1.21.11** hedefindeki No Jump Delay, Fast Stop,
  Fast Place, Sprint, Full Bright, Auto Tool, Fast EXP, Left/Right Clicker ve
  Inventory Clicker, Chest Stealer, Auto Armor, Elytra Swapper, Pot Refill ve
  Mouse Tweaks, Bridge Assist, Pearl Macro, Inv Walk ve Glow ESP'nin oyun erişimi Java
  köprülerine alındı.
  Mevcut C++ karar motorları, arayüz, config, tuşlar ve auth korunur. Sprint ve
  Full Bright devre dışı bırakma/renderer kapanışında sahip oldukları durumu
  oyun thread'inde bırakır; manuel sprint/parlaklık değişiklikleri korunur.
  Auto Tool'un uygunluk/hız hesabı ve Fast EXP'nin CPS zamanlaması C++'ta korunur.
  Fast EXP'nin yeni Java yolu tur başına tek vanilla use çağrısı yapar; fazladan
  SendInput üretmez. İlk beş modülün çalışması kullanıcı tarafından oyunda
  doğrulandı; kullanıcı Auto Tool/Fast EXP'nin de çalıştığını doğruladı.
  İlk üç grup sonrasındaki yeni yollar oyun içi test bekliyor.
  Bridge Assist'in kenar örnekleri ve bırakma kuralları C++'ta korunur;
  Pearl Macro gecikmeleri korunarak tur başına tek vanilla use çağrısına geçti.
  Sentetik sağ tuş basma/bırakma üretmez, manuel slot değişimini ezmez.
  Inv Walk'ın ekran/hareket politikası ve Glow ESP'nin ortak AntiBot filtresi
  C++'ta kalır. Yazı alanları hareketi durdurur; Glow ESP kapanışta yalnızca
  kendi değiştirdiği glow değerlerini geri alır, önceden var olan glow'u silmez.
  Envanter eşya seçimi/gecikmeleri C++'ta korunur; Java her adımda pencere,
  slot, eşya bileşenleri ve imleci tekrar doğrular. Clicker hesapları, Triggerbot
  saldırı önceliği ve ortak Game Input taşıması native kalır. Kalan modüller ve diğer
  sürümler henüz taşınmadı. Kapsam, derleme ve referans bildirimi:
  [Hibrit geçiş](docs/HYBRID_MIGRATION.md).
- Ortak kaynak koddan 1.21.1, 1.21.4, 1.21.8, 1.21.11, 26.1, 26.1.1 ve 26.1.2 için ayrı DLL hedefleri
  oluşturuldu. Yeni hedefler Windows derlemesi ve oyun içi doğrulama tamamlanana
  kadar geliştirme hedefidir. Adaptör yapısı, çıktı yolları ve kontrol listesi:
  [Minecraft hedefleri](docs/MINECRAFT_TARGETS.md).
  26.1 native adaptörü, NameProtect/Sky köprüleri ve tam SDK/JNI taraması eklendi;
  26.1.1/26.1.2 aynı adaptörü kullanır; her biri 740 SDK/JNI kontrolünden geçti.
  Bu üç hedef için geriye Windows derlemesi ve oyun içi doğrulama kaldı:
  [26.x geçiş durumu](docs/MINECRAFT_26_PORT.md).
- Auth kaynakları modüllere ayrıldı; koyu temalı yeni yönetim paneli, sayfalı lisans
  listesi, ayrıntı çekmecesi, işlem geçmişi ve sistem durumu hazırlandı. Bu kaynak
  değişikliği 19 Ekim 2026'da canlıya alındı; lisanslar ve sırlar korundu. Mimari, test ve güvenli yayın
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
- Oyun içi arayüz yalnızca İngilizcedir; Türkçe seçimi, etiketler ve çeviri katmanı
  kaldırıldı. Eski UI tercihlerindeki `language` alanı yok sayılır; diğer ayarlar
  korunur. Oyuncu adları ve kullanıcı metinleri Unicode olarak görüntülenebilir.
  Kaydırma çubukları gizlidir; listeler, temalar ve ayarlar fare tekerleğiyle kayar.
  ESC ana menüyü kapatır. Açık arama, açılır liste veya onay penceresi varsa ilk
  ESC onu kapatır; HUD düzenleyicide ESC menüye döner. Tuş ataması sırasında ESC
  atamayı temizler; menüyü kapatmaz. Menü kapanışındaki ESC tekrarı Minecraft'a
  iletilmez. Oyun içi Windows doğrulaması ayrıca yapılmalıdır.
- Arama alanı sol altta oyuncu bilgisinin üstündedir; doğrudan kutuya yazılır.
  Yarı saydam zemini vardır. Sonuçlar ayrı panel açmadan ana modül alanında tüm
  kategorilerden filtrelenir. Oklar/Enter ve otomatik tamamlama korunur; X veya
  ilk ESC aramayı temizler, eski sayfayı geri getirir. Sonraki ESC menüyü kapatır.
  Profilde yerel oyuncunun skin kafası (ikinci katman dahil) ve adı gösterilir.
  Skin yüklenene kadar varsayılan kafa kullanılır. Rastgele UID yerine doğrulanmış
  lisans bitiş tarihinden kalan süre (`30 days`, `2 hours`) veya `Lifetime` yazılır;
  bilinmeyen süre lifetime kabul edilmez. Bu etiket yalnızca görseldir, yetkilendirme
  ve oturum süresi kontrollerini değiştirmez.
- Features ve Other sayfaları aynı 960×620 menü boyutunu kullanır.
  Features iki sütunlu kompakt modül kartları kullanır. Sol menü ve modül
  alanı tek arka planda birleşir; panel yüksekliği sabittir ve liste çubuksuz kayar.
  Movement ve Player sekmeleri Misc altında birleşir (World modülleri dahil);
  bu yalnızca UI gruplamasıdır, modül kategorileri, kimlikler ve configler değişmez.
  Features görünümü, açık oyuncu önizlemesi ve sağ ayar paneli dahil ekrana ortalanır;
  elle sürükleme konumu merkezden göreli fark olarak korunur.
  Other sayfaları (Friends, Configs, Themes, HUD ve Keybinds) mevcut geniş görünümü korur.
  Modüller 62 piksel yüksekliğinde sade isim/açıklama kartlarıdır; kart üzerinde
  ayar dişlisi veya aç/kapat anahtarı yoktur. Etkin modül adı tema rengini kullanır.
  Ayarı olan kartın tamamına tıklamak ayarları ana pencerenin sağında bağımsız bir
  panelde açar; kapalı panel için listede boş alan ayrılmaz. Normal çözünürlükte
  kart genişliği sabittir; tüm grup birlikte ortalanır. Ekran daralırsa pencere
  genişliği uyarlanır ve gerektiğinde kartlar tek sütuna geçer.
  Tuş ataması ve mevcut modül ayarları korunur. Uzun ayarlar çubuksuz kayar.
  Enabled anahtarı ayar panelinde Keybind satırının üstündedir. Keybind dışında
  ayarı olmayan Full Bright, No Jump Delay ve Fast Stop gibi modüller panel
  açmadan doğrudan açılıp kapanır; tuşları Other > Keybinds'ten yönetilir.
  Arama/Enter da aynı davranışı kullanır. Panic anlık çıkış eylemini korur.
  Renderer tabanlı önizleme, etkileşim testleri ve güvenli temizlik notları:
  [GrimUI düzeni](docs/GRIM_UI_LAYOUT.md).
- Other > Keybinds, 104 tuşlu interaktif ANSI klavyesinden modül kısayollarını
  yönetir. Atanmış tuşlar işaretlidir; tuş seçilince aranabilir ve kategoriye göre
  filtrelenebilir modül listesi açılır. Aynı tuş birden fazla modüle atanabilir;
  mevcut tetikleme davranışı korunur. Atama/taşıma/silme gerçek modül keybind'ini
  değiştirir; ek ayarı olmayan modüller de desteklenir. Sabit Right Shift menü
  kısayolu olan Click GUI listelenmez. Esc, Right Shift, End ve Windows tuşları
  ayrılmıştır; eski atamalar silinebilir. Numpad Enter ve NumLock kapalı numpad
  tuşları ana klavyeden ayrılır. Kısayollar `%LOCALAPPDATA%\onestep\keybinds.json`
  dosyasına kaydedilir ve başlangıçta modülleri açmadan geri yüklenir.
  Mimari, özel tuş politikası, config uyumluluğu ve testler:
  [Keybinds](docs/KEYBINDS.md).
- Config ekranı tek paneldir: üstte isim alanı, altında sade kayıt satırları vardır.
  İsim yazılınca açıklama ve oluşturma düğmesi aşağı açılır; isim silinince kapanır.
  Hareketi azalt tercihi bu geçişi doğrudan uygular. İndirme/modül sayaçları yoktur.
  Satır ikonları yükleme, mevcut ayarlarla güncelleme ve silme içindir; güncelleme
  ve silme onay ister. Kayıt adına tıklamak isim/açıklama düzenlemesini açar.
  My Configs yalnızca kendi kayıtlarını, Public Configs yalnızca diğer kullanıcıların
  kayıtlarını gösterir. Favourites seçimi ve mevcut lisanslı public config servisi korunur.
- Friends ekranı da tek paneldir: Settings bölümündeki orta tık anahtarı mevcut
  Friends Manager modülünü açar/kapatır. Name modunda Enter veya artı ile isim
  eklenir; Server players modunda mevcut sunucu listesi aranıp arkadaş seçilebilir.
  Sade satırlardan arkadaş silinir; boş liste açıklaması, tema renkleri ve çubuksuz
  kaydırma kullanılır. Arkadaş listesi oturumluk kalır; mevcut combat korumaları
  değişmez. Yarıçapla ekleme veya global ESP override seçeneği eklenmedi.
- HUD sayfasında Watermark, ArrayList ve Target HUD ölçeği satırdaki Scale
  kontrollerinden ayarlanır; Spotify ölçeği sabittir. Satıra tıklamak öğeyi seçer,
  listenin altındaki tek Edit düğmesi ekran üzerindeki yerleşim düzenleyicisini açar.
  Başlık ve satır yazıları aynı sol hizadadır. Modül aç/kapat ve diğer ayarlar
  mevcut kategorilerinde kalır. Undo/Reset ve Grid/Snap düzenleyicinin araç
  çubuğundadır. Sürükleme, Alt ile
  serbest hareket, oklar/Shift, Ctrl+Z, ESC dönüşü ve yerel yerleşim kaydı korunur.
- Temalar sayfasında menü arka planı seçilebilir: Klasik (eski görünüm), Hisar,
  Pamuk Alacakaranlığı, Nebula ve Aurora. Hazır görseller DLL'e gömülüdür; indirme
  gerektirmez. **Görsel ekle** ile PNG/JPG seçilebilir (16 MB, kenar başına 8192
  piksel ve toplam 16 megapiksel sınırı). Görsel oranı korunarak ortadan kırpılır;
  yoğunluk ve karartma ayarlanır. Özel görselin yerel kopyası
  `%LOCALAPPDATA%\onestep\menu-background.image` içinde saklanır; kaynak dosya
  değiştirilmez. Seçim UI tercihlerine kaydedilir, paylaşılan oyun configlerine
  eklenmez. Eski tercihler klasik görünümü korur; kayıp görselde klasik görünüm
  kullanılır. Windows dosya seçicisi ve oyun içi görünüm ayrıca denenmelidir.
  Yeni görünümde sekiz vurgu paleti üstte küçük renk kartlarıyla, altı arka plan
  altta görsel kartlarla seçilir. Add image ve boş Custom image kartı aynı dosya
  seçicisini açar. Yoğunluk ve karartma yüzde olarak, geniş panelde yan yana
  gösterilir; dar panelde alt alta dizilir. Standart menü boyutunda tek ekrana sığar.
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
- Hedef DLL ile tam eşleşen Minecraft Fabric sürümü; Sky için Fabric API

## Derleme

Komutları depo kökünde çalıştır:

```powershell
# Release x64 DLL
.\scripts\Build-Release.ps1

# Temiz derleme
.\scripts\Build-Release.ps1 -Clean

# İmzasız yerel derleme
.\scripts\Build-Release.ps1 -SkipSign

# Belirli sürüm veya dört hedefin tamamı
.\scripts\Build-Release.ps1 -MinecraftVersion 1.21.8 -SkipSign
.\scripts\Build-Release.ps1 -AllMinecraftVersions -SkipSign

```

Proje yalnızca DLL olarak derlenir:

| Çıktı | Yol |
| --- | --- |
| İstemci DLL | `build\Onestep.dll` |
| Ek sürüm DLL'leri | `build\minecraft\<sürüm>\Onestep.dll` |

İmzalama scriptleri, mevcut kullanıcı sertifika deposundaki
`CN=Thedorius Lite Test` sertifikasını arar. Sertifika adı henüz değiştirilmedi.
GitHub Actions yapılandırması, offline testlerden sonra dört Windows x64 hedefini
ayrı işler olarak derler. 1.21.1 artifact'i `Onestep-Windows-x64`, diğerleri
`Onestep-<sürüm>-Windows-x64` adını kullanır. Bu çıktılar imzasızdır.

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
    common/               Ortak JNI domain başlıkları ve hedef manifesti
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
SDK testi sınıf adlarını, temel JNI imzalarını ve her domain başlığının
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
üye bağlarını, config anahtarlarını, sınırları ve İngilizce UI etiketlerini tanımlar.
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

Mapping tanımları `sdk/minecraft/common/` altında Java tipleri, geometri,
blok/world, inventory/network, entity tipleri, entity, client entity, input ve
client başlıklarına ayrılır. Domain bağımlılıkları açık include zincirleriyle
kurulur; JNI sınıf isimleri `manifest.json` ile denetlenir. `mappings.hpp` eski
kullanıcılar için tüm SDK'yı sunar; yeni kod ihtiyaç duyduğu domain'i kullanmalı.
Sürüme bağlı ad/tipler `platform/adapter/bindings_profile.hpp` içinde seçilir.
Modül hesapları ortaktır; başka bir sürüm için modül klasörleri kopyalanmaz.

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
