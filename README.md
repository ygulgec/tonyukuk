# Tonyukuk Programlama Dili

Tonyukuk, tamamen Turkce sozdizimi ile programlama yapmaya olanak saglayan, derlenen (compiled) bir programlama dilidir. x86_64 Linux uzerinde calisir ve C derleyici altyapisi kullanir.

## Kurulum

### Kaynaktan Derleme

```bash
git clone https://github.com/tonyukuk/derleyici.git
cd derleyici
make
```

Gereksinimler:
- GCC (C11 destekli)
- GNU as (assembler)
- GNU make
- Linux x86_64

### Dogrulama

```bash
make test
```

## Hizli Baslangic

`merhaba.tr` dosyasi olusturun:

```
yazdır("Merhaba Dünya!")
```

Derleyin ve calistirin:

```bash
./tonyukuk-derlemerhaba.tr -o merhaba
./merhaba
```

## Dil Referansi

### Degiskenler ve Tipler

```
tam sayi = 42
ondalık pi = 3.14
metin ad = "Tonyukuk"
mantık aktif = doğru
dizi sayilar = [1, 2, 3, 4, 5]
sözlük bilgi = {"isim": "Ali", "yas": 25}
```

Desteklenen tipler:
- `tam` — tamsayi (64-bit)
- `ondalık` — ondalikli sayi (double)
- `metin` — karakter dizisi (UTF-8)
- `mantık` — boolean (`doğru` / `yanlış`)
- `dizi` — dinamik dizi
- `sözlük` — anahtar-deger esleme

### Sabit ve Genel Degiskenler

```
sabit tam SINIR = 100
genel tam sayac = 0
```

### Islevler

```
işlev topla(a: tam, b: tam) -> tam
    döndür a + b
son

işlev selamla(isim: metin)
    yazdır("Merhaba ${isim}!")
son

# Varsayilan parametre
işlev kuvvet(taban: tam, us: tam = 2) -> tam
    döndür taban * us
son

# Coklu donus
işlev ikili(a: tam, b: tam) -> tam
    döndür a, b
son
tam x, tam y = ikili(10, 20)
```

### Kontrol Akisi

#### Kosul (eger/yoksa)

```
eğer sayi > 0 ise
    yazdır("Pozitif")
yoksa eğer sayi == 0 ise
    yazdır("Sıfır")
yoksa
    yazdır("Negatif")
son
```

#### Dongu

```
döngü i = 1, 10 ise
    yazdır(i)
son
```

#### Iken (while)

```
tam j = 0
iken j < 10 ise
    yazdır(j)
    j += 1
son
```

#### Her...icin (for-each)

```
dizi liste = [10, 20, 30]
her eleman için liste ise
    yazdır(eleman)
son
```

#### Esle/Durum (switch/case)

```
eşle değer ise
    durum 1:
        yazdır("Bir")
    durum 2:
        yazdır("İki")
    varsayılan:
        yazdır("Diğer")
son
```

### Siniflar

```
sınıf Hayvan
    tam yas
    metin isim

    işlev bilgi() -> metin
        döndür bu.isim
    son
son

# Kalitim
sınıf Kedi : Hayvan
    tam can
son

Hayvan h = Hayvan(5, "Kaplan")
yazdır(h.bilgi())
```

### Arayuzler

```
arayüz Yazilabilir
    işlev yaz() -> metin
son

sınıf Belge uygula Yazilabilir
    metin icerik
    işlev yaz() -> metin
        döndür bu.icerik
    son
son
```

### Sayimlar (Enum)

```
sayım Renk
    Kirmizi
    Yesil
    Mavi
son

tam r = Renk.Kirmizi
```

### Hata Yakalama

```
dene
    fırlat 42
    yazdır("Bu satır çalışmaz")
yakala
    yazdır("Hata yakalandı!")
son
```

### Moduller

```
kullan matematik
kullan metin
kullan sistem
kullan dizi
kullan zaman
kullan json
kullan ağ
kullan düzeni
kullan paralel
```

Dosya modulleri:

```
kullan "yardimci.tr"
```

### Operatorler

