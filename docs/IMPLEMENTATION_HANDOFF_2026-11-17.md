# OneStep: sonraki geliştirmeler için uygulama ve devir rehberi

Tarih: 17 Kasım 2026. Bu dosya inceleme ve iş tarifidir; bu turda uygulama kodu değiştirilmedi.

## Sonraki teslim — 18 Kasım 2026

R01–R07 kod düzeltmeleri `b4704dc0016d1a09501b692ac113259e3e9780e5` kaynak commit'inden Fabric 1.21.11 Windows x64 olarak başarıyla derlendi. [Run](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44279607803), [artifact](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44279607803/artifacts/14415654916). Ayrıntılar ve açık gerçek oyun kabulü: [18 Kasım yeniden inceleme/uygulama kaydı](CLIENT_RECHECK_2026-11-18.md). Aşağıdaki 17 Kasım teslimi önceki sürümdür.

## Son doğrulanan teslim

Fabric 1.21.11 Windows x64 DLL, `4d665ac7ab2669d30615e79d725e6496e83aef3d` kaynak commit'inden başarıyla derlendi. [Run](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44198194449), [artifact](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44198194449/artifacts/14380169548). Önceki `3174c0d` kaydı tarihsel bilgidir. Yeni DLL Insert ve sonraki yerel düzeltmeleri içerir. Oyun kabulü açık maddeler için son denetimin üst tablosunu kullan.

## Güncel yönlendirme

Aşağıdaki A–H tarifleri düzeltme öncesi bulgular ve uygulama tasarımının tarihsel kaydıdır. Güncel durum için bu üst bölüm ile [son denetimin uygulama tablosunu](CLIENT_CONSISTENCY_AUDIT_2026-11-17.md) kullan; alt paragraflardaki eski “mevcut durum” cümleleri tek başına açık iş listesi değildir.

| İş | Güncel durum |
| --- | --- |
| A/B/C/F | Kodlandı ve yerel fixture testleri geçti. CI'ya gerçek EGL shader ve 1.21.11 hedef seçimi de eklendi. |
| D/E | Nametag worker/JNI cleanup, taşıyıcı hatalarında kayıt koruma ve cleanup bekleme görünürlüğü kodlandı; gerçek JVM fixture testi geçti. |
| G | Insert/NumPad ve ayrılmış tuş politikası test edildi; gerçek Windows oyun kabulü açık. |
| H | Gerçek uzun oturum ölçümü açık. |
| Ek denetim | C03 render durması, C09 gerçek Haste, C10 aynı-değer sunucu glow sahipliği ve C12 oyun ölçümleri açık; ayrıntılar yeni raporda. |

### Sonraki uygulama turu — ilerleme

- **A uygulandı:** hedefli 1.21.11 workflow'una Ubuntu test job'ı ve Windows derlemesi için `needs` bağımlılığı eklendi. Güncel workflow çalıştırması ve Windows derlemesi başarılı; üstteki teslim kaydını kullan.
- **B uygulandı:** snapshot gecikme ilerlemesi 5 saniyede sabitleniyor; başlığa bağlı statik filtre kaldırıldı. Kontrollü zaman testi eklendi. Gerçek medya oturumunda seek ayrıca doğrulanmalı.
- **C uygulandı:** COM/thread başlangıç hatalarında 250 ms–5 s arası artan tekrar deneme gecikmesi var; gerçek üretim yaşam döngüsü fixture testi geçti. Gerçek Windows hata senaryosu ayrıca doğrulanmalı.
- **F uygulandı:** 18 Sky uniform konumu shader programı linklenince alınıyor; kapatmada sıfırlanıyor. EGL shader testi ve gerçek `initialize()` kodunu kullanan fake GL testi geçti: hatalı link, 1.000 karede yeniden arama olmaması ve yeni programda yeniden arama kontrol edildi. Gerçek GPU performansı ölçülmedi.
- **D/E uygulandı, oyun kabulü açık:** Worker kapanış hook'u, Nametag'e ait JNI kaynaklarını dünya geçersizken de bırakmayı deniyor. Başarısız takım geri yüklemeleri modül kapalıyken aynı scoreboard üzerinde bütçeli tekrar deneniyor; dünya değişimi ve nihai unload ayrı ele alınıyor. Üretim cleanup fonksiyonu hem platform fixture'ında hem de `-Xcheck:jni` etkin gerçek JVM'de test edildi. JVM testinde scoreboard işlemleri fixture'dır; gerçek Minecraft scoreboard'u test edilmedi. JNI erişimi yoksa referansların bırakıldığı iddia edilmiyor.
- **G kısmen doğrulandı:** üretim input servisi ve odak politikası birlikte test edilerek Insert / NumPad 0 ve odak sıfırlaması kontrol edildi. Gerçek oyun kabulü açık.
- **H açık:** uzun oturum performans ölçümü yapılmadı.
- **Triggerbot:** oyun tarafından bildirilen saldırı cooldown değeri kullanılıyor. Fixture API eşik değerini test eder; etkisiz Haste seviyesi alanı kaldırıldı. Gerçek oyun doğrulaması yapılmadı.

