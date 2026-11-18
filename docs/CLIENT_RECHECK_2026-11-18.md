# Client yeniden inceleme — 18 Kasım 2026

Bu dosya ilk incelemeyi ve ardından yapılan düzeltme/test teslimini birlikte kaydeder. Güncel uygulama ve teslim bilgileri üst bölümlerdedir; R01–R07 bulgu açıklamaları düzeltme öncesi tarihsel kanıtlardır.

## Doğrulanmış teslim — 18 Kasım 2026

- Hedef: **Fabric 1.21.11 Windows x64 Release**.
- Derlenen kaynak: `b4704dc0016d1a09501b692ac113259e3e9780e5`; dal: `codex/client-recheck-20261118`.
- [GitHub çalıştırması](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44279607803): **success**. Ubuntu testleri/EGL shader ve Windows Java köprü/native derleme adımları geçti. Windows işi 10 dakika 12 saniye sürdü.
- [DLL artifact'ı](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44279607803/artifacts/14415654916): `Onestep-1.21.11-Windows-x64`; ZIP 17.255.479 bayt. Saklama bitişi: 2 Aralık 2026, 10:01 UTC.
- GitHub'ın artifact ZIP'i için bildirdiği digest: `sha256:e4604fa35c0467575fbeb97e67160dc1e560c30b4c3193fd8e8f80c9ce1edf5c`. Ortam artifact indirmesine erişemediğinden yerel PE/DLL hash kontrolü yapılmış sayılmıyor.
- Yerelde tam 1.21.11 offline paketi, GrimUI, VisualRuntime, ClientConsistency ve üç yeni regresyon script'i geçti. Native Glow/Full Bright/Nametag kaynakları ayrıca 1.21.11 profiliyle GCC syntax kontrolünden geçti.
- Gerçek Minecraft, Win32 pencere/context kabulü, SMTC, ThreadSanitizer ile tüm client, Haste ve uzun oturum ölçümü yapılmadı. Başarılı fixture/derleme bu kabullerin yerine geçmez.

Bu teslim kaydı kaynak derlemesinden sonraki belge güncellemesidir; DLL'in kaynak kimliği yukarıdaki commit'tir. `44279054107` eski çalışma, son initialization korumasını derlemeye almak için iptal edildi; teslim artifact'ı **44279607803** çalışmasına aittir.

## Uygulama durumu — 18 Kasım 2026

Aşağıdaki bulgular düzeltme öncesi incelemenin kaydıdır. Bu uygulama turunda R01–R07 için kod değişiklikleri yapıldı.

| İş | Uygulanan çözüm | Yerel doğrulama |
| --- | --- | --- |
| R01 | İstenen modül durumu, GUI draw/loader ve yayımlanan HWND atomik. Toggle CAS ile tek işlem; ImGui yerel bool kullanıyor. Lifecycle callback'leri worker'da kalıyor. | Gerçek toggle helper'ıyla iki thread/200.001 toggle; GrimUI ve config testleri. Tüm client'ın ThreadSanitizer kabulü değildir. |
| R02 | GL/Win32 backend initialization durumları ayrı; yalnız kurulmuş backend kapatılıyor, cleanup tekrar çağrılabilir. | Üretim uninit/shutdown dalı: ilk frame öncesi unload, kısmi init, çift cleanup. |
| R03 | WndProc kayıtlı owner HWND'ye geri yükleniyor. Yaşayan başka pencerenin swap'ı normal ve kapanış yollarında pas geçiliyor. | İki HWND, context edinme hatası ve retry; callback'lerin oyun context'inde, UI kaynaklarının UI context'inde bırakılması. |
| R04 | Metadata refresh saati ile timeline sample saati ayrıldı. Bekleme sonrası timeline yeniden okunuyor; position/sample_wall/sampled_at birlikte alınıyor. | Gerçek poll örnekleme bloğu + gerçek snapshot hesabı: 0/100/2000 ms, pause ve aynı metadata ile oturum değişimi. |
| R05 | Glow undo kaydında pending/applied ayrımı var. Başarısız ilk yazma tekrar deneniyor; retry öncesi dışarıdan gelen true sahiplenilmiyor. | Gerçek run: geçici/sürekli hata, pending disable, dış durum, eksik tarama, ID reuse ve dünya değişimi. |
| R06 | Dünya kapsamlı entity ID map'i + Java identity kontrolü; seen generation ile üyelik taraması. | 100/200/400 oyuncuda 101/201/401 JNI kimlik karşılaştırması; eski ölçümler aşağıda tarihsel. Gerçek FPS ölçülmedi. |
| R07 | Hook kurulumu tamamlanan adımları sahiplenen RAII transaction kullanıyor. Başarısız aşamalar yalnız bu girişimin kaynaklarını geri alıyor. | Initialize/Create/Enable hataları ve başarılı commit için cleanup çağrı sayıları. |

Yeni testler: `scripts/Test-RendererLifecycle.py`, `scripts/Test-GlowRuntime.py`, `scripts/Test-SpotifyTimeline.py`. İki workflow'un Ubuntu test job'ına eklendiler. Gerçek Windows/oyun kabulünün yerine geçmezler. Windows derlemesi başarılı; teslim kaydı yukarıdadır.

Önceki raporda açık kalan, render tamamen durduğunda güvenli unload'un tamamlanamaması bu yedi maddenin dışında kalır. Context edinilemiyorsa kaynak sahipliği korunup uygun callback beklenir; zorla DLL boşaltılmaz. Gerçek Haste, SMTC, GPU ve uzun oturum kabulü ayrıca yapılmalı.

## Sonuç

**7 bulgu:** 3 yüksek öncelikli yaşam döngüsü/thread sorunu; 4 zamanlama, hata kurtarma, ölçeklenme veya başlangıç temizliği sorunu. Başarılı derleme bu davranış yollarını doğrulamıyor. Aşağıdaki kaynak bulgularının tamamının gerçek oyunda yaşandığı iddia edilmez.

| ID | Öncelik | Bulgu | Doğrulama |
| --- | --- | --- | --- |
| R01 | P1 | UI ve worker aynı modül durumunu senkronizasyon olmadan kullanıyor | Kaynak ve thread çağrı zinciri |
| R02 | P1 | İlk render initialization'ından önce unload, kurulmamış ImGui backend'lerini kapatıyor | Kaynak + gerçek backend önkoşulları |
| R03 | P1 | Pencere değişiminde eski WndProc yeni pencereye geri yazılıyor | Kaynakta pencere/handle akışı |
| R04 | P2 | Spotify metadata beklemesini ilerleme süresine iki kez katabiliyor | Kaynak + zaman sözleşmesi fixture'ı |
| R05 | P2 | Native Glow ilk yazma hatasından sonra tekrar denemiyor | Gerçek üretim `run()` fonksiyonuyla yeniden üretildi |
| R06 | P2 | Native Glow kimlik eşlemesi yaklaşık karesel büyüyor | Gerçek üretim `run()` fonksiyonunda çağrı sayımı |
| R07 | P2 | Hook başlangıcı yarıda kalırsa önceki adımlar geri alınmıyor | Kaynakta hata yolları |

R05/R06, son native Glow undo tasarımında kalan eksiklerdir. Önceki `3174c0d` kodunda flag her tur yazılıyordu; yeni kod mevcut undo kaydını bulunca yazmayı atlıyor. Yeni tasarımın geri yükleme sahipliği yararlı; eski koşulsuz kapatma davranışına geri dönülmemeli.

## R01 — Modül ve GUI durumlarında thread sahipliği belirsiz

**Kanıt:** `src/modules/core/module.hpp:55`, `src/ui/grim_ui.cpp:233` ve `:740`, `src/app/module_loop.cpp:48`, `src/modules/core/module_runtime_state.hpp`, `src/gui/gui.hpp:20`, `src/app/runtime.cpp:52`.

`module::enabled` sıradan `bool`. Render/UI yolu bunu doğrudan değiştiriyor; worker tuş yolu da değiştiriyor ve lifecycle fonksiyonları okuyor. Arada bu alanı koruyan ortak mutex, atomik erişim veya komut kuyruğu görünmüyor. `gui::draw` da render tarafından değiştirilip worker tarafından okunuyor; kapanışta worker tarafından yazılıyor.

Bu C++ açısından veri yarışı oluşturabilecek bir erişim düzenidir. Yanlış lifecycle geçişi, kaybolan toggle veya eski durum gözlemi mümkün; bu tur gerçek oyunda çökme gözlenmedi. Atomik `keybind` ve kilitli settings store bulunması, ayrı `enabled` alanını otomatik olarak korumuyor.

**Düzeltme yaklaşımı:** Modül açma/kapama komutlarını tek sahip thread'e ilet; render'ın kullanacağı durumu açık bir snapshot ile yayımla. Alternatif atomik alan çözümünde read-modify-write toggle ve ImGui'nin `bool*` ihtiyacı ayrıca tasarlanmalı. Sadece `bool` tipini atomik yapıp arayüze pointer cast etmek doğru çözüm değil.

**Kabul:** Aynı modüle UI/tuş/panic/config üzerinden eşzamanlı istekler; disable/enable callback sayılarının sözleşmeye uyması; mümkün olan platformda gerçek ortak durum kodunun ThreadSanitizer ile kontrolü. Mevcut tek thread'li `module_runtime` testi bunu kapsamıyor.

## R02 — İlk frame gelmeden unload için initialization aşaması kontrol edilmiyor

**Kanıt:** `src/gui/gui.cpp:75`, `:610`, `:635`, `:597`; `src/app/runtime.cpp:105`; `include/imgui/imgui_impl_win32.cpp:209`, `include/imgui/imgui_impl_opengl3.cpp:384`.

`gui::init()` hook'u kurup başarıyla dönebiliyor; ImGui/backend kurulumu ilk swap callback'inde yapılıyor. Bu ilk callback gelmeden worker unload isteği verirse, sonraki callback önce shutdown dalına giriyor. Bu dal `run_once`/backend initialization kontrolü yapmadan `uninit()` çağırıyor.

`uninit()` iki ImGui backend shutdown fonksiyonunu koşulsuz çağırıyor. Gerçek Win32 backend kodu backend verisinin null olmamasını bekliyor; assert kapalı Release yolunda da bu veriyi kullanıyor. Dolayısıyla bu erken kapanış yolu yalnız “henüz frame gelmediği için bekleme” sorunu değil; callback geldiğinde kurulmamış backend'e erişme riski de var.

**Düzeltme yaklaşımı:** Hook kuruldu / context oluştu / ImGui oluştu / backend'ler kuruldu durumlarını ayrı tut. Cleanup yalnız gerçekten kurulmuş kaynakları bıraksın ve tekrar çağrılabilir olsun. Kaynakların sahibi olan GL context kuralı korunmalı.

**Kabul:** İlk swap'tan önce End/unload; initialization'ın her aşamasında başarısızlık; normal unload; iki kez cleanup. Kurulmamış backend için shutdown çağrılmamalı. Windows oyunu veya Win32/OpenGL yaşam döngüsü harness'iyle kabul henüz yapılmadı.

## R03 — Pencere değişiminde geri yükleme yanlış HWND üzerinden yapılıyor

**Kanıt:** `src/gui/gui.cpp:629`, `:631`, `:600`, `:656`.

İlk pencerede `original_WndProc` kaydediliyor. Sonraki callback farklı `current_window` ile gelirse `uninit(current_window, device)` çağrılıyor. İçeride bu yeni HWND'ye, eski pencerenin `original_WndProc` değeri geri yazılıyor. Eski `window` henüz güncellenmemiş olmasına rağmen geri yüklemede kullanılmıyor.

İki pencere de yaşıyorsa eski pencerede DLL'in WndProc'u kalabilir; yeni pencereye ait original proc da bozulabilir. DLL unload sonrası eski pencerenin callback'i tehlikeli olabilir. Eski pencerenin tamamen yok olduğu normal yeniden yaratma senaryosunda bu sonuçların hepsi oluşmak zorunda değil; iki canlı HWND koşulu ayrıca sınanmalı.

**Düzeltme yaklaşımı:** Hook'un ait olduğu HWND, original WndProc, HDC ve GL context kimliklerini birlikte sakla. Eski pencerenin geri yüklemesini onun kaydı üzerinden tamamla; yeni pencerenin original proc'unu değiştirmeden yakala. Global swap hook'una başka pencere geldi diye o pencereye eski geri yükleme kaydını uygulama.

**Kabul:** İki farklı orijinal proc'u olan iki canlı HWND; eski pencerenin önce/yakında yok olması; context değişimi; unload. Sky'nin oyun context'ine ait VAO'su ile UI context kaynakları aynı sahiplikte varsayılmamalı.

## R04 — Spotify pozisyonu ile sampled_at farklı anları temsil edebiliyor

**Kanıt:** `src/modules/visual/spotify_source.cpp:847`, `:852`, `:877`, `:897`; `src/modules/visual/spotify_source.hpp:60` çevresindeki `extrapolate()`.

`now`, metadata isteği beklenmeden önce alınıyor. Bekleme sonrası `real_position`, daha sonraki `winrt::clock::now()` ile güncelleniyor. Ama `sampled_at`, hâlâ ilk `now` oluyor. Snapshot bu eski zaman damgasından itibaren bir daha süre ekliyor.

Örnek: t=0'da pozisyon 100; metadata 2 saniye beklesin. t=2'de hesaplanan gerçek pozisyon 102, fakat sampled_at=t=0. Aynı t=2'de snapshot 104 gösterir. Yeni poll bunu yeniden düzeltebilir; bu da ileri/geri sıçrama ve söz vurgusu hatasına dönüşebilir.

Üretim `now_playing::extrapolate()` fonksiyonuyla bu zaman sözleşmesi örneği çalıştırıldı: 102 → 104 sonucu doğrulandı. Gerçek WinRT `poll()` bu Linux ortamında çalıştırılmadı; beklemenin gerçek sıklığı/uzunluğu ölçülmedi.

**Düzeltme yaklaşımı:** Timeline pozisyonu ve sample zamanını aynı ölçüm anına bağla; metadata refresh zamanını ayrı tut. Metadata bekledikten sonra timeline/playback değişmiş olabileceğini de dikkate al. Önceki küçük seek ve oturum kimliği düzeltmelerini koru.

**Kabul:** Sahte saatle 0/100 ms/2 s metadata gecikmesi; pause/seek/track change; snapshot'ta bekleme süresinin tek kez sayılması. Yalnız saf `extrapolate()` testi yerine poll ile sample üretiminin birlikte kontrol edilmesi gerekiyor.

## R05 — Native Glow başarısız ilk uygulamayı tamamlanmış gibi tutuyor

**Kanıt:** `src/modules/visual/glow_esp.cpp:69`, `:75`, `:76`.

Undo kaydı, `setFlag()` çağrısından önce oluşturuluyor. Bu, kaynak sahipliği bakımından doğru. Fakat çağrı hata verdiğinde kayıt “uygulama bekliyor” durumunu taşımıyor. Sonraki turda aynı nesne kayıt içinde bulununca `continue` oluyor; flag yeniden denenmiyor.

Üretim `run()` ve cleanup fonksiyonlarıyla fixture sonucu:

```text
1. tur: setFlag hata verdi; flag=false; undo kaydı=1; yazma denemesi=1
2. tur: hata kalktı; flag=false; yazma denemesi hâlâ=1
```

**Kapsam:** Diğer sürümlerin native backend'i ve 1.21.11 native fallback'i. 1.21.11'in normal Java yolu farklı kod kullanıyor; bu bug ona otomatik olarak genellenmiyor.

**Düzeltme yaklaşımı:** Undo sahipliği ile uygulama başarısını ayrı durumlar olarak tut. Başarısız ilk yazmayı tekrar dene; kullanıcı/sunucu tarafından sonradan yapılan değişiklikleri zorla ezme. Kayıt bulundu diye her tur körlemesine flag yazmaya geri dönme.

**Kabul:** İlk yazma hatası ve sonraki başarı; sürekli hata; hata sonrası disable; sonraki dış güncelleme; dünya değişimi. Mevcut consistency testi yalnız `release_native_glow()` çalıştırdığından `run()` hatasını kaçırıyor.

## R06 — Native Glow aynı oyuncular için her tur karesel kimlik taraması yapıyor

**Kanıt:** `src/modules/visual/glow_esp.cpp:66`, `:79`; ortak worker aralığı `src/app/runtime.cpp:30`.

Her oyuncu tüm `glow_records` içinde `find_if` ile aranıyor. Ardından her kayıt `seen` vektöründe yeniden `find` ile aranıyor. Üretim `run()` fonksiyonunda, aynı dünya ve aynı oyuncularla ikinci worker turunda ölçülen JNI kimlik karşılaştırmaları:

| Oyuncu | `IsSameObject` çağrısı / tur |
| --- | --- |
| 100 | 5.051 |
| 200 | 20.101 |

Bu sayılar, ek `seen` pointer karşılaştırmalarını kapsamıyor. Oyuncu sayısı iki katına çıkınca ölçülen iş yaklaşık dört katına çıkıyor. Ortak worker'da 5 ms bekleme var; gerçek tur frekansı işlem süresine de bağlı. Bu ölçüm gerçek FPS kaybı ölçümü değildir.

**Düzeltme yaklaşımı:** JVM kimliğini koruyan indeksli sahiplik tasarla; geçici JNI handle adreslerini doğrudan kalıcı anahtar sayma. UUID/entity kimliği kullanılacaksa dünya nesli ve yeniden kullanım ayrımı olmalı. `seen` için sabit üyelik sorgusu veya nesil işareti düşün; doğru undo sahipliğini koru.

**Kabul:** Aynı davranış altında 100/200/400 oyuncu karşılaştırma sayıları ve hata sonrası kayıt temizliği. Algoritmanın tam çalışma süresini ve worker üzerindeki etkisini ayrı ölç. Oyuncu sayısını kısarak veya ortak worker beklemesini değiştirerek sorunu gizleme.

## R07 — Hook kurulum hatalarında rollback eksik

**Kanıt:** `src/gui/gui.cpp:1013`, `:1019`, `:1025`; `src/app/runtime.cpp:105`.

`MH_Initialize()` başarılı olduktan sonra CreateHook hata verirse fonksiyon doğrudan false dönüyor. CreateHook başarılı olup EnableHook hata verirse de kurulan hook geri kaldırılmadan dönüyor. `renderer_started` ancak `gui::init()` başarıyla döndükten sonra true oluyor; destructor bu başarısız başlangıçlarda GUI cleanup'ını çalıştırmıyor.

Bu, başlangıç adımlarının rollback sözleşmesinin eksik olduğunu gösteriyor. Gerçek MinHook hata enjeksiyonu veya process kaynak sızıntısı ölçümü yapılmadı; bu yoldan kesin oyun çökmesi iddia edilmiyor.

**Düzeltme yaklaşımı:** Sahip olunan engine/hook aşamalarını yerel bir RAII rollback kaydıyla yönet. Yalnız bu initialization'ın oluşturduğu kaynakları geri al. Başkasının hook engine durumunu sahiplenip koşulsuz kapatma. Kısmi renderer init ile R02 temizliğini uyumlu tasarla.

**Kabul:** Initialize/CreateHook/EnableHook için ayrı hata enjeksiyonu; beklenen RemoveHook/Uninitialize sayıları; başarılı başlangıçta rollback'in çalışmaması; tekrar başlangıçta temiz durum.

## Önceki raporla karşılaştırma

- Insert'in scan dalındaki engeli, ayrılmış tuş normalizasyonu ve kart yakalaması mevcut. Eski C01'i yeniden açık hata saymadım.
- Full Bright gerçek option ref'ini tutuyor ve başarısız restore'u koruyor. Eski C02'nin “worker cleanup yok” tespiti artık geçerli değil.
- Nametag taşıyıcı hatasında ref/undo kaydını koruyor; bütçeli/fair tekrar deneme ve bekleme bilgisi var. Eski C04'ün koşulsuz kayıt silme davranışı düzeltilmiş.
- Spotify küçük geri seek filtresi kaldırılmış, oturum değişimi track geçişine dahil edilmiş. R04 başka bir zaman damgası sorunu.
- Sky shader testi ve hedefli 1.21.11 test seçimi workflow'da mevcut.
- Eski C03'teki render durunca unload beklemesi hâlâ açık. R02/R03 bunun yanında ayrı initialization/pencere yollarını gösteriyor.
- Haste'ın gerçek oyun kabulü ve uzun oturum/GPU ölçümü hâlâ yapılmış sayılmıyor.
- Glow'daki gizli eski kutu/name/health config alanlarını boş görünen render seçeneklerinden dolayı hata saymadım; schema bunları açıkça compatibility için gizli tutuyor.

## Bu tur doğrulama

- `Test-ClientConsistency.py`: başarılı; buna rağmen Glow `run()` tekrar-deneme boşluğu yukarıdaki ayrı fixture ile bulundu.
- `Test-VisualRuntime.py`: başarılı; gerçek poll timestamp akışını ve Win32 hook başlangıç/kapanışını kapsamıyor.
- `Test-Infrastructure.py`: başarılı; 55 modül ID ve 128 derleme girdisi doğrulandı.
- Repository hygiene: 8 test başarılı. `git diff --check`: başarılı.
- Depo dışındaki geçici C++ fixture: üretim Glow `run()` tekrar deneme ve karşılaştırma sayıları; üretim Spotify snapshot hesaplaması çalıştırıldı. JVM/Win32/WinRT doubles kullanıldığı için gerçek oyun sonucu iddiası yok.
- ThreadSanitizer, Windows initialization/pencere kabulü, gerçek SMTC ve oyun oturumu: yapılmadı.

Önerilen sıra: **R02 → R03 → R01 → R05 → R04 → R06 → R07**. R01 geniş kapsamlı olduğundan önce sahiplik planı çıkarılmalı. Yeni testler gerçek caller/callee akışını da kapsamalı; yalnız cleanup helper'larının geçmesi bütün yaşam döngüsünü doğrulamıyor.
