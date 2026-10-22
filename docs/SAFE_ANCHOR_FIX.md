# Safe Anchor düzeltmesi

## İncelenen referanslar

- [Grizzly Auto Anchor / Safe anchor](https://grizzly.luka.onl/features/modules/combat/AutoAnchor/):
  oyuncu ile anchor arasına koruyucu glowstone koyma davranışı.
- [Meteor AnchorAura](https://github.com/MeteorDevelopment/meteor-client/blob/8d3d8c92a03da5509a9eafc4cc09b10473e6ec77/src/main/java/meteordevelopment/meteorclient/systems/modules/combat/AnchorAura.java):
  yerleştirme, doldurma ve patlatmanın ayrı işlemler olması; menzil/slot akışı.
- [minecraft-anchor-client SafeAnchorModule](https://github.com/godog122/minecraft-anchor-client/blob/350cf0b5132c8a4cfeaeecf28fc3a87286b262a6/src/main/java/com/anchormod/module/modules/SafeAnchorModule.java)
  incelendi; patlatma metodu yalnızca log yazan bir taslak olduğu için uygulama
  referansı olarak kullanılmadı. open-Respect kaynağına erişilemedi.

Kaynak kodu kopyalanmadı. Mevcut OneStep native motoru düzeltildi; Safe Anchor
Java'ya taşınmış değildir. Anchor Macro'nun Java akışı kullanılmaya devam eder.

## Eski akıştaki sorunlar

- Koruma bloğunu dolu anchor'ın üstüne koymak için anchor'a doğrudan tıklamak,
  koruma oluşmadan anchor'ı patlatabiliyordu.
- Başlangıçta dolu anchor seçilince koruma adımı atlanıyordu.
- Herhangi bir BlockItem (çiçek, slab, cobweb dahil) koruma malzemesi sayılıyor,
  üst hücrede herhangi bir hava dışı blok görmek yeterli kabul ediliyordu.
- Hareket halinde akış süresiz bekleyebiliyor; kapatmada slot işlemi modül
  worker'ından yapılabiliyordu. Ayarlar da render/config arasında ortak okunuyordu.

## Yeni akış

1. Sabit, yerde duran oyuncunun baktığı gerçek blok/anchor alınır; MISS kullanılmaz.
2. Anchor'a yatay komşu, oyuncunun alt gövdesi ile patlama merkezi arasında kalan
   bir koruma hücresi seçilir. Oyuncu kutusuyla çakışan yerler kullanılmaz.
3. Mevcut bir koruma varsa canlı blok ray'iyle kontrol edilir. Yoksa obsidyen,
   o yoksa glowstone kullanılarak yan zemine tıklanır. Koruma için anchor'a
   tıklanmaz. Slab/cobweb gibi rastgele eşya seçimi kaldırıldı.
4. Yeni anchor yerleştirilip görülür; sonra koruma koyulur ve görülmesi beklenir.
   Mevcut boş veya dolu anchor da aynı koruma kontrolünden geçer.
5. Boş anchor bir kez doldurulur, charge görüldükten sonra glowstone dışı
   slota geçilip bir kez patlatılır. Offhand glowstone varsa patlatma durdurulur.
6. Anchor kaybolduktan sonra hâlâ sahip olunan eski slot geri yüklenir ve modül
   kapanır. Tekrar çalıştırmak için tekrar etkinleştirilir.

Korumanın hâlâ arada bulunduğu, blok görüşü, gerçek oyun menzili, seçili eşya
ve güncel charge işlem öncesinde tekrar okunur. Anchor'ın görünen üst yüzü
tercih edilir; koruma tarafından kapanan yan yüz zorunlu tutulmaz.

Teyit başına 650 ms, zincir için 10 saniye sınırı vardır. Safe akışında teyitsiz
işlem tekrar gönderilmez; tick başına en fazla bir blok etkileşimi yapılır.
Hareket/menü/odak/oturum/manuel slot değişiminde bırakılır. Config snapshot ile
okunur; disable isteğindeki slot temizliği render thread'ine bırakılır.

Yeni varsayılan ayarda rastgele kamera hareketi kapalıdır. Eski ayar anahtarları
korunur. Panel mevcut adımı veya bekleme/iptal nedenini gösterir.

Alçak koruma bloğu patlama maruziyetini azaltır; sıfır hasar garantisi veya tam
hasar simülasyonu değildir. Oyun içinde kullanım testi henüz yapılmadı.

## Ateş bulunan yerleştirme alanı

Safe Anchor'ın anchor ve koruma bloğu hedefleri ile Anchor Macro Normal'ın
hedefi artık yalnızca `isAir()` ile değerlendirilmez. Vanilla'nın
`isReplaceable()` (`method_45474`; official `canBeReplaced`) kontrolü kullanılır.
Böylece ateş gibi yerine blok konabilen durumlar yerleştirmeyi engellemez.
Kontrol hem planlamada hem işlemden hemen önce yapılır. Korumanın gerçekten
dolu bir blok olması ve altında uygun zemin bulunması ayrı kontrollerdir.
