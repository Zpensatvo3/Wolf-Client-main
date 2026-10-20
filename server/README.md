# OneStep authentication service

Üretim hedefi `https://onestep-auth.gogogamesas.workers.dev`'dir.
`/admin` yönetim paneli, `/health` sağlık yoludur.
[deployment.json](deployment.json), 20 Ekim 2026 launcher-only v2 yayınını
kaydeder; bu belge canlı kontrol yerine geçmez.
Doğrudan DLL aktivasyonu / eski v1 launcher yolları kapalıdır (HTTP 410).
Lisans, device-key/epoch ve config verisi korunmalıdır.

## Okuma sırası

- [SECURITY_SETUP](SECURITY_SETUP.md): aktif bağlar, public build ayarları ve güvenli yayın sırası.
- [LAUNCHER_V2](../docs/LAUNCHER_V2.md): audience, proof, ticket, lease ve native hata politikası.
- [AUTH_HARDENING](AUTH_HARDENING.md): koruma sınırları, cihaz/VMProtect, tarihçe ve açık olay işleri.
- [Wolf arşivi](archive/wolf/README.md): çalıştırılmayan kurtarma referansları; aktif kurulum değil.

## Kaynak düzeni

| Kaynak | Sorumluluk |
| --- | --- |
| `worker.js` | Routing, health ve servis bileşimi |
| `core/http.mjs`, `core/crypto.mjs` | Sınırlı JSON, hash/kodlama ve P-256 araçları |
| `core/rate-limit.mjs` | SQLite Durable Object limiter |
| `auth/admin-access.mjs` | Access JWT/kimlik doğrulaması |
| `auth/protocol-v2.mjs`, `auth/runtime-v2.mjs` | V2 proof ve runtime sözleşmeleri |
| `auth/sessions.mjs`, `auth/nonces.mjs` | Oturum/lisans/epoch ve tekrar koruması |
| `api/launcher-v2.mjs`, `launcher.mjs` | Launcher/runtime yolları ve release araçları |
| `api/admin.mjs`, `api/admin-read.mjs` | Yetkili mutasyonlar, liste/istatistik/audit/status |
| `api/configs.mjs` | Lisanslı public config işlemleri |
| `api/auth.mjs` | Tarihsel v1 auth handler; aktif v2-only routing açmaz |
| `database/schema.mjs` | Salt okunur canonical şema doğrulaması |
| `dashboard/{index,styles,scripts,i18n}.mjs` | HTML/CSS/JS ve TR/EN metinleri |
| `device-proof.mjs`, `session-policy.mjs` | Paylaşılan proof/süre araçları |
| `schema.sql`, `migrations/` | Yeni DB şeması ve operatör incelemeli eklemeli geçişler |

Framework/CDN gerektirmeyen dashboard asset'leri Worker paketinin parçasıdır.
`wrangler.jsonc` içindeki **`keep_names: false` korunmalıdır**:
`dashboardApp.toString()` ile taşınan tarayıcı kodu paketleyicinin dış isim
yardımcısına bağlanırsa başlangıç bozulabilir. Bundle/browser testleri bunun içindir.

## Yönetim paneli

Overview sayıları D1 kayıtlarından gelir: total/active/unused/banned/expired.
Bitişi geçmiş lisans heartbeat beklenmeden expired gösterilir.
Uydurma zaman serisi veya sonradan üretilmiş audit geçmişi yoktur.

License listesi arama, durum filtresi, sıralama ve sayfalama sunar.
Drawer; tarih, aktivasyon, HWID/device-key bağı ve epoch'u gösterir.
Create/reset/ban/unban/delete işlemleri açık onay ve server sonucu bekler;
erişim değiştiren eylemler optimistic uygulanmaz. Ham/şifreli lisans normal
listede bulunmaz. Anahtar sadece create cevabında bir kez görünür;
reveal kaldırılmıştır ve hash'ten geri çıkarılamaz.

