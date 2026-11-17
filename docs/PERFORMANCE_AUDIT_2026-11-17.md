# Uzun oyun oturumları için performans incelemesi

Tarih: 17 Kasım 2026

İlk inceleme turunda yalnız bu rapor hazırlandı; kaynak kodu, test kodu ve derleme ayarları değiştirilmedi. İnceleme, önceki iyileştirmeleri içeren çalışma ağacında yapıldı. Aşağıdaki ilk bulgular ve satır numaraları `265d450` durumunu anlatır. Ardından kullanıcının uygulama isteğiyle aşağıdaki değişiklikler yapıldı.

## Uygulama durumu — 17 Kasım 2026

| Alan | Uygulanan değişiklik | Korunan sınır / tercih |
| --- | --- | --- |
| Nametag taramaları | Scoreboard ve taşıyıcı keşfi 100 ms aralıkla; diğer turlarda oyuncu listesi. Ekipman verisi 100 ms cache; taşıyıcı eşleştirmede 2 blokluk konum hücreleri ve bir kez küçük harfe çevrilmiş metinler. | Konum/sağlık okumaları ve render kamera güncellemesi sık çalışmaya devam ediyor. Yakınlık şartları değiştirilmedi. Ortak worker aralığı aynı. |
| Nametag geri yükleme | Ayrılan oyuncuların kayıtları başarılı geri yüklemeden sonra siliniyor. Her keşif turunda en fazla 64 kayıt inceleniyor; dönen imleç başarısız kayıtlarda diğerlerini bekletmiyor. | Eksik/başarısız taramada toplu geri yükleme yapılmıyor. Sunucunun yeni takım ataması korunuyor; başarısız geri yükleme tekrar denenebiliyor. |
| Spotify kapanış | Normal kapatma isteği thread join beklemiyor. Tek worker yeniden kullanılıyor; nesil kontrolü eski sonuçları reddediyor. HTTP ve asenkron medya okumaları iptal/zaman aşımı kontrolüyle bekleniyor. Servisi başlatma modül yaşam döngüsünde; render tekrar başlatmıyor. DLL kapanışında worker join ediliyor. | Ağ fallback'leri tek 4 saniyelik bütçeyi paylaşıyor; bu sayı bütün DLL kapanışı için mutlak süre garantisi değildir. WinRT sağlayıcısının senkron çağrıları ve decode süreleri ayrıca ölçülmeli. |
| Player ESP | Adaptörün oyuncu listesi tercih ediliyor; gerekirse eski varlık koleksiyonu fallback'i var. Çıktı vektörü yeniden kullanılıyor; 2.048 öğeyi aşan kapasite kare sonunda bırakılıyor. İsimler kapalıysa isim metni okunmuyor. | 2.048 bir çizilecek oyuncu sınırı değildir. Kamera, interpolasyon, sağlık ve AntiBot güncel okunmaya devam ediyor. |
| Kozmetik çizimler | Kırpma parçaları, yüzey/UV, çizgi ve stroke verileri karelik PMR bellek havuzundan hazırlanıyor. Küçük parçalar aynı çağrı içinde yeniden kullanılabiliyor. | Render thread'i başına 256 KiB sabit tampon tutuluyor; taşan tahsisler çağrı sonunda bırakılıyor. Geometri, örtüşme ve mevcut parça sınırları değişmedi. |
| Spotify tekrar eden sorgular | Manager ve seçili oturum metadata'sı tutuluyor. Konum/oynatma her poll'de; metadata en az 1 saniye aralıklı fallback veya oturum/süre/geri sarma değişiminde yenileniyor. Söz dönüşünde parça tekrar doğrulanıyor. | DLL yaşam döngüsünü basit tutmak için olay callback'leri yerine sınırlı polling seçildi. Aynı süredeki bir parça değişimi sonraki metadata poll'üne kadar, normal koşullarda yaklaşık 1,2 saniye gecikebilir. Ağ/sağlayıcı gecikmeleri buna eklenebilir. |
| Sky yüklemesi | WIC decode tek arka plan worker'ına taşındı. Kuyruk en son seçimi tutuyor; eski sonuçlar eleniyor. Hazır görüntü render context'inde yükleniyor ve geçiş bundan sonra başlıyor. | Bir callback'te en fazla bir GPU upload; hazırlık sürerken mevcut panorama korunuyor. İlk panorama hazır değilken özel çizim bekliyor. GPU upload/mipmap maliyeti tamamen kaldırılmış değil. |