## 1. Bu belgeyi kullanacak modele talimat

Bu depoda görev yapacak başka bir model için yazılmıştır. Kullanıcı uygulama istediğinde aşağıdaki işleri sırayla ele al. Her seferinde yalnız bir işi tamamla. Önce mevcut davranışı gösteren testi veya ölçümü oluştur, sonra küçük bir düzeltme yap ve aynı kontrolü tekrar çalıştır. Bir görev başarısızken sonraki göreve geçme.

Bu belge tek başına kod değiştirme, GitHub'a yükleme veya DLL yayımlama talimatı değildir. Kullanıcının o oturumdaki isteği kapsamı belirler. Çalışma alanı farklıysa önce mevcut kodun aşağıdaki sembollerle eşleştiğini kontrol et. Eşleşmiyorsa eski satır numaralarına göre değişiklik yapma.

İnceleme statiktir. Burada yazan tüm belirtilerin gerçek Minecraft oturumunda gözlendiği iddia edilmiyor. “Doğrulanmış kod yolu” kaynakta görülen koşulu; “aday” ise test veya ölçümle doğrulanması gereken sonucu ifade eder.

### Başlangıç durumunu doğru anla

- Son doğrulanan Windows DLL kaynak commit'i: `3174c0d4660e0c8f3f028e2149840feb4f570188`.
- Derleme: [Fabric 1.21.11 Windows x64](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44122970337). Bu, diğer Minecraft sürümleri için DLL doğrulaması değildir.
- Çalışma alanında bu commit'ten sonra ClickGUI'nin Right Shift'ten Insert'e taşınması ve ilgili test güncellemeleri var. Önceki DLL bunları içermiyor.
- Önceki çalışma dalı: `codex/build-sky-20261116165819`. Yerel `main`/index, uzaktaki çalışma dalının son ağacıyla aynı olmayabilir. Çok sayıda değiştirilmiş ve izlenmeyen dosya önceki işlere aittir.
- `git reset --hard`, `git clean -fd` ve toplu dosya geri yükleme kullanma. “Yeni dosya” veya “dirty” görülmesi onun gereksiz olduğu anlamına gelmez.
- Sadece `git diff` okumak izlenmeyen dosyaları göstermez. `git status --short` ve yeni dosyaların içeriklerini de oku.

### Korunacak davranışlar

1. Triggerbot'un son hedef doğrulaması, cooldown/reaction durumu ve hedeften çıkıp tekrar girince korunan zamanlaması değişmeyecek. Performans için bu kontrolleri seyrekleştirme.
2. Ortak modül worker'ının 5 ms beklemesini bütün modülleri etkileyen bir “FPS ayarı” olarak değiştirme.
3. Insert menü tuşu olarak kalacak. Right Shift normal modül atamalarında kullanılabilecek. NumLock kapalı NumPad 0, fiziksel Insert ile karışmayacak.
4. 55 stable module ID, config anahtarları ve sürüm uyumluluk sözleşmeleri korunacak.
5. OpenGL kaynakları sahibi olan render context'inde oluşturulacak/silinecek. Arka plan Sky worker'ına GL çağrısı ekleme.
6. JNI global referanslarını canlı ve bağlı JNI ortamında bırak. Yerel referansları başka thread'e veya sonraki frame'e taşıma.
7. Worker thread'lerini `detach()` ile bırakma. DLL boşaltılmadan yürütülen işler sonlanmış olmalı.
8. Görsel kaliteyi, çizilen oyuncu sayısını veya parçacık miktarını düşürerek ölçülmemiş performans kazancı ilan etme.

## 2. Zaten tamamlanmış işler — tekrar uygulama

Önce [önceki performans raporunun uygulama durumunu](PERFORMANCE_AUDIT_2026-11-17.md) oku.

- ESP'nin ikinci worker taraması kaldırıldı; oyuncu koleksiyonu tercih ediliyor ve render tamponu kapanışta bırakılıyor.
- Nametag'de 100 ms keşif/ekipman aralığı, konum hücreleri ve ayrılan oyuncular için bütçeli geri yükleme var.
- Spotify büyük snapshot verilerini paylaşıyor, söz satırını ikili arıyor, texture nesnesini yeniden kullanıyor.
- Spotify normal kapatmada join beklemiyor; son kapanışta join var. HTTP istekleri iptal edilebilir ve fallback'ler ortak süre bütçesi kullanıyor.
- Sky decode arka planda, GPU upload render tarafında; kuyruk eski istekleri biriktirmiyor.
- Kozmetik kırpma/çizim hazırlığında PMR çalışma belleği var. Büyük tampon TLS'de tutulmuyor; DLL başına sabit 256 KiB alan kullanılıyor.