Access-only/MFA yönetiminde bearer veya gömülü credential kullanılmaz.
Aynı-origin JS/CSS, CSP, `no-store`, `no-referrer`, native dialog odak/ESC,
Reduce Motion ve mobil sidebar vardır. Kullanıcı metni DOM'a `textContent`
ile yazılır; kapalı mobil menü klavye odağı almaz.

## API özeti

| Yol | Kimlik / sözleşme |
| --- | --- |
| `POST /api/v2/launcher/{login,manifest,download,ticket}` | Launcher audience |
| `POST /api/v2/runtime/{redeem,renew}` | Bağlı runtime audience/sequence |
| `POST /api/v1/configs/{list,publish,load,delete}` | Güncel v2 runtime lease |
| `POST /api/v1/auth`, `/api/v1/launcher/*` | Kapalı: 410, fallback yok |
| `GET /api/v1/admin/{overview,activity,status,list-keys}` | Access-only admin |
| `POST /api/v1/admin/{create-key,reset-hwid,ban-key,delete-key,delete-config}` | Access + aynı origin + girdi/onay denetimleri |
| `PUT /api/v1/admin/release`, `POST /api/v1/admin/revoke-release` | Access/MFA, aynı origin, korumalı release işlemleri |
| `reveal-key` | Kaldırıldı: 410 |

`list-keys?page=1&page_size=20&status=all&sort=newest&search=` sayfalı
`keys,total,page,page_size` döndürür; parametresiz eski `keys` biçimi korunur.
Sayfa boyutu 1–100; status `all/active/unused/banned/expired`, sort
`newest/oldest/expiry` izin listesidir. Activity de sayfalıdır.
SQL değerleri parametrelenir; raw device-key hash yerine binding bayrakları sunulur.

Admin API 401/405/400/404/409/429/500/503 durumlarını kimlik/yöntem/girdi/
yokluk/çakışma/limit/servis hatası için kullanır; edge Access önce redirect
döndürebilir. Unban `status: active` sözleşmesini korur; bağı olmayan kayıt
`unused` kalır, var olan cihaz/bitiş bağı bozulmaz.

## Yerel doğrulama

Node 24 ile depo kökünden:

```sh
npm ci --prefix server
npm test --prefix server
npm run test:v2 --prefix server
npm run test:v2:runtime --prefix server
npm run test:runtime --prefix server
npm run test:bundle --prefix server
npm run test:browser --prefix server
npm run check --prefix server
python scripts/Test-AuthConfiguration.py
python scripts/Test-Infrastructure.py
```

Testler synthetic key, geçici SQLite/JWKS/Miniflare/workerd kullanır; production
ve Wolf D1'a bağlanmaz. Browser kontrolü Chromium yoksa açıkça skip bildirir;
`CHROMIUM` yolu ve `ONESTEP_WORKER_BUNDLE` ile gerçek bundle seçilebilir.
Dry-run deploy değildir; bu komut listesi testlerin bu belge görevi sırasında
yeniden çalıştırıldığı veya Windows/Minecraft doğrulaması yapıldığı iddiası taşımaz.

## Şema ve yayın güvenliği

Şema doğrulaması `PRAGMA table_info` ile salt okunurdur; request handler
tablo dönüştürmez/ALTER/CREATE/DROP/veri taşıma yapmaz.
Eksik kolon veya raw-key Wolf şeması operatör incelemesi ister.
Yeni boş DB'ye `schema.sql`; mevcut DB'ye yalnızca incelenmiş, yerelde denenmiş
ve ayrıca onaylanmış migration uygulanır. Kayıtlı `0002`/`0003` geçişlerini
otomatik tekrarlama; özel backup veya ham anahtarları workspace/Git'e koyma.

`DB`, `RELEASES`, `RATE_LIMITER`, class/migration kimliği ve secret'lar
bu temizlikte değişmez. Gerçek yayın `npm run deploy` ile ayrı operatör
kararıdır; predeploy Access metadata kontrolü ve `--keep-vars` atlanmaz.
Eski Wolf ortamı hiçbir zaman bu yayının hedefi değildir.
