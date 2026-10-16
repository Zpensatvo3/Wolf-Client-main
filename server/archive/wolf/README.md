# Tarihsel Wolf kayıtları — çalıştırmayın

Bu dizin OneStep'in aktif import veya deployment grafiğinin parçası değildir.
Dosyalar `77373b6` öncesindeki Wolf ortamının ve eski migration kodunun kurtarma
referanslarıdır. Çalıştırılabilir dosya uzantıları özellikle `.txt` yapılmıştır.

- `wrangler.legacy.toml.txt`: eski Worker/D1 binding tanımları.
- `legacy-admin-redirect.mjs.txt`: eski Worker'a eklenen browser redirect yardımcısı.
- `license-schema-migration.js.txt`: eski raw-key tablo dönüştürme işlevinin bağlamlı örneği;
  kendi başına çalıştırılabilir bir migration değildir.
- `ADMIN_MIGRATION.md`: önceki operasyonun tarihsel notu; güncel durum kanıtı değildir.

Eski Worker ve D1 bağımsızdır. Bu dosyaları aktif OneStep kaynaklarıyla birleştirip
eski ortama deploy etmeyin. Tarihsel public configleri otomatik taşımayın veya silmeyin.
Kurtarma gerekiyorsa operatör ilgili eski commit'i ve mevcut şemayı ayrı incelemelidir.
Gizli değerleri görüntülemeyin; DPAPI/şifreli yedek dosyalarının içeriği burada yoktur.
