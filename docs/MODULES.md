# Modül ve config notları

Bu belge README'deki tekrarlanan özellik kayıtlarını tek yerde toplar.
Tam kayıt listesi ve stable ID'ler için kaynak otoritesi
`src/modules/module_catalog.cpp` ile `src/modules/settings_manifest.json`'dır.
Mevcut katalog 55 modül içerir; bu sayı oyun içi kabul testi sayısı değildir.
Arayüz gruplaması [UI rehberinde](GRIM_UI_LAYOUT.md), Java I/O kapsamı
[hibrit geçiş belgesinde](HYBRID_MIGRATION.md) açıklanır.

## Ayarlar ve config sözleşmesi

- `core/setting_schema.hpp` bool, tamsayı, sayısal değer, aralık, renk, metin
  listesi ve seyrek enum seçeneklerini tanımlar; `ui/settings_renderer.hpp`
  aynı alanları mevcut kontrollerle çizer.
- Combat/utility/visual ayarları kilitli snapshot depolarını kullanır. Vape/Chams
  mevcut kilitlerini, Jump Reset revision/iptal sahipliğini, Aim/Trigger/Anchor
  kendi runtime sınırlarını korur. HUD/ArrayList tercihleri render katmanındadır.
- Friends Manager, Click GUI, Self Destruct, Full Bright, No Jump Delay ve Fast Stop'un
  modüle özel config alanı yoktur. Arkadaş listesi ve UI tercihleri ayrı sahiptedir.
- Yeni public config kökü `modules`; her kayıt `id`, `enabled`, `keybind`,
  `settings` taşır. Eski sürüm etiketi okunur, yeni çıktıya yazılmaz.
- Sınırlar: 96 KB, en fazla 128 modül, JSON derinliği 32. Tam belge önce
  doğrulanır; yanlış tür, yinelenen alan/ID, bozuk JSON, geçersiz enum/tuş
  reddedilir. Kaldırılmış modüller atlanır, gizli alanlar korunur.
- Beklenmeyen uygulama hatasında eski ayarlar geri yüklenmeye çalışılır;
  enabled/keybind en son yayınlanır. Bu, worker'lara karşı modüller arası
  tek bir atomik transaction değildir.
