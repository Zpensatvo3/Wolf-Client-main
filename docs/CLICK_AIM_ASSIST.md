# Click Aim Assist

Legit Click Aim Assist ve Blatant Click Aim Assist tek `click_aim_assist`
kimliği altında birleştirilmiştir. Menü adı **Click Aim Assist**; ilk ayar
**Mode: Legit / Blatant**. Varsayılan Legit'tir. Toplam kayıtlı modül sayısı 59.

Her modun strength, FOV, menzil ve aim-point ayarları ayrı saklanır. Legit
Weapon Only; Blatant Only Weapon ve Last Target seçeneklerini korur. Mod değişimi
gizlenen profilin ayarlarını silmez. İçe/dışa aktarımda iki profil de bulunur.
Strength birimleri dönüştürülmez: Legit 0–100, Blatant 0.05–1.

Legit mevcut render/client-thread motorunu, Blatant mevcut worker motorunu
kullanır. Mod seçimi ve ayarlara erişim aynı mutex ile sıralanır; iki motor
aynı anda aktif olmaz. Değişiklikler bir revision artırır; her motor sonraki
çalışmasında kendi hedef/zamanlama durumunu sıfırlar. Legit JNI referansları
render kapanışında temizlenir.

Eski `legit_click_aim_assist` ve `blatant_click_aim_assist` config kayıtları
tek modüle aktarılır. İki profilin ayarları korunur; yalnız Blatant açıksa
Blatant seçilir, diğer eşitliklerde Legit önceliklidir. Seçilen eski modülün
enabled/keybind alanları aktarılır. Yeni canonical kayıt varsa seçim ve
keybind onda kalır. Sıra sonucu değiştirmez. Yeni dışa aktarımlar tek ID içerir.

Eski yerel keybind dosyalarında canonical ID önceliklidir; yoksa sıfır olmayan
Legit bindingi, ardından Blatant bindingi seçilir. Yeni modül tek keybind
taşıdığı için eski iki bağımsız tuştan biri kullanılabilir.

Config testleri aktarım, öncelik, eski aim-point alias'ları, round-trip,
geçersiz ayarda rollback ve her iki modun görünür alanlarını doğrular.
Oyun kabulü: basılı/serbest sol tıkla mod değiştirme, hedef kaybı,
modülü kapatma/açma, menü/odak kaybı ve eski profilleri yükleme denenmelidir.