| Operator | Aciklama |
|----------|----------|
| `+`, `-`, `*`, `/`, `%` | Aritmetik |
| `==`, `!=`, `<`, `>`, `<=`, `>=` | Karsilastirma |
| `ve`, `veya`, `değil` | Mantiksal |
| `=` | Atama |
| `+=`, `-=`, `*=`, `/=`, `%=` | Bilesik atama |
| `\|>` | Pipe (boru) |
| `->` | Donus tipi |
| `??` | Bos birlestirme (null coalescing) |

### String Interpolasyon

```
metin ad = "Dünya"
yazdır("Merhaba ${ad}!")
```

### Yorumlar

```
# Tek satirlik yorum

/* Cok satirlik
   blok yorum */
```

## Standart Kutuphane

### matematik

```
kullan matematik
yazdır(pi())           # 3.14159...
yazdır(mutlak(-42))    # 42
yazdır(kuvvet(2, 10))  # 1024
yazdır(karekök(16))    # 4.0
yazdır(min(3, 7))      # 3
yazdır(maks(3, 7))     # 7
```

### metin

```
kullan metin
yazdır(kırp("  merhaba  "))   # "merhaba"
yazdır(tersle("abc"))          # "cba"
yazdır(bul("merhaba", "ha"))  # 3
yazdır(içerir("merhaba", "er")) # 1 (doğru)
```

### sistem

```
kullan sistem
yazdır(tam_metin(42))       # "42"
yazdır(metin_tam("123"))    # 123
```

### dizi

```
kullan dizi
dizi a = [1, 2, 3]
a = ekle(a, 4)              # [1, 2, 3, 4]
a = çıkar(a)                 # [1, 2, 3]
yazdır(uzunluk(a))           # 3
dizi c = birleştir(a, [4, 5])
```

### zaman

```
kullan zaman
yazdır(şimdi())     # Unix zaman damgasi
uyku(1000)           # 1 saniye bekle
```

### json

```
kullan json
metin j = json_oku("{\"a\":1}")
```

### ag (ag)

```
kullan ağ
# HTTP istekleri
```

### duzeni (regex)

```
kullan düzeni
# Düzenli ifadeler
```

### paralel

```
kullan paralel
# Eşzamanlı programlama
```

## Gelistirici Araclari

### Bicimleyici (Formatter)

```bash
make bicimle
./bicimle dosya.tr          # stdout'a yaz
./bicimle -w dosya.tr       # dosyayi yerinde duzenle
```

### Denetleyici (Linter)

```bash
make denetle
./denetle dosya.tr
```

Denetim kurallari:
- Kullanilmayan degiskenler
- Erisilemez kod (dondur/kir sonrasi)
- Bos bloklar
- Dondur eksikligi
- Degisken golgeleme uyarilari

### Paket Yoneticisi (ton)

```bash
make ton
./ton başlat           # ton.toml olustur
./ton yükle            # bagimliliklari indir
./ton liste            # yuklu paketleri listele
```

### VSCode Eklentisi

`vscode-tonyukuk/` dizinini VSCode eklenti dizinine kopyalayarak sozdizimi renklendirme destegi alabilirsiniz:

```bash
cp -r vscode-tonyukuk ~/.vscode/extensions/tonyukuk-0.1.0
```

### Hata Ayiklama (Debug)

GDB ile hata ayiklamak icin `-g` bayragi ile derleyin:

```bash
./tonyukuk-derle-g program.tr -o program
gdb ./program
```

## Komut Satiri Kullanimi

```
tonyukuk-derle [secenekler] <kaynak.tr>

Secenekler:
  -o <dosya>    Cikti dosya adi
  -s            Assembly dosyasini sakla
  -g            Hata ayiklama bilgisi ekle (DWARF)
  -r, --repl    Etkilesimli REPL modunu baslat
  -h            Yardim mesajini goster

Ornekler:
  tonyukuk-derle merhaba.tr
  tonyukuk-derle -o program merhaba.tr
  tonyukuk-derle -g -o program merhaba.tr
  tonyukuk-derle -r
```

## Testler

Tum testleri calistirmak icin:

```bash
./test_hepsi.sh
```

## Lisans

Tonyukuk acik kaynak bir projedir.
