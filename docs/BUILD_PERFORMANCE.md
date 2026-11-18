# Derleme performansı ve önbellek

## Ölçülen başlangıç durumu

18 Kasım 2026 tarihli [44344276834 numaralı çalıştırma](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44344276834) toplam 14 dakika 28 saniye sürdü. İlk test job'ı 1 dakika 20 saniye, Windows Java kontrolleri 1 dakika 11 saniye, Release DLL adımı 11 dakika 33 saniye ve artifact yüklemesi 2 saniyeydi. Logda 128 C/C++ kaynak dosyasının derlendiği görüldü.

Workflow `-Clean` ile MSBuild `Rebuild` çalıştırıyor, her çalıştırmada yeni Windows ortamı açılıyor ve derleme önbelleği kullanılmıyordu. Yerel Release derlemesinde PCH ve `/MP` zaten açıktı.

## Yeni düzen

Her iki Windows workflow'u `Build-Release.ps1 -CompilerCache -Timing` kullanır. Mozilla sccache v0.16.0, GitHub Actions cache backend'ine derleme sonuçlarını kaydeder. Önbellek kimliği kaynak/başlık içeriklerinden, derleyiciden ve derleme seçeneklerinden türetilir; farklı Minecraft hedeflerinin tanımları birbirinin çıktısı yerine kullanılmaz. Link, kaynak paketleme ve paketlenmiş Java sözleşme kontrolleri normal derleme akışında kalır.

MSBuild'in CL görevi [CompilerCache.cs](../scripts/CompilerCache.cs) üzerinden `sccache <gerçek-cl.exe> <argümanlar>` çalıştırır. Gerçek derleyicinin yolu seçilen MSBuild toolset'inden bulunur. Boş, boşluklu, tırnaklı ve Unicode argümanlar ile hata kodu aktarımı [Windows testinde](../scripts/Test-CompilerCache.ps1) doğrulanır.

**sccache, MSVC PCH kullanımını desteklemiyor.** Bu yüzden yalnız `-CompilerCache` modunda PCH kapalıdır. `pch.hpp` normal zorunlu include olarak kalır. `/MP` yerine `UseMultiToolTask` ile kaynak başına bir derleyici çağrısı yapılır; eşzamanlı süreç sayısı mantıksal işlemci sayısıyla sınırlandırılır. Normal yerel derlemenin PCH ve `/MP` ayarları korunur. Soğuk cache derlemesi, eski PCH derlemesinden daha uzun sürebilir; ilk ve sonraki çalıştırmalar ayrı ölçülmelidir.

İlk çalıştırma cache'i doldurur. Sonraki çalıştırmalar da dosyaları preprocess eder; aynı derleyici/seçeneklerle aynı içerik bulunduğunda pahalı derleme aşaması önbellekten karşılanır. Kaynak/başlık değişiklikleri ilgili sonuçları geçersiz kılar. Önbelleğin doluluğu, branch erişim kapsamı, runner/toolchain güncellemeleri ve GitHub cache tahliyeleri süreyi etkiler. Sabit bir hızlanma katsayısı vaat edilmez.

Test kapıları kaldırılmadı: DLL job'ı offline testleri bekler ve artifact yalnız başarılı derlemeden sonra yüklenir. Güncelliğini yitiren aynı dal/workflow çalıştırmaları iptal edilir. Sadece Markdown değişiklikleri otomatik matris derlemesi başlatmaz. Genel workflow 1.21.1 dahil yedi hedefi üretir; hedefli workflow yalnız 1.21.11 içindir.

## Yerel kullanım

```powershell
# Artımlı, PCH kullanan normal yerel derleme
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign

# Tam yeniden derleme
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -Clean -SkipSign

# sccache.exe kuruluysa yerel derleyici önbelleği
$env:SCCACHE_PATH = (Get-Command sccache.exe).Source
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -CompilerCache -Timing -SkipSign
```

Yerel sccache kullanımında GitHub backend'i zorunlu değildir; varsayılan yerel disk önbelleği kullanılabilir. Normal ve cache modları arasında geçiş derleyici seçeneklerini değiştirdiği için ilk geçişte yeniden derleme beklenir.

## Doğrulama ve süre karşılaştırması

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_build_cache.py' -v
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
```

Windows workflow'u ayrıca `Test-CompilerCache.ps1` çalıştırır. `-Timing` MSBuild görev sürelerini ve toplam süreyi konsola yazar; ayrıntılı `release.binlog` yerelde `build/logs/<sürüm>/` altında kalır. `sccache --show-stats`, derleme hata verse de sonunda yazdırılır. Bu çıktıdaki cache hit/miss ve non-cacheable nedenleri kontrol edilmelidir; yalnız action'ın kurulmuş olması cache'in çalıştığını kanıtlamaz.

Aynı kaynak ve hedef için iki başarılı GitHub çalıştırması karşılaştırılır: ilki önbelleği doldurur, ikincisi kullanımını gösterir. İkinci çalıştırmada hit sayısı artmıyorsa compiler argümanları, PCH/batch ayarları ve cache erişimi incelenir. DLL dosyası her başarılı çalıştırmanın kendi artifact'inden alınır; önceki artifact yeniden yayımlanmaz.
