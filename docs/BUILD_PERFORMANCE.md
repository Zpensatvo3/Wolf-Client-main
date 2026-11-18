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

## 18 Kasım 2026 teslimi ve gerçek ölçüm

Önceki geliştirmelerin tamamı `main` dalında birleştirildi. Derleme düzeninin kaynak commit'i **`41f15868b1f95bb00b2c2da073ff49a438438120`**. Ardından yapılan README/teslim belgesi güncellemeleri DLL kaynak kodunu değiştirmez.

[Genel workflow](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44390600356) başarılı: geniş offline test paketi, gerçek EGL Sky testi, Java köprüleri ve yedi Windows x64 Release hedefi geçti. 1.21.1, 1.21.4, 1.21.8, 1.21.11, 26.1, 26.1.1 ve 26.1.2 için ayrı DLL artifact'leri bu çalıştırmada bulunur. Gerçek Minecraft/oyun kabulü bu derleme kaydından çıkarılamaz.

Aynı commit'in 1.21.11 hedefi [ikinci çalıştırmada](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44394889306) tekrar üretildi:

| Ölçüm | DLL derleme adımı | Cache hit / istek |
| --- | --- | --- |
| Önceki yöntem, `44344276834` | 11 dk 33 sn | Önbellek yok |
| Yeni yöntem, ilk 1.21.11 derlemesi | 13 dk 15 sn | 0 / 128 |
| Yeni yöntem, ikinci 1.21.11 derlemesi | **1 dk 53 sn** | **127 / 128 (%99,22)** |

İlk ve ikinci yeni çalıştırma arasında DLL adımı yaklaşık 7 kat hızlandı. İkinci hedefli workflow'un testler, kurulum ve yükleme dahil toplam süresi **4 dk 44 sn**; eski hedefli çalıştırma 14 dk 28 sn idi. Genel workflow'un toplam süresi 28 dk 50 sn: daha geniş test paketi ve yedi hedefin en yavaş ilk derlemesini içerdiği için tek hedefli workflow ile aynı ölçüm değildir. Hosted runner değişkenliği ve cache tahliyeleri sonraki süreleri etkileyebilir.

İkinci çalıştırmanın ayrıntılı sayacı: 127 hit, 1 miss, 0 derleme hatası, 0 cache okuma hatası, 0 cache dışı derleme ve **1 cache yazma hatası**. Bu yazma hatası derlemeyi/artifact yüklemesini engellemedi; ayrıntılı hata nedeni normal logda yok. Dolayısıyla %100 hit veya tüm cache işlemlerinde sıfır hata iddia edilmez. İlk 1.21.11 çalıştırmasında 128 sonuç başarıyla yazılmıştı.

- [Güncel 1.21.11 DLL](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44394889306/artifacts/14471370175): `Onestep-1.21.11-Windows-x64`.
- [Güncel 1.21.1 DLL](https://github.com/Zpensatvo3/Wolf-Client-main/actions/runs/44390600356/artifacts/14467624332): `Onestep-Windows-x64`.
- Diğer beş sürümün çıktıları yukarıdaki genel workflow'un artifact listesindedir.

Windows compiler launcher argüman/Unicode/hata kodu testi her iki çalıştırmada geçti. Yerelde altyapı, repository hygiene, önbellek sözleşme kontrolleri ve tam C++ offline paketi geçti; workflow YAML dosyaları actionlint ile doğrulandı.