Bu alanları yeniden tasarlamadan önce aşağıdaki daha dar eksikleri ele al.

## 3. İlk incelemenin öncelik ve bağımlılıkları — tarihsel

| Sıra | İş | Öncelik | Kanıt / durum | Zorluk |
| --- | --- | --- | --- | --- |
| A | Hedefli DLL workflow'una test kapısı eklemek | P1 | Hedefli workflow ile genel workflow'un test kapsamı farklı | Orta |
| B | Spotify snapshot zamanını ve parça kimliğini düzeltmek | P1 | 5 saniyelik sınırda süre eklemesi tamamen kesiliyor; kimlik yalnız başlık | Orta |
| C | COM/thread başlangıç hatasına kontrollü tekrar deneme | P2 | Hata sonrası çalışan bayrağı kapanıyor; modül tekrar başlatmayı sık deneyebilir | Orta |
| D | Geçersiz dünya ile kapanışta yerel/JNI temizliğini ayırmak | P1 | `clear_modules()` geçersiz cache'te `on_disable()` çağrısını atlıyor | Yüksek |
| E | Nametag kapanışında başarısız geri yüklemeyi kaybetmemek | P1 | `restore_team()` sonucu kullanılmadan takım/kayıt temizleniyor | Yüksek; D ile ilişkili |
| F | Sky uniform konumlarını program başına saklamak | P3 | Render yolunda aynı uniform isimleri tekrar aranıyor | Düşük–orta; kazanç ölçülmeli |
| G | Insert için gerçek giriş ve odak kabul testi | P1 doğrulama | Yerel testler geçti; gerçek oyun/hook davranışı ayrıca doğrulanmalı | Orta |
| H | Uzun oturum ölçümü ve kaynak sayaçları | P2 | Gerçek FPS/RAM/VRAM kazancı henüz ölçülmedi | Orta |

Önerilen yürütme: **A → B → C → D → E → F → G → H**. G'nin gerçek Windows kontrolünü ortam uygunsa erkenden de yap. D ve E'yi aynı büyük yama içinde karıştırma; önce temizliğin kime ait olduğunu belirle. F düşük önceliklidir; ölçüm kazanç göstermiyorsa değişikliği büyütme.

## 4. Her işe başlamadan önce ortak iş akışı

1. Depo kökünde `README.md`, varsa `AGENTS.md`, bu belge ve önceki raporun uygulama bölümünü oku.
2. `git status --short`, `git branch --show-current`, `git log -3 --oneline`, `git remote -v` ile durumu kaydet.
3. İncelenecek sembolü `rg -n 'sembol' src tests scripts .github/workflows` ile bul. Çağıran ve çağrılan fonksiyonları birlikte oku.
4. Değiştireceğin dosyaların başlangıç kopyasını veya diff'ini depo dışına kaydet. Önceki kullanıcı değişikliklerini kendi işinle karıştırma.
5. Kabul ölçütünü koddan önce yaz. Hangi giriş, hangi beklenen sonuç ve hangi hata durumunun sınandığı açık olsun.
6. Bir davranış hatasında mevcut üretim fonksiyonunu kullanan başarısız test oluştur. Üretim algoritmasının kopyasını ayrı bir test içine yazıp onu doğrulama.
7. En küçük düzeltmeyi yap. Gereksiz dosya yeniden adlandırma, bütün dosyayı formatlama veya bağımsız refactor ekleme.
8. İlgili testi, altyapı kontrolünü ve diff boşluk kontrolünü çalıştır. Native Windows API değiştiyse Windows derlemesini tamamla.
9. Sonuçları aşağıdaki teslim şablonuna yaz. Gerçek oyun testini yapmadıysan açıkça “yapılmadı” de.

## A. Hedefli DLL workflow'unda test kapsamı

**Dosyalar:** [.github/workflows/build-1.21.11.yml](../.github/workflows/build-1.21.11.yml), [.github/workflows/build.yml](../.github/workflows/build.yml), [Test-HybridBridge.py](../scripts/Test-HybridBridge.py), [Test-VisualRuntime.py](../scripts/Test-VisualRuntime.py).

**Mevcut durum:** Hedefli workflow altyapı, Java köprüleri ve Windows DLL derlemesi yapıyor. Genel workflow'daki GrimUI ve yeni görsel runtime testleri hedefli workflow'da doğrudan görünmüyor. `Test-HybridBridge.py`, TriggerLifecycle ve TriggerTiming'i zaten çağırıyor; onları yok sayma veya gereksiz yere iki kez çalıştırma. Build script'lerinin dolaylı çağrılarını da incele.

**Uygulama adımları:**