Yeni davranış testleri [visual_runtime_test.cpp](../tests/visual_runtime_test.cpp) ve [Test-VisualRuntime.py](../scripts/Test-VisualRuntime.py) içinde. İkinci script gerçek üretim fonksiyonlarını çıkarıp platform fixture'larıyla çalıştırır; Minecraft'a veya gerçek medya/ağ servisine bağlanmaz. Mevcut Triggerbot kodu ve Java köprüleri bu uygulama turunda değiştirilmedi.

Uygulama sonrası kontroller başarılı:

- Yeni runtime testi: konum hücrelerinin kaba kuvvet yakınlık kontrolüyle eşdeğerliği, 10.000 geri yükleme kaydı, başarısız kayıtta adil yeniden deneme, 10.000 hızlı seçimde eski decode sonucunun elenmesi ve worker kapanışı.
- Üretim fonksiyonlarını kullanan fixture'lar: geometri/UV/örtüşme, 1.000 karelik küçük geometri işinde PMR çalışma tamponundan ek heap tahsisi olmaması ve büyük tamponun bırakılması; sunucu takım değişikliğinin korunması; medya zaman aşımı/iptal, başarısız COM başlangıcından dönüş, 100 aç/kapat ve join ile kapanış. Aynı fixture'lar AddressSanitizer ve UndefinedBehaviorSanitizer ile de geçti.
- Spotify snapshot/texture, görsel animasyon, kozmetik fizik, oturum ve ayar sözleşmesi testleri.
- Triggerbot zamanlama ve yaşam döngüsü regresyonları; son hedef doğrulaması ve hedefe tekrar giriş davranışı korunuyor.
- Sky shader EGL derleme/çizim testi; Nametag, ESP ve kozmetik kaynaklarının 1.21.11 ve 26.1.2 hedefleriyle yerel C++ sözdizimi kontrolü; altyapı ve 8 depo hijyeni testi.

Windows DLL doğrulaması GitHub Actions üzerinden ayrıca yürütülür. Yerel platform fixture'ları gerçek Windows API/sürücü kabulü değildir. Bu uygulama, aşağıdaki ölçüm planının yerini tutmaz; gerçek oyun ortamında FPS/uzun oturum RAM/VRAM kazancı henüz ölçülmedi.

## Sonuç ve kapsam

Öncelikli adaylar Nametag'in ortak modül döngüsünde yaptığı tekrarlı taramalar, Spotify kapanışının bekleyen işlemlere bağlı olması ve ESP'nin her render karesinde tüm varlıkları dolaşmasıdır. Nametag'in geri yükleme kayıtları da aynı scoreboard uzun süre yaşarken büyüyebilir. Sky'da ilk panorama yükleme anı ayrıca ölçülmelidir.

Bu bir kaynak incelemesidir. Oyun içi CPU/GPU profili, saatler süren RAM/VRAM ölçümü veya yeni Windows DLL derlemesi yapılmadı. Aşağıdaki maliyetlerin kod yolları doğrulandı; FPS artışı, milisaniye kazancı veya gerçek ortamda bellek sızıntısı kanıtlandığı iddia edilmiyor. Sürekli işlem yükü, oturum boyunca birikme ve geçiş anındaki takılma ayrı değerlendirilmiştir.

İncelenen alanlar: modül döngüsü ve yaşam döngüsü; Nametag, Player ESP, kozmetik çizici, Sky, Spotify; Radar ve sınırlı görsel koleksiyonların temizlenmesi.

## Önceki turda zaten yapılanlar

| Değişiklik | Azaltılan maliyet / korunan davranış |
| --- | --- |
| Player ESP'nin ikinci, kullanılmayan worker taraması kaldırıldı. | Aynı verinin iki ayrı yoldan toplanması kaldırıldı; render tarafındaki güncel okuma sürüyor. |
| Spotify kapak ve söz verileri değişmez, paylaşılan tamponlara taşındı. | Snapshot kopyaları büyük vektörleri derin kopyalamıyor; küçük metin alanları hâlâ kopyalanabilir. |
| Spotify söz satırı araması ikili aramaya geçirildi. | Sıralı zaman damgalarında doğrusal arama yerine logaritmik arama kullanılıyor. |
| Spotify kapak texture nesnesi yeniden kullanılıyor ve render tarafında temizleniyor. | Şarkı değişimlerinde nesne silme/oluşturma döngüsü azaltıldı; yeni kapak için veri yüklemesi hâlâ gerekli. |
| Kozmetik çizgiler birleştirildi, ortak nokta tamponu kullanıldı. | Çizgi başına bazı tahsisler ve birleşim noktalarındaki gereksiz parlama azaltıldı. |

