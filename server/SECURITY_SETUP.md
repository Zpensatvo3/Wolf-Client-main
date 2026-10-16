# Onestep — Lisans girişi kurulumu

16 Ekim 2026'da servis `https://onestep-auth.gogogamesas.workers.dev` adresine
yayımlandı. Yönetim paneli `/admin`, sağlık kontrolü `/health` yolundadır.
`onestep-licenses` veritabanı ve SQLite RateLimiter bu servise aittir; eski
`wolf-auth-api`, `wolf-db` ve diğer projeler değiştirilmedi.

Native C++ probe ile gerçek HTTPS giriş, ECDSA imzası/nonce, hatalı anahtar
reddi, lisanslı config listesi ve heartbeat doğrulandı. Oyun içi panel testi
yapılmadı. `server/AuthDeployment.props`, normal x64 build'lerinde auth'u
etkinleştirir ve yalnızca public key/hostname içerir. Yayın bilgileri
`server/deployment.json` dosyasındadır.

## Giriş bilgilerine erişim

Proje kökündeki `Onestep-Auth.cmd` dosyasını aç. PowerShell 7 kullanır;
PATH'te yoksa bu bilgisayardaki Codex runtime kopyasını otomatik bulur.
Yerel penceredeki düğmeler yönetici parolasını veya client lisansını sen
istediğinde panoya kopyalar; parola ekranda/log'da gösterilmez. Paneldeki
parola alanına yönetici bilgisini, oyun içi girişe client lisansını yapıştır.

İlk lisans süresizdir, kimliği `1`'dir ve native kontrolde bu bilgisayarın
HWID'sine bağlandı. Başka bilgisayarda kullanılacaksa admin panelinden HWID
sıfırlanabilir. Yeni lisansları admin panelinden oluşturabilirsin.

`server/.credentials/worker-secrets.enc.json`, sunucu sırları/yönetici girişi/
ilk lisansın AES-GCM + RSA-OAEP şifreli yedeğidir. RSA private key aynı klasörde
`wrapping-key.dpapi` olarak Windows CurrentUser korumasıyla saklanır. İkisi de
git dışında tutulur; birlikte ve güvenli şekilde yedeklenmelidir. Başka Windows
hesabına doğrudan taşımak şifreyi çözmez. Yeni anahtar üretip mevcut pepper veya
imza anahtarını rastgele değiştirme; çalışan lisansları geçersizleştirebilir.

Komut satırı alternatifi: `scripts/Copy-AuthCredential.ps1 -Kind Admin` veya
`-Kind License`. Bu yardımcılar yalnızca kullanıcı çalıştırdığında panoya yazar.

## İstemci akışı

`AuthRequired=true` derlemesinde giriş tamamlanana kadar ClickGUI, HUD,
modül tick'leri ve public config işleri açılmaz. Renderer yalnızca giriş
paneli için çalışır. Aynı anda tek doğrulama yapılır; ağ işi render thread'ini
bekletmez. Panel İngilizce/Türkçe arayüz tercihini kullanır.

İstemciye girilen anahtar diske, config'e veya log'a kaydedilmez. Gönderimden sonra panelin
tamponları temizlenir; oturum bellekte tutulur. Kapatma, bekleyen giriş işini
ve heartbeat'i bitirir; geç gelen cevap yeniden erişim açamaz. Ağ işlemleri
WinHTTP zaman aşımıyla sınırlıdır; kapanış devam eden isteği bekleyebilir.

Heartbeat 55–75 saniyede bir yeniler. Geçici ağ hatalarında 12–90 saniye
arasında geri çekilerek tekrar dener; son imzalı oturumun süresini uzatmaz.
Oturum en fazla 45 dakika ve lisansın kalan süresi kadar geçerlidir.
Yerel süre sınırı monotonic Windows sayacıyla izlenir. Sunucu reddi, cihaz/
epoch değişimi veya süre bitiminde modül/çizim kapıları kapanır ve istemci
normal kaynak temizliğiyle unload olur; Minecraft kapatılmaz.

Giriş/heartbeat ayrıca export izni kapalı CNG cihaz anahtarıyla imzalanır.
Yeni sürümle ilk girişte cihaz public key hash'i lisansa bağlanır. Bu geçişten
sonra eski DLL kullanımı HWID Reset gerektirir. TPM/VMProtect ayrıntıları ve
korumalı derleme yolu [PROTECTION.md](PROTECTION.md) dosyasındadır. Canlı cihaz
kontrolü ayrı geçici lisansla yapıldı; asıl lisansın cihaz anahtarı bağı bu
kontrol sırasında değiştirilmedi.

## Sunucuyu hazırlama

1. Hedef Cloudflare hesabında Worker, D1 ve `RATE_LIMITER` Durable Object
   kaynaklarını yapılandır. Güncel dosya `wrangler.jsonc`'dir. Eski
   `wrangler.legacy.toml` yalnızca geçmiş Wolf ortamı içindir.
