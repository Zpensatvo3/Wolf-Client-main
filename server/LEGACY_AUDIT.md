# Legacy Wolf auth incelemesi

Başlangıç: temiz çalışma ağacı, `77373b6f9bb330e145122642414fb9e49bed2abc`.
İnceleme C++, JS, MSBuild, PowerShell, testler, şema, deployment metadata ve
belgelerdeki referansları kapsar. Uzak servis ve veritabanı sorgulanmadı.

| Bulgu | Sınıf | Bağımlılık ve yapılan işlem |
| --- | --- | --- |
| Eski gömülü yönetim paneli | A — kaldırılabilir uygulama | Yalnızca Worker'ın `/admin` HTML cevabında çalışıyordu; ayrı dashboard modülleriyle değiştirildi. |
| `WOLF_AUTH_REQUIRED`, `WOLF_AUTH_SERVER_DOMAIN`, `WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64` | B — yerel adlandırma | `config.hpp`, `auth.cpp`, DLL/launcher MSBuild tanımları birlikte `ONESTEP_AUTH_*` oldu; eski macro alias'ı bırakılmadı. |
| `WOLF_AUTH_HPP` include guard | B | Dosya içi include koruması `ONESTEP_AUTH_HPP` oldu. |
| `Wolf-Client/2.1` | B | İki WinHTTP User-Agent alanı `OneStep-Client/2.1` oldu; HTTPS, redirect ve sertifika politikası aynı. |
| Varsayılan eski Wolf API hostname'i | A | Fallback kaldırıldı. Auth açık yapılandırmada boş/yanlış hostname veya eksik public key derleme hatasıdır. Eski hostname açıkça verilse de reddedilir. |
| `wolf-client-v1` | C — aktif protokol | Native imza doğrulaması, normal ve launcher oturumları kullanır. Aynen korundu; eski imzalı oturumla regresyon testi var. |
| `WOLF-HWID-V3|` | C | Native fingerprint girdisinin parçası. Aynen korundu; cihaz kimliği ve kayıtlı bağlar değiştirilmedi. |
| Otomatik `key_code` → secure_v2 tablo yeniden oluşturma | D — tarihsel migration | Canonical OneStep şeması bu yolu kullanmaz. Aktif Worker'dan çıkarıldı; kurtarma referansı `archive/wolf/license-schema-migration.js.txt`. Temsilî raw-key şemasında hiçbir veri taşınmadığı/silinmediği test edilir. |
| Request sırasında ALTER/CREATE yolları | D | Yerini salt okunur şema doğrulaması aldı. Operatör kontrolü olmadan migration yapılmaz. `schema.sql` yeni boş DB kurulumunda korunur. |
| `wrangler.legacy.toml` | D | Aktif JSONC config tarafından kullanılmıyor. Yanlış hedefe yayın riskini azaltmak için `.txt` uzantısıyla arşivlendi. |
| `legacy-admin-redirect.mjs` | D | Aktif OneStep import grafiğinde yoktu. Tarihsel deploy'un kurtarma referansı olarak `.txt` arşivinde tutuldu. Eski çalışan Worker değiştirilmedi. |
| Eski admin consolidation notu | D | Tarihsel operasyon kaydı olarak arşivlendi. Güncel kurulum belgesine karıştırılmaz; içindeki geçmiş iddialar bu çalışma sırasında yeniden doğrulanmadı. |
| Raw HWID okuma/bağı güncelleme uyumluluğu | C — ihtiyatla korundu | Lisans satırlarının tüm geçmişi yerelde bilinmiyor. Aktivasyonun eşleşmiş eski `hwid_hash` değerlerini koruyan kodu bırakıldı. |
| `ALLOW_DEVICE_CONFIGS` yolu | C — kapalı uyumluluk seçeneği | Üretimde `false`, geçerli lisans oturumu zorunlu. Mevcut açıkça yapılandırılan alternatif config sahipliği sözleşmesi kaldırılmadı. Lisans veya admin yetkisi vermez. |
| DPAPI vault / CNG anahtar adı / epoch | C | Yerel sır kurtarma ve aktif cihaz/oturum protokolü. Dosyalara veya değerlere dokunulmadı. |

## Aktif bağımlılık grafiği

DLL ve launcher, MSBuild public deployment metadata üzerinden `auth::config` alır.
WinHTTP, OneStep API'ye HTTPS/443 ile gider. Native auth, P-256 imzayı, nonce'u,
audience'ı, cihaz anahtarını, epoch'u ve süreyi doğrular. Client worker/heartbeat ve
public config servisi bu durumu kullanır. `src/auth/client.cpp` bu refaktörde değişmez.

`worker.js`, ortak HTTP/crypto/rate-limit araçlarını API handler'larına bağlar.
Auth ve config yolları aynı session modülünü kullanır. Launcher kendi mevcut proof,
release ve tek kullanımlık ticket denetimlerini kullanmaya devam eder. Admin okuma
handler'ına yalnızca ortak rate limit ve Bearer hash doğrulamasından sonra girilir.
Dashboard asset'leri hiçbir sır veya lisans kaydı içermez; korumalı API'den veri ister.

## Veritabanı ve yayın sınırı

Mevcut OneStep binding kimlikleri, public key, süre politikası, schema.sql ve
deployment metadata korunur. Active Worker kodunda DROP/ALTER/otomatik veri taşıma
yolu yoktur. Eski Wolf D1 içindeki tarihsel configler okunmadı veya taşınmadı.
Yerel test anahtarları rastgele üretilir ve üretimde kullanılmaz.

Önemli düzeltmeler: RateLimiter aynı milisaniyedeki istekleri de transaction içinde
sayar. Ban/reset audit satırı ve epoch değişikliği aynı D1 batch işlemindedir.
Bağsız bir lisansın yasağını kaldırmak onu `unused` bırakır; sonraki normal
aktivasyon cihaz bağını kurar. Bu işlemler mevcut kayıtları topluca değiştirmez.