Bunlar bu raporda yeni öneri olarak tekrar sayılmadı. Önceki görsel/Sky geliştirmeleri, Triggerbot düzeltmeleri, clicker kaldırmaları ve kullanılmayan kod temizliği de yüklenen çalışma ağacının parçasıdır.

## Öncelik sırası

P1 ilk ele alınacak aday, P2 ikinci aşama anlamındadır; ölçülmüş hız sıralaması değildir.

| No | Öncelik | Alan | Etki türü | İlk ölçüm |
| --- | --- | --- | --- | --- |
| 1 | P1 | Nametag tarama sıklığı ve eşleştirme | Kalabalıkta sürekli CPU/JNI yükü | `run()` süresi, taranan varlık ve aday eşleşme sayısı |
| 2 | P1 | Spotify durdurma ve ağ beklemesi | Kapatma/geçiş anında ortak worker gecikmesi | Ağ kesikken kapatma ve unload süresi |
| 3 | P1 | Nametag geri yükleme kayıtları | Uzun oturumda kayıt birikmesi | Benzersiz oyuncu sayısına göre kayıt sayısı ve kapatma süresi |
| 4 | P2 | Player ESP varlık taraması | FPS ve toplam varlık sayısıyla artan CPU/JNI yükü | Render süresi ve kare başına JNI çağrısı |
| 5 | P2 | Kozmetik geçici vektörler | Tahsis ve kırpma maliyeti | Kare başına tahsis, kırpma parçası, CPU süresi |
| 6 | P2 | Spotify tekrar eden metadata sorgusu | Sabit arka plan işi | Aynı şarkıda WinRT sorgu sayısı ve worker CPU süresi |
| 7 | P2 | Sky ilk panorama yüklemesi | İlk geçişte kare süresi sıçraması | Soğuk/sıcak panorama geçişinin CPU/GPU süreleri |

## 1. Nametag: sık tarama ve iç içe eşleştirme

**Kanıt:** [runtime.cpp](../src/app/runtime.cpp), satır 29 ve 111–114; [module_loop.cpp](../src/app/module_loop.cpp), `tick()`; [nametag.cpp](../src/modules/visual/nametag.cpp), satır 392–624.

Ortak worker her turdan sonra 5 ms bekliyor. Nametag açıkken her turda kamera/scoreboard verisi okunuyor, varlıklar dolaşılıyor ve yeni snapshot vektörleri oluşturuluyor. Nametag'in bu yolunda ayrı bir tarama aralığı yok. 5 ms bekleme, iş süresi hariç teorik olarak yaklaşık 200 tur/saniye üst sınırı demektir; gerçek sıklık daha düşüktür.

Oyuncu ve ArmorStand/TextDisplay etiketlerini eşleştiren iki döngü, en kötü durumda oyuncu sayısı × taşıyıcı sayısı kadar karşılaştırma yapıyor. Küçük harfe çevirme de yakın adaylarda tekrar ediliyor. Kalabalık sunucuda bu iş diğer modüllerle aynı worker süresini tüketebilir.

**Öneri:** Konum güncellemesiyle isim/ekipman/scoreboard ve taşıyıcı keşfinin güncelleme sıklıklarını ayırmak. Değişmeyen metinleri yeniden kullanmak; taşıyıcıları konuma göre gruplandırıp yalnız yakın grupları eşleştirmek. Aralıkları sabit bir tahminle seçmek yerine yoğun sahnede ölçmek.

**Korunacaklar:** Sunucu etiketlerini doğru gizleme/geri yükleme, teleport ve dünya değişiminde geçersizleştirme, akıcı konum çizimi. Ortak 5 ms döngüyü tüm modüller için yavaşlatmak bu önerinin parçası değildir.

## 2. Spotify: kapatma sırasında bloklayan işlemler

