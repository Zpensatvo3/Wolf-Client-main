# Aim Assist V2

Normal Aim Assist artık menüde **Aim Assist V2** olarak görünür. Stable ID
`aim_assist`, eski config alanları, varsayılan %42 güç, menzil ve FOV korunur.
Modül sayısı değişmez. Legit Click Aim Assist klasik davranışı kullanır.

## Vape karşılaştırması

| Davranış | Önceki normal Aim Assist | Vape | V2 |
| --- | --- | --- | --- |
| Kamera yazımı | Yalnız güncel yaw/pitch | Önceki ve güncel açı birlikte kaydırılır | Önceki ve güncel açı birlikte kaydırılır |
| Hitbox sınırı | Kesişimde hız hemen sıfırlanır | Yakın hedefte sönümleme, biriken hareket | 0.12–1.5 derece arasında sürekli yumuşatma |
| Hedef değiştirme | 2.5 puan avantaj + 80 ms challenger | Basılı aktivasyonda geçerli hedef tutulur; sürekli modda 700 ms | Geçerli hedef en az 700 ms tutulur, ardından mevcut challenger kuralı |
| Sayısal adım | Frame-time tabanlı kritik sönümlü çözüm | Simple: 1 ms alt adımlar; Adaptive: zaman tabanlı | Mevcut zaman tabanlı çözüm ve hız sınırı |
| Dikey kontrol | Yaw ve pitch | Varsayılan dikey yardım kapalı | Mevcut yaw/pitch ve aim-point sözleşmesi |

En somut fark interpolasyon geçmişidir. Render açısı `previous + alpha *
(current - previous)` ise yalnız current'a eklenen 1 derece düzeltme, render'a
alpha kadar yansır. İki uç da kaydırıldığında her alpha için tam 1 derece yansır.
Vape adaptörü bunu zaten yapıyor; normal motor yapmıyordu. Bu, gerçek oyundaki
görsel sonucun tamamını kanıtlamayan, sayısal olarak doğrulanabilir bir farktır.

Normal motorda hitbox'a girince `stop_motion`, çıkınca tekrar hızlanma çalışıyordu.
V2 bu sert sınır yerine seçilen aim point'e açısal yaklaşımı yumuşatır. Bu nedenle
hitbox içinde de küçük düzeltmeler yapabilir; 0.12 derece içinde durur. Manuel
ters yön girişi, odak/menü kontrolleri, görünürlük, arkadaş/bot filtreleri ve
uzun kare kesintisinde reset devam eder. Geçersiz hedef 700 ms beklenmeden bırakılır.

Vape'nin rastlantısal drift'i ve sensitivity quantizer'ı tek başına daha akıcı
olduğunun kanıtı değildir. İki modülün varsayılan dikey yardım, aktivasyon, FOV ve
menzil değerleri farklıdır; kullanıcı hissi karşılaştırılırken bunlar eşitlenmelidir.

## Doğrulama

`aimassist_math_test.cpp`: interpolasyonun tüm alpha değerlerinde eşit düzeltme,
yaw wrap, pitch sınırı, geçersiz sayılar, yumuşatma bandının sürekliliği,
30/60/144/240 FPS yaklaşımı, overshoot ve sıfır güç kontrolleri.
Mevcut config ve Vape testleri de korunur. Kaynak kontrolü V2'nin normal modüle
bağlandığını ve geçmiş açıların yazıldığını denetler; JNI oyun testi yerine geçmez.

Windows/Minecraft kabulü: iki modülü ayrı ayrı aynı ayarlarda çalıştırıp
sabit ve strafing hedef, hitbox kenarı, hedeflerin kesişmesi, +/-180 yaw,
manuel ters mouse hareketi, menü/odak kaybı ve tekrar etkinleştirmeyi dene.
FPS ve frame-time kaydıyla görsel akıcılık ayrıca doğrulanmalıdır.
