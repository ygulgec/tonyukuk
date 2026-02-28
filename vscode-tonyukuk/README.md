# Tonyukuk VS Code Eklentisi

Tonyukuk Türkçe programlama dili için Visual Studio Code eklentisi.

## Özellikler

- **Sözdizimi vurgulama**: `.tr` dosyaları için tam sözdizimi renklendirme
- **Otomatik tamamlama**: Anahtar kelimeler ve yerleşik fonksiyonlar için tamamlama önerileri
- **Üzerine gelme bilgisi (Hover)**: Fonksiyon ve anahtar kelime açıklamaları
- **Hata tanılama (Diagnostics)**: Sözdizimi ve anlam hatalarını gerçek zamanlı gösterme
- **Kod parçacıkları (Snippets)**: Sık kullanılan kalıplar için hazır şablonlar

## Kurulum

### Gereksinimler

- `tonyukuk-lsp` sunucusu sisteminizde kurulu olmalıdır
- Derlemek için: `cd derleyici && make tonyukuk-lsp`
- Ardından `tonyukuk-lsp` dosyasını PATH'e ekleyin veya ayarlardan yolunu belirtin

### Eklentiyi Yükleme

```bash
cd vscode-tonyukuk
npm install
npm run compile
```

Geliştirme modunda test etmek için VS Code'da `F5` tuşuna basarak Extension Development Host açın.

`.vsix` paketi oluşturmak için:

```bash
npx vsce package
```

## Ayarlar

| Ayar | Açıklama | Varsayılan |
|------|----------|-----------|
| `tonyukuk.lspPath` | LSP sunucusu yolu | `tonyukuk-lsp` |

## Örnek

```tonyukuk
# Merhaba Dünya
işlev ana() ise
    yazdır("Merhaba, Dünya!")
son
```

## Lisans

MIT
