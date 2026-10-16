# Onestep cihaz ve yayın koruması

HWID V3 algoritması değiştirilmedi; mevcut lisansların cihaz özeti korunur.
Yeni istemci ayrıca Windows CNG'de `Onestep.Auth.Device.V1` adlı, private key
export izni olmayan kalıcı kullanıcı anahtarı oluşturur. Önce mevcut anahtarı
açar; anahtarı üzerine yazmaz. Yeni anahtarda TPM Platform Crypto Provider
tercih edilir; destek yoksa Software Key Storage Provider kullanılır.
Bu bilgisayardaki native kontrolde TPM yolu çalıştı.

Giriş ve heartbeat isteği P-256/SHA-256 ile imzalanır. İmza; işlem türünü,
HWID'yi, nonce'u, zamanı ve lisans/oturum token'ının hash'ini kapsar. Yalnızca
public point ve imza gönderilir. Sunucu public key hash'ini lisansa bağlar ve
imzalı cevabına ekler; istemci cevapta kendi anahtarının eşleşmesini kontrol eder.
Donanım bilgisi yetersiz olduğunda bile yalnızca HWID metnini kopyalamak cihaz
private key'inin yerine geçmez. Bu akış bir TPM attestation protokolü değildir;
server bir provider adını güvenlik kanıtı olarak kabul etmez.

## Geçiş ve kurtarma

Mevcut lisanslar ilk yeni-sürüm girişine kadar eski akışı korur. Yeni sürümün
başarılı girişinde `device_key_hash` atomik olarak bağlanır; bundan sonra aynı
lisans için imzasız/eski DLL girişleri ve başka cihaz anahtarı reddedilir.
Bu noktadan sonra eski DLL'ye geri dönmek HWID sıfırlama gerektirir.
Admin panelindeki HWID Reset hem HWID hem cihaz anahtarı bağını temizler ve
epoch'u artırır. Windows hesabı/anahtar deposu kaybolursa da bu yol kullanılır.
Normal kapatma veya DLL güncellemesi anahtarı silmez.

Private cihaz anahtarı DLL'de, repoda veya `.credentials` yedeğinde bulunmaz.
Lisans/cihaz kontrolleri sunucuda kalır; istemci dosyasını değiştirmek sunucuda
kayıtlı cihaz anahtarını değiştirmez. Yerel makine tamamen ele geçirilirse bu
katmanın koruması sınırlıdır; yazılım provider'ı TPM ile aynı güvenceyi sunmaz.

## VMProtect bağlantısı

Bu makinede VMProtect yoktur. Bu nedenle henüz VMProtect uygulanmış bir DLL
üretilmedi. Derleme bağlantısı hazırdır:

```powershell
.\scripts\Build-Release.ps1 -ProtectWithVMProtect -VMProtectPath 'C:\VMProtect\VMProtect_Con.exe' -VMProtectProject 'C:\Private\Onestep.vmp'
```

Geçerli, lisanslı console sürümü ve GUI ile kaydedilmiş gerçek `.vmp` projesi
gerekir. Eksikler hata verir; koruma sessizce atlanmaz. `.vmp` dosyaları olası
lisans materyali içerdiğinden git dışında tutulur. `AuthProtectionMap=true`
koruma hedeflerini seçmek için MAP dosyası üretir. Giriş, imza kontrolü ve cihaz
imzası işlevleri ayrı, inline edilmeyen hedeflerdir. Render/input döngülerini
toplu sanallaştırmak yerine yalnızca lisans işlevlerini seçip uyumluluğu dene.

VMProtect orijinal DLL'yi değiştirmez; her çalıştırmada `build/protected` altında
ayrı çıktı üretilir. Başarı kodu, dosya ve farklı hash kontrol edilir. Kod imzası
varsa koruma işleminden **sonra** son dosyaya uygulanır. Bu kontroller tek başına
oyun/JNI uyumluluğunu veya seçilen VMProtect projesinin koruma gücünü doğrulamaz.

Kaynaklar: [Microsoft CNG persisted keys](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptcreatepersistedkey),
[CNG key export policy](https://learn.microsoft.com/en-us/windows/win32/seccng/key-storage-property-identifiers),
[VMProtect console kullanımı](https://vmpsoft.com/support/user-manual/working-with-vmprotect/).
