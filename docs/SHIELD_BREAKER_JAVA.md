# Shield Breaker — Java geçişi

Tarih: 18 Kasım 2026. Davranış referansı, kullanıcının paylaştığı Argon `ShieldDisabler` sınıfıdır. OneStep modül ID’si `shield_breaker` olarak kalır; yeni bir clicker veya ikinci modül eklenmez.

## Sahiplik ve yürütme

Oyun mantığı `src/bridge/WorldBridge.java` içindeki 9 numaralı özelliktedir. Hedef, kalkanın yönü, güncel ray, silah/hotbar okuması, vanilla slot senkronizasyonu, tick zamanlaması, saldırı, el sallama ve slot geri dönüşü Java client thread’inde yürür. Fabric START_CLIENT_TICK kullanılır; event yoksa client/render çağrısından en fazla 20 Hz fallback uygulanır. Kare sayısı Switch/Hit Delay sayacı değildir.

C++ `shield_breaker` yalnız ayar/UI, etkinlik/ayar nesli ve bridge çağrılarını taşır. Worker `run()` oyun I/O yapmaz. Eski native `old_slot`, `axe_slot`, restore timer, saldırı ve Triggerbot hazırlama kodları kaldırıldı. Ortak arkadaş/Anti Bot, odak/menü izni, manuel input nesli ve saldırı sahipliği servisleri native callback olarak paylaşılmaya devam eder. Bunlar bağımsız bir native Shield Breaker motoru değildir.

## Ayarlar

| Ayar | Varsayılan | Davranış |
| --- | --- | --- |
| Switch Delay | 0 tick | Uygun hedefte balta geçişinden önce bekleme; `switch_ticks` |
| Hit Delay | 0 tick | Balta hazırlandıktan sonra bekleme; `hit_delay` |
| Switch Back | Açık | Başarılı vuruştan sonra sahip olunan slotu geri verme |
| Stun (Standalone) | Kapalı | Bağımsız modda, ikinci kez güncel hedef kontrolü ardından isteğe bağlı ikinci hit |
| Click Simulation | Kapalı | Başarılı hit sonrası oyun mouse callback’inde senkron DOWN/UP; Windows SendInput yok |
| Hold Axe | Kapalı | Başlangıçta balta tutulmasını gerektirir; kılıçtan otomatik geçmez |
| Preferred Axe Slot | 1 | Elde balta varsa onu kullanır; aksi halde tercih edilen slot, sonra diğer hotbar slotları aranır |
| Return Delay | 110 ms | Başarılı vuruş/gerçek manuel tıklama sonrası geri dönüş gecikmesi; 75–500 ms |

Eski `axe_slot` ve `return_delay` anahtarları korunur. Eski native uygulamada davranışa bağlanmamış gizli `switch_delay` alanı yalnız eski config’leri okumak/yazmak için korunur; yeni tick ayarının anahtarı `switch_ticks`’tir. Eski config’lerde yeni seçeneklerin bulunmaması geçerlidir.

Tamamlanmamış bir hazırlık iptal edilirse slot rollback’i yapılır; Switch Back kapalı olması başarısız/iptal edilmiş bir Triggerbot hazırlığının slotu sahipsiz bırakması anlamına gelmez. Başarıyla tamamlanan vuruşta Switch Back kapalıysa balta elde kalır. Kullanıcının sonradan seçtiği farklı slot geri yazılmaz.

## Triggerbot ile çalışma

- Triggerbot etkin olarak istendiğinde Shield Breaker bağımsız hit göndermez. Bu kontrol yalnız Triggerbot’un o anki geçerli hedefine bağlı değildir: On Click Only beklemesi, uygunsuz hedef ve askıya alınmış Triggerbot, Shield Breaker’a gizlice otomatik hit başlatmaz.
- Kalkan hedefinde balta hazırlığı, slot lease’i ve geri dönüş aynı Java nesnesindedir. C++ Shield hazırlama callback’i kullanılmaz. Mace Swap’ın mevcut servisi ayrı kalır.
- Sword Only filtresi otomatik balta hazırlığı yüzünden kendini reddetmez: modülün gerçekten sahip olduğu başlangıç slotu kullanılır. Kullanıcının elle tuttuğu balta, Sword Only filtresini aşmaz.
- Slot değişiminden sonra aynı hazırlama tick’inde hit yapılmaz. Oyunun güncel cooldown’u ve Triggerbot’un cooldown/reaction/deadline durumları korunur. Hedeften çıkıp girmek zamanlamayı yeniden örneklemez.
- Saldırıdan önce hedef, dünya, oyuncu, inventory, manager, slot, kalkan yönü, yeni ray, izin ve input nesli tekrar kontrol edilir. Tüm kontroller client thread’inde senkrondur.
- Stun, Triggerbot açıkken ek vuruş göndermez; Triggerbot tek saldırı sahibidir; vuruşun Swing Hand ve Click Simulation tercihleri Triggerbot ayarından gelir. Referanstaki global non-physical attack cancellation kopyalanmadı; aksi halde Triggerbot’un kendi Java hit’i de iptal edilebilirdi.

