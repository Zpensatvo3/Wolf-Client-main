# C++ / Java hibrit geçişi — ilk sekiz grup, Hit Crystal ve Triggerbot

## Kapsam

İlk sekiz grup yalnızca Fabric **1.21.11** içindir:

- No Jump Delay ve Fast Stop: `src/bridge/GameBridge.java`.
- Fast Place, Sprint ve Full Bright: `src/bridge/UtilityBridge.java`.
- Auto Tool ve Fast EXP: `src/bridge/ItemBridge.java`.
- Left Clicker ve Right Clicker: ItemBridge'in bağımsız, lazy hazırlanan 3/4
  özellikleri; önceki iki özellik değişmeden korunur. Inventory Clicker kaldırıldı.
- Chest Stealer, Auto Armor ve Elytra Swapper: `src/bridge/InventoryBridge.java`.
- Pot Refill ve Mouse Tweaks: InventoryBridge'in bağımsız 4/5 container özellikleri.
- Bridge Assist ve Pearl Macro: `src/bridge/WorldBridge.java`.
- Inv Walk ve Glow ESP: `src/bridge/ClientBridge.java`.

No Jump Delay politikası ve Fast Stop release-edge hesaplama motoru C++'ta
aynen korunur. Fast Place'ın gecikme ayarı, Sprint'in eşya kullanımı tercihi ve
Full Bright'ın 16.0 hedefi değişmez. UI, module ID, config, keybind, launcher,
lisans ve kalan modüller değişmez. Bu sekiz grubun diğer altı hedefi mevcut native yolu kullanır.
Bu aşama bütün modüllerin Java'ya taşındığı anlamına gelmez.

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
| 4 | [Left/Right Clicker](#dördüncü-grup-clickerlar) | Offline; oyun testi bekliyor |
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

Köprü aktifliği/kalıcı hata Windows debug çıktısına bir kez yazılır; arayüze
açıklama/status satırı eklenmez. Auth/odak/native UI koşulları hem capture hem
apply öncesi kontrol edilir. Kapatırken render callback'leri bitirildikten sonra
Java stop ve global ref temizliği yapılır; mevcut JVM session daha sonra kapanır.

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

### Dördüncü grup: clicker'lar

Left/Right Clicker'ın CPS, fatigue, jitter aralığı, ilk tıklama gecikmesi ve
kamera delta hesabı değiştirilmedi; C++'ta kalır. Java güncel world/player,
ölü/eşya kullanma/ekran durumunu ve Right Clicker'ın Only Blocks filtresini okur.
Her karar tek kullanımlık nonce ile yeniden doğrulanır; eşya/oturum değişimi veya
fiziksel tuş bırakma tıklamayı geçersizleştirir. Direct modda vanilla attack/use
Java üzerinden bir kez çağrılır. Kamera deltasını C++ seçer; Java yalnızca başarılı
tıklamaya bağlı tek kamera yazması yapar, pitch -90..90 ile sınırlıdır.

**Game Input taşıması bilinçli olarak mevcut ortak native input servisinde
korunur.** Java burada doğrulama ve Right Clicker cooldown hazırlığını yapar;
ikinci bir tıklama üretmez. Native servis fiziksel basışı tekrar denetleyip mevcut
Minecraft Mouse callback yolunu kullanır. Bu aşama input servisinin tamamının
Java'ya taşındığı anlamına gelmez. Left Clicker'ın `trigger_owns_attack` ve
`try_unowned_attack` koordinasyonu C++'ta aynen korunur; Triggerbot önceliği değişmez.

Inventory Clicker modülü ve ona ait envanter tıklama köprüsü kaldırılmıştır.

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

Kurulum/binding hatasında eski backend kullanılabilir. Runtime exception'ında
ilgili özellik askıya alınır; mümkün olan sahiplik cleanup'ı render thread'inde
denenir, mutasyon başka backend üzerinden tekrarlanmaz. Renderer cleanup sonrası
Java stop yalnızca referansları/durumu bırakır; oyuna yazmaz. Auth, launcher veya
combat/hotbar koordinatörleri yeniden yazılmadı.

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

Glow'un local değeri ve flag 6 ilk yazmadan önce ayrı saklanır. Yalnızca değişen
değerler sahiplenilir; kapanış/yeni bot kararı yalnızca bu değerleri geri alır.
Önceden true olan doğal glow silinmez, unload olmuş veya eski dünyaya ait entity'ye
gecikmiş cleanup yazılmaz. Aynı true değerini sonradan başka kaynak da yazarsa
bunun kaynağı ayırt edilemez; bu mekanizma bir sunucu sahiplik protokolü değildir.
Görsel modül menü/odak kaybında aktif kalabilir; auth/loader/unload yeni yazmayı
durdurur. Normal worker yolu pasiftir; disable yalnızca atomik iptal ister.

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

- JDK 17+ ile `scripts/Build-GameBridge.ps1`; sınıf bytecode hedefi Java 17'dir.
- MSBuild resource adımından önce kaynak değişmişse altı köprü yeniden derlenir.
  Java kodu/resources birlikte sürümlenir; ayrı launcher indirmesi gerekmez.
- `python scripts/Test-HybridBridge.py`: sahte 1.21.11 oyun nesneleriyle source
  ve gömülü class davranışı; ilk grubun 41, utility grubunun 34, item grubunun 36,
  clicker grubunun 27, envanter grubunun 32, container grubunun 31, world grubunun 35,
  client grubunun 31 kontrolü
  vardır. Çalışan Minecraft'a bağlanmaz, native DLL üretmez.
- Mevcut `no_jump_delay` ve `fast_stop_engine` offline testleri hesaplama
  davranışını doğrular. Oyun içi uyumluluk/performance ayrıca denenmelidir.
- `auto_tool_scoring` offline testi eski uygunluk/hız/epsilon kuralını korur.
- `gear_policy` offline testi eski zırh puanı, elytra koruması, slot eşleme ve
  ana envanter/hotbar önceliğini korur.
- `pot_refill_engine` offline kontrolü iksir motorunun mevcut kurallarını doğrular.
- `bridge_assist_logic` offline kontrolü geometri, yönlü tahmin, iki edge/üç
  safe örneği, manuel sneak ve bırakma gecikmesi kurallarını doğrular.
- Mevcut `inv_walk_engine` ve `anti_bot_engine` offline testleri ekran/hareket
  politikası ile ortak bot filtresi kurallarının korunduğunu doğrular.

Sonraki modüller aynı sınır üzerinden tek tek taşınabilir; combat hesaplamaları
veya ortak input/hotbar sahipliği bu sekiz grupta yeniden yazılmamıştır.

## Referans / kaynak bildirimi

Kullanıcının verdiği `6vb8h8/VapeV4.21-main` klasöründeki `README_EN.md`,
`LICENSE`, `native/README.md`, `NativeBridge.java` ve
`NativeBridgeLifecycleAdapter.java` incelendi. Yararlanılan fikir yalnızca
native kabuk ile Java oyun adaptörünü ayırmak ve açık bir kapatma sınırı kurmaktır.
Bu projedeki mevcut Sky/NameProtect loader yaklaşımı da kullanıldı.

Referans README'si bunun resmî kaynak değil bir recovery/decompile projesi olduğunu
ve CC0 bildiriminin üçüncü taraf haklarını kapsamadığını söyler. **Oradan Java/C++
kod, modül algoritması, auth, anti-analysis, native blob veya asset kopyalanmadı.**
Yeni GameBridge özgün OneStep uygulamasıdır; recovered kodun doğruluğu veya
lisanslanabilirliği hakkında garanti verilmez.

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
