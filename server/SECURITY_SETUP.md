# Onestep — Gelecekteki auth kurulumu

## Mevcut durum

Onestep şu an lisans girişi istemiyor. `src/app/runtime.cpp` lisans doğrulaması
veya lisans heartbeat başlatmadan uygulamayı çalıştırır. Devre dışı eski konsol
ekranı altyapı yenilemesinde `main.cpp` içinden kaldırıldı.

`src/auth/` ve `server/` kaynakları gelecekteki auth entegrasyonu için korunuyor.
Bu kaynakların bulunması, çalışan veya yayına alınmış bir auth sistemi olduğu
anlamına gelmez. Bu belge mevcut bir servise deployment yapıldığını varsaymaz.

Public config bağlantısı ayrıdır: istemci `configure_config_service` ile
cihaz kimliğini ve servis adresini hazırlar. Bu işlem lisans doğrulaması yapmaz
ve lisans heartbeat başlatmaz. Kodda bulunan eski Worker adresinin çalıştığı
veya gelecekte kullanılacağı doğrulanmış değildir.

## Korunan dosyalar

| Dosya | Amaç |
| --- | --- |
| `src/auth/auth.cpp`, `auth.hpp` | İstemci doğrulama, imza kontrolü, oturum ve servis istekleri |
| `server/worker.js` | Worker API, admin işlemleri ve public config uçları |
| `server/schema.sql` | D1 veritabanı şeması |
| `server/wrangler.toml` | Worker, D1 ve Durable Object yapılandırması |

Depoda `wrangler.toml.example` yok. Yeni ortam kurulurken mevcut
`wrangler.toml` içindeki eski Worker adı ve D1 bilgileri hedef ortama göre
düzenlenmeli; eski kaynaklar doğrudan hedef kabul edilmemeli.

## Auth eklendiğinde izlenecek sıra

1. Lisans, oturum süresi, cihaz eşleme ve public config erişim modelini belirle.
   Mevcut lisanssız config akışının nasıl devam edeceğini açıkça tanımla.
2. Hedef Cloudflare hesabında Worker ve D1 kaynaklarını oluştur;
   `wrangler.toml` içindeki `DB` bağlantısını yeni veritabanına ayarla.
3. `schema.sql` dosyasını inceleyip yeni veritabanına uygula. Var olan veriye
   uygulanacak değişiklikler için ayrıca migration hazırla.
4. Mevcut Worker kullanılacaksa `RATE_LIMITER` Durable Object bağlantısını ve
   `RateLimiter` migration tanımını yapılandır.
5. Aşağıdaki sırları Worker secrets olarak tanımla.
6. İstemciyi aynı imza anahtarının public SPKI değeriyle derle.
7. `src/app/runtime.cpp` içindeki servis adresini hedef ortama göre güncelle;
   lisans girişini, oturum yönetimini ve heartbeat akışını uygulama yaşam
   döngüsüne entegre edip test et. Mevcut auth fonksiyonlarının bulunması tek
   başına tamamlanmış bir entegrasyon sayılmaz.

## Mevcut Worker'ın beklediği sırlar

| Ad | İçerik |
| --- | --- |
| `KEY_PEPPER` | Lisans hash ve mevcut lisans şifreleme işlemleri için rastgele sır |
| `HWID_PEPPER` | Cihaz kimliği hash işlemleri için ayrı rastgele sır |
| `ADMIN_TOKEN_HASH` | Uzun rastgele admin token'ının SHA-256 özeti |
| `SIGNING_PRIVATE_KEY_PKCS8` | ECDSA P-256 private key; PKCS#8 DER verisinin base64url karşılığı |
| `SIGNING_PUBLIC_KEY_SPKI` | Aynı anahtar çiftinin public key'i; SPKI DER verisinin base64url karşılığı |

Anahtar değerleri PEM metninin base64 karşılığı olmamalı.
Private key, pepper değerleri ve ham admin token kaynak koduna veya istemci
paketine eklenmemeli.

İstemci için yalnızca public key kullanılır. Mevcut MSBuild özelliği:

```powershell
msbuild custom-base.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:AuthPublicKeySpkiBase64=BASE64URL_SPKI
```

`BASE64URL_SPKI` gerçek public key değeriyle değiştirilmeli.
Bu derleme parametresi tek başına lisans girişini etkinleştirmez.

## Devreye almadan önce doğrulama

- Geçerli, hatalı, süresi dolmuş ve iptal edilmiş lisanslar.
- İmza, nonce, token süresi ve cihaz eşleme kontrolleri.
- Ağ kesintisi, sunucu hatası, rate limit ve heartbeat davranışı.
- Admin erişimi ve lisanssız public config isteklerinin yetki sınırları.
- Oturum kapanışı ve yerel token temizliği.

Kurulum ve kontrollü testler tamamlanana kadar auth, proje belgelerinde
gelecekte eklenecek özellik olarak belirtilmeli.

## İleride anahtar değişimi

İmza anahtarı değiştirilirse Worker ve istemcinin public key'i birlikte
güncellenmeli. Mevcut veriler oluştuğunda `KEY_PEPPER` veya `HWID_PEPPER`
değişikliği migration gerektirebilir. Eski şemada yalnızca tek yönlü hash
olarak saklanmış lisansların orijinal değerleri geri elde edilemez.
