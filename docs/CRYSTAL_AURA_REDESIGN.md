# Crystal Aura yeniden tasarımı

İnceleme tarihi: 22 Ekim 2026. Aşağıdaki yedi açık kaynak uygulamanın kodu
incelendi. Bu liste internetin tamamının tarandığı anlamına gelmez; erişilebilen
farklı mimariler karşılaştırıldı. Kaynakların kodu OneStep'e kopyalanmadı;
OneStep'in Java köprüsüne uygun bir denetleyici bağımsız olarak yazıldı.

| Kaynak | İncelenen düzen | OneStep için karar |
| --- | --- | --- |
| [Meteor CrystalAura](https://github.com/MeteorDevelopment/meteor-client/blob/8d3d8c92a03da5509a9eafc4cc09b10473e6ec77/src/main/java/meteordevelopment/meteorclient/systems/modules/combat/CrystalAura.java) | Ayrı place/break zamanlayıcıları, hasar puanlaması, isteğe bağlı support, offhand. | Temel davranış modeli. |
| [LiquidBounce CrystalPlacer](https://github.com/CCBlueX/LiquidBounce/blob/f2e592106c5c7bff4489fc879828fcc6cf9a3889/src/main/kotlin/net/ccbluex/liquidbounce/features/module/modules/combat/crystalaura/place/SubmoduleCrystalPlacer.kt) ve [Destroyer](https://github.com/CCBlueX/LiquidBounce/blob/f2e592106c5c7bff4489fc879828fcc6cf9a3889/src/main/kotlin/net/ccbluex/liquidbounce/features/module/modules/combat/crystalaura/destroy/SubmoduleCrystalDestroyer.kt) | Yer seçimi, dönüş ve son doğrulama ayrı; farklı kristal yüzeylerinden nişan. | Aday seçimi ile atomik son etkileşim ayrıldı. |
| [ThunderHack AutoCrystal](https://github.com/Pan4ur/ThunderHack-Recode/blob/49c76bcc5a5e4fbd12b06305c09f0c8fb76c510b/src/main/java/thunder/hack/features/modules/combat/AutoCrystal.java) | Place/break menzilleri, hasar sınırları, spawn takibi ve deneme sınırları. | Bağımsız menzil ve zamanlama, süreli teyit ve sınırlı tekrar. |
| [BlackOut AutoCrystalPlus, 1.21.11](https://github.com/HYPE115/BlackOut-Continuation/blob/f5e58af2352663a39ff1fa3f2de7870aeda033d0/src/main/java/kassuk/addon/blackout/modules/AutoCrystalPlus.java) | Hedef/kendi/arkadaş hasarı, ayrı yerleştirme ve patlatma değerlendirmesi. | Ortak friend servisiyle patlama çevresindeki arkadaşları da kontrol et. |
| [Wurst CrystalAura](https://github.com/Wurst-Imperium/Wurst7/blob/c040d7accda6d270707ac257f906d7387a495af3/src/main/java/net/wurstclient/hacks/CrystalAuraHack.java) | Mevcut kristalleri önce kır; sonra uygun taban bulup yerleştir. | Kılıç ve oyuncuyu havalandırma şartını kaldır. |
| [BleachHack CrystalAura](https://github.com/BleachDev/BleachHack/blob/2a30d34be73aa62484bb1ede97a7f657044fe333/src/main/java/org/bleachhack/module/mods/CrystalAura.java) | Hasar oranı, yerleştirme kara listesi, modern/1.12 yerleştirme farkı. | Reddedilen alanlara kısa geri çekilme; modern sürümlerde ikinci hava bloğu şartı yok. |
| [Argon AutoCrystal](https://github.com/LvStrnggg/Argon/blob/738e75f0623581ffbbc116656835352f8df9eb22/src/main/java/dev/lvstrng/argon/module/modules/combat/AutoCrystal.java) | Bakılan blok/kristale bağlı yardımcı, ayrı sayaçlar. | Hit Crystal için uygun; tam otomatik alan/hasar seçimine tek başına uygun değil. |

## Önceki döngünün sorunu

Eski canlı modül kılıç → havalanma → obsidyen → kristal → kırma zincirini
zorunlu tutuyordu. Kılıç olmaması, havalanmanın gerçekleşmemesi veya mevcut
obsidyen varken hotbar'da obsidyen bulunmaması modülü durdurabiliyordu.
Ayrıca modern yerleştirmede gerekli olmayan ikinci hava bloğunu şart koşuyordu.
Bu davranış, genel Crystal Aura yerine belirli bir vuruş kombosuna benziyordu.
Önceki Java geçişi bu tasarımı korumuştu; bu revizyon canlı tasarımı değiştirir.

## Yeni canlı davranış

- Java client tick'inde hedefi ve yakın kristalleri bulur; normal durumda kılıç
  veya vanilla melee cooldown beklemez. Weakness altında yeterli Strength yoksa
  kılıç/balta seçimi yapılabilir; Triggerbot denetleyicisine bağımlı değildir.
- Hazır bir kristal kırılabiliyorsa önce onu değerlendirir. Place ve break
  gecikmeleri ayrıdır. Bir oyun tick'inde en fazla bir etkileşim gönderilir.
- Hotbar veya offhand kristalleri kullanılır. Mevcut obsidyen/bedrock üzerinde
  yerleştirmek için hotbar obsidyeni gerekmez. Support açıkken gerekirse
  obsidyen taban yerleştirilip dünyada görülmesi beklenir.
- Adaylar tahmini hedef hasarı ve kendi hasarına göre sıralanır. Blok görüşü,
  entity boşluğu, gerçek oyun menzili ve seçili eşya işlem anında tekrar okunur.
  Visible rotation bir normal oyuncu tick'i boyunca uygulanmadan işlem yapılmaz.
- Hareketli hedef daha yüksek puanlı başka bir tabanı/kristali öne çıkarsa da
  nişan alınan işlem geçerli kaldığı sürece tamamlanır. Böylece her tick aday
  değiştirip nişan beklemesini sürekli yeniden başlatmaz. Menzil, görüş,
  yerleştirme boşluğu ve güncel hasar sınırları her tick tekrar kontrol edilir;
  geçersiz aday bırakılıp yeni aday seçilir.
- Modern EndCrystalItem koşulu uygulanır: ilk hücre hava, entity hacmi 1×2×1.
  Üstte ikinci hava bloğu şartı bulunmaz. Taban etkileşimi için görülen üst yüz
  kullanılır. Kristal kırarken yakın görünen nokta, merkez veya alt/üst nokta
  denenir; yalnızca kristalin merkezine bakılabilmesi zorunlu değildir.
- Sunucudan base/spawn/removal teyidi beklenir. Yerleştirme reddi/timeout ilgili
  tabana 500 ms geri çekilme uygular; bir kristal kimliği için en fazla üç
  onaysız kırma denemesi vardır. Aynı ID'nin başka entity'ye atanması bekleyen
  saldırıyı yeni nesneye yönlendirmez.
- Menü, item use, odak/izin kaybı, kamera değişimi, oturum değişimi, disable ve
  manuel slot değişimi iptal eder. Slot yalnızca aynı oturumda hâlâ sahip
  olunan seçim için ve Restore Slot açıksa geri yüklenir.
- Triggerbot önceliği ve clicker'ların ortak combat lock'u korunur. Oyun
  sorguları, hasar hesabı, dönüşler ve vanilla işlemler Java'dadır.

## Hasar tahmini ve maliyet sınırları

12 blok etki yarıçapı, kutu üzerinden blok ray örneklemesi, zorluk, zırh,
armor toughness, Resistance, Protection/Blast Protection ve absorption ile
anti-suicide kontrolü kullanılır. Hesap yalnızca okur; zırh hasarlayan vanilla
damage/armor mutator'ları tahmin için çağrılmaz. Creative/invulnerable oyuncu
hasarı sıfır kabul edilir. Self / Friend Protection aynı eşiği yakın arkadaşlar
için de uygular. Oyun modları veya özel sunucu hasar kurallarıyla tam eşitlik
iddiası yoktur; arayüz değerleri tahmin olarak gösterir.

Taban adayları dört tick'e kadar önbelleklenir; oyuncu hareketi ve hedef değişimi
önbelleği geçersiz kılar. Tick başına en fazla 16 geçerli yerleştirme adayına ve
32 kırma adayına ayrıntılı hasar hesabı yapılır. Olağandışı büyüklükte entity
kutuları için ray örneklemesi sınırlandırılır. Bu, bütün küredeki matematiksel
global optimumu bulma garantisi vermez. Son işlem öncesinde canlı dünya ve
hasar sınırları tekrar kontrol edilir.

## Ayarlar ve tanılama

Targets, Place/Break Crystals, Auto Switch, Restore Slot, Place Obsidian Support,
Target/Place/Break Range, Place/Break Delay, Self / Friend Protection,
Minimum Target Damage, Maximum Self Damage, Slot Switch Delay ve Result Timeout.
Eski action_delay_ms, mob_ground_test ve launch_timeout_ms config anahtarları
okunabilir kalır; eski kılıç/havalanma zincirini etkinleştirmez.

Durum alanı eksik kristal, geçersiz taban, manuel slot molası, yetersiz hedef
hasarı, self/friend limiti, Weakness, sunucu teyidi, timeout ve Java hatasını
ayrı gösterir. Normal default self-damage kontrolü çıplak survival oyuncusunda
yakın patlamaları engelleyebilir; bu durumda arayüz bunun nedenini yazar.

## Doğrulama

- Test-CrystalAura.py gerçek native isim listesini tekilleştirmeden Java'ya
  verir; canlı denetleyiciyi izole oyun/tick örnekleriyle hem kaynak hem
  gömülü sınıftan çalıştırır. Diğer köprülerin kontrolleri Test-HybridBridge.py'dedir.
- Test-CrystalAuraBindings.py, Dawn cache'indeki gerçek 1.21.11 client JAR ve
  Fabric intermediary haritasını okuyarak kullanılan alan/metot tiplerini,
  inheritance ve static durumunu kontrol eder. Oyun başlatmaz/değiştirmez.
- Hareketli mobun her tick en iyi tabanı değiştirdiği tekrar testi ve gerçek
  slot/place/break sürelerini koruyan uzun döngü testi vardır. Bekleyen tabanın
  kapanması, self damage sınırının değişmesi ve hareketle kristal sıralamasının
  değişmesi de sınanır. Sahte kristaller gerçek oyundaki gibi farklı ID alır.
- Gerçek Dawn oturumunda kullanım testi yapılmış değildir. Ağ gecikmesi,
  özel server kuralları ve mod etkileşimleri bu offline kontrollerin dışındadır.
