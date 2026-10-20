# Wolf arşivi — çalıştırmayın

Bu dizin aktif OneStep import/build/deployment grafiğinde değildir.
Dosyalar `77373b6f9bb330e145122642414fb9e49bed2abc` dönemi ve önceki Wolf
ortamının kurtarma referanslarıdır. Çalıştırılabilir uzantılar bilerek `.txt`
yapılmıştır. Tarihsel Worker/D1 bağımsızdır; otomatik config taşıma, silme veya
eski hedefe güncel Worker deploy'u yapılmaz.

Güncel belgeler: [sunucu](../../README.md), [kurulum](../../SECURITY_SETUP.md),
[güvenlik ve açık olay işleri](../../AUTH_HARDENING.md).
Aşağıdaki notlar yeniden yapılmış üretim doğrulaması değildir.

## Arşiv dosyaları

| Dosya | Tarihsel amaç |
| --- | --- |
| `wrangler.legacy.toml.txt` | Eski Worker/D1 binding tanımları; aktif deploy girdisi değil |
| `legacy-admin-redirect.mjs.txt` | Eski browser redirect entry yardımcısı |
| `license-schema-migration.js.txt` | Raw-key tablo dönüşümünün bağlamlı örneği; bağımsız çalıştırılabilir migration değil |

## 16 Ekim 2026 admin konsolidasyonu

Eski kayıt, `wolf-auth-api` root/`/admin`/`/admin/` GET/HEAD isteklerinin
`https://onestep-auth.gogogamesas.workers.dev/admin` adresine yönlendirildiğini
belirtir; query string ve credentials aktarılmaz.

Deployment `04ee59c59b16473daa841247e3b2a99a`, eski `worker.js` önüne
`redirectLegacyAdmin` çağrısını ekler; diğer istekler asıl handler'a gider,
`RateLimiter` yeniden export edilir. Eski API/binding'ler değiştirilmemiş,
asıl script Cloudflare version history'de kalmıştır.

Operatör isteğiyle tarihsel D1
`d2da3fa4-c02c-4928-bb31-9a29826f0e4c` üzerindeki 13–22 lisans ID'leri
denetlenip batch silinmiş, ardından sıfır eski lisans kaydedilmiştir.
Public configler, kullanıcılar, auth/audit geçmişi ve yeni OneStep DB silinmemiştir.

Silmeden önce on tam satır ignored
`.credentials/legacy-licenses-2026-10-16.enc.json` dosyasına şifrelenmiş,
decryption doğrulanmıştır. Eski snapshot RSA wrapping key/Windows DPAPI'ye
bağlıdır; ilgili Windows hesabı ve `wrapping-key.dpapi` olmadan tek başına
şifreli dosya kurtarma sağlamaz.

**Bu bir güncel kasa açma talimatı değildir.** Tarihsel
`Read-OnestepAuthVault -EnvelopePath <snapshot>` ve
`Copy-AuthCredential.ps1 -Kind Admin` örnekleri eski arayüz/politikayı anlatır.
Güncel helper'lar otomatik legacy açma/kopyalama yapmaz; production için yeni
yerel secret kasası oluşturulmaz. Kurtarma eski commit ve şemayla ayrı
incelenmelidir. Ham satırlar raporlanmaz, iptal edilmiş lisanslar otomatik geri
yüklenmez: geri yükleme onları tekrar yetkilendirebilir.
Sonraki yerel cleanup engeli [güvenlik kaydında](../../AUTH_HARDENING.md#açık-olay-işleri)
korunur; bu temizlik secret dosyalarını kaldırmamıştır.

## İlk legacy auth denetimi

İlk inceleme C++, JS, MSBuild, PowerShell, test, şema ve deployment referanslarını
kapsar; uzak Wolf servisi/DB sorgusu değildir.
Sınıflar: A kullanılmayan kod, B yerel adlandırma, C o dönemin protokol bağı,
D tarihsel kurtarma/deploy artifaktı.

| Bulgu | Sınıf | O dönemde yapılan / korunması gereken |
| --- | --- | --- |
| Gömülü eski admin paneli | A | Ayrı dashboard modülleriyle değiştirildi. |
| `WOLF_AUTH_*`, `WOLF_AUTH_HPP` | B | DLL/launcher/build tanımları birlikte `ONESTEP_AUTH_*` / `ONESTEP_AUTH_HPP` oldu; alias bırakılmadı. |
| `Wolf-Client/2.1` | B | İki WinHTTP User-Agent `OneStep-Client/2.1` oldu; HTTPS/redirect politikası korunmuştu. |
| Eski Wolf hostname fallback | A | Kaldırıldı; eksik/yanlış production host/public key derleme hatasıdır. |
| `wolf-client-v1` | C | İlk refaktörde signed audience olarak korundu; güncel launcher-only v2 bu eski oturumu kabul etmez. |
| `WOLF-HWID-V3|` | C | Fingerprint/device kimliği girdisi korunur. |
| `key_code` → secure_v2 dönüşümü | D | Request grafiğinden çıkarıldı, yalnız arşiv örneği kaldı. Raw-key şeması otomatik dönüştürülmez. |
| Request-time ALTER/CREATE | D | Salt okunur canonical şema denetimine dönüştü. |
| Legacy wrangler/redirect | D | Aktif import yok; yanlış deploy riskine karşı `.txt` arşivi. |
| Raw HWID uyumluluğu | C | İlk incelemede korundu; sonraki hardening raw fallback'i kaldırdı. Geçmiş durumu güncel destek gibi yorumlama. |
| `ALLOW_DEVICE_CONFIGS` | C | İlk incelemede kapalı alternatif yol korunmuştu; güncel kaynak bu değişkenle lisans denetimini atlatmaz. |
| DPAPI/CNG/epoch | C | İlk inceleme dosyalara/bağlara dokunmadı; sonraki signer/vault politikası güvenlik kaydındadır. |

İlk bağımlılık grafiği native P-256/nonce/audience/device/epoch ve Worker ortak
session/limiter yollarını korumuştu. Admin o zaman bearer hash kullanıyordu;
**bu güncel admin kimlik modeli değildir**. Ban/reset audit ve epoch aynı D1
batch'indeydi; bağlı olmayan lisans unban sonrası `unused` kalıyordu.
Rate limiter aynı milisaniye/eşzamanlı çağrıları transaction içinde sayıyordu.

Arşiv lisans/secret içeriklerini taşımaz. Kurtarma ve yeni migration için yetkili
operatör doğru kaynağı/şemayı ayrıca inceler; Git'teki tarihsel örnekler üretime
uygulama izni değildir.