**Kanıt:** [spotify_source.cpp](../src/modules/visual/spotify_source.cpp), satır 152–201, 694–705, 767–818 ve `refresh_lyrics()`; [spotify_hud.cpp](../src/modules/visual/spotify_hud.cpp), `on_disable()`; [module_runtime_state.hpp](../src/modules/core/module_runtime_state.hpp), yaşam döngüsü çağrıları.

`stop()` çalışan thread'i `join()` ile bekliyor. Uyandırma bildirimi bekleme süresini kesebiliyor fakat devam eden senkron WinHTTP isteğini veya WinRT `.get()` çağrısını iptal etmiyor. Söz sorguları birden fazla alternatif isteği art arda deneyebiliyor; aralarında durdurma kontrolü yok. HTTP aşama zaman aşımı 8 saniye; bu değer bütün kapatma işlemi için 8 saniyelik toplam sınır değildir.

Spotify `on_disable()` ortak modül döngüsünden çağrılabildiği için ağ gecikmesi diğer modüllerin işlenmesini de geciktirebilir. Bu, render thread'inin mutlaka tamamen durduğu anlamına gelmez.

**Öneri:** İptal edilebilir ağ/WinRT işlemleri, işlem genelinde süre bütçesi ve alternatif sorgular arasında iptal kontrolü. Eski şarkıya ait sonuçların yeni duruma yazılmasını önleyen istek nesli kontrolü. Ortak worker'ı bloklamayan durdurma tasarımı; DLL boşaltılmadan bütün işler ve callback'ler güvenle sonlanmalı. Thread'i ayırıp çalışır bırakmak çözüm değildir.

**Kabul senaryosu:** Çevrimdışı ağ, yavaş yanıt, hızlı şarkı değiştirme, sözleri açıp kapatma, modülü kapatma ve DLL unload. Hem gecikme hem de kapanış sonrası erişim/callback bulunmaması doğrulanmalı.

## 3. Nametag: oturum boyunca geri yükleme kaydı birikmesi

**Kanıt:** [nametag.cpp](../src/modules/visual/nametag.cpp), satır 385–386, 430–437 ve 633–685.

`original_team_by_player`, gizlenen oyuncuların eski takım bilgisini tutuyor. Kayıtlar scoreboard kimliği değişince veya modül kapanınca temizleniyor; oyuncu bazında ayrılma temizliği görünmüyor. Aynı scoreboard ile çok sayıda farklı oyuncu görülen uzun oturumda kayıt sayısı ve kapanışta yapılan geri yükleme işi büyüyebilir.

Bu veri işlevsel bir geri yükleme kaydıdır; sınırsız bir bellek sızıntısı kanıtı değildir. Yalnız kapasite sınırı koyup eski kayıtları silmek, takım bilgisinin geri yüklenmesini bozabilir.

**Öneri:** Oyuncu ayrılmasını takip etmek; kaydı bırakmadan önce gerekli scoreboard geri yüklemesini güvenle tamamlamak. Oturum/kimlik sınırlarını açık tutmak. Temizliği küçük parçalara bölmek ancak geri yükleme garantisini korumak.

**Kabul senaryosu:** Aynı sunucuda çok sayıda oyuncu giriş/çıkışı, takım değişikliği ve modül aç/kapat. Kayıt sayısının tamamlanmış geri yüklemelerden sonra azalması ve eski takım/etiket durumunun korunması kontrol edilmeli.

## 4. Player ESP: her karede tüm varlıkları dolaşma

**Kanıt:** [player_esp.cpp](../src/modules/visual/player_esp.cpp), `read_render_players()`, satır 73–127.

Render okuyucusu `world.getEntities()` üzerinden tüm varlıkları dolaşıp oyuncuları seçiyor. Çok sayıda eşya/mob bulunan sahnede, gösterilen oyuncu sayısı az olsa bile eleme işi büyüyor. Oyuncu isimleri de her okumada Java'dan metne çevriliyor. Önceki turda kaldırılan worker taraması bu render işini ortadan kaldırmadı.

**Öneri:** Desteklenen sürümlerde adaptörün oyuncu koleksiyonunu kullanmak; gerekli sürümlerde doğru fallback bırakmak. İsim gibi daha seyrek değişen veriler için dünya/oyuncu kimliğiyle sınırlı cache değerlendirmek. Geçici çıktı kapasitesini kontrollü yeniden kullanmak.

