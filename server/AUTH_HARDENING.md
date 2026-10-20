# Güvenlik, cihaz koruması ve olay kaydı

## Güncel durum ve otorite

Depodaki 20 Ekim 2026 [yayın kaydı](deployment.json), launcher-only v2'nin ve
Access-only/MFA yönetiminin aktif olduğunu kaydeder.
Güncel protokol [LAUNCHER_V2](../docs/LAUNCHER_V2.md), kurulum
[SECURITY_SETUP](SECURITY_SETUP.md) içindedir.
Bu belge bir canlı durum sorgusu veya olayın tamamlandığı iddiası değildir.
Ara Access/signing sürümleri tarihçedir; güncel rollback hedefi sayılmaz.

**Açık işler:** eski yerel secret dosyalarının insan tarafından kontrolü,
kontrollü pepper migration'ı, açığa çıkmış müşteri lisanslarının reissue planı ve
canlı lisanslı launcher→runtime/oyun kabul testi tamamlanmış sayılmamalıdır.

## Korunması gereken denetimler

- Production auth zorunlu, eksik public build metadata derleme hatasıdır.
  Teste özel işaretli non-release probe dışında devre dışı bırakılamaz.
- Admin HTML/asset/API/release yolları doğrulanmış Cloudflare Access JWT ve
  bağımsız MFA ister; bearer/password fallback yoktur. Mutasyonlarda aynı
  Origin kontrol edilir. İmza, issuer, audience, expiry, issue time ve izinli
  insan kimliği doğrulanır; eksik ayar fail-closed'dur.
- Lisanslar 128 bit rastgele üretilir; zayıf özel key kabul edilmez. Ham key
  yalnızca bir kez gösterilir; hash saklanır, `reveal-key` HTTP 410 döner.
- Device proof zorunludur; raw HWID eşleştirme kaldırılmıştır.
  D1 signed nonce'u beş dakika atomic tüketir; replay/eşzamanlı kopya kabul edilmez.
- Config erişimi device-bound güncel runtime lease ister. Eski
  `ALLOW_DEVICE_CONFIGS=true` kalmış olsa bile lisans denetimi atlanmaz.
- Yanlış/bilinmeyen/banlı/süresi dolmuş lisans yanıtları aynı biçimdedir.
- Signed payload `kid` taşır; aktif `SIGNING_KEY_ID` ile eşleşmeyen/no-kid
  oturum reddedilir. Çalınmış anahtar grace-period halkasında tutulmaz.
- Eski DPAPI yardımcıları otomatik kasa açmaz, admin/license kopyalamaz veya
  health kontrolü sırasında owner lisansı oluşturmaz.
- V2 launcher/runtime/release audience'ları ayrıdır. İlk başarısız yenileme
  erişimi kapatır; cleanup kapatılamaz, Minecraft zorla kapatılmaz.
  Ayrıntılı TTL/sequence/IPC sözleşmesi protokol belgesindedir.

Bu maddeler kaynak kontrolleridir; değiştirilmiş bir DLL'nin imkânsız olduğu,
anti-dump veya donanım attestation garantisi vermez.

## Cihaz anahtarı ve kurtarma

Native istemci Windows CNG'de `Onestep.Auth.Device.V1` adlı, private export'u
kapalı kalıcı kullanıcı anahtarını açar; var olan anahtarı ezmez.
Yeni anahtarda TPM Platform Crypto Provider tercih edilir; destek yoksa
Software Key Storage Provider kullanılır. Önceki operatör kaydı TPM yolunun
çalıştığını belirtir; bu bir sunucu TPM attestation protokolü değildir.

P-256/SHA-256 proof; istek türünü, yolunu, HWID'yi, nonce/zamanı, credential
özetini ve v2 fields bağını doğrular. Sunucu device public key hash'ini lisansa
bağlar; native cevap kendi cihaz bağını doğrular. Yalnızca HWID kopyalamak
private key yerine geçmez; software provider TPM ile aynı güvenceyi sunmaz.

HWID Reset, HWID/device-key bağını temizler ve license epoch'u artırır.
Windows hesabı/key store kaybında yetkili operatör bu yolu değerlendirir.
Normal kapanış/DLL güncellemesi cihaz anahtarını silmez. Private cihaz anahtarı
DLL/repo/legacy vault yedeğinde değildir. Tarihsel `WOLF-HWID-V3|` fingerprint
girdisi korunur; v1'e dönmek bir kurtarma yöntemi değildir.

## VMProtect: isteğe bağlı, doğrulanmış koruma iddiası yok

Önceki yerel kayıt VMProtect'in kurulu olmadığını ve korunmuş DLL üretilmediğini
belirtir. Mevcut isteğe bağlı derleme bağlantısı:

