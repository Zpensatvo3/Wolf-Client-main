# C++ / Java hibrit geçişi — ilk sekiz grup, Hit Crystal, Triggerbot, Jump Reset ve Anchor Macro

## Kapsam

İlk sekiz grup yalnızca Fabric **1.21.11** içindir:

- No Jump Delay ve Fast Stop: `src/bridge/GameBridge.java`.
- Fast Place, Sprint ve Full Bright: `src/bridge/UtilityBridge.java`.
- Auto Tool ve Fast EXP: `src/bridge/ItemBridge.java`.
- Chest Stealer, Auto Armor ve Elytra Swapper: `src/bridge/InventoryBridge.java`.
- Pot Refill ve Mouse Tweaks: InventoryBridge'in bağımsız 4/5 container özellikleri.
- Bridge Assist ve Pearl Macro: `src/bridge/WorldBridge.java`.
- Inv Walk ve Glow ESP: `src/bridge/ClientBridge.java`.

**Hit Crystal** ayrı bir tam Java denetleyicisidir: WorldBridge'in 3 numaralı
özelliği desteklenen yedi tam hedef için kaynakta bağlanır. Kararlar, zamanlama,
hedef takibi, slot ve oyun işlemleri Java'dadır; C++ UI/config, fiziksel giriş,
izin/odak kapısı ve çağrı yaşam döngüsünü sağlar. Ayrıntılar [Hit Crystal](#hit-crystal-tam-java-denetleyicisi) bölümündedir.

Kullanıcı ilk üç gruptaki yedi modülün oyun içinde çalıştığını doğruladı.
Dördüncü–sekizinci grupların kaynak/offline kontrolleri vardır; oyun içi doğrulama bekliyor.

## Sınır ve yaşam döngüsü

Native oyun classloader'ının altında, DLL'e gömülü Java köprülerini yükler.
Java yalnızca JDK reflection kullanır; ek Fabric API/JAR/ASM bağımlılığı yoktur.
Bindingler mevcut OneStep SDK'sındaki exact intermediary adları ve tiplerden
çözülür; 1.21.11 thread kontrolü Yarn `class_1255.method_18854()Z` ile yapılır.
İsim/tip uyuşmazlığında yaklaşık eşleşmeye veya başka sürüme geçilmez.

Akış: mevcut render callback → izin/odak/menü kontrolü → Java snapshot → mevcut
C++ karar motoru → tekrar doğrulanan Java yazması. Java yanlış thread'de oyun
nesnelerini okumaz/yazmaz. Worker'a oyun nesnesi veya bekleyen Java işi verilmez.
Java event kaydı, kalıcı native callback ve yeni worker thread eklenmez.

## Grup dizini

Aşağıdaki teknik bölümler uygulama sırasıyla 1–8 olarak düzenlenmiştir.
Sürüm kapsamı yalnızca 1.21.11'dir; durum tablosu yeni bir oyun testi iddiası değildir.

| Grup | Modüller | Oyun içi doğrulama kaydı |
| --- | --- | --- |
| 1 | [No Jump Delay, Fast Stop](#ilk-grup-karar-snapshotı) | Kullanıcı doğruladı |
| 2 | [Fast Place, Sprint, Full Bright](#ikinci-grup-utility-yaşam-döngüsü) | Kullanıcı doğruladı |
| 3 | [Auto Tool, Fast EXP](#üçüncü-grup-eşya-snapshotı-ve-tek-kullanımlık-işlem) | Kullanıcı doğruladı |
| 5 | [Chest Stealer, Auto Armor, Elytra Swapper](#beşinci-grup-envanter-işlemleri) | Offline; oyun testi bekliyor |
| 6 | [Pot Refill, Mouse Tweaks](#altıncı-grup-iksir-doldurma-ve-sürükleme) | Offline; oyun testi bekliyor |
| 7 | [Bridge Assist, Pearl Macro](#yedinci-grup-kenar-desteği-ve-pearl-makrosu) | Offline; oyun testi bekliyor |
| 8 | [Inv Walk, Glow ESP](#sekizinci-grup-envanterde-hareket-ve-glow) | Offline; oyun testi bekliyor |

### İlk grup: karar snapshot'ı

Snapshot paketi `{bridge, player, world, double[11]}` yalnızca çağıranın JNI
local frame'i süresince geçerlidir. Sayısal sıra: şema, işlem, flagler, jump
cooldown, hurtTime, ileri/yan eksen, velocity x/y/z, tek-kullanımlık sıra numarası.
Flagler: alive=1, ground=2, water=4, lava=8, climb=16, glide=32, riding=64,
flying=128, jumping=256, directional pressed=512. Java bir modülün son snapshot'ı
dışındaki/başka bridge'e ait/önceden tüketilmiş kararı reddeder.

Yazmadan önce thread, player/world kimliği, ekran ve alive tekrar kontrol edilir.
Fast Stop ayrıca giriş, hurt ve gözlenen velocity'nin değişmediğini doğrular;
yalnızca yatay hızı sıfırlar, dikey hızı korur. İki backend aynı karede yazmaz.
Java kurulumu başarısızsa native yol korunur; runtime JNI hatası olan karede
işlem iptal olur, sonraki kare native'e geçerken Fast Stop state'i sıfırlanır.
Menü/oyun dışı geçici snapshot yokluğu backend arızası sayılmaz.

### İkinci grup: utility yaşam döngüsü

UtilityBridge, GameBridge'den bağımsız yüklenir; her özelliğin bindingleri ilk
kullanımda ayrı ayrı doğrulanır. Eksik item bindingi Sprint/Full Bright'ı veya
önceki iki modülü devre dışı bırakmaz. Fast Place güncel ana/el dışı eşyayı,
ölü/ekran durumunu ve oyunun atanmış use bindingini kontrol eder. Native fiziksel
sağ tuş bilgisi de kabul edilir. Blok/iksir yoksa işlem yapmaz; delay 0–4 ile
sınırlıdır, mevcut cooldown'u artırmaz, fazladan click/use çağrısı yapmaz ve
iksir içme süresine dokunmaz.

Sprint, vanilla `canStartSprinting()` sonucunu kullanır; eksik eşleşmede ileri
tuşa bakarak zorla sprint açmaz. Java yalnızca kendisinin başlattığı sprint'i
sahiplenir. Önceden açık manuel sprint veya gerçek sprint bindinginin devraldığı
durum kapanışta bozulmaz. Farklı world/player'a geçildiğinde eski nesneye yazmaz.
Eşya kullanımı ayarı mevcut modüldeki gibi yeni sprint başlatmayı engeller.

Full Bright, oyuncu/dünya gerektirmeden mevcut gamma option üzerinde çalışır;
görsel olduğu için ana menü/odak kaybında da kullanılabilir. Modül açıldığında
orijinal değeri saklar; sonradan değişen manuel gamma tercihini korur. Kapanışta
yalnızca hâlâ kendisinin yazdığı 16.0 değeri bulunan aynı option'ı geri alır.
Değiştirilmiş veya yerini başka option almış nesneye eski ayarı yazmaz.

Sprint/Full Bright'ın 1.21.11 worker yolu normalde pasiftir; worker disable
yalnızca atomik iptal ister. Kapalı modül render cleanup'ı ve `on_render_shutdown`
Java cleanup'ını oyun thread'inde yapar. Renderer join'den sonraki Java `stop`
yalnızca ref/durum temizler, oyuna yazmaz. Kurulum/binding hatasında ilgili modülün
orijinal native yolu korunur. Bir runtime mutator hata verdiyse o kare iptal edilir;
Java sahipliği başarıyla temizlenmeden native backend'e geçilmez. Yeni bir iş
kuyruğu veya render sonrasında çalışabilecek bekleyen yazma eklenmez.

### Üçüncü grup: eşya snapshot'ı ve tek kullanımlık işlem

ItemBridge önceki köprülerden bağımsız yüklenir. Auto Tool ve EXP bindingleri
ayrı ayrı hazırlanır; birinin eşleşmemesi diğerini veya önceki beş modülü etkilemez.

Auto Tool'un mevcut uygunluk önceliği ve `speed > current + 0.01f` kuralı
değiştirilmeden `auto_tool_scoring.hpp` içine çıkarıldı. Native ve Java I/O
yolları aynı karşılaştırmayı kullanır. Java crosshair bloğunun state'ini ve
hotbar'daki dokuz stack'in skorlarını toplar; en iyi slotu **C++ seçer**.
Uygulamada player/world, inventory, interaction manager, blok konumu/state'i,
manuel seçili slot, stack kimlikleri ve skorlar tekrar kontrol edilir. Eski
karar aynı turda ikinci kez kullanılamaz. Slot seçimi mevcut `hotbar::select`
sözleşmesi gibi önce yerel selectedSlot'u günceller, ardından vanilla
syncSelectedSlot çağırır. Yeni bir slot sahibi/geri alma kuyruğu eklenmez;
diğer modüllerin mevcut slot kontrolleri aynı oyun durumunu görmeye devam eder.
Java backend aktifken Auto Tool worker'ı oyun verisine erişmez.

Fast EXP'nin min/max CPS hesabı, 1–50 aralığı, 1000/CPS dönüşümü, ±1.5 ms jitter
ve tek next-click zamanı C++'ta aynen korunur. Java yalnızca güncel EXP tespitini,
yerel kullanım gecikmesinin sıfırlanmasını ve vanilla doItemUse çağrısını yapar.
Ana/el dışı EXP tespiti ve vanilla el önceliği korunur; Java el/slot değiştirmez.
Önceki yolda aynı turda SendInput + doItemUse vardı. Yeni Java yolunda **yalnızca
bir doItemUse** vardır; Windows tıklaması veya tuş bırakma üretmez. Fiziksel sağ
basış giriş servisinin sentetikten ayrılmış durumundan alınır.
1.21.11'in native fallback'i de tek vanilla giriş noktasını kullanır; diğer
altı hedefin önceki giriş yolu değiştirilmedi.

EXP packet'i frame-local player/world ve main/off stack kimliği, EXP bayrakları
ve tek kullanımlık sıra numarası taşır. Tuş bırakma, ekran, oyuncu/dünya/eşya
değişimi eski kullanımı geçersizleştirir. Dünya/oyuncu oturumu native tarafta
izlenir ve zamanlama döngüsü sıfırlanır. EXP bittiğinde veya use çağrısı başarısız
olduğunda fiziksel tuş bırakılmadan tekrar denenmez; kısmen uygulanmış bir çağrı
native fallback üzerinden tekrarlanmaz. Disable sadece atomik reset ister;
session/timer state'i render thread'inde temizlenir. Java geleceğe iş bırakmaz.

### Beşinci grup: envanter işlemleri

InventoryBridge bağımsız yüklenir ve üç özelliğin bindinglerini ayrı hazırlar.
Chest Stealer'ın sıralama/rastgele seçim, 25–800 ms açılış tepkisi ve mevcut
tıklama aralıkları C++'ta kalır. Yeni player/world/screen/handler kimliği yeni
bir açılış tepkisi başlatır. Java yalnızca GenericContainerScreen'in sandık
slotlarına tek QUICK_MOVE uygular; oyuncu envanterini adaylara dahil etmez.
Native'ın kareler arasında tuttuğu oturum referansları global JNI ref'tir.

Auto Armor'ın malzeme sıralaması, en yüksek puan kazancı ve elytra koruması;
Elytra Swapper'ın ana envanter/hotbar arama sırası `gear_policy.hpp` içine
aynı kurallarla çıkarıldı. Gecikmeler ve mevcut ayarlar/config kimlikleri korunur.
Normal 1.21.11 Auto Armor worker'ı pasiftir; tüm Java işlemleri render/client
thread'inde yapılır. Elytra Swapper tek kullanımlı modül olarak kalır; envantersiz
çalışma kapalıysa envanter açılana kadar bekler.

İki takas da yalnızca oyuncunun kendi PlayerScreenHandler'ında çalışır; kapalı
envanter tercihi diğer konteynerleri kabul etmez. Her PICKUP öncesi aynı
player/world/screen/handler/manager ve syncId, kaynak/hedef slot kimliği,
ItemStack kopyasının adet/bileşenleri ve imleç doğrulanır. Üç adım Java'da açık
sahiplikle izlenir; karar paketleri tek kullanımlıdır. Ekran, eşya veya manuel
imleç değişimi eski takası durdurur. İptal yalnızca sahipliği bırakır: kullanıcı
imlecine kör bir "geri koy" tıklaması yapmaz; yarım kalan imleci kullanıcı yönetir.

Odak/UI/disable/kapanış sonraki otomatik adımı iptal eder. Yalnızca başlangıç
kurulum/binding hatasında eski native yol kullanılabilir. Envanter okuma/yazma
exception'ında ilgili özellik bu yükleme boyunca askıya alınır; kısmen uygulanmış
tıklama native fallback ile tekrarlanmaz. Java stop oyun nesnelerine yazmaz.

### Altıncı grup: iksir doldurma ve sürükleme

Pot Refill'in `pot_refill_engine.hpp` seçim, sıralama/rastgelelik, 50–1000 ms
gecikme, sonraki kare gözlemi ve 1500 ms timeout motoru değiştirilmedi.
Java her Slot'un gerçek backing inventory/index/id bilgisini okur; standart
PlayerScreenHandler yerleşimi varsayılmaz. Yalnızca oyuncunun kendi inventory
slotları adaydır; healing/strong healing, PotionItem ve potion component üzerinden
tanınır. `ComponentsAccess.method_58694` interface'inden reflection yapılır;
ItemStack üzerinde aynı metodun ayrıca tanımlı olması beklenmez.

Her SWAP öncesi oturum/screen/handler/manager/syncId, kaynak/hedef Slot nesneleri,
stack adet/bileşenleri, boş imleç ve boş hedef tekrar kontrol edilir. Native giriş
servisi güncel fiziksel sol/sağ tuşu kontrol eder; manuel işlem varken refill durur.
Kaynak stack kopyası ve işlem kimlikleri Java'da tutulur; sonraki karede aynı
kaynağın boş, aynı hedefin beklenen stack/adette olması yerel tüketim göstergesidir,
sunucu ACK'i değildir. Engine hazır olmadan ikinci işlem gönderilmez. Timeout
sonrası envanter oturumu yenilenene kadar tekrar denenmez.

Mouse Tweaks'in 1.21.11 yolunda sürükleme durumu, tuş okuması ve yuva işlemleri
Java'dadır. Her çizim karesinde tek köprü çağrısı yapılır; Java yalnızca üstündeki
yuva, imleçteki stack ve etkin oturum nesnelerini okur. Tüm envanteri kopyalayan
snapshot turu ve C++ gesture motoru kullanılmaz. RMB dağıtımı, geri dönülen yuvaya
tekrar bir adet koyma, LMB uyumlu stack toplama, Shift quick-move ve crafting
output davranışı korunur. Tıklama aralığı zamanlayıcısı eklenmez.

LMB birleştirmede ikinci PICKUP öncesi imlecin boşaldığı ve yuvadaki stack'in
beklenen sayıya ulaştığı kontrol edilir; crafting output için ikinci tıklama yoktur.
Java, oyun durumuna yalnızca client/render thread'inde erişir. UI, odak, ekran,
oyuncu, dünya veya handler değişince sürükleme durumu sıfırlanır. Kurulum/binding
başarısız olursa özellik 1.21.11 oturumunda askıya alınır; kısmi işlem başka bir
backend ile tekrarlanmaz. Diğer oyun hedefleri mevcut native yollarını korur.
Mevcut ayarlar, UI ve bildirim tercihleri değiştirilmedi.

### Yedinci grup: kenar desteği ve pearl makrosu

WorldBridge bağımsız yüklenir; iki özellik ayrı hazırlanır. Mevcut UI/config
kimlikleri ve diğer altı sürümün native yolları değişmez. Normal 1.21.11 worker
yolları pasiftir; Java okuma/yazmaları client/render thread'indedir.

Bridge Assist'in `.0004` hareket eşiği, hitbox merkez/yarı genişlik hesabı,
`.025` minimum yarı genişlik, 0–.27 offset ve `.35` yönlü tahmini C++'ta kalır.
`bridge_assist_logic.hpp` sekiz köşe için 24 blok koordinatını üretir; Java yalnızca
o koordinatlardaki BlockState/isAir değerlerini okur. Air-only destek tanımı
bilinçli olarak değiştirilmedi. İki edge örneği, üç güvenli örnek ve 0–300 ms
bırakma gecikmesi aynı kurallardır; örnekler artık render callback başına alınır.
Geçersiz/sonlu olmayan veya JNI int alanı dışındaki koordinatlar reddedilir.

Java force öncesi world/player/options/key/boundKey, eldeki stack kopyası,
onGround, hız ve hitbox değerlerini yeniden doğrular. Manuel basılı sneak
sahiplenilmez. Disable/menü/odak/oturum değişiminde yalnızca kendi bastığı aynı
binding temizlenir; KEYSYM/SCANCODE/MOUSE descriptor'ı native fiziksel giriş
servisine çevrilir. Sonradan basılan veya yeniden atanmış fiziksel tuş korunur.
Okuma ve cleanup arasında binding tekrar değişirse cleanup yeniden denenir.

Pearl Macro'nun ilk hotbar pearl seçimi C++'tadır; 0–150 ms switch, 55 ms dwell
ve 0–250 ms return aşamaları korunur. Java seçimi vanilla syncSelectedSlot ile
eşler, kullanım öncesi aynı world/player/inventory/manager, seçili slot ve pearl
stack adet/bileşenlerini doğrular. Kullanım sahipliği çağrıdan **önce** tüketilir.
Yeni yolda tek vanilla doItemUse vardır: sentetik sağ tuş basma/bırakma veya 55 ms
boyunca tekrarlayan kullanım yoktur. Dwell hâlâ slot geri dönüşünden önce beklenir.
Manuel slot değişimi pending kullanımı iptal eder ve eski slotla ezilmez.
Restore yalnızca aynı oturum ve hâlâ sahip olunan seçili slot için yapılır.

### Sekizinci grup: envanterde hareket ve glow

ClientBridge bağımsız yüklenir; Inv Walk ve Glow ESP bindingleri ayrı hazırlanır.
Inv Walk'ın `inv_walk_engine.hpp` ekran/odak/typing politikası, container tercihi,
jump/sneak maskesi ve vanilla'ya fiziksel tuş devri C++'ta aynen kalır. Java
oyunun güncel altı hareket bindingini ve KEYSYM/SCANCODE/MOUSE descriptor'larını
okur; native giriş servisi sentetik olaylardan ayrılmış fiziksel durumu verir.
Apply öncesi fiziksel basış tekrar kontrol edilir. Attack/use bindinglerine yazılmaz.

Yazı ağacı aynı 12 derinlik/512 düğüm sınırıyla taranır; odaklı, görünür ve aktif
TextFieldWidget, creative arama ve açık recipe-book arama alanları hareketi
durdurur. Traversal/binding hatası kapalı davranır. Yazmadan önce aynı player,
world, screen, options, KeyBinding, boundKey ve descriptor değerleri yeniden
doğrulanır; typing durumu apply sırasında tekrar okunur. Opsiyonel/unbound bir
tuş diğer hareket tuşlarını bozmaz. Yalnızca Java'nın bastığı tuşlar sahiplenilir.

Disable/odak/loader/unload cleanup'ı aynı render/client sınırından geçer. KeyBinding
GameOptions'a ait global durum olduğundan dünya değişse de hâlâ aynı key nesnesi
üzerindeki kendi basışı temizlenir; eski entity/world nesnelerine yazılmaz.
Vanilla'ya dönüşte güncel fiziksel basış korunur; suspension'da kendi basışı
bırakılır. Kısmi setPressed exception'ında sahiplik cleanup için korunur.

Glow ESP'de Java geçerli dünyadan canlı, local oyuncu olmayan PlayerEntity
adaylarını okur; **mevcut `services::anti_bot::is_bot` kararı native'de kalır**.
AntiBot'un veri okuması/belleği Java'ya taşınmadı veya başka hesapla değiştirilmedi.
Boolean izin maskesi aynı packet sırasındadır; Java apply öncesi oturumu, entity
kimliğini, dünyada hâlâ yüklü oluşunu ve alive durumunu tekrar doğrular. Tarama
16384 entity/4096 player ile sınırlıdır; paketler tek kullanımlıdır.

Her iki özellikte kurulum/binding hatası eski backend'i korur. Runtime exception
ilgili özelliği bu yükleme boyunca askıya alır; kendi durumunu cleanup etmeye devam
eder, başka backend üzerinden mutasyonu tekrarlamaz. Renderer cleanup sonrası
Java stop yalnızca referansları bırakır. UI/config/modül kimlikleri değişmez.

## Hit Crystal: tam Java denetleyicisi

`src/modules/combat/hit_crystal.cpp` yalnızca ayarları aktarır ve yaşam döngüsünü
bildirir. Native karar motoru veya oyun işlemi kalmadı. WorldBridge özelliği 3,
1.21.1/1.21.4/1.21.8/1.21.11/26.1/26.1.1/26.1.2 için tam sürüme ait class/member
adlarını C++ profilinden alır; isim/tip uyuşmazlığında yakın eşleşme yapılmaz.
İlk iki WorldBridge özelliği yalnızca 1.21.11 olarak kalır ve ayrı hazırlanır.

Fiziksel sağ tuş basılıyken obsidyen/bedrock veya ona bağlı mevcut kristal
hedefinden başlanır. Otomatik seçim ve isteğe bağlı geri dönüş, ayrı milisaniye
yerleştirme/kırma gecikmeleri ve bütün döngü Java'dadır. Reflection üyeleri bir
kez çözülür; entity sorgusu yalnızca tabanın üstündeki 1×2 blok hacmindedir.
Kırma onayında dünya üzerinde ID ile aynı entity aranır; menzil dışına çıkmak
silinme onayı sayılmaz. ID tekrar kullanılırsa yeni entity'ye saldırılmaz.

Yerleştirmeden önce taban, hava, entity boşluğu, el/slot, oturum ve vanilla
erişim mesafesi yeniden okunur. Kırmada gerçek entity menzili ve blok raycast'i
kontrol edilir. `interactBlock` kabul sonucu okunur; `attackEntity` ve main-hand
swing vanilla yollarıdır. Yerel entity silinmez, paket veya Windows tıklaması üretilmez.
Spawn için 480 ms, kırma onayı için 360 ms beklenir; en fazla üç yerleştirme
denemesi ve ilk saldırıdan sonra üç kırma tekrarı vardır. Kare başına bir
etkileşim yapılır; çağrıdan sonra hata olduysa aynı işlem yeniden yürütülmez.

Makro kullanım bindinginin basılı/queued durumunu sahiplenerek ikinci bir
vanilla yerleştirmesini engeller. Gerçek binding descriptor'ı native fiziksel
giriş servisiyle doğrulanır; bırakmada aynı world/player/options/key/boundKey
üzerinde hâlâ geçerli manuel basış devredilir. Yeni atamaya eski tuş yazılmaz.
Slot elle değiştirilirse veya world/player/inventory/manager değişirse döngü
iptal edilir. Restore yalnızca aynı oturum ve hâlâ sahip olunan slot için yapılır.
Odak/menü/izin kaybı ve disable cleanup'ı client thread'indedir; worker sadece
atomik iptal ister. `stop()` renderer katıldıktan sonra yalnızca referans bırakır.

[Argon inceleme kaydı](ARGON_REFERENCE.md) davranış kökenini ve kapsamı açıklar.
`tests/hit_crystal/HitCrystalBridgeTest.java` izole kaynak senaryolarını içerir;
gerçek Minecraft sürüm desteği ve oyun içi davranış ayrıca doğrulanmalıdır.
Bu değişiklikte build alınmadı; gömülü WorldBridge.class sonraki normal build'de
mevcut Java kaynak bağımlılığı üzerinden yeniden üretilir.

## Derleme ve sınırlı kontroller

Sonraki modüller aynı sınır üzerinden tek tek taşınabilir; combat hesaplamaları
veya ortak input/hotbar sahipliği bu sekiz grupta yeniden yazılmamıştır.

## Referans / kaynak bildirimi

İkinci–sekizinci gruplarda bu referanstan ek kod/algoritma alınmadı; mevcut
OneStep kaynakları ve Yarn'ın exact sürüm API/mapping belgeleri kullanıldı.

Thread mapping doğrulaması:
https://maven.fabricmc.net/docs/yarn-1.21.11+build.4/net/minecraft/util/thread/ThreadExecutor.html

Utility binding doğrulaması (özellikle `method_48300`, `field_1867`, `method_7909`):

- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/network/ClientPlayerEntity.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/option/GameOptions.html

ClientBridge exact Entity glow/flag, ClientWorld getEntities ve ParentElement
mapping doğrulaması:

- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/entity/Entity.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/world/ClientWorld.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/gui/ParentElement.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/item/ItemStack.html

Item binding doğrulaması (özellikle selectedSlot/main, mining speed/suitability ve slot sync):

- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/entity/player/PlayerInventory.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/network/ClientPlayerInteractionManager.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/gui/screen/ingame/HandledScreen.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/screen/ScreenHandler.html

Envanter bağlamı (`field_7498` PlayerScreenHandler / `field_7512` ScreenHandler):

- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/entity/player/PlayerEntity.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/screen/PlayerScreenHandler.html

Container binding doğrulaması:

- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/component/ComponentHolder.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/screen/slot/Slot.html
- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/potion/Potions.html

WorldBridge bindingleri kendi SDK'sındaki Entity/Box, InputKey/KeyBinding,
Items/PlayerInventory ve vanilla kullanım mappinglerinden alınmıştır. Güncel
sneak seçeneği ayrıca GameOptions API belgesiyle kontrol edildi:

- https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/option/GameOptions.html

## Triggerbot: Java denetleyicisi

WorldBridge'in lazy hazırlanan 4 numaralı özelliğidir. Yedi tam hedef için
sürüm profiline göre exact adlar kullanır: eski hedeflerde renderer ray sorgusu,
1.21.11/26.1.x'te client player static ray sorgusu; eski SwordItem veya yeni
sword tag'i ve sürüme uygun float/double fallDistance. Bu bağlamaların kaynakta
bulunması oyun içi uyumluluk kabulü değildir.

Hedef/silah filtreleri, güncel ray sorgusu, cooldown/reaction örneklemesi,
Smart/Only critical, kalkan yönü, manuel saldırı ve slot değişiminde yeniden
başlama, helper sonrası tick bekleme ve vanilla attackEntity/swing Java'dadır.
Hedef, dünya, oyuncu, manager, inventory, slot ve ayar nesli saldırı öncesinde
tekrar doğrulanır. Menü/odak/disable/unload iptali client thread'de yapılır.
Mutasyon hatasında özellik askıya alınır; başka backend'e geçilip vuruş tekrarlanmaz.

C++ UI/config/reset, izin ve manuel giriş neslini taşır; On Click Only sol tuşu
Java tarafında GLFW'den okur. Java'nın dar native callback'i yalnızca
ortak Friends/Anti Bot, combat ownership ve mevcut Shield Breaker/Mace Swap
hazırlama/bitirme servislerini çağırır. Bu servislerin kendi politikaları korunur.
`trigger_adapter` artık native hedef/saldırı motoru çalıştırmaz ve Windows click
ticket'i üretmez. Eski saf C++ model ve fixture'ları tarihsel regresyon referansı
olarak kalır; canlı modül o modeli kullanmaz. Eski config alanları ve stable ID korunur.

Kaynak Java/C++ derleme kontrolleri oyun içi test değildir. Bu geçişten sonra
DLL yeniden üretilmeden önceki test DLL'i yeni Triggerbot'u içermez.

Triggerbot `swing_hand` seçeneğini config'e yazar (varsayılan true); vanilla
saldırıdan sonraki ek el sallama çağrısı bu seçeneğe bağlıdır. GLFW metotları ve
window erişimi lazy feature hazırlığında bir kez çözülür; tuş yalnızca client
thread'de, sıfır olmayan güncel handle ile okunur. On Click Only açıkken tuşun
bırakılması helper hazırlığını da iptal eder. Windows click üretilmez; isteğe bağlı Click Simulation oyunun Java mouse callback'ini kullanır.

`click_simulation` varsayılan false'tur; açıkken Java callback pulse'u doğrudan
saldırıdan sonra üretilir. Pulse senkrondur; worker/timer/release kuyruğu yoktur.
Mouse'a bağlı bütün KeyBinding'lerin press kuyruğu/basılı halleri korunarak ikinci
vanilla saldırı önlenir. Pencere/odak/oturum kaybında eski basılı hal geri yazılmaz.
1.21.11 ve sonraki hedeflerde MouseButtonInfo, eskilerde dört argümanlı mouse
callback imzası kullanılır; lazy çözümleme yalnızca seçenek açıkken yapılır.


## Jump Reset: Java denetleyicisi

`src/modules/combat/jump_reset.cpp` ayar/preset/config arayüzünü ve yaşam döngüsünü
sağlar; çalışan native karar motoru veya JNI oyun okuması içermez. WorldBridge
özelliği 5, mevcut SDK profilinin exact adlarıyla yedi DLL hedefinde bağımsız
hazırlanır. Binding hatası modülü pasif bırakır; native karar motoruna dönülmez.
`jump_reset_engine.hpp` yalnızca eski sözleşmenin offline geçiş referansıdır.

Java client thread'inde dünya/oyuncu/options/binding kimliği, canlılık, ekran,
eşya kullanımı, yüzme/uçuş/tırmanma/araç ve fiziksel jump durumu okunur. Hasar
kenarı bir kez örneklenir; Living Attacks Only için taze, kendisi dışındaki canlı
saldırgana ait damage source en çok 105 ms içinde eşlenir. Gecikme 0–100 ms,
karar ömrü 170 ms, yerde gözlem toleransı 85 ms'dir; havada jump gönderilmez.
Game Binding vanilla binding'i basar; oyun tick'i ilerlediğinde veya en geç
200 ms'de bırakır. Aynı hasar animasyonu tekrar denemez; backoff 140 ms'dir.

Giriş yolu Java'dadır. `GLFW.glfwGetKey` ve `glfwGetMouseButton`, güncel oyun
penceresinin fiziksel durumunu okur; SCANCODE atamaları `glfwGetKeyScancode`
üzerinden platforma göre eşlenir. Basma/bırakma vanilla `KeyBinding.setPressed`
çağrısıdır. GLFW_FOCUSED ve window handle tekrar doğrulanır; kullanıcı basışı
korunur, odak kaybı veya pencere değişiminde eski basış yeniden yaratılmaz.

Eski System Space / `mode: 1` kayıtları **Game Space** olur. Bu mod Space jump
ataması ister ve tamamen Java game-binding yolundan çalışır. Native Space
biletleri, receipt/validator callback'leri ve Windows input üretimi kaldırıldı.
Native callback yalnızca izin/odak ve ayar iptal neslini okur. Basma client
thread'inde senkron uygulanır; bir sonraki tick veya en geç 200 ms'de bırakılır.
Disable, ayar veya oturum değişimi işi iptal eder; kısmen uygulanmış press
istisnasında cleanup denenir, modül askıya alınır ve aynı hasar tekrar oynatılmaz.

Argon'un [AutoJumpReset davranış kaydı](ARGON_REFERENCE.md#jump-reset-için-incelenen-akış)
referanstır. `tests/jump_reset/JumpResetBridgeTest.java` bağımsız oyun/platform
fixture'ları ile karar, iptal, GLFW/game input sahipliği ve yedi exact binding protokolünü
sınar; `Test-HybridBridge.py` hem kaynak hem gömülü sınıfı çalıştırır. Bu kontroller
Windows DLL bağlama veya gerçek Minecraft/oyun içi sürüm kabulü değildir.


## Anchor Macro: Java denetleyicisi

`src/modules/combat/anchor_macro.cpp` yalnızca ayar snapshot'ını ve yaşam döngüsünü
WorldBridge'e iletir. Özellik 6; hedefe özel SDK adları, GLFW giriş okuması,
vanilla envanter seçimi/senkronizasyonu ve `interactBlock`/el sallama ile çalışır.
Native callback yalnızca runtime izin/odak ve iptal neslini denetler. Ayar okumasından
önce alınan nesil dispatch boyunca doğrulanır; eski ayarla yeni neslin eşleşmesi
önlenir. Hazırlama hatası native Anchor Macro motoruna dönmez.

[Modül sözleşmesi](MODULES.md#anchor-macro) Mode ve 13 ortak kontrolü açıklar.
Normal, OneStep koyma/dolum/patlama sırasını korur. Normal her tick'te en fazla bir slot/oyun
mutasyonu yapar; sıfır gecikmeler aynı tick'te art arda paket üretmez. Sonraki
aşama yerleşim/dolum gözlenmeden açılmaz. Bir gözlem 650 ms'de gelmezse veya
döngü toplam 10 saniyeyi geçerse döngü durur; belirsiz/istisna sonrası işlem
tekrarlanmaz. Patlatma için yapılandırılan slot seçilir; glowstone kabul edilmez.

Input sahipliği options/key/boundKey/window kimliklerine, slot sahipliği
dünya/oyuncu/manager/inventory ve seçili slota bağlıdır. Kapanış temizliği client
thread'indedir; module worker yalnızca iptal neslini değiştirir. Disable, odak,
loader, ayar veya oturum değişimi yeni işlemleri durdurur. Runtime exception
özelliği askıya alır; yarım uygulanmış etkileşim yeniden oynatılmaz.

Eski Legit/Blatant değerleri schema'da gizli olarak saklanır ve public config
alias dönüşümü devam eder. Eski switch/charge/detonate gecikmeleri açıkça içe
aktarılmışsa milisaniyeden tick'e yukarı yuvarlanır; yeni kontrol değerleri
önceliklidir. Safe Anchor mevcut `anchor_engine` yolunda kalır.

`tests/anchor_macro/AnchorMacroBridgeTest.java` kaynak ve gömülü sınıf için
bağımsız oyun/GLFW/Mouse fixture'larıyla çalışır. Kaynak senaryoları gerçek
Minecraft/Fabric, Windows DLL linki veya oyun içi sürüm kabulü değildir.


### Anchor Macro Legit olay bağlantısı

`execution_mode`: 0 Normal (varsayılan), 1 Legit. Eski public config içe aktarımı
bu alan yoksa Normal seçer; tarihi gizli `mode` alanı repurpose edilmedi.
Legit'in üç sayacı, koordinat sahipliği ve fake-use işareti Normal'in transaction
state'inden ayrıdır. Normal regresyon senaryoları iki mod eklendikten sonra da
aynı davranışı denetler.

Legit ilk seçildiğinde kendi JVMTI ortamında yalnızca MinecraftClient.tick ve
doItemUse HEAD'lerine `AnchorEvents` çağrıları eklenir. 26.x exact Mojang adları
tick/startUseItem'dır. `AnchorEvents` game loader'a, WorldBridge ve ASM transformer
izole loader'a yüklenir. Callback'ler Java Consumer/Predicate üzerinden geçer;
oyun tick/use sıcak yolunda native hook/callback yoktur (izin kontrolü hariç).
Şema değişmeden retransform edilir. Kurulum reddedilirse Legit devre dışıdır,
Normal hazırlığı etkilenmez. Stop owner-token kontrolüyle Java callback'lerini
bırakır; JVMTI ortamı kendi dönüşümünü kaldırıp dispose edilir.

Source ve gömülü sınıflar için:

```sh
python scripts/Test-HybridBridge.py
python scripts/Test-AnchorHooks.py --asm-classpath '<ASM 9.9+ ve ASM Tree classpath>'
```

`Test-AnchorHooks.py` gerçek dönüştürülmüş bytecode'u JVM verifier ile yedi hedef
protokolünde çalıştırır. Tick/use HEAD sırası, fake-use iptali, şema korunumu,
hatalı giriş ve stop/owner davranışı sınanır. `tests/anchor_legit/` gerçek event
relay'iyle sayaç/şans/hedef, Only Own, Only Charge, slot, swing ve 35 ms pulse
yaşam döngüsünü izole fixture'larda denetler. Bu kontroller Windows DLL linki veya
Minecraft üzerinde canlı retransform kabulü değildir.

Normal Java kaynakları/AnchorEvents normal BuildGameBridge hedefinde derlenir.
Transformer değişirse `scripts/Build-AnchorHooks.ps1 -AsmClasspath ...` ASM/ASM Tree
ile gömülü AnchorTransformer.class dosyasını yeniden üretir (diğer ASM köprüleri
gibi). Runtime Fabric'in ASM sınıflarını kullanır; harici bir oyun mod JAR'ı
paketlenmez. 35 ms release client thread'deki ilk uygun tick/render'a bağlıdır;
Argon'un thread havuzunun birebir zamanlayıcı yapısı değildir.

## Auto Totem Java denetleyicisi

Auto Totem WorldBridge feature 7 içinde, Java oyun thread’inde çalışır ve Argon
AutoInventoryTotem akışını izler: Blatant/Random kaynak seçimi, önce offhand,
isteğe bağlı hotbar hedefi, Auto Switch ve modülün açtığı envanteri gecikmeli
kapatma. Native `tick_totem` yalnızca ayarlar ve generation yetkisini iletir;
oyuncu/envanter okuma ile SWAP çağrıları Java’dadır. Blatant ilk, Random
uygun kaynaklar arasından rastgele seçim yapar. Oyun ekranı Windows tuş
simülasyonuyla değil vanilla InventoryScreen API’siyle açılır. Gerçek oyun
üzerinde henüz doğrulanmadı.

Auto Totem gerçek client tick HEAD olayını Anchor Legit ile aynı Java relay
üzerinden alır. `AnchorTransformer.inventoryScreen` hedef oyunun InventoryScreen
alt sınıfını üretir; slot/mouse click metotları etkisizdir. Tipler ve metot
imzaları yansımayla alınır; fare API değişiklikleri için sabit descriptor tahmini
yapılmaz. Test komutu:

```sh
python scripts/Test-AutoTotem.py --asm-classpath <asm.jar:asm-tree.jar>
```


## Crystal Aura: ilk Java geçişi (eski tasarım)

Crystal Aura artık WorldBridge feature 8 içinde çalışır. Native modül yalnızca
ayarları ve yaşam döngüsü sürümünü iletir, Java durumunu arayüzde gösterir.
Mevcut config anahtarları ve kılıç → havalanma → obsidyen → kristal → kırma
sırası korunur. Hedef seçimi, kilit, mob testi, menzil/görüş/boşluk sorguları,
vanilla cooldown, görünür kamera dönüşü, hotbar değişimi/geri yükleme, gecikmeler
ve sunucudan doğma/kaybolma teyidi Java’da yürütülür.

Fabric API varsa START_CLIENT_TICK kullanılır; render çağrısı ayarları yayımlar.
API yoksa aynı Java denetleyicisi client frame üzerinden çalışır; oyuncu yaşı
kontrolü aynı oyun tick’inde tekrarlı çalışma ve etkileşimi engeller.
200 ms ayar akışı kesilmesi, menü/odak/yetki kaybı, disable/config sürümü,
oyuncu/dünya/envanter değişimi ve manuel slot seçimi döngüyü iptal eder.
Slot yalnızca aynı oturum ve hâlâ sahip olunan seçim için geri yüklenir.
Hata veren oyun işlemi Java denetleyicisini askıya alır; native tekrar yoktur.
Kristalin sorgu alanından çıkması veya aynı ID’nin farklı entity tarafından
kullanılması silinme teyidi sayılmaz. Tüm beklemeler süre sınırına tabidir.

Native auraService yalnızca ortak arkadaş/Anti Bot ve combat ownership
servislerini sunar. Saldırı kilidi altında Java executeAuraAction çağrılır;
burada oturum, hedef, seçili eşya, güncel vanilla ray ve mesafeler son kez
kontrol edilir, vanilla attackEntity/interactBlock/swing çağrıları Java’dan
yapılır. Triggerbot önceliği ve ortak saldırı sahipliği koordinasyonu korunur.
crystal_aura_engine.hpp yalnızca eski offline regresyon modeli olarak kalır;
çalışan modül tarafından kullanılmaz. Desteklenen yedi SDK profilinin kesin
isimleri aynı native binding tablosundan alınır.


Doğrulama: `scripts/Test-CrystalAura.py` canlı Java denetleyicisini hem kaynak
hem gömülü sınıf üzerinden izole oyun/tick örnekleriyle kontrol eder; genel
`Test-HybridBridge.py` paketi de bu senaryoları içerir. Bu kontroller gerçek
Minecraft oturumunda çalışma/oyun uyumluluğu doğrulaması yerine geçmez.


### 1.21.11 native binding regression

Crystal Aura eklenirken ortak native listede method_6032 iki kez bulunması,
WorldBridge constructor’ının bütün listeyi reddetmesine neden oluyordu.
Tekrarlanan kayıt kaldırıldı. world_binding_fixture.py artık C++ koşullarını
yedi hedef için preprocess eder ve tekrarları kontrol eder; 1.21.11’de gerçek
native liste tekilleştirilmeden Java test constructor’ına verilir. Böylece
önceki testlerin Set kullanarak gizlediği hata tekrar testten geçemez.

Test DLL’de std::terminate/SIGABRT olursa hata yığını ve DLL adres ofsetleri
Windows temp klasörüne OneStep-abort-<PID>.log adıyla yazılır. CRT kapanış
sonucu değiştirilmez; raporlayıcı normal unload’da eski handler’ları geri
koyar. Java köprüsünün kurulum/hazırlık/işlem hataları stderr’e ayrıntılarıyla
yazılır. Dawn’ın mevcut logunda yalnızca normal Stopping! ve process exit 3
bulunduğu için eski kapanış abort’unun kesin nedeni doğrulanmış değildir.


## Crystal Aura: güncel yeniden tasarım

Önceki kılıç/havalanma zinciri canlı modülden kaldırıldı. Güncel davranış,
yedi kaynakla karşılaştırma, ayarlar ve doğrulama sınırları
[CRYSTAL_AURA_REDESIGN.md](CRYSTAL_AURA_REDESIGN.md) içindedir.
WorldBridge tickAura ABI artık `(ZJ[I)[I`; native taraftan 11 tamsayılı ayar
paketi gelir. Minimum/maximum hasar ve menziller 1/1000 ölçeklidir, son tamsayı
place/break/auto-switch/restore/support/protection bitleridir. Eski config
alanları yalnızca profil uyumluluğu için saklanır.
