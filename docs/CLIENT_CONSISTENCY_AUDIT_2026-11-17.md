# Client eksikleri ve tutarlılık denetimi

Tarih: 17 Kasım 2026. Kapsam: mevcut yerel çalışma ağacı. Bu tur yalnız inceleme ve bu raporun hazırlanmasıdır; uygulama kodu, test dosyaları ve workflow'lar değiştirilmedi.

## Doğrulanmış DLL teslimi

- Sürüm: **Fabric 1.21.11, Windows x64 Release**.
- Derlenen kaynak commit'i: `4d665ac7ab2669d30615e79d725e6496e83aef3d`; dal: `codex/client-consistency-20261117`.
- [Başarılı GitHub çalıştırması](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44198194449): offline testler, EGL Sky shader testi, Windows Java köprü kontrolleri ve native derleme başarılı. Windows job süresi 14 dakika 33 saniye.
- [DLL artifact'ı](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44198194449/artifacts/14380169548): `Onestep-1.21.11-Windows-x64`; 17.252.362 bayt ZIP. Saklama bitişi: 1 Aralık 2026, 23:34 UTC.
- GitHub'ın artifact için bildirdiği ZIP özeti: `sha256:df91144fe346105197434f1a830ff3f171816ad55d92461a9e977e59dc7c173b`. Bu DLL dosyasının bağımsız hesaplanmış özeti değildir.
- Ortamın artifact storage erişimi indirmeyi engelledi; yerel DLL/PE/SHA-256 doğrulaması yapılmış sayılmıyor. GitHub artifact'ı başarıyla yayımlandı.
- Gerçek Minecraft/SMTC/GPU kabulü yapılmadı. C03 render durması, C09 gerçek Haste, C10 ayırt edilemeyen aynı-değer sunucu güncellemesi ve C12 uzun oturum ölçümü açık.

Bu teslim kaydı kaynak derlemesinden sonraki belge güncellemesidir; yukarıdaki commit hangi kodun DLL'e girdiğini gösterir.

## Uygulama durumu — 17 Kasım 2026

Bu bölüm yukarıdaki incelemeden sonraki uygulama turuna aittir. Aşağıdaki bulgu metinleri **düzeltme öncesi** kanıtlardır.

| İş | Güncel durum |
| --- | --- |
| C01 | Uygulandı: ortak binding politikası, kartta ret, profilde ayrılmış tuşları temizleme, scan sonrası worker engeli. Fare ve eski sayısal VK desteği korundu. |
| C02 | Uygulandı: gamma'nın değiştirilen gerçek option nesnesi global ref ile tutuluyor; dünya yokken worker cleanup, başarısız yazmada yeniden deneme, nihai ref bırakma var. |
| C03 | Kısmi: pencere redraw isteği ve 5 saniyede bir bekleme tanısı eklendi. Render tamamen durursa güvenli unload kabulü açık; etkin callback/context sahipliği çözülmeden zorla DLL boşaltılmıyor. |
| C04 | Uygulandı: taşıyıcı setter hatasında undo/ref korunuyor, normal temizlik 64 kayıt bütçeli, nihai unload ayrı. |
| C05 | Uygulandı: bekleyen temizlik menüde gösteriliyor; 5 saniye aralıklı kayıt sayısı tanısı var. Yeniden açılma önce eski cleanup'ın tamamlanmasını bekler. |
| C06 | Uygulandı: poll içindeki küçük geri seek'i bastıran filtre kaldırıldı; gerçek timeline örneği kabul ediliyor. |
| C07 | Uygulandı: oturum değişimi metadata aynı olsa bile track geçişi sayılıyor. |
| C08 | Uygulandı: hedefli testler 1.21.11 seçiyor; iki workflow'da EGL shader derleme/çizim ve yeni cleanup regresyon testi var. |
| C09 | Fixture'daki etkisiz Haste alanı/üç kez aynı test kaldırıldı. Oyun API'si otoritesi korunuyor. Gerçek Haste oyun kabulü açık; üretime ikinci bir etki çarpanı eklenmedi. |
| C10 | Native yola oyuncu başına undo kaydı, yerel oyuncuyu dışlama, dünya kimliği, kapalıyken tekrar deneme ve JNI kapanışı eklendi. Önceden açık glow korunuyor; gözlenebilir farklı sunucu değeri ezilmiyor. Aynı değere yazılmış sunucu güncellemesini polling ayırt edemez; paket düzeyinde sahiplik kabulü açık. 1.21.11 Java yolu bu tur değiştirilmedi. |
| C11 | Önceki handoff'un durum tablosu ve başlangıç talimatı güncellendi; eski görev paragrafları tarihsel bağlam olarak işaretlendi. |
| C12 | Yerel testler ve Windows derlemesi başarılı; teslim kaydı yukarıda. Gerçek 60 dakikalık oyun/GPU ölçümü bu Linux ortamında yapılamıyor; açık. |

Yeni regresyon script'i `scripts/Test-ClientConsistency.py`, üretim worker ve cleanup fonksiyonlarını hata enjeksiyonlu doubles ile çalıştırır. Gerçek oyun kabulünün yerini tutmaz. `Test-Offline.py` tüm 1.21.11 hedefleri, HybridBridge, Nametag gerçek JVM, GrimUI ve EGL Sky testleri yerelde geçti. Son teslimin commit/run/artifact bilgileri ayrıca paylaşılır.

## 1. Sonuç ve kanıt sınırı

En önce tuş atama kurallarının birleştirilmesi, native Full Bright kapanışının düzeltilmesi ve render durduğunda unload davranışının doğrulanması gerekiyor. Spotify ve Nametag'de önceki düzeltmelerin dışında kalan davranış tutarsızlıkları var. Testlerin geçmesi bu yolların tamamının doğru olduğunu göstermiyor.

Bu rapor bir tam güvenlik denetimi veya bütün Minecraft sürümlerinin oyun kabulü değildir. İnceleme; modül yaşam döngüsü, giriş, config, görsel modüller, Spotify, JNI temizliği, testler ve derleme akışına odaklanır.

- **Çalıştırılarak doğrulandı:** Üretim kodundan alınan ilgili bloklar küçük, geçici C++ fixture'larında çalıştırıldı. Tam Minecraft oturumu değildir.
- **Kaynakta doğrulandı:** Çağrı zinciri ve koşullar kaynakta mevcut. Kullanıcıya görünen sıklık oyun içinde ölçülmedi.
- **Doğrulama açığı:** Henüz hatalı davranış kanıtı değil; mevcut test veya ölçüm beklenen sonucu kapsamıyor.

Çalışma ağacı çok sayıda önceki değişiklik içeriyor. Yerel dal `main`, HEAD `fea6739`; bu HEAD mevcut dosyaların tamamını temsil etmiyor. Başlangıçta mevcut 1.056 dosyanın SHA-256 özeti depo dışında kaydedildi. Önceki işleri geri alma veya temizleme yapılmamalı.

Önceki devir kaydında son doğrulanan Windows DLL kaynak commit'i `3174c0d4660e0c8f3f028e2149840feb4f570188`. Bu tur GitHub'daki son durum yeniden sorgulanmadı ve yeni DLL derlenmedi. Yerel değişikliklerle eski artifact'ın aynı olduğu varsayılmamalı.

## 2. Öncelik sırası

| Kimlik | Konu | Öncelik | Kanıt |
| --- | --- | --- | --- |
| C01 | Ayrılmış tuşlar farklı giriş yollarında kabul ediliyor | P1 | Üretim bloklarıyla doğrulandı |
| C02 | Native Full Bright dünya yokken kapanışta gamma'yı bırakamayabilir | P1 | Kaynakta doğrulandı |
| C03 | Render durunca unload beklemesinin çıkış yolu yok | P1 doğrulama | Koşullu kod yolu; oyun testi gerekli |
| C04 | Nametag taşıyıcı geri yükleme hataları kaybediliyor | P2 | Kaynakta doğrulandı |
| C05 | Nametag bekleyen temizlik yüzünden açık görünüp çalışmayabilir | P2 | Kaynakta doğrulandı |
| C06 | Spotify küçük geri seek'i bastırıyor | P2 | Üretim bloğuyla doğrulandı |
| C07 | Spotify oturum kimliği kapak/söz kimliğine dahil değil | P2 | Kaynakta doğrulandı |
| C08 | Hedefli CI sürüm seçimi ve Sky shader test kapsamı eksik | P2 | Workflow/script karşılaştırması |
| C09 | Haste testi gerçek etki hesabını doğrulamıyor | P1 doğrulama | Fixture incelendi |
| C10 | Glow ESP native yolu mevcut glow durumunu korumuyor | P2 | Kaynakta doğrulandı |
| C11 | Önceki MD'nin durum özeti ile görev metni çelişiyor | P2 | Belge içinde doğrulandı |
| C12 | Uzun oturum performansı ve güncel DLL kabulü ölçülmemiş | P2 doğrulama | Ölçüm/kabul açığı |

P1 önce ele alınmalı; P2 sonraki küçük işlerdir. Buradaki sıraya göre her madde ayrı uygulanabilir. Doğrulama maddesinde önce ölçüm/test yapılmalı; varsayıma dayanarak kod değiştirilmemeli.

## C01 — Tuş atama kuralları tek değil

**Kanıt:** `src/ui/keybinds_model.hpp:52`, `src/modules/core/module.cpp:36` ve `:96`, `src/modules/core/public_config.cpp:190`, `src/app/module_loop.cpp:40`.

Keybinds sayfası `assignable()` kullanıp Insert, End, Esc ve Windows tuşlarını engelliyor. Modül kartındaki yakalama yolu ise `set_keybind()` çağırıyor; setter ve profil okuyucu yalnız sayısal `valid()` kontrolü yapıyor. Böylece arayüzün yasakladığı bir tuş karttan veya eski profilden kabul edilebiliyor. Esc kartta ayrıca sıfırlama olarak ele alınıyor, fakat profilde sayısal olarak kabul ediliyor.

Worker önce `binding != gui::menu_key` koşulunu kontrol ediyor; ardından scan-code dalı `pressed` değerini bu koşulu korumadan yeniden yazıyor. Üretim bloğu fixture'ında `binding=VK_INSERT`, fiziksel scan basılı iken sonuç `true` oldu. Menü açılmasıyla modülün birlikte tetiklenip tetiklenmemesi render/worker sırasına da bağlıdır; her basışta kesin çift tetikleme iddiası yok.

**Uygulama:**

1. Atanabilirlik politikasını setter, kart, profil ve worker için açıkça tanımla. `0` atamayı temizlemeye devam etsin.
2. Eski profilde ayrılmış tuş için temizleme veya açıklamalı ret politikası seç; yarım uygulanmış profil bırakma.
3. Scan-code okumasından sonra da ayrılmış tuş engelinin geçerli olmasını sağla.
4. `valid()` fonksiyonunu körlemesine `assignable()` ile değiştirme: fare tuşları ve mevcut eski VK uyumluluğu ayrıca korunmalı.

**Kabul:** Karttan, profil yüklemeden ve Keybinds sayfasından aynı sonuç alınmalı. Insert yalnız menüyü açmalı; End yalnız unload amacı taşımalı. NumLock kapalı NumPad 0/1 bunlarla karışmamalı. Right Shift, fare yan tuşları ve Num Enter regresyonları geçmeli. Test yalnız `key_edge` sınıfını değil gerçek worker giriş hesabını da çalıştırmalı.

## C02 — Full Bright'ın native gamma temizliği dünya geçerliliğine bağlı

**Kanıt:** `src/modules/core/module_registry.cpp:40`, `src/modules/visual/full_bright.cpp:42`, `:64`, `:107`, `src/modules/visual/full_bright.hpp`, `src/cache/cache.cpp`.

Native yol eski gamma'yı yakalayıp değeri `16.0` yapıyor. Kayıt defteri, kapanışta cache geçersizse `on_disable()` çağırmıyor. Full Bright'ın ayrı worker kapanış temizliği yok; render kapanışı yalnız 1.21.11 Java utility yolunu bırakıyor. Dünya yokken client/options erişilebilir kalabildiği için, native gamma'yı geri yükleyebilecek bağlam bulunmasına rağmen bu çağrı atlanabiliyor. Diğer sürümlerin native yolu ve 1.21.11 native fallback'i incelenmeli.

Normal `on_disable()` içinde de `write_gamma()` başarısızlığı yeniden denenecek durum olarak korunmuyor; `original_gamma_captured` sonunda sıfırlanıyor.

**Uygulama:**

1. Full Bright'ın değiştirdiği değerin ve ait olduğu options nesnesinin sahipliğini tanımla.
2. Dünya/player gerektirmeyen gamma temizliğini JNI/client-options kullanılabilirliğiyle yürüt.
3. Tekrarlı cleanup'ı güvenli yap; başarısız yazmayı başarılı geri yükleme sayma.
4. Registry'deki geçerli-cache koşulunu bütün modüller için topluca kaldırma. Her `on_disable()` geçersiz bağlamı kabul etmiyor.

**Kabul:** Dünya açıkken kapat, ana menüde unload et, JNI erişimi kesildiğinde kapat ve iki kez cleanup çağır. Eski gamma uygun bağlamda geri gelmeli; yanlış options nesnesine yazılmamalı. Gerçek native sürüm ve 1.21.11 fallback ayrı doğrulanmalı.

## C03 — Render çağrısı gelmezse kapanış sonsuza kadar bekleyebilir

**Kanıt:** `src/gui/gui.cpp:610`, `:622`, `:1160`; `src/app/runtime.cpp` içindeki kapanış sırası.

`gui::shutdown()` istek bayrağını açıp `shut_down` gelene kadar bekliyor. Bu bayrağı tamamlayan yol `detour_wglSwapBuffers()`. İstekten sonra swap çağrısı gelmezse döngünün çıkış yolu yok. Küçültme, pencere/context kaybı veya renderer'ın durması altında bunun yaşanıp yaşanmadığı bu tur ölçülmedi. Kapanışın sonraki Spotify ve modül temizliği de bu beklemenin arkasında.

Ayrıca detour'un tamamlandığı bayrağından sonra sabit 50 ms bekleniyor. Sabit bekleme tek başına bütün callback'lerin sona erdiğine dair bir sahiplik kanıtı değildir; bu rapor mevcut kodda gerçekleşmiş bir unload çökmesi iddia etmez.

**Uygulama:**

1. Render çağrılarını durdurabilen kontrollü bir yaşam döngüsü testi kur; shutdown isteğinin hangi durumda ilerlemediğini göster.
2. Çalışan callback, context sahipliği ve render cleanup tamamlanması için açık durumlar tanımla.
3. Süre aşımı gözlemlenebilir olmalı. Timeout dolunca GL temizliğini rastgele worker'da çalıştırma veya etkin callback varken DLL'i boşaltma.
4. Güvenli tamamlanma mümkün değilse kaynakları kullanılır halde bırakıp durumu bildir; güvenli render geri dönüşünde işlemi tamamla.

**Kabul:** Normal unload, render durması, yeniden render başlaması ve pencere değişimi sınanmalı. Yanlış context'te GL çağrısı veya DLL boşaltıldıktan sonra callback çalışması olmamalı.

## C04 — Nametag takım temizliği ile taşıyıcı temizliği aynı hata politikasını kullanmıyor

**Kanıt:** `src/modules/visual/nametag.cpp:168`, `:195`, `:686`.

Takım geri yüklemesi başarısız olursa kayıt sonraki deneme için korunuyor. Buna karşılık armor stand ad görünürlüğü veya text display metni geri yazılırken JNI hatası oluşursa `restore_and_release_carrier()` hatayı temizleyip global referansları bırakıyor. Sonrasında taşıyıcı listesi temizleniyor. Geçici hata sonrası eski görünürlüğü/metni yeniden yüklemek için kayıt kalmıyor.

**Uygulama:** Geri yükleme sonucunu açıkça döndür; normal kapatma ile nihai unload'u ayır. Normal kapatmada başarısız taşıyıcıları kendi dünya/nesne kimliğiyle bütçeli yeniden dene. Nihai unload'da bırakılacak kaynakları ve geri yüklenemeyen kayıt sayısını raporla. Eski nesneyi yeni dünya üzerinden değiştirme.

**Kabul:** İlk setter çağrısı JNI hatası versin, ikinci başarılı olsun; kayıt ilkinde korunmalı, ikincide yalnız bir kez bırakılmalı. Dünya değişimi, nesnenin yok olması ve JNI erişimsizliği ayrı sınanmalı. Mevcut Nametag JVM testindeki taşıyıcı işlemleri fixture olduğundan bu yolun kapsandığını varsayma.

## C05 — Nametag temizlik beklerken açık görünüp çalışmayabilir

**Kanıt:** `src/modules/visual/nametag.cpp:433`, `:686`, `:724`, `:743`.

`cleanup_pending` varken `run()` hemen dönüyor. Aynı scoreboard üzerinde sürekli geri yükleme hatası olursa maintenance her 100 ms tekrar deniyor; modülü yeniden açmak bu bekleyişi çözmüyor. Kullanıcı açık modül görüp yeni etiket alamayabilir. Bu sınırsız bir yeni kayıt kuyruğu iddiası değildir; sorun bekleme durumunun süresi ve görünmezliği.

Nihai cleanup başarısız takım geri yüklemelerinden sonra referansları bırakırken kalan kayıt sayısını da silip devam ediyor; bu sonucun tanısal kaydı yok.

**Uygulama:** Bekleyen, tamamlanan ve geri yüklenemeyen temizlik durumlarını görünür kıl. Hata nedenini ve deneme sayısını sınırlı sıklıkta kaydet. Yeniden açılış politikasını açıkça belirle; yalnız engeli kaldırıp aynı scoreboard üzerinde ikinci gizleme durumu üretme.

**Kabul:** Sürekli hata, geçici hata ve dünya değişimi testleri; tekrar açıldığında tanımlı sonuç; aşırı log üretimi olmaması; nihai unload'da kalan sayıların raporlanması.

## C06 — Spotify'ın poll filtresi küçük geri seek'i hâlâ bastırıyor

**Kanıt:** `src/modules/visual/spotify_source.cpp:888`. Önceki snapshot düzeltmesi ayrı bir katmandadır.

Üretim bloğu ile doğrulanan örnek: aynı parça çalarken önceki pozisyon `100`, yeni gerçek timeline pozisyonu `99` saniye olsun. Kod `99` değerini tekrar `100` yapıyor. `1.5` saniyeden küçük gerçek geri seek ile ölçüm oynaklığı ayrılmıyor. HUD ve söz vurgusu bu sırada yeni pozisyonu izleyemeyebilir.

**Uygulama:** Timeline örneğinin kimliğini/zamanını, gerçek seek'i ve yerel extrapolation'ı ayır. Her geriye hareketi engelleyen ikinci bir snapshot filtresi ekleme. Örnek kimliği aynıysa jitter politikası, yeni timeline güncellemesi varsa seek politikası açık olsun.

**Kabul:** 0.2 / 1.0 / 1.49 / 1.5 / 3 saniyelik geri seek, pause/resume ve yeni örnek gelmeden 5 saniyeyi geçme testleri. Mevcut extrapolation sınırındaki düzeltme korunmalı. Test `poll()` filtresini de içermeli; yalnız `now_playing::extrapolate()` yeterli değil.

## C07 — Spotify oturum değişiminde aynı metadata eski kapak/sözleri tutabilir

**Kanıt:** `src/modules/visual/spotify_source.cpp:849`, `:870`, `:923`.

Seçilen oturum değişince metadata yenileniyor; fakat `track_changed` yalnız başlık + sanatçı + albümden hesaplanıyor. Kesintisiz biçimde aynı metadatayı bildiren başka oturuma geçildiğinde `track_changed=false` kalabiliyor. Yeni oturumun kapağı/sözleri ve önceki pozisyon filtreleme durumu bağımsız olarak sıfırlanmıyor. Oturum yokluğu araya girerse `active_key` temizlendiği için aynı sorun yolunun oluşması gerekmez.

**Uygulama:** Seçilen oturum değişimini açık bir nesil/kimlik geçişi olarak işle. Parça metadatası aynı olsa da oturuma ait durumları yenile; her poll'da artwork indirmeye dönme. Metadata, kapak ve sözlerin tek yayında aynı kimliğe ait olduğunu koru.

**Kabul:** Aynı metadata, farklı oturum ve farklı kapak; aynı oturumda normal poll; stop/start; oturumun kaybolup dönmesi. Yeni oturumda yeni kapak gelmeli, normal poll'da tekrarlı indirme olmamalı.

## C08 — CI'daki hedef ve görsel doğrulama kapsamı tamamlanmalı

**Kanıt:** `.github/workflows/build-1.21.11.yml:26`, `scripts/Test-Offline.py:65`, `.github/workflows/build.yml`, `scripts/Test-SkyShader.py`, `scripts/Test-VisualRuntime.py:153`.

1. Hedefli 1.21.11 workflow'u `Test-Offline.py` çağrısına `--minecraft-version 1.21.11` vermiyor. Script'in varsayılanı `1.21.1`; bu adımın C++ fixture'ları bu hedefle derleniyor. Bu, Windows DLL'in yanlış sürümle derlendiği anlamına gelmez: Windows komutu 1.21.11 seçiyor.
2. Sky'nin gerçek shader derleme testi iki workflow'da da çağrılmıyor. Mevcut fake GL testi konum aramalarını ve initialize davranışını sınar; GLSL derleyicisinin yerini tutmaz. “1.000 kare” döngüsü gerçek renderer'ı 1.000 kez çalıştırmıyor, önbellekteki alanları okuyor.

**Uygulama:** Hedefli test çağrısına doğru sürümü ver; gerekli sürüme özgü testleri seç. Ayrı bir adımda EGL/Mesa ve Python bağımlılıklarını açıkça kurup `Test-SkyShader.py` çalıştır. Eksik GL ortamını başarı sayma. Mevcut Linux test job'ı ve Windows `needs` bağımlılığını yeniden oluşturma; bunlar zaten var.

**Kabul:** Log'da hedef 1.21.11 olmalı. Kontrollü GLSL hatası test adımını başarısız yapmalı ve DLL job'ını engellemeli. Fake GL testi ve gerçek shader testi farklı amaçlarla kalmalı. Gerçek Windows GPU görsel kabulü ayrıca yapılmalı.

## C09 — Haste testinde etki seviyesi hesabı değiştirmiyor

**Kanıt:** `scripts/Test-TriggerTiming.py:40`, `:43`, `:178`.

Fixture'da `hasteLevel` alanına 0/1/2 yazılıyor; fakat `cooldown(float)` bu alanı kullanmadan önceden atanmış `cooldown` değerini döndürüyor. Dolayısıyla üç geçiş aynı eşik davranışını test ediyor. Oyun tarafından dönen cooldown'un dikkate alındığı sınanıyor; Haste'ın gerçek sürümde saldırı hızına nasıl yansıdığı ve doğru mapping'in kullanıldığı sınanmıyor.

Bu bir doğrulama açığıdır; tek başına üretim cooldown hesabının yanlış olduğunu kanıtlamaz. Oyunun zaten uyguladığı etkiyi ikinci kez elle çarpmak yeni hata yaratabilir.

**Uygulama:** Önce hedef sürümde kullanılan gerçek cooldown API'sini ve etki/attribute bağlantısını doğrula. Aynı silahla Haste yok/I/II için API değerlerini kontrollü tick noktalarında kaydet. Üretim davranışını ancak bu kanıt gerektiriyorsa değiştir. Fixture çıktısını gerçek Haste mekaniği testi gibi sunma.

**Kabul:** Etkili/etkisiz durumda gerçek API sonucu doğru okunmalı. Son hedef/ray doğrulaması, hedef dışına çıkıp geri girince korunan cooldown ve düşük hasarlı tekrar vuruşu önleyen durum korunmalı. Bu tur gerçek oyun testi yapılmadı.

## C10 — Glow ESP native yolu mevcut glow sahipliğini ayırmıyor

**Kanıt:** `src/modules/visual/glow_esp.cpp`, `run()` ve `on_disable()`.

Native run diğer oyuncuların glow durumunu yazıyor; kapatma ise oyuncular üzerinde `setGlowing(false)` ve `setFlag(6,false)` çağırıyor. Hangi değerin modül tarafından değiştirildiği ve önceden sunucu/oyun etkisi nedeniyle açık olup olmadığı kaydedilmiyor. Run yerel oyuncuyu atladığı halde disable taramasında aynı dışlama da yok. Sonraki sunucu güncellemesi görünümü düzeltebilir; kapanış anındaki koşulsuz silme yine tutarsızdır.

**Uygulama:** Native ve 1.21.11 bridge yollarını ayrı değerlendir. Modülün değiştirdiği nesneleri ve başlangıç durumunu sahiplik bilgisiyle takip et; sonraki oyun/sunucu değişikliklerini ezmeyen geri yükleme sözleşmesi kur. Tüm oyunculara koşulsuz false yazma yaklaşımını kaldırmadan önce davranışı testle göster.

**Kabul:** Başlangıçta glow açık oyuncu, modülün glow açtığı oyuncu, yerel oyuncu, dünya değişimi ve modül açıkken gelen yeni sunucu glow güncellemesi. Kapatma modüle ait olmayan durumu bozmamalı. Oyuncu tarama sıklığı optimizasyonu bu sahiplik düzeltmesinden ayrı ölçülmeli.

## C11 — Devir MD'si eski işleri tekrar yaptırabilecek durumda

**Kanıt:** `docs/IMPLEMENTATION_HANDOFF_2026-11-17.md` üstteki ilerleme listesi, bölüm 3 öncelik tablosu, A/B/F metinleri ve son başlangıç isteği.

Üst liste A/B/C/F uygulandı diyor. Alt tabloda ve bazı “mevcut durum” paragraflarında eski sorunlar hâlâ güncelmiş gibi anlatılıyor. Sondaki hazır başlangıç isteği hâlâ “Yalnız A görevini uygula” diyor. Bir sonraki model doğru düzeltmeyi yeniden uygulamaya veya yanlış yere değişiklik yapmaya yönlendirilebilir.

**Uygulama:** Görev başına tek güncel durum kullan: açık / kodlandı / offline doğrulandı / Windows derlendi / oyun kabulü tamamlandı. Eski hata anlatımını “düzeltme öncesi” diye işaretle. Yeni başlangıç isteği ilk gerçekten açık işe yöneltsin. Bu rapordaki işleri eski görevlerin üzerine sessizce yazma.

**Kabul:** Bir okuyucu yalnız durum tablosundan hangi kod işinin, testin ve oyun kabulünün kaldığını anlayabilmeli. Her artifact kendi kaynak commit'ine bağlı olmalı. Bu tur eski MD değiştirilmedi.

## C12 — Uzun oturum ölçümü ve güncel teslim doğrulaması eksik

**Kanıt:** Önceki handoff'ta H açık; aynı belgede son yerel değişikliklerin gerçek oyun kabulü yapılmadığı belirtiliyor. Bu tur yeni performans ölçümü veya Windows derlemesi yapılmadı.

Kuyruk sınırlama, paylaşılan medya tamponları, PMR alanı, uniform önbelleği ve keşif aralıkları uygulanmış durumda. Bunları yeniden yazmak yerine etkileri ölçülmeli. Bu rapor FPS kazancı veya bellek sızıntısı olduğu iddiasında bulunmaz.

**Ölçüm akışı:**

1. Aynı Minecraft sürümü, donanım, çözünürlük, dünya, oyuncu sayısı ve ayarlarla karşılaştırılabilir başlangıç belirle.
2. Isınma sonrası 0/10/30/60 dakika örnekleri al. Ortalama FPS yanında p95/p99 frame süresini, worker süresini, process private bytes'ı, thread sayısını ve mümkünse GPU belleğini kaydet.
3. Sky, Nametag, ESP, Spotify ve kozmetikleri tek tek, sonra birlikte aç. Modül başına maliyeti toplam maliyetten ayır.
4. En az 20 aç/kapat, dünya değişimi, Spotify parça/oturum değişimi ve Sky hızlı preset değişimi uygula. Önceden seçilmiş sabit bekleme sonrası kaynakların ne kadarının geri geldiğini kaydet.
5. Worker ve render ölçümünü ayrı tut; JNI ref/GL nesnesi gibi sayaçlar eklenecekse sahip olduğu thread'de güncellensin ve ölçüm maliyeti hesaba katılsın.
6. Son kaynak ağacını Windows'ta derle, commit/run/artifact ilişkisinden emin ol ve oyun kabulünü o DLL ile yap.

**Kabul:** Tekrarlanabilir ham ölçüm tablosu; kötüleşen senaryoların da raporu; sürekli büyümenin cache kapasitesi mi kayıp kaynak mı olduğunun ayrılması. Ölçüm olmadan “daha hızlı” veya “sızıntısız” sonucu yazılmamalı.

## 3. Sonraki uygulama için iş akışı

1. Önce bu rapordaki sembollerin mevcut dosyalarda aynı kaldığını doğrula. Satır numaraları bu anlık ağaca aittir.
2. C01 için kart/profil/worker davranışını aynı testte yeniden üret; küçük düzeltme ve regresyonlarını tamamla.
3. C02'yi native kaynak sahipliği üzerinden düzelt. C03 için önce render durması testini yap; kanıt olmadan genel shutdown refactor'ı başlatma.
4. C04 ve C05'i ayrı aşamalarda çöz: önce geri yükleme kaydının korunması, sonra bekleme ve yeniden açılma davranışı.
5. C06 ve C07'yi ayrı testlerle ele al. Snapshot paylaşımını, bounded extrapolation'ı ve artwork texture yeniden kullanımını koru.
6. C08'in CI kapsamını tamamla; C09'un gerçek API doğrulamasını yap; C10'un sahiplik politikasını uygula.
7. C11 belgelerini güncelle; C12 ölçüm ve güncel DLL kabulüyle teslimi tamamla.

Her işte kaydet: yeniden üretim, değişen dosyalar, korunan davranışlar, geçen testler, yapılamayan testler ve varsa kaynak/artifact kimliği. Bir maddenin yalnız kodlanmış olması oyun kabulünün tamamlandığı anlamına gelmez.

## 4. Bu tur yapılan kontroller

- `python3 scripts/Test-Infrastructure.py`: başarılı; 55 modül ID'si, 128 derleme girdisi ve proje/manifest sınırları doğrulandı.
- `python3 -m unittest discover -s tests -p test_repository_hygiene.py -v`: 8 test başarılı.
- `python3 scripts/Test-Offline.py --test key_codes --test public_config --test module_runtime --test spotify_snapshot --jobs 2`: 4 test hedefi başarılı. Bu çağrı varsayılan 1.21.1 hedefini kullanır.
- Geçici C++ fixture: gerçek worker basılı-tuş bloğu Insert engelinin scan dalında aşılabildiğini; gerçek Spotify poll bloğu 100 → 99 saniye seek'inin 100'e bastırıldığını doğruladı. Tam uygulama entegrasyon testi değildir; depoya test kodu eklenmedi.
- `git diff --check`: başarılı.
- Gerçek Minecraft, Windows SMTC, Windows DLL derlemesi, uzun oturum ölçümü ve yeni GitHub workflow çalıştırması: yapılmadı.

Geçen testlerle yukarıdaki bulgular çelişmiyor: örneğin `module_runtime` testi worker'ın VK/scan dallarını çalıştırmıyor; snapshot testi Spotify `poll()` filtresini kapsamıyor. Yeni düzeltmelerin testleri doğrudan bu eksik yolları hedeflemeli.