```powershell
./scripts/Build-Release.ps1 -ProtectWithVMProtect -VMProtectPath 'C:/VMProtect/VMProtect_Con.exe' -VMProtectProject 'C:/Private/Onestep.vmp'
```

Lisanslı console sürümü ve gerçek GUI-kayıtlı `.vmp` gerekir; eksikler sessizce
atlanmaz. `.vmp` Git dışında tutulur. `AuthProtectionMap=true` MAP üretir;
giriş/imza/device proof inline edilmeyen hedeflerdir. Render/input'u topluca
sanallaştırmak yerine uyumluluk ayrıca sınanmalıdır.

Asıl DLL korunur; ayrı çıktı `build/protected` altındadır. Başarı kodu, dosya
ve farklı hash kontrol edilir; code signing korumadan **sonra** yapılır.
Bunlar seçilen projenin gücünü veya oyun/JNI uyumluluğunu kanıtlamaz.
Driver, gizli loader, AV/debugger kapatma veya sahte şifreleme eklenmiş sayılmaz.

## Açık olay işleri

Önceki metadata envanteri bu beş dosyayı iki inceleme dizininde buldu;
private içerikleri açılmadı. O zamanki silme girişimi komut politikasıyla
çalışmadan reddedildi; hiçbir dosyanın silindiği iddia edilmez.
Bu belge düzenlemesinde de secret dosyası aranmaz, açılmaz veya silinmez;
önceki red alternatif araçla aşılmamalıdır.

İncelenmiş her `server/` dizini altındaki dosya adları:

- `signing-ec.pem`
- `signing-private.pk8`
- `.credentials/worker-secrets.enc.json`
- `.credentials/wrapping-key.dpapi`
- `.credentials/legacy-licenses-2026-10-16.enc.json`

Tarihsel inceleme dizinleri:

- `C:/Users/zpens/Downloads/Wolf-Client-main/Wolf-Client-main/server/`
- `C:/Users/zpens/OneDrive/Dokumenty/Wolf-Client-main1/Wolf-Client-main/server/`

İnsan operatör güvenli kurtarma ve rotasyon sonrası tam dosya hedeflerini
değerlendirmelidir. OneDrive geçmişi, recycle bin, diğer checkouts ve yedekler
tam taranmamıştır. Dosya silmek açığa çıkmış anahtarı tek başına iptal etmez.
Public doğrulama bilgisi korunmalıdır.

`KEY_PEPPER` / `HWID_PEPPER` mevcut hash'leri korumak için değiştirilmemiştir.
Kör değişim lisans/cihaz/config bağlarını bozar. Açığa çıkan müşteri key'leri
kontrollü reissue gerektirir; admin girişini değiştirmek onları iptal etmez.
Güncel rotasyonda production private key/pepper yeni bir yerel kasaya taşınmaz.
PBKDF2/AES-GCM, Windows ACL ve 16+ karakter passphrase kullanan operator-vault
yardımcıları eski kurtarma araçlarıdır; aktif cloud-only akışın ön koşulu değildir.
Passphrase, OTP veya Access cookie chat/log/argüman/env değişkenine aktarılmaz.

## Güvenli değişiklik ve rollback

1. Operatör güvenilir hesap/cihazdan Access policy/MFA ve doğru hedefi inceler.
   `/admin*` ve `/api/v1/admin/*` birlikte korunur; oyun API'si yanlışlıkla
   Access arkasına alınmaz. Email OTP tek başına bağımsız MFA sayılmaz.
2. D1 kurtarma noktası/şema ve gereken migration önce yerel temsilî kopyada incelenir.
   Eski Worker/DB hedeflenmez; kayda göre uygulanmış migration tekrar çalıştırılmaz.
3. Signer değişimi onaylı yeni DLL/launcher dağıtımıyla koordinelidir. Eski pin'li
   binary yeni imzayı doğrulayamaz; yalnız `kid` değişimi çalınan key'i iptal etmez.
4. Yeni private/public secret çifti ve native pin birlikte doğrulanır.
   Aktarılan secret'ların hangi Worker version'dan geldiği açıkça kontrol edilir;
   son upload'ın mevcut live sürümle aynı olduğu varsayılmaz.
5. V1/bearer-only, eski signer veya ara coexistence sürümüne rollback yapılmaz.
   Yayın, lisans reissue, pepper migration ve dosya temizliği ayrı onaylı işlerdir.

### Koşullu eski HWID kurtarma örneği

Aşağıdaki SQL yalnızca tarihsel `hwid` kolonu olan, ayrıca incelenmiş temsilî
şema içindir; güncel DB'ye uygulanacak kurulum adımı değildir.
Hash'li bağları, ban/expiry durumunu korur; raw-only bağlarda reaktivasyon gerekir.
Gerçek üretimde çalıştırma bu belgenin veya bu temizlik görevinin kapsamı dışındadır.

