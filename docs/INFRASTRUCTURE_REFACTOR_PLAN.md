# Thedorius Lite altyapi yenileme plani

## Hedef

UI yeniden yazilmadan once oyun mantigi, JNI erisimi, modul kaydi, ayarlar,
config ve derleme bagimliliklari birbirinden ayrilacak. Yeni bir modul eklemek
yalnizca kendi klasorunu ve kayit tanimini degistirecek; tek bir ortak baslikta
yapilan degisiklik tum projeyi yeniden derlemeyecek.

## Uygulama durumu (tamamlandi)

- 41 modul sinifi kendi kategori basligina ayrildi. `modules.hpp` 1.335
  satirdan ince bir uyumluluk basligina indirildi.
- 42 runtime modul kaydi (normal ve blatant Anchor ayri kayitlar) benzersiz
  stable ID denetimi yapan factory descriptor kataloguna tasindi.
- Click GUI ve Panic kendi Client kaynak dosyalarina ayrildi; lifecycle yalniz
  init/shutdown sorumlulugu tasiyor.
- `module.hpp` artik `cache.hpp`/`mappings.hpp` include etmiyor. Oyun SDK'si
  yalniz gercek implementasyon `.cpp` dosyalarinda goruluyor.
- `jni_context`, `client_context` ve `client_dispatcher` eklendi. Modullerdeki
  yerel JNI env/exception kopyalari kaldirildi.
- `hotbar_service`, `inventory_service`, `target_service`, `attack_service`
  ve `input_service` eklendi. Modul kaynaklarinda dogrudan `SendInput` ve
  `selectedSlot.set` kullanimi kalmadi.
- Combat engine basliklari combat klasorune tasindi. Referanssiz eski
  `aim_engine.hpp/.cpp` tamamen kaldirildi.
- Derlenmeyen eski UI/HUD `#if 0` bloklarindan 793 satir silindi.
- Visual Studio filters dosyasi gercek proje girdilerinden yeniden uretildi.
- `Build-Release.ps1`, `Test-Infrastructure.ps1` ve filtre yenileme araci
  eklendi. Altyapi testi 41 ayrik basligi, 42 benzersiz katalog kaydini,
  eksik proje yolunu ve servis siniri ihlallerini denetliyor.
- Config field round-trip testi gercek MSVC x64 derlemesiyle calistirildi.

Tek parca `mappings.hpp` su an ABI uyumluluk girisi olarak korunuyor; fakat
temel modul ve UI basliklarindan ayrildigi icin artik mappings degisikligi UI
ve katalog derlemesini tetiklemiyor. Fiziksel domain dosyalarina bolme, yeni
Minecraft surumu icin mapping manifesti degistirilirken yapilabilir.

## Denetim ozeti (1 Agustos 2026)

- `build` klasoru 3,35 GB idi. Eski hedef adlarina ait 20'den fazla PCH,
  eski test DLL'leri, hiz denemeleri ve analiz ciktilari temizlendi.
- Proje kaynaklarinda gercek bos dosya bulunmadi. Dort sifir baytlik `.inl`
  dosyasi GLM dagitiminin bilincli parcasi oldugu icin korundu.
- `modules.hpp` 1.335 satir ve neredeyse butun modul siniflarini tasiyor.
- `gui.cpp` 4.295 satir; shell, input hook, sayfalar, widgetlar ve loader
  sinirlari birbirine karismis durumda.
- PCH, degisken proje basliklarini (`modules.hpp`, mappings, cache) iceriyordu.
  Bu nedenle kucuk bir modul degisikligi tam derlemeyi tetikliyordu.
- MinHook hem kaynak koddan derleniyor hem de hazir `.lib` olarak linkleniyordu.
- Visual Studio filters dosyasinda tasinmis 21 eski kaynak yolu bulunuyor.
- Auth sunucusunun yerel ozel anahtarlari proje klasorunde. Silinmedi; fakat
  `.gitignore` ile yanlislikla commit edilmesi engellendi.

## Hedef klasor yapisi

```text
src/
  app/                    DLL girisi, baslatma, kapanma, lifecycle
  platform/
    windows/              pencere, raw input, SendInput, hook adaptoru
    jni/                  thread attach, exception, global/local ref RAII
  sdk/
    minecraft/            mappings, wrappers, client context
    math/                 projection ve oyun matematigi
  core/
    events/               tick, render, input ve world lifecycle olaylari
    modules/              Module, ModuleRegistry, metadata ve settings
    config/               yerel/public config serilestirme
    services/             ortak oyun islemleri
  modules/
    combat/
    player/
    world/
    render/
    visual/
    client/
  services/
    auth/
    configs/
    friends/
  ui/                     sonraki asamada tamamen yeniden yazilacak
tests/
```

Her modul kendi klasorunde `module.hpp` ve `module.cpp` tasiyacak. Bir modulun
ozel state'i veya ayarlari baska bir modulu derlemeye zorlamayacak.

## Faz 1 - Derleme ve baslik sinirlari

1. PCH yalnizca Windows, JNI, ImGui ve stabil standart kutuphane basliklarini
   icerecek. Proje basliklari PCH'de bulunmayacak. Bu adim uygulandi.
