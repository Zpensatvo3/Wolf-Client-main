# OneStep güvenli kurulum

Bu belge güncel kaynak yapılandırmasını anlatır. 16 Ekim'deki DLL'den lisans
girişi, admin bearer ve otomatik DPAPI/kopyalama talimatları artık geçerli değildir.
Kaynakta launcher-only v2 ve Access-only yönetim esas alınır.
Kayıtlı yayın durumu [deployment.json](deployment.json), protokol
[LAUNCHER_V2](../docs/LAUNCHER_V2.md), olay/kurtarma notları
[AUTH_HARDENING](AUTH_HARDENING.md) içindedir.
Hiçbir adım bu belge düzenlemesi sırasında canlıda çalıştırılmamıştır.

## Dağıtım ve güven sınırı

Aktif hedef `onestep-auth.gogogamesas.workers.dev` Worker'ıdır.
Tarihsel Wolf Worker/D1 ayrı kaynaklardır; onlara deploy, veri silme veya
otomatik config taşıması yapılmaz.

| Yapılandırma | Sorumluluk |
| --- | --- |
| `DB` | OneStep D1; lisans, config, audit, release/runtime kayıtları |
| `RELEASES` | KV; onaylı DLL baytları |
| `RATE_LIMITER` / `RateLimiter` | SQLite Durable Object; sınıf/migration kimliği korunur |
| `ADMIN_ACCESS_TEAM_DOMAIN`, `ADMIN_ACCESS_AUD`, `ADMIN_ACCESS_EMAIL` | Worker'ın doğruladığı public Access kimlik ayarları |
| `SIGNING_KEY_ID` | Aktif imza anahtarının public kimliği |
| `AUTH_V2_ONLY=true`, `AUTH_V2_RELEASES=true` | Üretimde v2 akışını zorunlu tutan ayarlar |

Bağlantılar ve public vars `wrangler.jsonc` içindedir; secret değerleri değildir.
Cloudflare Access `/admin*` ve `/api/v1/admin/*` yollarını, asset/release alt
yollarıyla birlikte aynı audience altında korumalıdır. Worker ayrıca JWT'nin
imza/issuer/audience/süre/izinli insan kimliğini doğrular. Mutasyonlar aynı
origin ister; geniş Bypass veya bearer fallback açılmaz. İnsan MFA'sını
operatör kendi tarayıcısında tamamlar; QR/OTP/oturum bilgisi rapora aktarılmaz.

## Sırlar ve lisanslar

| Secret | Kullanım |
| --- | --- |
| `KEY_PEPPER` | Lisansın tek yönlü hash'i |
| `HWID_PEPPER` | Cihaz özeti |
| `SIGNING_PRIVATE_KEY_PKCS8` | P-256 private signing key |
| `SIGNING_PUBLIC_KEY_SPKI` | Aynı çiftin public doğrulama anahtarı |

Var olan değerleri koru. Public key, key ID ve Access issuer/audience istemci
ve config'te bulunabilir; private key/pepper bulunamaz. Güncel rotasyon politikası
production sırlarını Cloudflare secret binding'lerinde tutar; yeni yerel vault
veya private-key dosyası üretmek gerekli kurulum adımı değildir.
`ADMIN_TOKEN_HASH` ekleme, `ALLOW_DEVICE_CONFIGS` ile lisans denetimini açma.

Lisans anahtarı oluşturma cevabında bir kez gösterilir; tekrar reveal yoktur.
Launcher lisansı diske kaydetmez, DLL'ye kalıcı key devretmez. Cihaz imzası,
lisans epoch'u, runtime lease ve release onayı kaldırılamaz.
Pepper'ları körlemesine değiştirmek mevcut lisans/cihaz/config hash'lerini bozar;
kontrollü migration ayrı operatör kararıdır.

## Native derleme

`server/AuthDeployment.props` yalnızca hostname ve public doğrulama bilgisini
MSBuild'e taşır. Normal DLL üretiminde `AuthRequired=false` reddedilir.
Eksik host/public key veya eski Wolf host'una fallback derleme hatasıdır.
Yalnızca açıkça işaretlenmiş non-release test probe'ları auth'suz olabilir;
bu bir dağıtım modu değildir.

```powershell
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
./scripts/Build-Launcher.ps1
```

Özel, operatöre ait geliştirme ortamı için public ayarların açıkça verilmesi:

```powershell
./scripts/Build-Release.ps1 -RequireAuth -AuthServerDomain 'auth.example.com' -AuthPublicKeySpkiBase64 'BASE64URL_SPKI'
```

Karşılıkları `ONESTEP_AUTH_REQUIRED`, `ONESTEP_AUTH_SERVER_DOMAIN` ve
`ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64` makrolarıdır. Host yalnızca hostname'dir:
şema, port veya yol verilmez. WinHTTP HTTPS/443 kullanır; redirect takip edilmez.
DLL/launcher, server ile aynı public key/key ID ve protokolü kullanmalıdır.
`WOLF-HWID-V3|` girdisi cihaz kimliğidir; kozmetik isim temizliği için değiştirilmez.
Tarihsel `wolf-client-v1` audience güncel v2'de yeniden açılmaz.

## Şema, test ve yayın sırası

1. Hedef hesap/Worker/DB/binding kimliklerini ve public config'i doğrula.
2. Yeni boş DB için `schema.sql` kullanılabilir. Mevcut D1'da şema ve yetkili
   kurtarma noktası incelenmeden hiçbir SQL çalıştırma.
3. Request-time şema kontrolü salt okunurdur. Eksik kolon, raw-key Wolf şeması
   veya yanlış binding otomatik CREATE/ALTER/DROP/taşımayla düzeltilmez.
4. Gerekli migration'ı temsilî yerel kopyada test et; `0002` ve `0003`
   migration'larının kayıtlı üretimde uygulandığını unutma, rutin olarak tekrar çalıştırma.
5. [Sunucu testlerini](README.md#yerel-doğrulama), native kontrolleri ve dry-run'ı çalıştır.
6. Canlı deploy/release upload/imza rotasyonu ayrı onaylı operatör işlemidir.
   Yeni private/public signer ve native pin birlikte planlanır; compromise olmuş
   eski signer'a veya v1/bearer akışına rollback yapılmaz.

`npm run check` dry-run'dır. `npm run deploy` gerçek yayındır; predeploy Access
ayarlarını kontrol eder, `--keep-vars` kullanır. Sırları/config'i bu belge görevi
kapsamında değiştirme. Lisanslar, device key bağları, epoch, config sahipliği ve
audit/runtime kayıtları korunmalıdır. Sırf dokümantasyon temizliği lisans reseti
veya incident kapanışı gerekçesi değildir.