```sql
BEGIN TRANSACTION;
UPDATE license_keys
SET hwid_hash = NULL, hwid_hint = NULL, device_key_hash = NULL,
    status = CASE WHEN status IN ('banned','expired') THEN status ELSE 'unused' END,
    license_epoch = COALESCE(license_epoch, 1) + 1
WHERE hwid IS NOT NULL AND hwid <> ''
  AND (hwid_hash IS NULL OR length(hwid_hash) <> 64 OR hwid_hash GLOB '*[^0-9a-f]*');
UPDATE license_keys SET hwid = NULL WHERE hwid IS NOT NULL;
COMMIT;
```

## Tarihsel geçiş kaydı: 20 Ekim 2026

Bu tablo geçmiş ara aşamaları sıralar, **aktif kurulum talimatı değildir**.
İmzalı dosya hash/boyutları ve güncel v2 sürümü `deployment.json` içindedir;
eski binary'ler v2'nin yerine dağıtılmamalıdır.

| Aşama | Tarihsel referans / sonuç |
| --- | --- |
| İlk Access-only | Worker `31f51779-db23-43a9-982b-52f64a618a6a`, deployment `128d53d1-5631-4d8d-b967-fe6e2a94645a`. Bearer kaldırıldı, nonce migration uygulandı. Dört lisans korundu, dört recoverable kopya temizlendi. |
| Migration öncesi kurtarma | Ön inceleme `0000003e-00000000-0000510a-8827027c722e3d3f7185e2bae86d33e2`; uygulama öncesi `00000040-00000000-0000510a-edc02fae08f123351203caae8e16833f`. Yerel plaintext DB yedeği oluşturulmadı; cloud retention ayrıca değerlendirilir. |
| Ara eski signer | Üretim referansı `10c8095c-776c-4d8d-b205-d96d04573f59`; mevcut native dosyalar korunmuştu. |
| Yeni signer staging | `51f35f2f-7b65-4cae-8846-7d806431800c`, key ID `onestep-20261020-7615fc79`. Eski admin binding'ini içerdiği için staging sürümü doğrudan deploy hedefi değildir. |
| Signer aktivasyonu | Worker `ff471720-3fd7-45a1-bab2-3b576415dc32`, deployment `3188282e-27c3-4879-a557-359165f3f0b2`, `2026-10-20T12:03:58.141077Z`. Önce onaylı DLL hash/boyutu doğrulandı; eski key aktif server'da güvenilmez oldu. |
| Eski native çift | `build/auth-rotation-20261020/` ve `auth-20261020-1.21.11`; v2 için superseded. Public installer olarak yayımlanmadı. |
| Launcher-only v2 | V1 kapatıldı, kaynakta current Access-only guard korundu; [v2 yayın kaydı](../docs/LAUNCHER_V2.md#kayıtlı-yayın-durumu). |

Tarihsel Access kurulum kimlikleri: uygulama
`66e9a254-7842-4092-97ba-e88f328c9215`, policy
`f305cb66-d4e3-4322-89bb-905216fd0cd1`; eksik App Launcher düzeltmesi
`f4ead4bd-3ecf-43a6-a024-832576359281`, policy
`859909c3-1fe7-4193-85a6-fe4f1068e1ff`.
İzin yalnız operatör email'i ve 30 dakikalık application/MFA session için
tanımlanmıştı; issuer/audience `wrangler.jsonc`'dedir. İlk enrollment'da stale
`#/NoAuth` yerine team root / `AddMfaDevice` yolu kullanılmıştır.
Bu kimlikler public metadata'dır, oturum/OTP değildir.

Operatör daha sonra normal Access/MFA tarayıcısıyla panele ulaştığını doğruladı;
upload sırasında cookie/bearer/private key çıkarılmadı. Sağlık 200 ve anonymous
admin redirect kaydedildi. Bu olaylar yeni bir canlı test gibi sunulmamalıdır.
Eski cloud sürüm geçmişi var olmaya devam eder; anahtarın tüm yedeklerden
silindiği veya geçmiş key materyalinin kaldırıldığı varsayılmaz.

## Yerel doğrulama ve referanslar

[Server testleri](README.md#yerel-doğrulama), native
`scripts/Test-AuthConfiguration.py`, `scripts/Test-AuthVault.ps1` ve
[launcher protokol kontrolleri](../docs/LAUNCHER_V2.md#doğrulama-ve-dağıtım)
synthetic key/JWKS ve izole SQLite/workerd kullanır; production vault PoC'si
çalıştırılmaz. Eski bir timeout/retry kaydı güncel koşu için başarı sayılmaz.

- [CNG persisted keys](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptcreatepersistedkey)
- [CNG export policy](https://learn.microsoft.com/en-us/windows/win32/seccng/key-storage-property-identifiers)
- [VMProtect console](https://vmpsoft.com/support/user-manual/working-with-vmprotect/)