1. İki workflow'un doğrudan ve dolaylı test listesini çıkar; hangi testin hangi runner'da çalıştığını yaz.
2. Hedefli workflow'a Ubuntu üzerinde bir test job'ı ekle. g++ gerektiren Python fixture script'lerini doğrudan Windows job'ına koyma.
3. Python, g++ ve Java gereksinimlerini açıkça kur. Testler için JDK 17+ gerekir; mevcut CI'daki Java 21 seçimi kullanılabilir.
4. Bu job'da hedefli offline testleri, `Test-VisualRuntime.py`, `Test-SpotifyTexture.py` ve `Test-GrimUI.py` çalıştır. Mevcut sınır kontrollerini koru.
5. Windows DLL job'ının `needs` alanını test job'ına bağla. Test başarısızken DLL/artefact başarıyla yayımlanmamalı.
6. Sky shader testini ekleyeceksen Linux'ta `moderngl`, `glcontext` ve çalışan EGL/Mesa ortamını doğrula. Ortam kurulu değilken testi sessizce geçmiş sayma. Bu kurulumu ayrı küçük değişiklik yapabilirsin.
7. Kırmızı test → Windows job atlanıyor; yeşil test → DLL/artefact üretiliyor akışını doğrula. Bilerek kırılan test yalnız izole test dalında olsun; teslimde kalmasın.

**Bitti ölçütü:** Workflow bağımlılığı açık, test başarısızlığı yayını engelliyor ve başarılı çalışmanın commit SHA'sı raporlanıyor. Önceki derlemenin DLL'ini yeni değişikliğin çıktısı olarak sunma.

## B. Spotify snapshot zamanında geriye sıçrama ve kimlik

**Dosyalar/semboller:** [spotify_source.cpp](../src/modules/visual/spotify_source.cpp): `source::snapshot()`, `poll()`, `publish()`; [spotify_source.hpp](../src/modules/visual/spotify_source.hpp): `now_playing`; [Test-VisualRuntime.py](../scripts/Test-VisualRuntime.py); [spotify_snapshot_test.cpp](../tests/spotify_snapshot_test.cpp).

**Doğrulanmış kod davranışı:** Snapshot ilerleme eklemesini yalnız `0 < elapsed < 5` iken yapıyor. Eşik aşılınca eklenen süre sıfıra düşüyor. Monotonluk filtresi 1,5 saniyeden büyük düşüşü seek kabul edebiliyor. Ayrıca geçmiş snapshot kimliği yalnız `title`; sanatçı, albüm ve oturum ayrımı yok.

**Somut test senaryosu:** Başlangıç pozisyonu 100 saniye, playing=true, yeni yayın yok. Önce örnek yaşı 4,9 saniyeyken snapshot al: yaklaşık 104,9. Sonra yaş 5,1 saniye olsun: mevcut kod tekrar 100'e dönebiliyor. Bu, kullanıcı seek yapmadan üretilen bir düşüş. Söz sorgusu ve medya doğrulamasının gecikmesi böyle bir yayın aralığı yaratabilecek adaydır; gerçek görülme sıklığı ölçülmedi.

**Uygulama adımları:**

1. Zamanı kontrollü verebilen küçük bir hesaplama noktası tasarla. Testi gerçek 5 saniye uyutarak yazma; sahte saat kullan.
2. Yaşlar 0 / 4,9 / 5,0 / 5,1 / 30 saniye için beklenen politikayı yaz. Öneri: sınırlı extrapolation yapılacaksa sınıra ulaşınca ekleme kaybolmasın, son sınırlı değerde kalsın.
3. Snapshot yaşı yüzünden oluşan düşüş ile yeni timeline örneğinden gelen gerçek seek'i ayır. Her geri hareketi bastıran bir filtre yazma.
4. Başlık + sanatçı + albümden oluşan mevcut `poll()` kimliğini incele; snapshot da aynı kimlik sözleşmesini kullanmalı. Oturum/yeniden başlatma için nesil veya eşdeğer açık bir kimlik ekle.
5. Fonksiyon içindeki `static last_snapshot_*` durumunu kontrollü servis durumuna taşı veya hesaplamayı durumsuz hâle getir. Stop/start ve yeni oturumda nasıl sıfırlandığını açıkça yaz. Mutex sahipliğini koru.
6. Aynı başlıklı farklı sanatçı/albüm, pause/resume, duration=0, parçanın sonu ve gerçek küçük/büyük geri seek için test ekle.
7. Paylaşılan artwork/lyrics tamponlarını derin kopyalamaya geri dönme. Söz satırı arama davranışını koru.

**Bitti ölçütü:** Yeni örnek veya seek olmadan 5 saniye eşiğinde süre geriye atlamıyor; gerçek seek çalışıyor; eski parçanın monotonluk durumu yenisini etkilemiyor.

## C. Spotify başlangıç hatasında tekrar deneme

**Dosyalar/semboller:** `spotify_source.cpp`: `start()`, `worker()` ve başlangıç hatasındaki `completion`; [spotify_hud.cpp](../src/modules/visual/spotify_hud.cpp): `run()`, `on_enable()`; [runtime.cpp](../src/app/runtime.cpp): `worker_interval`.