2. `modules.hpp` parcalanacak:
   - Her modul kendi basligina tasinacak.
   - Ortak dosyada yalnizca registry/factory deklarasyonlari kalacak.
   - Modul kaynaklari yalnizca kullandiklari servisleri include edecek.
3. Kokte duran aim/trigger/anchor engine basliklari ilgili combat alt
   klasorlerine tasinacak.
4. Visual Studio project ve filters girdileri gercek klasorlerle yeniden
   olusturulacak; eski 21 yol tamamen kaldirilacak.
5. ImGui demo ve gereksiz GLM explicit-instantiation birimi derlemeden
   cikarilacak. MinHook yalnizca kaynak koddan derlenecek. Bu adim uygulandi.

Kabul kriteri: Tek bir modul `.cpp` degisikliginde PCH ve ilgisiz moduller
yeniden derlenmemeli.

## Faz 2 - Runtime ve thread modeli

Tek ve acik bir thread kurali uygulanacak:

- Minecraft/JNI state degistiren islemler yalniz client/render thread'inde.
- Worker thread yalniz zamanlama, salt-okunur snapshot ve ag isleri yapacak.
- Bir modul worker'dan oyun islemi istemek zorundaysa `client_dispatcher`
  kuyruguna komut birakacak.

Ortak tipler:

- `jni_context`: thread attach ve exception temizleme.
- `client_context`: instance, player, world, interaction manager ve ekran.
- `client_snapshot`: worker'lar icin sahipliksiz, salt-okunur veri.
- `client_dispatcher`: client thread'inde calistirilacak komut kuyrugu.
- `global_ref<T>` / `local_frame`: JNI referans omru icin RAII.

Kabul kriteri: Modul dosyalarinda tekrar eden `ensure_client_jni_env` ve
`clear_java_exception` fonksiyonlari kalmamali.

## Faz 3 - Ortak oyun servisleri

Modullerde kopyalanan davranislar merkezilestirilecek:

- `hotbar_service`: item arama, slot secme, sunucuyla senkronizasyon ve geri donus.
- `inventory_service`: inventory slotlari ve guvenli click islemleri.
- `target_service`: crosshair, mesafe, gorus, friend ve entity filtreleri.
- `input_service`: fiziksel/yapay input ayrimi ve tek bir input koordinasyonu.
- `attack_service`: cooldown, swing ve vanilla attack akisi.
- `render_service`: projection, kamera snapshot'i ve overlay cizim sirasi.

Mace Swap, Shield Breaker, Anchor, Hit Crystal, Auto Tool ve Pearl Macro ayni
slot servisini kullanacak. Bir modulu duzeltmek digerindeki kopya kodu geride
birakmayacak.

## Faz 4 - Modul sistemi ve ayarlar

- Modul kimligi, gorunen isimden tamamen ayrilacak ve sabit kalacak.
- Kayit `new module()` listesi yerine factory descriptor tablosuyla yapilacak.
- Metadata: id, kategori, varsayilan durum, aciklama anahtari ve yetenekler.
- Ayarlar ImGui cagrisi olmayacak; `bool_setting`, `number_setting`,
  `range_setting`, `enum_setting`, `color_setting` olarak tanimlanacak.
- UI daha sonra bu descriptorlari cizecek; config ayni descriptorlari
  serilestirecek.
- Modul state'i ile UI state'i birbirinden ayrilacak.

Kabul kriteri: Yeni bir slider eklemek icin GUI dosyasina kod yazilmayacak.

## Faz 5 - SDK ve mappings

- Tek parca `mappings.hpp` Java, network, entity, inventory, render ve world
  basliklarina ayrilacak.
- Mapping adlari 1.21.1 surum etiketiyle tek manifestte tutulacak.
- Ham field/method cagirilari servislerin arkasina alinacak.
- JNI exception yutma yerine debug build'de sinif/metot baglami raporlanacak.

Kabul kriteri: Yanlis bir mapping hangi servis ve metotta kirildigini gostermeli;
modul sessizce calismamaya donmemeli.

## Faz 6 - Config, Friends ve Auth sinirlari

- Public config v3 korunacak; schema migrasyonu servis katmaninda olacak.
- Modul ayarlari descriptorlardan eksiksiz export/import edilecek.
- Friends oturumluk bellekte kalacak fakat oyun taramasi UI'dan ayrilacak.
- Auth heartbeat ve ag kodu modul dongusunden ayrilacak.
- Ozel signing key dosyalari proje disinda guvenli bir klasore alinacak;
  Cloudflare secret degerleri kaynak agacinda bulunmayacak.

## UI yeniden yazimina gecis kosullari

UI calismasi ancak su maddeler tamamlandiginda baslayacak:

- `modules.hpp` monoliti kaldirilmis olmali.
- Ortak JNI/input/hotbar/target servisleri kullaniliyor olmali.
- Moduller ayar descriptorlari sagliyor olmali.
- Registry, config ve modul lifecycle testleri geciyor olmali.
- Temiz Release x64 derlemesi ve ikinci incremental derleme dogrulanmis olmali.

Bu noktadan sonra yeni UI yalnizca shell, layout, animation ve descriptor
rendering ile ilgilenecek; oyun mantigi `gui.cpp` icine geri sizmayacak.
