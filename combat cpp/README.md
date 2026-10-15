# Vape V4.21 Combat Suite (C++20 Port)

Bu kütüphane, **Vape V4.21** combat modüllerinin **Fabric 1.21+** mantığıyla **Modern C++20** standardına, **en basitten en karmaşığa** doğru dönüştürülmüş halidir.

Toplam **36 başlık dosyası (header-only)**, tüm dahili zamanlayıcıları, algoritmik hesaplama mekaniklerini ve komparatörleri birebir içerir.

---

## 📊 Karmaşıklık Hiyerarşisi (Basitten Karmaşığa)

### 🔹 Seviye 1: Temel Matematik, Vektörler ve Primitifler
- **`math/Vector3.hpp`**: 3D koordinatlar, AABB kutu kesişimi ve `getClosestPoint` (Vape'in `T()` fonksiyonu).
- **`math/RotationUtil.hpp`**: Açı farkları, `wrapDegrees` ve Minecraft fare hassasiyeti GCD kuantizasyon formülü.
- **`utility/Timer.hpp`**: Yüksek hassasiyetli `std::chrono` zamanlayıcı.
- **`utility/Sprint.hpp`**: Sürekli sprint ve görünmezlikte partikül çıkarmama koruması.
- **`utility/NoItemRelease.hpp`**: GUI açıkken yay, kalkan veya yiyecek kullanımının iptal edilmesini önleme.

### 🔹 Seviye 1.5: Hesaplama Mekanikleri ve Modül Besleme Kaynakları (Utility & Engines)
- **`utils/RayTraceUtil.hpp`**: 3D Ray-AABB kesişim çözücüsü, hitbox genişletme (boxExpand), blok arkası görüş hattı (Line-of-Sight) kontrolü ve pitch/yaw yön vektörleri.
- **`utils/ItemStackScoreUtil.hpp`**: 1.21 silah materyal hasarı, keskinlik (Sharpness), 1.21 Mace Smash hasarı + Density bonusu, Breach zırh delme ve zırh/sertlik (toughness) hasar azaltma matematiği.
- **`utils/AttackCooldownUtil.hpp`**: 1.21 Attack Cooldown eğrisi ($0.2 + cd^2 \times 0.8$), tick offset hazırlık kontrolü, süpürme atağı (sweep) ve kritik vuruş uygunluk kuralları.
- **`utils/EntityComparators.hpp`**: Modüllerin hedefleri sıralamak için beslendiği 5 temel komparatör (FOV/Yaw farkı, 3D mesafe, can miktarı, zırh seviyesi, tehdit/ekipman skoru).

### 🔹 Seviye 2: Hareket ve Zamanlama (Movement & Timing)
- **`movement/WTap.hpp`**: Her vuruşta bonus knockback almak için paket veya klavye sprint resetleyici.
- **`movement/JumpReset.hpp`**: Hasar paketinin geldiği tickte zıplayarak yatay savrulmayı sıfırlama, insansı zamanlama doğruluğu (% accuracy) ve sıvı kontrolü.
- **`movement/Velocity.hpp`**: Paket düzeyinde knockback ölçekleme (% H / % V), 0% knockback paket iptali ve kaçış için Kite modu (%120 H).
- **`movement/KnockbackDelay.hpp`**: Gelen hız paketlerini yerde veya havada kuyrukta bekleterek desync oluşturma.

### 🔹 Seviye 3: Tıklama Motorları ve Basit Tetikleyiciler
- **`clicker/AutoClickerTimingState.hpp`**: Irwin-Hall 6 örneklem biyolojik dağılımı, kas yorgunluğu (fatigue), dinlenme toparlanması (idle recovery), %6 gecikme tekrarlama eğrisi ve periyodik drift.
- **`clicker/ClickEngine.hpp`**: 3 Farklı mod (Normal, Legacy Extra patlama, Extra+ biyomimetik), adım interpolasyonlu mikrotitreme (jitter).
- **`clicker/LeftClicker.hpp`**: Blok kazma korumalı ve silah filtreli insansı sol tık tetikleyicisi.
- **`clicker/RightClicker.hpp`**: Hızlı blok koyma (fast place), sarf malzemesi/inci desteği ve offhand önceliği.
- **`combat/Triggerbot.hpp`**: Mouse-over gecikmesi (0-200ms), 1000ms toleranslı "Select First Hit", havada kritik vuruş (Air Crits), Balta/Topuz kalkan baypas kontrolü ve erken/ıskalama vuruş simülasyonu.
- **`combat/Reach.hpp`**: Ray-AABB kesişim matematiği, istemci tarafı misplace telafisi ve dikey mesafe filtreleri.

### 🔹 Seviye 4: Taktiksel Mekanikler ve Dinamik Silah Değişimi
- **`combat/ShieldBreaker.hpp`**: Kalkan kaldıran hedefe anında balta ile vurup kalkanını devre dışı bırakma (5 sn), çift vuruş (double-click) ve zamanlama gecikmeleri.
- **`combat/AutoMace.hpp`**: 1.21 Topuz/Mace düşüş takibi ($\ge 1.5$ blok), Elytra otomatik çıkarma/geri takma durum makinesi, Density vs Breach akıllı seçici ve smash atağı.
- **`combat/HitSwap.hpp`**: Gelişmiş **Stun-Slam** çok fazlı kombo: Kalkan kırıcı Balta $\rightarrow$ Topuz/Mace bitirici vuruşu $\rightarrow$ Kılıca geri dönüş.
- **`combat/HitSelect.hpp`**: Hurt-time kontra atak kombo kilidi ve erken vuruş baskılama.
- **`combat/BlockHit.hpp`**: 1.21 Kalkan bloklama ve 1.8 kılıç blok-hit (Predict 8-aralık vuruş analizi, reaktif ve gecikmeli bloklama).

### 🔹 Seviye 5: Takip ve Durum Tahmini Motorları
- **`tracker/AttackStrengthTracker.hpp`**: 1.21 rakip silah saldırı hızı (Kılıç: 1.6, Balta: 0.8, Topuz: 0.6), el sallama zamanlaması, cooldown doluluğu ve hasar tahmini.
- **`tracker/AttackPacketTimingTracker.hpp`**: Vuruş paketi ile sunucu hasar onayı arasındaki gerçek RTT ping gecikmesini ölçme.

### 🔹 Seviye 6: En Karmaşık - Rotasyon, Aimbot ve Aura
- **`rotation/RotationManager.hpp`**: Sessiz rotasyon yönetim merkezi. **Silent Strafe Movement Fix** ile kamera hedefe dönükken dahi oyuncunun orijinal hareket vektörünü korur.
- **`aimassist/AimAssistRotation.hpp`**: 1.7x hedef takip kinematiği, 1.6x strafe hızlandırma desteği, yakınlık ivmesi, ölü bölge (deadzone) sönümleme ve GCD kuantizasyonu.
- **`aimassist/AimAssistTargeting.hpp`**: Bağıl hız (relative motion), hedef tahminleme (lead prediction) ve havada zıplama telafisi.
- **`aimassist/AimAssist.hpp`**: 5 hedef sıralama komparatörü, madencilik/GUI kontrolü ve çift motor orkestrasyonu.
- **`combat/BowAimbot.hpp`**: Yerçekimi ($g=0.05$) ve ok hızı balistik yörünge çözücüsü, hareket tahmini, -45° menzil dışı kilitleme ve alçak/yüksek yay seçimi.
- **`combat/HitFlick.hpp`**: 4 fazlı durum makinesi (IDLE $\rightarrow$ FLICKING $\rightarrow$ ATTACKING $\rightarrow$ RETURNING), Blink paket tamponlama ve strafe açı sapması.
- **`combat/SilentAura.hpp`**: PID yumuşatma, 4 hedefleme modu, 4 vuruş bölgesi modu, 3D jitter ve ekstra vuruş menzili ön-nişanlama.
- **`combat/CrystalAura.hpp`**: Gerçekçi patlama fiziği, zırh sertliği ölçekleme, Direnç & Patlama Koruması indirgemesi, intihar önleme ve modern 1.21 1-hava-bloğu kuralı.