**Korunacaklar:** Güncel kamera ve interpolasyon, AntiBot elemesi, sağlık değişiklikleri, oyuncu adı değişikliği ve kimlik yeniden kullanımı. Render optimizasyonundan combat hedef verisi için eskimiş cache türetilmemeli.

## 5. Kozmetik çizici: geçici tahsisler ve kırpma

**Kanıt:** [cosmetic_renderer.cpp](../src/modules/visual/cosmetic_renderer.cpp), `unoccluded_texture()`, `unoccluded()`, `unoccluded_edge()` ve satır 378–433 civarındaki çizim hazırlığı.

Yüzeyler, çizgiler, köşeler, UV'ler ve kırpılmış parçalar için iç içe geçici vektörler kuruluyor. `reserve()` aynı vektörün büyümesini azaltıyor; her çağrıda yeniden oluşturulan yerel vektörün kapasitesini sonraki kareye taşımıyor. Yoğun kozmetik ve çok sayıda örtücü geometri olduğunda tahsis/kırpma maliyeti ölçülmeye değer.

**Öneri:** Önce tahsis profili almak; sıcak yollarda render thread'ine ait yeniden kullanılabilir çalışma tamponu veya karelik arena değerlendirmek. Tutulan kapasiteyi üst sınır ve yaşam döngüsü temizliğiyle yönetmek. Aşırı büyük tek sahnenin belleğini sürekli elde tutmamak.

**Korunacaklar:** Derinlik/örtüşme doğruluğu, parçaların ömrü, görsel yoğunluk ve Reduce Motion davranışı. Mevcut polygon/kenar parçası sınırları bu yolun zaten bazı korumaları olduğunu gösterir; sınırsız parçalanma iddiası yapılmıyor.

## 6. Spotify: aynı şarkıda metadata sorgusunu tekrar etme

**Kanıt:** [spotify_source.cpp](../src/modules/visual/spotify_source.cpp), satır 31, 801 ve 816–879.

Worker her poll sonrasında 400 ms bekliyor. `poll()` her çalışmada session manager'ı `RequestAsync().get()` ile alıyor; oturum bulma ve `TryGetMediaPropertiesAsync().get()` işlemlerini tekrar yapıyor. Şarkı aynıyken bu işin önemli bölümü değişmeyen bilgiyi yeniden elde ediyor. İş süresi nedeniyle gerçek poll aralığı 400 ms'den uzundur.

**Öneri:** Manager ve seçili oturumun yaşam döngüsünü korumak; parça/oynatma değişikliklerini olaylarla izlemek ve düşük sıklıklı fallback sorgusu bırakmak. Konum ilerletmesini metadata yenilemesinden ayırmak. Olay abonelikleri kapanışta kaldırılmalı; callback/thread sahipliği açık olmalı.

**Kabul senaryosu:** Aynı parçanın uzun süre çalması, pause/resume, seek, Spotify yeniden açılışı, farklı medya uygulamasına geçiş. Eski kapak/söz veya kaybolmuş oynatma durumu oluşmamalı.

## 7. Sky: ilk kullanımda render yolunda decode ve upload

**Kanıt:** [sky_panorama.cpp](../src/modules/visual/sky_panorama.cpp), `load_texture()`, satır 133–181; texture tablosu, satır 268; ilk yükleme, satır 496–497; render kapanış temizliği.

İlk kez seçilen panorama render yolunda WIC ile çözülüyor, RGBA tamponu oluşturuluyor, `glTexImage2D` ile yükleniyor ve mipmap üretiliyor. Sonraki kullanımlar mevcut texture'ı kullanıyor. Dolayısıyla aday sorun sürekli yeniden yükleme değil, ilk seçim/geçiş karesinin pahalı olmasıdır. Altı elemanlı texture tablosu sınırlıdır; bu yapıya dayanarak sınırsız VRAM sızıntısı denemez.

**Öneri:** CPU decode işini uygun COM yaşam döngüsüyle önceden hazırlamak; GPU yüklemesini context sahibi render thread'inde kontrollü gerçekleştirmek. Yeni panorama hazır olana kadar mevcut panoramayı çizmek ve geçişi hazır olduktan sonra başlatmak. Bütün panoramaları tek karede yüklemekten kaçınmak; decode tamponları ve iptal edilen hazırlıklar temizlenmeli.

