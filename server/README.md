# OneStep authentication service

Aktif üretim API'si: `https://onestep-auth.gogogamesas.workers.dev`.
Yönetim paneli `/admin`, sağlık kontrolü `/health` yolundadır. Buradaki yeni
panel ve kaynak düzenlemesi yereldir; deploy edilene kadar canlı servisi değiştirmez.

## Kaynak düzeni

```text
worker.js                 HTTP yönlendirme, sağlık kontrolü, servislerin birleştirilmesi
core/http.mjs             Sınırlı JSON okuma ve güvenli JSON cevapları
core/crypto.mjs           Hash, kodlama, AES-GCM lisans saklama
core/rate-limit.mjs       RATE_LIMITER Durable Object ve çağrı adaptörü
auth/sessions.mjs         P-256 oturum imzası ve aktif lisans/epoch denetimleri
api/auth.mjs              Aktivasyon ve heartbeat
api/admin.mjs             Admin yetkisi ve lisans işlemleri
api/admin-read.mjs        İstatistikler, filtreli liste, audit geçmişi, sistem durumu
api/configs.mjs           Lisansa ait public config işlemleri
database/schema.mjs       OneStep şemasının salt okunur doğrulaması
dashboard/index.mjs       HTML, aynı origin asset yolları ve güvenlik başlıkları
dashboard/styles.mjs      Tasarım değişkenleri, responsive yerleşim ve hareket tercihleri
dashboard/scripts.mjs     Tarayıcı etkileşimleri; framework ve harici CDN gerektirmez
dashboard/i18n.mjs        Türkçe ve İngilizce metinler
device-proof.mjs          Mevcut cihaz imzası protokolü
session-policy.mjs        Mevcut oturum ve lisans süre politikası
launcher.mjs              İmzalı sürüm, indirme ve tek kullanımlık launcher bileti
schema.sql                Yeni, boş OneStep veritabanı için şema
archive/wolf/             Çalıştırılmayan tarihsel kurtarma referansları
```

## Yönetim paneli

Sol menüde genel bakış, lisanslar, işlem geçmişi, sistem durumu ve tercihler vardır.
Genel bakıştaki toplam/aktif/kullanılmamış/yasaklı/süresi dolmuş sayıları D1'dan gelir.
Bitiş tarihi geçmiş lisans, bir sonraki heartbeat beklenmeden süresi dolmuş gösterilir.
Zaman serisi verisi olmadığı için tahmini grafik üretilmez. Audit listesi yalnızca
sunucuda mevcut kayıtları gösterir; geçmişte tutulmayan olaylar sonradan uydurulmaz.

Lisans listesi ID/parmak izi/cihaz ipucu araması, durum filtresi, sıralama ve 20 kayıtlık
sayfalar sunar. Ayrıntı çekmecesinde tarih, aktivasyon, HWID/cihaz anahtarı bağı ve epoch
görülür. Oluşturma, anahtarı gösterme, cihaz sıfırlama, yasaklama/yasağı kaldırma ve silme
sunucu onayından sonra tamamlanır. Erişimi değiştiren işlemler sonuçları açıklanan bir
onay ister. Oluşturma formunun son düğmesi, seçilen süre ve açıklama ile açık onaydır.

Ham veya şifreli lisans materyali normal listede bulunmaz. Tam anahtar yalnızca
oluşturma cevabında veya korumalı `reveal-key` isteğinde gelir. Çekmece kapatılınca
gösterilen anahtar temizlenir. Hash olarak saklanan eski lisansların anahtarı geri
çıkarılamaz; korumalı endpoint bunun için 409 döndürmeye devam eder.

Admin Bearer anahtarı sekme belleğindedir. localStorage, sessionStorage, cookie veya URL
ile tutulmaz. Yenileme ve çıkış oturumu bitirir. CSP, aynı origin JS/CSS bağlantıları,
`no-store`, `no-referrer`, native dialog odak yönetimi, Escape ve azaltılmış hareket
desteği vardır. Değişken metinler DOM'a `textContent` ile yazılır. Mobilde sol menü
açılır/kapanır ve kapalıyken klavyeyle erişilemez.

## API uyumluluğu

Aktif endpointler ve alanları korunur:

- `POST /api/v1/auth`: `activate` ve `heartbeat`.
- `POST /api/v1/configs/{list,publish,load,delete}`.
- `GET /api/v1/admin/list-keys`: parametresiz eski `keys` cevabı korunur.
- `POST /api/v1/admin/{create-key,reveal-key,reset-hwid,ban-key,delete-key,delete-config}`.
- `PUT /api/v1/admin/release` ve `POST /api/v1/launcher/{manifest,download,ticket,redeem}`.