**Kanıt:** COM başlangıcı başarısız olursa worker, çalışan bayrağını kapatıyor. HUD'un worker tarafı çalışmayan servisi yeniden başlatıyor. Ortak turda 5 ms bekleme var. Dolayısıyla kalıcı başlangıç hatasında sık thread oluşturma/sonlandırma mümkündür; bunun gerçek makinelerde görüldüğü ölçülmedi.

**Uygulama adımları:**

1. Mevcut testteki bir kez başarısız `init_apartment` durumunu sürekli başarısızlıkla genişlet. Deneme sayısını say ve sahte saat kullan.
2. Servis içinde bir sonraki deneme zamanını tut. İlk başlangıç hemen çalışsın; hata sonrası örneğin 250 ms, 500 ms, 1 s, en fazla 5 s bekleme politikası kullan. Bunlar başlangıç önerisidir; sözleşmeyi testte açıkça belirt.
3. Beklemeyi ortak modül thread'inde `sleep` ile uygulama. `start()` uygun zaman gelmediyse hızlıca dönsün.
4. Başarılı başlangıçta hata sayacını sıfırla. Kullanıcı disable/enable, shutdown ve başlatılamayan thread durumlarının zamanlayıcıyı nasıl etkilediğini tanımla.
5. `running`, `worker_finished`, `closing`, generation ve lifecycle/wake mutex kilit sırasını koru. Başka thread join beklerken aynı thread'in ihtiyaç duyduğu kilidi tutma.
6. Mevcut “başlangıç hatasından hemen sonraki çağrı başarılı olur” testini yeni zaman sözleşmesine göre düzenle; yalnız assertion silme.

**Bitti ölçütü:** Kalıcı hatada saniyede yüzlerce thread başlatılmıyor; normal ilk başlangıç hızlı; stop/shutdown gecikmesiz uyanıyor; aynı anda tek worker var.

## D. Geçersiz dünya ile kapanışta temizlik

**Dosyalar/semboller:** [module_registry.cpp](../src/modules/core/module_registry.cpp): `clear_modules()`; [module_runtime_state.hpp](../src/modules/core/module_runtime_state.hpp): `stop_module()`; [modules.cpp](../src/modules/modules.cpp): `shutdown()`; [runtime.cpp](../src/app/runtime.cpp): `runtime_services`; [nametag.cpp](../src/modules/visual/nametag.cpp).

**Kanıt:** `clear_modules()` yalnız `cache.is_valid()` iken `stop_module()` çağırıyor, ardından modülü siliyor. Nametag'in statik `scoreboard_identity` ve taşıyıcı referanslarının temizliği `on_disable()` içinde. Nametag sınıfında bunları ayrıca temizleyen bir destructor görünmüyor. Bu, geçersiz oyun bağlamında cleanup yolunun atlandığını gösterir; gerçek JVM'de hangi kaynakların kaldığı ayrıca test edilmelidir.

**Önemli:** `cache.is_valid()` ile JNI ortamının hâlâ kullanılabilir olması aynı şey değildir. Oyuncu/dünya bulunmazken JNI env canlı olabilir. Bütün `on_disable()` fonksiyonları geçersiz cache kabul ediyor varsayımı da yapma.

**Uygulama adımları:**

1. Tüm modüllerin `on_disable`, destructor ve `on_render_shutdown` fonksiyonlarını listele. Her biri için CPU belleği / JNI referansı / oyun durumunu geri yükleme / GL nesnesi sütunlarından oluşan sahiplik tablosu çıkar.
2. Nametag için üç test bağlamı kur: dünya geçerli ve JNI canlı; dünya yok ama JNI canlı; JNI artık kullanılamıyor.
3. Dünya gerektirmeyen kaynak bırakmayı oyun durumunu geri yüklemeden ayır. Gerekirse açık bir idempotent kaynak-temizleme hook'u ekle; hook'un hangi thread'de çağrılacağını yaz.
4. Genel `cache.is_valid()` koşulunu körlemesine silme. Önce bütün etkilenen cleanup fonksiyonlarının geçersiz bağlam davranışını güvenli hâle getir veya yeni hook ile yalnız kaynak bırakmayı garanti et.
5. GL temizliği render tarafında, JNI global ref temizliği bağlı JNI thread'inde yapılmalı. CPU cache'leri oyun dünyası olmadan bırakılabilmeli.
6. JNI artık kullanılamıyorsa `DeleteGlobalRef` çağırma; esas çözüm normal kapanış sırasının JNI detach öncesinde temizliği tamamlamasıdır. Referansı `nullptr` yapmak Java tarafını serbest bırakmış sayılmaz.
7. İki kez kapanış, bağlantı kopması ardından unload ve ana menüde unload testlerini ekle.