2. Yeni veritabanında `schema.sql` uygula. Mevcut veritabanı varsa önce şemayı
   incele ve gereken migration'ı hazırla; üretim verisini yeniden oluşturma.
3. Aşağıdaki sırları Worker secrets olarak kaydet.
4. `worker.js` ve içe aktardığı `session-policy.mjs` dosyasını birlikte yayımla.
5. Admin panelinden test lisansı oluştur; aynı ortamın public key'iyle auth
   etkin istemci üret. Yukarıdaki Onestep ortamında bu adımlar tamamlandı.

İlk yayın bağlı Cloudflare eklentisiyle yapıldı. Yerel Wrangler oturumunun eski
token'ı süresi dolmuştu; yeniden giriş istemek yerine eklentinin mevcut yetkisi
kullanıldı. İleride CLI'dan yayın için `npx wrangler login`, ardından `npm run
check` ve `npm run deploy` kullanılabilir. Yeni yayında mevcut `secret_text`
binding'leri korunmalı; sırlar config'e eklenmemelidir. Sunucu modülünün başlangıç
süresi canlı upload cevabında **2 ms** olarak raporlandı.

| Secret | İçerik |
| --- | --- |
| `KEY_PEPPER` | Lisans hash/şifreleme için rastgele en az 32 karakter |
| `HWID_PEPPER` | Cihaz hash'i için ayrı rastgele en az 32 karakter |
| `ADMIN_TOKEN_HASH` | Uzun rastgele admin token'ının SHA-256 özeti |
| `SIGNING_PRIVATE_KEY_PKCS8` | ECDSA P-256 PKCS#8 DER private key, base64url |
| `SIGNING_PUBLIC_KEY_SPKI` | Aynı çiftin SPKI DER public key'i, base64url |

İstemciye yalnızca public key eklenir. Private key, pepper ve ham admin token
istemciye veya repoya konulmaz. Önceden bulunan yerel signing dosyaları bu
entegrasyonda okunmadı/değiştirilmedi. Anahtar değişiminde Worker ve istemci
public key'i birlikte güncellenir; pepper değişikliği veri migration'ı gerektirir.

## Auth etkin derleme

```powershell
.\scripts\Build-Release.ps1 -RequireAuth -AuthServerDomain 'auth.example.com' -AuthPublicKeySpkiBase64 'BASE64URL_SPKI'
```

Adres yalnızca hostname olmalıdır; `https://`, port veya yol eklenmez.
Tüm istekler HTTPS/443 kullanır ve yönlendirmeler takip edilmez.
[WinHTTP yönlendirme politikası](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags#winhttp_option_redirect_policy)
`NEVER` olarak ayarlanır; lisans/token başka host'a yönlendirilmez.
MSBuild karşılığı:

```powershell
msbuild custom-base.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:AuthRequired=true /p:AuthServerDomain=auth.example.com /p:AuthPublicKeySpkiBase64=BASE64URL_SPKI
```

Bu projede değerler `server/AuthDeployment.props` içinden gelir; normal
`scripts/Build-Release.ps1` de auth açık üretir. İzole geliştirme ihtiyacında
MSBuild'e `/p:AuthRequired=false` açıkça verilmelidir. Auth etkin ama hostname/
public key eksikse derleme durur. Auth ve config aynı hostname'i kullanır.

Yeni Onestep servisinde `ALLOW_DEVICE_CONFIGS=false` olduğu için config uçları
geçerli lisans oturumu ister. Eski Wolf servisindeki lisanssız cihaz-sahipliği
yolu değiştirilmedi; eski public configler yeni veritabanına taşınmadı.

## Kısa doğrulama

`tests/auth_policy_test.cpp` zaman/alan/anahtar sınırlarını;
`tests/auth_client_test.cpp` tek istek, hatadan sonra tekrar deneme, iptal,
geç cevap ve lisanssız geliştirme davranışını sahte backend ile denetler.
`tests/auth_session_test.cpp`, gerçek oturum durumunu ve ilk ağ isteğinden
önce heartbeat'in uyandırılıp tamamen bitirilmesini kontrol eder.
`node --test tests/auth_server_policy_test.mjs`, Worker süre sınırlarını ve
D1 tarihlerini kontrol eder. `tests/auth_live_probe.cpp` yalnızca açıkça
çalıştırılan canlı smoke kontrolüdür; stdin'den lisans alır ve hiçbir anahtar/
token yazdırmaz. `scripts/Test-AuthDeployment.ps1 -Probe <exe>` şifreli vault'u
kullanarak çalıştırır. Bu kontroller oyun içi panel testi yerine geçmez.

Yayından önce geçerli, hatalı, iptal edilmiş ve süresi dolmuş lisanslar; yanlış
cihaz, bozuk imza/nonce, ağ kesintisi, yeniden giriş ve unload akışı gerçek
test ortamında denenmelidir. Şu an kullanıcı kaydı, parola girişi, ödeme veya
kalıcı “beni hatırla” akışı bulunmaz; giriş mevcut lisans anahtarı modelini kullanır.