Bağımsız modda ana hit için ayrıca en az %90 oyun cooldown’u gerekir. Stun açıkken isteğe bağlı ikinci hit, referanstaki gibi aynı işlemde gönderilebilir; her ikinci hit öncesi hedef tekrar doğrulanır. Dolayısıyla bu seçenek her iki hit’in de tam hasarlı olacağı garantisi değildir. Varsayılanı kapalıdır.

## İptal ve manuel giriş

Menü/odak kaybı, disconnect, dünya/player/manager/inventory değişimi, disable, ayar nesli ve unload, önceki lease’i yeni oturuma taşımaz. Slot restore/sync başarısız olursa eski slot bilgisi hemen silinmez; aynı client thread’inde retry yapılabilir. Restore için seçili slot zaten eski değere yazılmışsa sync işlemi yine tamamlanır; bu durum manuel değişiklik diye yanlış yorumlanmaz.

Gerçek sol tıklama Java’da yalnız hazırlık yapar; ekstra saldırı göndermez. Manuel tıklama Triggerbot’un hazırlığını devralırsa Triggerbot iptali o tıklamadan önce baltayı geri alamaz; kendi zamanlı geri dönüşü devam eder. Click Simulation sol tuşa bağlı tüm KeyBinding pressed/queue durumlarını korur, ek vanilla saldırı kuyruğu bırakmaz ve aynı çağrıda UP temizliğini tamamlar.

## Doğrulama

- `scripts/Test-ShieldBreaker.py`: gerçek WorldBridge sınıfında 47 kontrol; kaynak ve gömülü classfile ayrı çalıştırılır. Oyun API’leri inert fixture’lardır; gerçek Minecraft uyumluluk kabulü değildir.
- Kontroller: tick/render ayrımı, switch/hit gecikmesi, balta yokluğu, Hold Axe, Switch Back, manuel override, menü/oturum kaybı, yön/arkadaş/using/ray filtreleri, sahiplik sonrası hedef/izin değişimi, Stun, callback queue/hold koruması, sync hatası/retry, Triggerbot Sword Only ve tek hit, On Click iptali, mutasyon hatası sonrası replay olmaması, manuel takeover, fallback tick sınırı, geçiş öncesi aday/manuel override dünya temizliği ve stop referans temizliği.
- Mevcut TriggerTiming/TriggerLifecycle testleri korunur. Test-HybridBridge yeni testi hem kaynak hem gömülü bytecode için çalıştırır. JNI sözleşme kontrolü yeni 5 bridge sözleşmesiyle 78 giriş doğrular.
- Yeni ayarların varsayılanları ve eski config alanları remaining_settings/public_config/trigger_config testlerinde kontrol edilir. Modül kataloğu 55 ID olarak kalır.
- Gerçek oyun/sunucu, shield-disable paketi, ek CPS HUD’larının callback algısı ve diğer Minecraft sürümlerinin oyun kabulü ayrıca yapılmalıdır.

## Doğrulanmış DLL teslimi

Derleme önbelleği güncellemesinden sonra bu Shield Breaker kodunu da içeren kaynaklar `main` dalında birleştirildi ve yedi hedef için yeniden derlendi. [Güncel kaynak, 1.21.1/1.21.11 artifact'leri ve doğrulama kaydı](BUILD_PERFORMANCE.md#18-kasım-2026-teslimi-ve-gerçek-ölçüm). Aşağıdaki `bfc6b8e` kaydı ilk Java Shield Breaker tesliminin tarihçesidir.

- Hedef: **Fabric 1.21.11 Windows x64 Release**.
- Kaynak commit’i: `bfc6b8e47a8a891015539e5beae7e94e3c156bad`; dal: `codex/shield-breaker-java-20261118`.
- [GitHub çalıştırması](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44344276834): **success**. Ubuntu offline/EGL ve Windows kaynak/gömülü Java köprü testleri, JNI sözleşmeleri ve native derleme başarılı. Windows işi 13 dakika 1 saniye sürdü.
- [DLL artifact’ı](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44344276834/artifacts/14447435290): `Onestep-1.21.11-Windows-x64`, ZIP 17.254.568 bayt; saklama bitişi 2 Aralık 2026, 16:25 UTC.
- GitHub’ın ZIP için bildirdiği digest: `sha256:03605dbd5204167b5184e183430796ae930647c3ce2425f458a92ef65f1841c6`. Bu bağımsız hesaplanmış DLL hash’i değildir; ortam artifact indirmesine erişemediği için yerel PE/DLL kontrolü yapılamadı.
- Yerelde tam 1.21.11 offline paketi, GrimUI, TriggerTiming/TriggerLifecycle, kaynak+gömülü Shield Breaker’ın 47 kontrolü, altı game bridge’in mevcut testleri ve repository hygiene başarılı. Modül kataloğu 55 ID olarak korundu.
- İlk `44341114282` çalıştırması da başarılıydı; son aday/manuel override referans temizliği eklenince yukarıdaki güncel kaynak ayrıca derlendi. Teslim edilen artifact ikinci çalıştırmaya aittir.
- Gerçek Minecraft/sunucu/CPS HUD kabulü yapılmadı; fixture veya DLL derleme başarısı bunların yerine geçmez.

Bu bölüm derlemeden sonraki belge güncellemesidir; DLL’in kaynak kimliği yukarıdaki commit’tir.