**Bitti ölçütü:** Geçersiz dünya cleanup'ı tamamen atlatmıyor; aynı kaynak iki kez silinmiyor; yanlış thread/context'te API çağrısı yok. Saf fixture testinin yanına gerçek JVM yaşam döngüsü doğrulaması ekle. Bu görev yüksek risklidir; belirsiz sahiplik tablosuyla genel registry değişikliğine geçme.

## E. Nametag kapanışında başarısız geri yükleme

**Dosyalar/semboller:** `nametag.cpp`: `restore_team()`, `on_disable()`, `original_team_by_player`, `scoreboard_identity`; [runtime_budget.hpp](../src/modules/visual/runtime_budget.hpp): `retire_records()`; `Test-VisualRuntime.py`.

**Kanıt:** Normal bütçeli temizlikte başarısız `restore_team()` kaydı saklanıyor. Fakat `on_disable()` dönüş değerini kullanmıyor; ardından hidden team'i kaldırıp geri yükleme haritasını temizliyor. Restore işlemi başarısız olursa deneme bilgisi kaybolabilir.

**Uygulama adımları:**

1. Yalnız `restore_team()` helper'ını değil, gerçek kapanış akışını test et. İlk oyuncunun restore çağrısı başarısız, ikinci oyuncununki başarılı olsun.
2. Başarılı, zaten sunucu tarafından değiştirilmiş ve başarısız kayıtları ayır. Başarısız kaydı başarılı gibi silme.
3. D görevindeki sahiplik kararına göre, devre dışı modül için de işleyebilen sınırlı bir cleanup yolu tasarla. Yalnız `run()` içine retry koymak çalışmaz; kapalı modülün `run()` fonksiyonu çağrılmıyor.
4. Kuyruk kullanılırsa kayda scoreboard kimliğini de bağla. Eski dünyanın kaydını yeni scoreboard üzerinde uygulama. Sınırsız kuyruk veya kalıcı JNI ref tutma oluşturma.
5. Hidden team'i ne zaman kaldıracağını açıkça belirle: geri yüklemesi tamamlanmamış üyelerin bilgisini önce yok etme. Son unload sırasında dünya/JNI kaybı olasılığı için kaynak bırakma ve geri yükleme sonucunu ayrı raporla.
6. Sunucu oyuncuyu yeni takıma aldıysa o yeni takımı eski kayıtla ezme. Mevcut `restore_team()` korumasını tut.
7. “Bir kez hata, sonra başarı”, “sürekli hata”, “takım silinmiş”, “sunucu takımı değiştirmiş”, “dünya değişmiş” ve “ikinci kez disable” durumlarını sınayarak tamamla.

**Bitti ölçütü:** Geçici hata geri yükleme bilgisini sessizce kaybettirmiyor; bekleyen iş kapanışı sonsuza kadar tutmuyor; eski dünya kayıtları yeni dünyaya uygulanmıyor. Dünya/JNI tamamen yok olduğunda geri yüklemenin mümkün olmadığını başarı gibi göstermeme.

## F. Sky uniform konumu cache'i

**Dosya/semboller:** [sky_panorama.cpp](../src/modules/visual/sky_panorama.cpp): `runtime::initialize()`, `render()`, `renderer::shutdown()`; [Test-SkyShader.py](../scripts/Test-SkyShader.py).

**Kanıt:** `render()` aynı program için `Panorama`, `PanoramaNext`, `Exposure`, `FogColor` ve diğer uniform isimlerini tekrar `GetUniformLocation()` ile arıyor. Kaynakta bu tekrar var; bunun önemli bir FPS darboğazı olduğu ölçülmedi.

**Uygulama adımları:**

1. Render içindeki bütün uniform isimlerini listele; shader tanımlarındaki isimlerle eşleştir.
2. `runtime` içine `GLint` alanlarından oluşan bir uniform konum yapısı ekle. Başlangıç değerleri `-1` olsun.
3. Program başarıyla link edildikten sonra konumları bir defa çöz. Başarısız link durumunda cache'i geçerli sayma.
4. Render içinde yalnız saklanan konumları kullan. Uniform değerleri kareye göre güncellenmeye devam etsin; sadece isim araması taşınacak.
5. Program silme/yeniden oluşturma/context değişiminde cache'i sıfırla ve yeni program için tekrar çöz. Konumları farklı programlar arasında paylaşma.
6. `-1` konumu OpenGL'in optimize ettiği kullanılmayan uniform için geçerli olabilir; bütün `-1` sonuçlarını otomatik fatal hataya dönüştürme.
7. Fake GL sayacıyla aynı programda 1.000 frame boyunca lookup sayısının artmadığını, yeni programda tekrar çözüldüğünü göster. Gerçek EGL shader testini de çalıştır.

**Bitti ölçütü:** Shader çıktısı ve GL state geri yükleme aynı; program başına lookup sabit. Ölçülmeyen FPS artışı yüzdesi yazma.

## G. Insert: gerçek giriş kabul kontrolü