Yeni admin okuma yolları: `GET /api/v1/admin/{overview,activity,status}`.
Sayfalı liste isteği `list-keys?page=1&page_size=20&status=all&sort=newest&search=` biçimindedir.
`page_size` 1–100, durum `all/active/unused/banned/expired`, sıralama
`newest/oldest/expiry` olabilir. Sayfalı cevapta `keys,total,page,page_size` bulunur.
`activity` de `page/page_size` kullanır. SQL arama değerleri parametrelenir; sıralama
yalnızca sabit bir izin listesinden seçilir. Liste cihaz anahtarı hash'i yerine
`device_bound` ve `hwid_bound` bayraklarını da sağlar.

Admin API 401 yetkisiz, 405 yanlış yöntem, 400 geçersiz girdi, 404 olmayan kayıt/yol,
409 çakışma veya gösterilemeyen anahtar, 429 istek sınırı, 500/503 servis hatası döndürür.
Aktif istemcilerin auth/config hata sözleşmeleri değiştirilmedi. Yasağı kaldırma için
mevcut `status: active` kullanılır; cihaz bağı henüz yoksa kayıt `unused` olur ve normal
aktivasyonla bağlanır. Bağlı lisansın cihazı ve bitiş tarihi korunur.

## Bağlantılar ve sırlar

`wrangler.jsonc` içindeki mevcut `DB` (OneStep D1), `RATE_LIMITER` (SQLite Durable
Object) ve `RELEASES` (KV) bağlantıları korunur. `ALLOW_DEVICE_CONFIGS=false` kalır.
`RATE_LIMITER` class adı ve migration etiketi değiştirilmez. Sayaç transaction içinde
artırılır; aynı milisaniyedeki ve eşzamanlı istekler de sayılır.

Gerekli sırlar: `KEY_PEPPER`, `HWID_PEPPER`, `ADMIN_TOKEN_HASH`,
`SIGNING_PRIVATE_KEY_PKCS8`, `SIGNING_PUBLIC_KEY_SPKI`. Var olan değerleri koruyun.
Kurulum ayrıntıları [SECURITY_SETUP.md](SECURITY_SETUP.md) içindedir. Bu refaktör için
anahtar üretme, pepper değiştirme veya lisans sıfırlama gerekmez.

## Yerel doğrulama

Node 24 ve depodaki kilit dosyasıyla:

```sh
cd server
npm ci
npm test
npm run test:browser   # Chromium; CHROMIUM değişkeniyle yol belirtilebilir
npm run test:runtime   # Wrangler dry-run + yerel workerd/D1/SQLite DO
npm run check          # Yalnızca dry-run; deploy yapmaz
cd ..
python scripts/Test-AuthConfiguration.py
python scripts/Test-Infrastructure.py
python scripts/Test-Offline.py --jobs 4
```

Testler geçici SQLite/Miniflare kaynakları ve bellekte üretilen test anahtarları kullanır.
Canlı API veya tarihsel Wolf D1'a bağlanmaz. Browser testi Chromium yoksa bunu açıkça
skip olarak bildirir. MSVC için Developer PowerShell'de aynı Python kontrolleri ve
`scripts/Build-Release.ps1 -Clean -SkipSign` kullanılabilir. Bu komut DLL üretir;
üretim API'sini değiştirmez. Launcher: `scripts/Build-Launcher.ps1`.

## Güvenli şema ve manuel yayın

Request işleyicisi artık tablo dönüştürmez, kolon eklemez, veri taşımaz veya tablo
silmez. `database/schema.mjs` gereken tabloları ve kolonları `PRAGMA table_info` ile
kontrol eder. Ham `key_code` şeması veya eksik kolon varsa istek kapalı hata verir;
işletmeci incelemesi gerekir. Mevcut lisanslar, configler, audit kayıtları ve launcher
biletleri korunur. Doğrulama kayıtların içeriğini değiştirmez.

Yeni boş OneStep veritabanında `schema.sql` kullanılabilir. Mevcut veritabanında önce
yetkili bir operatör şemayı ve yedeği kontrol etmeli, gerekli ise yalnızca incelenmiş
eklemeli bir migration hazırlamalı ve yerel temsilî kopyada denemelidir. Wolf şemasını
OneStep'e otomatik dönüştürmeyin. Ham anahtarları veya hassas kayıtları raporlamayın.

Yayın operatörün ayrı kararıdır: güncel OneStep hedefini kontrol edin, testleri ve
dry-run'ı çalıştırın, sonra bilinçli olarak `npm run deploy` kullanın. Bu script mevcut
vars değerlerini `--keep-vars` ile korur; secrets yönetimine dokunmaz. Bütün modüller
Wrangler tarafından tek Worker paketine dahil edilir. Ayrı manuel asset yüklemesi
gerekmez. Eski Wolf Worker ve veritabanı bu sürecin hedefi değildir.

`wolf-client-v1` signed-session audience'ı ve native `WOLF-HWID-V3|` fingerprint
önekini değiştirmeyin. Bunlar mevcut lisans/cihaz uyumluluğunun parçasıdır.
