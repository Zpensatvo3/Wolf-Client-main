# Ortak Java köprüsü başlatma hatası

## Bulgular

2026-10-22 tarihinde açık Minecraft işleminin oyun klasörü Modrinth
`Fabric 1.21.11` profiliydi. Bu oturumdaki `logs/latest.log` içinde 13:59:55,
14:29:55 ve 14:51:19 saatlerinde şu hata kaydedilmişti:

```text
java.lang.NoSuchMethodError: Method 'long net.onestep.hybrid.WorldBridge.auraService(int, java.lang.Object, int, long, long)' name or signature does not match
```

Kaynak ve gömülü `WorldBridge.class`, Crystal Aura'nın Java bölümünü içermiyordu.
Native başlatma hâlâ `auraService` kaydını ve `tickAura`/`cancelAura`/
`executeAuraAction` bağlantılarını zorunlu tutuyordu. `RegisterNatives` aşamasındaki
hata diğer WorldBridge modüllerinin de başlatılmasını engelliyordu.

İkinci bağımsız sorun `ItemBridge` içindeydi: kaldırılmış Inventory Clicker'ın
`captureInventory` ve `pickup` metotları Java'da yokken native bağlantı sırasında
aranıyordu. Bu da Auto Tool, EXP ve clicker'ların ortak ItemBridge'ini kapatıyordu.
Bu ikinci bulgu kaynak/classfile karşılaştırmasıyla tespit edildi; logdaki
`auraService` hatasıyla aynı hata olarak sunulmamalıdır.

## Düzeltme

- Crystal Aura'nın bağımsız Java denetleyicisi geri getirildi. Önceki
  `mob-repeat-fix-20261022-123648` DLL'inden alınan 108893 baytlık sınıf
  karşılaştırma kaynağı olarak kullanıldı. Hareketli hedef düzeltmesi,
  Weakness bağımsızlığı, eşya/hasar kontrolleri ve temizleme korundu.
- Son Hit Crystal, Safe Anchor, InventoryBridge ve ClientBridge değişiklikleri
  korundu. Tick olaylarına Aura geri eklendi; özellik sayısı yeniden 9 oldu.
- Inventory Clicker'a ait kullanılmayan native metotlar ve bağlantıları kaldırıldı.
- `scripts/Check-GameBridgeContracts.py` altı köprünün native/Java metotlarını
  ve JNI native bildirimlerini gerçek derlenmiş sınıflardan kontrol eder.
  DLL verilirse RCDATA sınıflarını DLL'i çalıştırmadan okuyup kaynak kaynaklarıyla
  karşılaştırır. Kaynak uyuşmazlığına rağmen başarılı DLL üretilemez.
- Kontrol Java derleme betiğine, resource derlemesinden önceye ve link sonrasına
  eklendi. Davranış test paketi de bu kontrolü çalıştırır ve Aura'yı yeniden içerir.

## Kullanım sınırı

Yeni DLL açık oyundaki eski DLL'in yerini kendiliğinden almaz. Temiz bir oturumda
Minecraft yeniden açılıp yeni çıktı kullanılmalıdır. Çalışan oyun bu düzeltme
sırasında kapatılmadı ve yeni kod canlı oyuna enjekte edilmedi.