**Dosyalar:** [gui.cpp](../src/gui/gui.cpp), [gui.hpp](../src/gui/gui.hpp), [key_codes.hpp](../src/platform/input/key_codes.hpp), [input_service.cpp](../src/services/input/input_service.cpp), [inv_walk.cpp](../src/modules/misc/inv_walk.cpp), [key_codes_test.cpp](../tests/key_codes_test.cpp), [grim_ui_test.cpp](../tests/grim_ui_test.cpp).

**Mevcut durum:** Insert değişikliği uygulanmış; yeni hata olduğu ileri sürülmüyor. Menü artık fiziksel scan `0x152` okuyor. `GetAsyncKeyState(VK_INSERT)` tek başına NumLock kapalı NumPad 0'ı ayıramaz; bu nedenle doğrulama yöntemi önemlidir.

**İş akışı:**

1. `WndProc` içindeki `observe_physical_key()` çağrısından menüdeki `physical_scan_down()` okumasına kadar veri akışını izle. Focus kaybında reset ve `focused_shortcut` kenar algılama sırasını oku.
2. Fixture'da Insert için `0x152`, NumPad 0 için `0x52` üret; NumLock açık/kapalı durumlarını kapsa. Tuş basılı tutulunca yalnız tek aç/kapat olmalı.
3. Focus kaybı, basılı tuşla focus dönüşü ve yeni bir basış testlerini ekle. Pencere/hook yeniden kurulmasında önceki basılı durum taşınmamalı.
4. Gerçek Windows oyununda normal ekran, envanter, sohbet, InvWalk açık/kapalı ve Alt+Tab dönüşünü kontrol et.
5. Insert ataması olan eski yerel profil yüklenince rezerv tuş kuralının uygulandığını doğrula. Right Shift ataması saklanabilmeli.
6. Sorun yeniden üretilirse yalnız kaybolan giriş/focus yolunu düzelt. Sırf olasılık nedeniyle ikinci bağımsız menü toggle yolu ekleme; çift tetikleme yaratabilir.

**Bitti ölçütü:** Gerçek Insert tek kez aç/kapatır; NumPad 0 açmaz; Right Shift menüyü açmaz; odak dönüşünde kendiliğinden toggle olmaz. Testler geçtiyse bu görev kod değişikliği olmadan kabul kaydıyla kapanabilir.

## H. Uzun oturum ölçümü

**Amaç:** Tahmini maliyeti gerçek oyun verisinden ayırmak. Yeni bir büyük optimizasyon paketi yazmadan önce bu ölçümü yap.

1. Aynı Minecraft/DLL sürümü, JVM, GPU sürücüsü, çözünürlük, render distance ve modül ayarlarıyla karşılaştırma yap. Değişen her koşulu kaydet.
2. Gerekirse kapalı varsayılan geliştirici sayaçları ekle: Nametag keşif süresi/kayıt sayısı, ESP taranan oyuncu sayısı, kozmetik PMR taşma tahsisi, Sky decode/upload süresi, Spotify yeniden başlatma sayısı.
3. Her frame log yazma. Sayaçları sınırlı bellekte biriktir ve seyrek özetle. Oyuncu adı, şarkı başlığı veya oturum içeriğini performans log'una yazmak gerekmiyor.
4. Boş sahne, kalabalık oyuncu sahnesi, çok mob/eşya bulunan sahne ve yoğun kozmetik sahnesini ayrı ölç.
5. Isınma sonrası, 30, 60 ve 120 dakikada örnek al. Aynı döngüyü dünya değişimi, modül aç/kapat ve unload sonrası da incele.
6. Ortalama FPS yanında frame time p50/p95/p99, private bytes/commit, VRAM, thread sayısı ve uygulama koleksiyon boyutlarını kaydet. JVM heap'ini native bellekten ayır.
7. RAM'in yalnız yüksek olmasını sızıntı sayma. Art arda aynı sahne döngülerinde sürekli artış ve kaynak sahibinin bırakmaması gibi kanıt ara. Allocator belleği OS'ye hemen döndürmeyebilir.
8. Başlangıç ve değişiklik sürümünü aynı senaryoda birden fazla kez çalıştır; tek koşudaki küçük farkı kesin kazanç diye sunma.

**Bitti ölçütü:** Tekrarlanabilir ölçüm tablosu, koşullar ve kaynak sürümleri var. Bir sorun bulunursa bu tabloya dayanarak yeni, dar bir görev açılır. Gerçek oyun ortamı yoksa bu iş “bekliyor” olarak kalır; sentetik testle tamamlandı denmez.

## 5. Test komutları ve ortam sınırları

