# GCC'yi değiştir, derle, indir

Bu düzen, kaynak kodundaki değişiklikten yeni bir **Linux x86_64 C/C++ GCC**
üretir. Derleyici, çalışma zamanı kütüphaneleri, testler ve indirilebilir
paket aynı kaynak commit'inden hazırlanır.

## Kullanım

Bu PR birleştirildikten sonra:

1. Kaynak kodunda değişikliğini yap ve bir branch'e commit gönder.
2. **Actions → Build GCC → Run workflow** bölümünde o branch'i seç.
3. İlk denemede `mode=quick`, `tests=selected` kullan.
4. Başarılı çalışmanın **Artifacts** bölümünden `gcc-linux-x86_64-…` indir.
   İçinde derleyicinin `.tar.gz` paketi ve SHA-256 dosyası bulunur.
5. Test bozulursa `gcc-reports-…` içindeki `.sum`, `.log`, yapılandırma ve
   derleme günlüklerine bak; değişikliği düzeltip yeniden çalıştır.

`workflow_dispatch` düğmesinin görünmesi için workflow dosyası varsayılan
`main` dalında bulunmalıdır. Seçilen branch de workflow ve yardımcı dosyaları
içermelidir. Bu düzen henüz taslak PR'dayken otomatik tam derleme başlamaz.

| Seçim | Yapılan iş | Ne zaman? |
| --- | --- | --- |
| `quick` + `selected` | Tek aşamalı C/C++ derlemesi, dahili self-test'ler, seçili DejaGnu testleri, kurulum ve çalıştırma denemesi, paket | Küçük değişiklik ve günlük geliştirme |
| `quick` + `full` | Tek aşamalı derleme; C/C++ ön uç ve libstdc++ test takımlarının tamamı | Daha geniş regresyon kontrolü |
| `bootstrap` + `selected` | Üç aşamalı derleme, stage2/stage3 karşılaştırması ve seçili testler | Derleyicinin kendisini doğru derlediğini kontrol |
| `bootstrap` + `full` | Bootstrap ve tam C/C++ + libstdc++ test takımları | Önemli değişiklik sonrası kapsamlı kontrol |

PR **Ready for review** olduğunda ve kod `main` dalına gönderildiğinde
`quick + selected` otomatik çalışır. Taslak PR'larda yalnızca CI dosyalarının
kontrolü çalışır. Markdown, `gcc/doc/` ve `maintainer-scripts/` değişiklikleri
tek başına derleme başlatmaz. Zamanlanmış gece derlemesi yoktur. Yeni bir
gönderim aynı PR/dal için önceki çalışmayı iptal eder.

## Değiştirdiğin alanın testlerini seç

Varsayılanlar, C için `dg.exp=inline*.c`, C++ için
`dg.exp=template/explicit*.C`. Bunlar küçük bir başlangıç kümesidir;
değiştirdiğin her alanı otomatik olarak kapsamaz.

Örneğin yeni C regresyon testin `gcc/testsuite/gcc.dg/pr12345.c` ise:

```text
c_tests: dg.exp=pr12345.c
```

C++ testin `gcc/testsuite/g++.dg/template/my-change.C` ise:

```text
cxx_tests: dg.exp=template/my-change.C
```

Her alanda tek bir `harness.exp=dosya-glob` seçicisi kabul edilir; `*` ve `?`
kullanılabilir. Çoklu seçici, ek DejaGnu seçenekleri ve kabuk ifadeleri
desteklenmez. `tests=full` olduğunda bu seçiciler kullanılmaz.

Eksik sonuç dosyası, hiç `PASS` üretmeyen seçim, `FAIL`, `XPASS`, `ERROR` veya
`UNRESOLVED` sonucu işi başarısız yapar. `XFAIL` ve `UNSUPPORTED` ayrıca
raporlanır. Tam test takımındaki mevcut upstream hataları da başarısızlık
olarak görünür: bu başlangıç düzeni bir temel sürümle otomatik karşılaştırma
yapmaz. Bir hata görünce aynı testi değişiklik öncesi commit'te de çalıştır.
İleride iki sürümün `.sum` dosyaları `contrib/compare_tests` ile karşılaştırılabilir.

## İndirdiğin derleyiciyi kullan

Artifacts ZIP dosyasını açtıktan sonra, içindeki paketin olduğu dizinde:

```bash
sha256sum --check gcc-uaglbr-linux-x86_64-*.tar.gz.sha256
tar -xzf gcc-uaglbr-linux-x86_64-<commit>.tar.gz
export GCC_HOME="$PWD/gcc-uaglbr"
export PATH="$GCC_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$GCC_HOME/lib64:$GCC_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
gcc --version
g++ --version
gcc program.c -o program
g++ -std=c++17 program.cc -o program-cxx
```

`<commit>` yerine indirilen dosyanın adındaki değeri yaz. Sistem GCC'sini
değiştirmene gerek yok; bu değişkenler mevcut terminal oturumu için geçerlidir.
Paketin içindeki `BUILD-INFO.txt` kaynak commit'ini, runner ortamını ve test
seçimlerini kaydeder.

Paket Ubuntu 24.04 x86_64 üzerinde hazırlanır; aynı veya uyumlu glibc ve
paylaşılan kütüphaneleri olan Linux ortamı içindir. Statik/evrensel dağıtım
paketi değildir. Windows, macOS, Android, ARM, diğer GCC dilleri ve 32-bit
multilib bu başlangıç kapsamına dahil değildir.

## Runner ve saklama

- Runner: GitHub tarafından sağlanan `ubuntu-24.04` x86_64.
- Derleme: 4 paralel iş; test: 2 paralel iş. Toplam süre sınırı 350 dakika.
  Bunlar kaynak sınırlarıdır, çalışma süresi tahmini değildir.
- `bootstrap-lean` ara aşama nesnelerini temizler; debug bilgisi üretilmez.
  Disk ya da süre sınırına ulaşan büyük işler için daha büyük runner gerekir.
- Paketler 7 gün, günlükler 14 gün saklanır. Paket yalnızca derleme, regresyon
  testleri ve kurulu derleyiciyle C/C++ çalıştırma denemeleri başarılıysa üretilir.
- Workflow izinleri `contents: read`; paketler Artifacts olarak yüklenir.
  Otomatik release, kaynak commit'i veya dağıtım yapılmaz.

## CI'nin kendisini doğrula

`Validate CI`, actionlint ve ShellCheck ile workflow/kabuk kodunu,
Python testleriyle sonuç denetleyicisini kontrol eder. Yerelde:

```bash
actionlint .github/workflows/*.yml
shellcheck .github/ci/build-gcc.sh
bash -n .github/ci/build-gcc.sh
python3 -m unittest discover -s .github/ci -p 'test_*.py' -v
bash .github/ci/build-gcc.sh validate
```

## Sonraki genişletmeler

Önce bu Linux C/C++ hattından başarılı bir temel sonuç al. Sonrasında ihtiyaca
göre temel commit ile regresyon karşılaştırması, ARM64 runner, ek diller,
sanitizer ile derlenen GCC ve seçilmiş sürümler için release paketleri eklenebilir.
Nesne dosyası önbelleği başlangıçta yok; her çalışmada temiz derleme yapılır.

Kaynaklar: [GCC building](https://gcc.gnu.org/install/build.html),
[GCC testing](https://gcc.gnu.org/install/test.html),
[GitHub runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