- UI/import sınırları her zaman aynı olmak zorunda değildir. Eski Auto Tool
  alanları, Radar'ın geniş ekran konumları ve özel Block ESP/Spotify/Jump Reset
  panelleri korunur. Numpad Enter'ın genişletilmiş kodu için
  [keybind uyumluluğunu](GRIM_UI_LAYOUT.md#keybind-uyumluluğu-ve-kayıt) incele.

## Birleştirilmiş modüller

### Anchor Macro

Anchor Macro'nun canlı denetleyicisi Java WorldBridge özelliği 6'dır. Varsayılan
**Normal**, mevcut OneStep akışıdır; **Legit**, Argon'un tick/eşya kullanımı
olaylarına dayalı akışını uygular. `execution_mode` bu iki seçimi kaydeder.
Menüde Mode ve iki modun kullandığı aynı 13 kontrol bulunur: While Use, Stop on Kill, Click Simulation,
Place Chance, Switch Delay/Chance, Glowstone Delay/Chance, Explode Delay/Chance,
Explode Slot, Only Own ve Only Charge. Gecikmeler 0–20 **oyun tick'i**, şanslar
0–100%, patlatma slotu 1–9'dur. Fiziksel sağ tuş GLFW'den okunur.

Normal mod yerleştirme → yerleşimi gözleme → glowstone → dolumu gözleme → patlatma
→ kaybolmayı gözleme → eski slotu geri alma akışını korur. Mevcut bir anchor'dan
da başlanabilir. Sağ tuş basışı başına tek döngü çalışır; yeni deneme için tuşun
bırakılması gerekir. Her aşamada şans bir kez örneklenir; başarısızlık aynı
basışı tekrar tekrar denemez. Bir tick'te en fazla bir slot/oyun işlemi uygulanır.
Dolum bilinmiyorsa patlatılmaz; glowstone patlatma slotu kabul edilmez.

Only Own yalnızca aynı dünya/oyuncu oturumunda modülün yerleştirdiği ve dünyada
gözlediği anchor'ları kabul eder. Only Charge dolumdan sonra durur. While Use
kapalıysa elde/ikinci elde yemek veya kalkan varken ve aktif eşya kullanımında
bekler. Stop on Kill altı blok içindeki ölü oyuncu için durur. Click Simulation
başarılı doğrudan etkileşimden sonra Java Mouse callback'ine sağ basma/bırakma
çifti yollar; ek vanilla kullanım kuyruğu yaratmaz, Windows input üretmez.

Manuel slot, ayar, dünya/oyuncu/pencere veya odak değişimi döngüyü iptal eder.
Slot yalnızca hâlâ modülün seçtiği slot ise aynı envanterde geri alınır. Gerçek
vanilla erişim ve blok görüşü tekrar okunur. Yerleşim/dolum/kaybolma 650 ms içinde
gözlenmezse döngü biter; yerel gözlem sunucu onayı garantisi değildir.

Legit otomatik anchor koymaz: normal oyun kullanımıyla koyduğun veya şu an
nişangâhında bulunan anchor'ı işler. Gerçek client tick HEAD olayında güncel
hedefi okur; sağ tık basılıyken sürekli çalışır. Şans başarısızlığı sonraki
tick'te yeniden denenir. Switch/glowstone/explode sayaçları hedef/sağ tuş
bırakma/ayar değişiminde sıfırlanmaz; modül yeniden etkinleşince sıfırlanır.
Place Chance boş anchor'da switch/doldurma yolunu kapılar. Only Charge patlatma
çağrısını engeller fakat Explode Slot'a geçişi engellemez. Eski slota dönülmez;
sıfır gecikmede dolum ve patlatma aynı tick'te gerçekleşebilir.

Legit Only Own, gerçek normal eşya kullanımı HEAD olayında tahmin edilen yerleşim
konumunu kaydeder; kaynak gibi yerleşim onayı beklemez ve konumları disable/enable
boyunca korur. Kaynak davranışına uygun olarak koordinat listesi dünya bazında
ayrılmaz. Java `ActionResult` istemci el sallamasını istediğinde swing yapılır.
Click Simulation nominal 35 ms sağ mouse basışı üretir; client use HEAD hook'u
bir sonraki fake kullanımı iptal eder. Basma/bırakma oyun thread'indedir; bırakma
süresi dolduğunda ilk uygun tick/render çağrısında uygulanır. Argon'un worker
havuzu kullanılmaz. Odak/izin/kapanış kontrolleri ve input temizliği devam eder.
Legit için tick/use hook kurulumu başarısızsa Normal kullanılmaya devam edebilir.

Eski Legit/Blatant profil alanları config'te gizli saklanır. Eski alias birleştirme
ve keybind seçimi sürer; açıkça gelen eski switch/charge/detonate milisaniyeleri
üst tick'e yuvarlanır. `execution_mode` içermeyen eski public config'ler Normal
açılır; eski gizli `mode: 1` yeni Legit seçimi değildir. Yeni alanlar varsa
önceliklidir. Safe Anchor ayrı modül
olarak mevcut native motorunu kullanır. Koruma bloğu anchor ile oyuncu arasına
yerleştirilip dünyada görülmeden doldurma/patlatma yapılmaz. Obsidyen, yoksa
glowstone kullanılır; hem koruma hem dolum için glowstone kullanılacaksa iki
adet gerekir. Yerde sabit durup erişilebilir blok/anchor'a bakarak etkinleştirilir;
bir zincir sonunda kapanır. [Safe Anchor akışı](SAFE_ANCHOR_FIX.md).
[Java kapsamı](HYBRID_MIGRATION.md#anchor-macro-java-denetleyicisi).

### Auto Totem

Auto Totem’in Java akışı Argon’daki Auto Inventory Totem davranışını izler:
Blatant ilk uygun ana envanter totemini, Random rastgele uygun totemi seçer.
Önce offhand doldurulur; Hotbar açıksa eldeki slotun boş olması veya Force Totem
kullanılması durumunda seçili slot da doldurulur. Auto Switch seçilen totem slotuna
geçer. Auto Open modülün açtığı envanteri, Stay Open For süresi bitince kapatır.
Tüm kararlar ve SWAP işlemleri oyun thread’inde Java’dan yapılır.

## Hedefleme ve oyun I/O

- **Anti Bot:** yeni kullanımda açık, kaydedilmiş false korunur. İlk 40 tick
  bekler; ardından 40 tick kayıtsız kalan oyuncuyu ortak hedef/görsel filtresinden
  çıkarır. Görünmezlik, isim veya hareketsizlik tek başına kanıt değildir.
  Tab listesi güvenilmezse elemez; dünya/sunucu değişiminde geçmiş temizlenir.
- **Triggerbot:** hedef/silah ve kritik kontrolleri, rastgele cooldown/reaction,
  güncel ray sorgusu ve vanilla saldırı Java WorldBridge'dedir. Arkadaş/bot
  filtreleri ve Mace Swap hazırlığı ortak native servislere bağlanır. Shield
  Breaker hazırlığı/geri dönüşü de Java’dadır. On Click Only, Java tarafında GLFW ile sol tuşu okur. Varsayılan
  açık Swing Hand seçeneği el sallamayı yönetir. Click Simulation (varsayılan
  kapalı), saldırı sonrası oyunun mouse callback'ine bir basma/bırakma çifti
  yollar; eklenen vanilla press kuyruğunu temizler. Eski config anahtarları korunur. Menü, odak, oturum,
  manuel slot veya ayar değişimi döngüyü iptal eder; Windows click üretilmez.
- **Shield Breaker:** WorldBridge Java özelliği 9. Verilen Argon ShieldDisabler
  davranışı referans alınarak yenilendi. Switch/Hit Delay tick, Switch Back,
  Hold Axe ve Click Simulation seçenekleri var. Triggerbot açıkken yalnız balta
  hazırlığı yapar; saldırı, cooldown ve son ray doğrulaması Triggerbot’ta kalır.
  Stun yalnız bağımsız modda isteğe bağlı ikinci vuruştur. [Ayrıntılar](SHIELD_BREAKER_JAVA.md).
- **Crystal Aura:** WorldBridge Java özelliği 8; hasara göre taban/kristal seçer,
  yerleştirme ve kırmayı ayrı zamanlar. Kılıç/havalanma şartı yoktur; mevcut
  tabanlar, hotbar/offhand kristalleri ve isteğe bağlı obsidyen desteği kullanılır.
  Hasar tahmini blok görüşü, zırh/toughness, direnç ve koruma büyülerini içerir.
  Self / Friend Protection açıkken hasar ve anti-suicide sınırları uygulanır.
  Gerçek menzil, entity boşluğu, görünür dönüş, slot sahipliği ve sunucu teyidi
  Java'da kontrol edilir. İşlem neden bekliyorsa ayar panelinde görünür.
  [Kaynak karşılaştırması ve güncel tasarım](CRYSTAL_AURA_REDESIGN.md).
- **Vape Aim Assist:** Simple/Adaptive motorlarıyla Combat kategorisindedir;
  mevcut `vape_aim_assist` config kimliği korunur.
- **Hit Crystal:** fiziksel sağ tuş basılıyken bakılan obsidyen/bedrock üzerinde
  kristal yerleştirip kırar; mevcut kristal hedefinden de başlayabilir. Kararlar,
  slot sahipliği, gecikmeler ve etkileşimler tamamen WorldBridge Java denetleyicisindedir.
  Auto Switch, Restore Slot ve milisaniye gecikmeleri korunur. Boşluk, gerçek
  erişim ve görüş denetlenir; spawn/kaybolma gözlenir, tekrarlar sınırlıdır.
  Menü, odak/izin kaybı ve manuel slot değişimi döngüyü iptal eder.
  [Java geçiş kapsamı ve kaynak kontrolleri](HYBRID_MIGRATION.md#hit-crystal-tam-java-denetleyicisi).
- **Jump Reset:** hasar/zemin kontrolleri, şans/gecikme, Living Attacks Only
  onayı ve jump binding sahipliği WorldBridge Java denetleyicisindedir.
  Tuşlar Java'da GLFW ile okunur; basma/bırakma vanilla KeyBinding yolundadır.
  Game Binding ve Game Space modları mevcut config/preset'leri korur;
  eski System Space kayıtları Windows input üretmeden Game Space olarak çalışır.
  [Java geçiş kapsamı](HYBRID_MIGRATION.md#jump-reset-java-denetleyicisi).
- **No Jump Delay:** tekrar zıplama cooldown'unu kaldırır; jump basışı üretmez.
- **Fast Stop:** yerde hareket girdisi kesilince yatay kaymayı bir kez durdurur;
  düşey hızı korur. Hava, sıvı, tırmanma, uçuş, araç ve hurt koşullarında durur;
  dış itmelere sürekli hız sıfırlamayla müdahale etmez.
- **Inv Walk:** izinli envanter/container ekranlarında fiziksel movement
  bindinglerini izler. Jump açık, sneak varsayılan kapalıdır. Sohbet, kitap,
  tabela ve odaklı arama alanları hareketi durdurur; yalnızca kendi basışını temizler.
- **Pot Refill:** boş hotbar slotlarına Healing I/II taşır; slot sırası ve gecikme
  ayarlanır, eski `delay_ms` korunur. Creative'de çalışmaz. Kaynak/hedef/adet/
  bileşenler her işlemde okunur; 1,5 saniye sonuç yoksa tekrar tıklamak yerine
  envanter oturumu yenilenene kadar durur. Manuel işlem ve bağlam değişimi iptal eder.

1.21.11'de bazı I/O yolları Java'ya taşınmıştır; hesaplama, giriş sahipliği ve
diğer sürümlerin native yolları için [grup sözleşmeleri](HYBRID_MIGRATION.md)
esas alınır. Kaynak notları sunucu kabulü veya tüm modüllerin oyun testi değildir.

## Görseller ve yerel sınırlar

### ESP, Chams ve hedef bilgisi

ESP/Chams/Storage ESP/Block ESP/Nametag için ayrı mesafe seçimi yoktur.
Yalnızca oyunun yüklediği/verdiği nesneler görülebilir. Block ESP'nin 10.000
işaret sınırı korunur; tarama bütçelidir. İkon seçicisi görünür blok item
modellerini kare başına iki okuma bütçesiyle hazırlar; item'i olmayan blok küple gösterilir.
Block ESP seçilen blok hücrelerini dünya koordinatlarına oturan 3D kutularla
gösterir: 12 kenar ve altı yarı saydam yüzey kullanılır; dolgu sıfırsa yalnız
kenarlar çizilir. Kutu ve yüzeyler uzaktan yakına sıralanır, kamera yakın düzlemi
ve ekran sınırlarında kesilir. Çizgi kalınlığı ve dolgu ayarları geçerlidir.
Görüş dışındaki kutular çizilmez; ek bir mesafe veya minimum piksel boyutu sınırı yoktur.
Item ESP yüklenmiş düşen eşyaların kutu/isim/adet/mesafesini gösterir.
Dünya/oyuncu kimliğine bağlı overlay snapshot'ları geçersiz oturumda temizlenir;
önceki worker sonuçları yeni sunucunun üstüne yazılamaz.
Radar işaretçileri ve kamera yönü kare hızından bağımsız yumuşatılır; açı
geçişleri ±180° sınırında kısa yolu izler. Yeni işaretçiler yumuşak belirir;
oturum değişiminde ve büyük konum sıçramalarında animasyon sıfırlanır.
Radar tema rengi yumuşak geçer; işaretçiler katmanlı halo ve küçük bir parlak
merkezle çizilir. Mesafe halkaları açıkken hafif tarama animasyonu görünür;
Reduce Motion taramayı kapatır ve işaretçi büyümesini anlık yapar.
Player ESP can çubuğu kademeli güncellenir, kaybedilen can kısa süre sıcak
renkli bir iz bırakır. Tema/özel renk değişiklikleri yumuşatılır. Oyuncu
listesinden çıkan kayıtların animasyonları tutulmaz. Reduce Motion bu
hareketleri ve can geçişlerini anlık yapar.
Player ESP kamera/oyuncu verisini yalnızca çizim sırasında okur; kullanılmayan
ikinci worker taraması yoktur. Güncel oturum ve AntiBot kontrolleri korunur.

Target HUD gerçek skin, can, mesafe, zırh ve eli 232×94 kartta gösterir.
Native item projeksiyonu GUI dönüşümlerini, atlası ve renk katmanlarını korur;
built-in özel Java çizicisi isteyen modelde soru işareti kullanılabilir.
Dayanıklılık çubuğu/büyü çerçevesi vardır. Varsayılan iki saniyelik hedef tutma
süresi ayarlanır; ölüm/dünya/oyuncu kaybında temizlenir. HUD editörü taşıma,
ölçek, hizalama, undo ve yerel kaydı yönetir.
Yeni hedef kartı 220 ms'de hafif büyüyerek belirir; can çubuğu yuvarlatılmış
kenarları koruyan bir renk geçişi kullanır. Reduce Motion ve editör önizlemesi
kartı doğrudan gösterir; animasyon kartın yerleşim sınırları içinde kalır.

### Kozmetikler ve efektler

Sky panorama modunda Atmosphere seçimi Off/Auto/Mist/Aurora/Nebula sunar.
Auto, Aurora panoramasına dalgalı ışık perdeleri, Nebula'ya renkli bulutsu
katmanları, diğer panoramalara hafif ufuk sisi ekler. Atmosphere Density,
Horizon Mist Height ve Animation Speed yoğunluğu, yüksekliği ve hızı ayarlar.
Katmanlar gökyüzünde çizilir; arazi üzerindeki sisin mesafesi değiştirilmez.
Match Fog arazi sisinin rengini gökyüzüne eşler. Ufuk çizgisinde efekt rengi
oyunun sis rengine birleşir. Panorama seçimi iki görsel arasında karışarak
geçer; hızlı seçimlerde mevcut geçiş tamamlanıp en son seçime geçilir. Parlaklık,
dönüş, yoğunluk ve gökyüzü/sis renkleri yumuşatılır. Reduce Motion hareketi
dondurur ve ayar geçişlerini anlık yapar. Animasyon fazı ortak dalga periyodunda
sarılır; uzun oturumlarda kayan nokta hassasiyeti korunur. Animate Atmosphere kapalıyken
katmanlar sabit kalır; Slow Rotation bağımsızdır. Mevcut hava/alt dünya,
su/lav ve Iris shader kontrolleri geçerlidir.
`scripts/Test-SkyShader.py`, moderngl ve EGL ile gerçek GLSL derleme ve
çizimini sınar; Minecraft içi kabul testinin yerine geçmez.

China Hat, Wings ve Cape yalnızca yerel üçüncü şahıs kozmetikleridir; diğer
oyunculara gönderilmez. Gövde/blok maskesi tam uzuv veya kısmi/şeffaf blok
simülasyonu değildir; desteklenmeyen pozlarda çizim durur.
Wings'in Dragon/Angel/Butterfly görselleri ve Cape'in Phoenix/Astral stilleri
gömülüdür. Eski Cape indeksleri 0–7 korunur. Tint Artwork açık değilse özgün
renkler kullanılır. Kumaş 9×13 parçacık, kanatlar sönümlü eklemlerle çalışır;
120 Hz adım ve kare başına altı adım bütçesi vardır. Teleport/oturum/disable
sıfırlar; Reduce Motion/Animation kapalıyken sabit poz kullanılır.
[Asset kökeni ve üretim istemleri](../assets/cosmetics/README.md).

Hit Particles en fazla 96, Jump Circles en fazla 8 efekt tutar; Block Highlight
gerçek blok şeklinin en fazla 24 kutusunu çizer. Hasar, yerel saldırıya yakın
gözlenmelidir; kısa hedef kaybında takip en fazla 500 ms, gözlem sırasına tolerans
75 ms'dir. Bunlar sunucu vuruş ACK'i değildir ve saldırı göndermez.
Trails, Hit Particles ve Jump Circles kısa bir görünme geçişiyle başlar,
ömürleri boyunca yumuşakça söner ve canlı tema rengini takip eder. Parçacık
boyutları ve çift halkalar kademeli açılır; Reduce Motion ek dönüş/büyüme
hareketlerini sabitler. Block Highlight yeni blokta 120 ms'de belirir.
Hit Particles halkaları ve Jump Circles ekran/framebuffer boyutuna göre
12–128 parçaya ayrılır; küçük efektlerde geometri azaltılır. Birleşen konturlar
tek çizgi yolu olarak çizilir, uçların üst üste binmesinden doğan parlak noktalar
azaltılır. Glow dört sönümlü katman kullanır. Çift Jump Circle stilinde iki halka
arasındaki kısa vurgu yayı döner; Reduce Motion bu ek yayı kapatır.
Target HUD can rengi düşük canda kırmızıya yumuşak geçer; hasar çubuğu kısa
bir beklemeden sonra mevcut cana iner. Reduce Motion bu can geçişlerini anlık yapar.
Spotify HUD ilerleme ve görselleştirici geçişleri kare hızından bağımsızdır;
parça kimliği başlık, sanatçı ve süreyle ayrılır. Reduce Motion ilerleme
çubuğunu doğrudan günceller.
Spotify kapak pikselleri ve şarkı sözleri snapshot'lar arasında salt okunur
paylaşılır; her karede büyük vektör kopyası yapılmaz. Şarkı sözü satırı sıralı
zaman damgalarında ikili aramayla bulunur. Kapak dokusu parça değişiminde yeniden
kullanılır; modül kapatılınca, kapak gizlenince ve render kapanışında bırakılır.
`spotify_snapshot` offline testi uzun snapshot/değiştirme döngülerini,
`scripts/Test-SpotifyTexture.py` gerçek upload/cleanup kodunu etkisiz GL
karşılıklarıyla sınar. Bunlar oyun içi FPS veya GPU sürücü testi değildir.
View Model yalnızca vanilla el push/pop aralığındaki eşya transformunu değiştirir;
Vanilla/Compact/PvP/Low/Cinematic profilleri, elle değişince Custom görünümü vardır.
Menzil, kamera, envanter ve saldırı mekaniği değişmez; sözleşme eşleşmezse durur.
`scripts/Build-ViewModelBridge.ps1` kaynak değişince bytecode'u yeniler;
`scripts/Test-ViewModel.py` yerel JAR/ASM ile dönüşümü test eder.

Trails en fazla 96 parçacık / 48 ayak izi tutar. Slab/merdiven yüzeyleri vanilla
collider raycast'inden gelir. Görünürlük kare başına 12 ray ve yaklaşık 200 ms
önbellekle bütçelidir; düşük FPS'de yenileme uzayabilir. Uzun karelerde toplu
emission yoktur. Oturum/teleport/disable izleri siler.

### NameProtect ve Sky

NameProtect yerel görünen adı değiştirir; hesap/UUID, sunucu adı ve gönderilen
mesajı değiştirmez. Alias 1–16 harf/rakam/alt çizgi, varsayılan `Protected`.
Tam isim sınırı ve stil parçaları korunur; önceden satırlara bölünmüş isimler
ve vanilla font kullanmayan üçüncü taraf çizimler kapsam dışıdır.
Vanilla font çizimi/ölçümü ile Target HUD/Nametag/ESP/Radar aynı filtreyi kullanır.
JVMTI dönüşümü kapanışta geri alınır, pasif Java yardımcıları kalabilir.

Sky renk ve gömülü panorama seçenekleri sunar; saat/hava/ışık vanilla kalır.
Sis, güncel terrain fog rengi/mesafesiyle eşleşir; alt yarı küre gerçek sis
rengini kullanır, doku satırı uzatılmaz. Sarma/zenith yumuşatma ve periyodik
türevler seam/mipmap çizgilerini azaltır. Aktif Iris shader paketi, ışığı olmayan
boyutlar veya su/lav/kar içindeki kamera panorama yolunu durdurur.
Başka modun sonradan değiştirdiği renk eski kayıtla ezilmez.
Fabric olayı/GLSL/JNI farkları [sürüm denetiminde](MINECRAFT_26_PORT.md) belgelenir.
Kapanışta listener pasifleştirilir; Fabric kayıt silme API'si olmayan yolda pasif
listener kalabilir. Sky/NameProtect bytecode'u kaynak değişince
`scripts/Build-SkyBridge.ps1` / `scripts/Build-NameProtectBridge.ps1`
araçlarıyla yenilenir.
[Panorama kaynak ve indeksleri](../assets/skies/README.md).

Kozmetik/efekt/overlay ayrıntıları için oyun içi görünüm, shader/resource-pack
uyumluluğu ve FPS ayrıca test edilmelidir. Eski bir DLL veya yalnızca offline
kontrol için verilmiş başarı kaydı yeni kaynak ağacının tamamını doğrulamaz.