Komutları depo kökünde çalıştır. Her komutun exit code'unu ayrı denetle; son komutun başarısı öncekinin hatasını örtmesin.

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
git diff --check
```

Görsel/runtime işleri için hedefli Linux testleri:

```sh
python scripts/Test-Offline.py --test visual_runtime --test spotify_snapshot --test visual_animation --test snapshot_session --test cosmetic_physics --test remaining_settings --jobs 2
python scripts/Test-VisualRuntime.py
python scripts/Test-NametagJvm.py --java-home "$JAVA_HOME"
python scripts/Test-SpotifyTexture.py
```

Insert/arayüz işleri için:

```sh
python scripts/Test-Offline.py --test key_codes --test keybinds_presentation --test input_tracking --test inv_walk_engine --jobs 2
python scripts/Test-GrimUI.py
```

Triggerbot koruma regresyonları, JDK'nın `PATH` üzerinde olduğu ortamda:

```sh
python scripts/Test-TriggerLifecycle.py
python scripts/Test-TriggerTiming.py
```

Sky shader testi için çalışan EGL ortamı ve Python OpenGL bağımlılıkları gerekir:

```sh
python scripts/Test-SkyShader.py
```

Windows native derleme, Visual Studio C++ araçları/Windows SDK/PowerShell/JDK kurulu ortamda:

```powershell
./scripts/Build-Release.ps1 -Clean -SkipSign -PlatformToolset v143 -MinecraftVersion 1.21.11
```

Native DLL, gerçek medya servisi veya JVM cleanup davranışı yalnız Linux mock testleriyle tamamen doğrulanamaz. `Test-VisualRuntime.py` gerçek üretim fonksiyonlarını fixture'larla çalıştırır fakat gerçek ağ/SMTC/sürücü testi değildir. Source fixture çıkarma işlev imzasına bağlıysa refactor sırasında script'i de güncelle; testi sessizce devre dışı bırakma.

## 6. GitHub ve DLL teslim akışı

Bu bölümü yalnız kullanıcının yayımlama/derleme isteği kapsamında uygula.

1. Çalışma ağacındaki önceki işler ile bu görevin diff'ini ayır. Eski HEAD'e göre yüzlerce değişiklik görmek hepsinin bu göreve ait olduğu anlamına gelmez.
2. Uzak dalın son commit'ini kontrol et. Başka kişinin yeni commit'lerini eski yerel ağaçla ezme; force push yapma.
3. Her tamamlanmış mantıksal iş için küçük commit tercih et. Yerel eski index/HEAD düzenini gerekçesiz değiştirme. Gerekirse mevcut çalışma ağacını kaybetmeden izole checkout veya kontrollü snapshot yöntemi kullan.
4. Testler geçtikten ve kaynak değişiklikleri tamamlandıktan sonra final commit'i yükle. Her küçük ara değişiklik için tam DLL workflow'u başlatma.
5. Workflow'u ilgili dal/ref üzerinde çalıştır. Run'ın `headSha` değerinin teslim edilecek kaynakla aynı olduğunu doğrula.
6. Workflow sonucu `success` ve artifact mevcut olmadan “DLL hazır” deme. Eski başarılı run yeni commit'in kanıtı değildir.
7. Sürüm adını, commit'i, run bağlantısını ve artifact bağlantısını kaydet. Artifact süreli saklanır; kalıcı release ile aynı değildir.
8. İndirebildiysen dosyanın Windows x64 DLL olduğunu kontrol et ve SHA-256 üret. Ortam indirmeyi engelliyorsa indirdim/doğruladım deme; GitHub artifact bağlantısını ver.
9. Sonradan yalnız raporu güncellediysen hangi kaynak commit'inin derlendiğini koru. Rapordaki commit ile artifact'ın kaynak ilişkisi açık olsun.

## 7. Her görev için doldurulacak teslim kaydı

```text
İş kimliği ve başlığı:
Başlangıç commit'i / çalışma ağacı notu:
Yeniden üretim veya ölçüm:
Değişen dosyalar ve semboller:
Seçilen çözüm:
Korunan davranışlar:
Çalıştırılan testler ve sonuçları:
Windows derlemesi: başarılı / başarısız / yapılmadı
Gerçek oyun kabulü: başarılı / başarısız / yapılmadı
Kalan sınırlamalar:
Kaynak commit'i:
Varsa workflow / artifact bağlantısı:
```

Bir sonraki modele verilecek kısa başlangıç isteği:

> Bu deponun IMPLEMENTATION_HANDOFF_2026-11-17.md dosyasını oku. Önce başlangıç durumunu doğrula. Son denetimin güncel tablosunda gerçekten açık ilk işi doğrula; tamamlanmış A–F işlerini tekrar uygulama. Önceki kullanıcı değişikliklerini koru. Üretim kodunun kopyasını test etmek yerine gerçek yolu kullan. Başarıyı test çıktısı ve commit bilgisiyle raporla; yapılamayan oyun testini yapılmış gibi gösterme.

Sonraki görevlerde güncel durum tablosundaki açık işin kimliğini kullan. D/E gibi sahiplik gerektiren işlerde önce dosya/çağrı/temizlik tablosunu tamamla; belirsizliği büyük bir genel refactor ile örtme.