GPU ölçümü yüksek maliyet gösterirse sis/aurora shader'ını ayrıca incelemek. Kaynakta shader bulunması tek başına GPU darboğazı kanıtı değildir. Çözünürlüğü veya animasyon kalitesini doğrudan azaltmak ilk öneri değildir.

**Kabul senaryosu:** Altı panoramayı ilk kez ve tekrar seçmek, geçiş sırasında yeniden seçim, farklı çözünürlükler ve GPU'lar, modül aç/kapat ve unload. Sis, arazi derinliği ve OpenGL state geri yüklemesi korunmalı.

## Yanlış pozitiflerden kaçınma

- Nametag'in `suppressed_carriers` listesi etkin olmayan taşıyıcıları geri yükleyip JNI global referanslarını siliyor. Bu liste, yukarıdaki takım geri yükleme haritasıyla aynı yaşam döngüsüne sahip değil.
- Radar kaybolan işaretçileri temizliyor; kozmetik parçacıklarda sayı/ömür sınırları var. Sadece bir koleksiyon görmek sızıntı bulgusu sayılmadı.
- Spotify HTTP cevabında 2 MiB sınırı var. Sorun sınırsız cevap birikmesi olarak değil, iptal ve toplam bekleme süresi olarak değerlendirildi.
- OpenGL state koruma çağrıları, JNI sahipliği ve oturum doğrulamalarını sırf çağrı sayısını azaltmak için kaldırmak önerilmiyor.

## Ölçüm ve uygulama planı

1. Aynı Minecraft sürümü, JVM, çözünürlük, modül ayarları ve sahneyle başlangıç kaydı almak. Isınma dönemini uzun oturumdan ayırmak.
2. Boş sahne, kalabalık oyuncu sahnesi, oyuncusu az ama varlığı fazla sahne ve yoğun kozmetik sahnesini ayrı ölçmek. Modülleri tek tek ve birlikte açmak.
3. Kare süresi p50/p95/p99, en uzun takılmalar, modül CPU süreleri, JNI/tahsis sayıları, process private bytes/commit, VRAM ve ilgili koleksiyon boyutlarını izlemek. JVM belleği ile DLL tarafındaki yerel belleği ayırmak.
4. Önerilen uzun oturum kontrol noktaları: ısınma sonrası, 30, 60 ve 120 dakika. Dünya/sunucu değişimi ve modül kapatma sonrası kalan kaynakları ayrıca kaydetmek. Bunlar yapılmış ölçümler değil, test planıdır.
5. Önce P1 adaylarını birer birer uygulamak ve aynı senaryoda karşılaştırmak. Sonra ESP, kozmetik tahsisleri, Spotify olay modeli ve Sky ön hazırlığını değerlendirmek.
6. Triggerbot'un mevcut son hedef doğrulamalarını ve hedeften çıkıp girince korunan zamanlama durumunu değiştirmemek. Görsel performans uğruna bu kontroller seyreltilmemeli; regresyon testleri korunmalı.

Başarı ölçütü yalnız ortalama FPS değildir: uzun oturumda bellek eğiliminin kararlı olması, kapatma gecikmesinin kontrol altında tutulması ve p95/p99 kare sürelerinin aynı görüntü kalitesiyle iyileşmesi birlikte değerlendirilmelidir.

## İlk incelemenin doğrulama kaydı

Önceki turdaki hedefli Spotify snapshot testi; 100.000 paylaşılan snapshot, söz zamanlaması eşdeğerliği ve 10.000 payload değişiminde geri kazanımı doğruladı. Sahte OpenGL kullanan texture testi 100.000 kare ve 10.000 kapak değişiminde nesne yeniden kullanımı, state geri yükleme ve temizleme/yeniden denemeyi doğruladı. Bunlar gerçek sürücü veya saatlerce oyun testi değildir.

Bu rapor için yeniden çalıştırılan belge/depo kontrolleri başarılı: altyapı doğrulaması 55 modül ve 128 derleme girdisini kontrol etti, depo hijyeni testlerinin 8'i geçti ve diff boşluk denetimi hata vermedi.

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
git diff --check
```

İlk, yalnız dokümantasyon turunda kaydedilen 1.050 dosyanın SHA-256 değerleri karşılaştırılmıştı: o turda mevcut dosyalarda değişiklik veya silme yoktu; tek yeni depo dosyası rapordu. Sonraki uygulama turunun değişiklikleri ve kontrolleri üstteki uygulama durumunda kayıtlıdır.
