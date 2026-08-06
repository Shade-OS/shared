# ShadeOS / shared

ShadeOS araçlarının ortak kalbi. Tek başına bir program değildir; `winver`,
`cleanmgr`, `SystemProperties` ve `control` depolarına **submodule** olarak girer.

| Dosya | Ne yapar |
|---|---|
| `darkmode.*` | Uygulama geneli karanlık mod: uxtheme ordinalleri, başlık çubuğu, palet |
| `darkdlg.*` | Denetim boyama katmanı — koyu temanın **tanımlamadığı** parçalar |
| `hostui.*` | Sistem modülünü kendi sürecimizde barındırma + `CPlApplet` sürücüsü |
| `sysres.*` | Sistemin kendi MUI kaynaklarından dize/diyalog/ikon okuma |
| `strutil.h` | Küçük dize yardımcıları |
| `shadeos.props` | Ortak MSBuild ayarları (v145, statik CRT, `/utf-8`, mimari manifesti) |
| `re/` | Ölçüm takımı: `pixdiff.ps1`, ekran yakalama, kaynak dökümü |

## Neden bu katman var

Windows'un koyu tema sınıfları bazı parçaları **hiç tanımlamamış**. Tema
motoruna doğrudan soruldu:

| Parça | Koyu tema | Açık tema |
|---|---|---|
| `TEXT_HYPERLINKTEXT` | `#0066CC` | `#0066CC` |
| `EP_EDITBORDER_NOSCROLL` | `#ABADB3` | `#ABADB3` |

Aynı değerler. Yani `SetWindowTheme` ne verilirse verilsin bu parçalar
düzelmiyor; çizmek bize düşüyor. Hangi parçanın eksik olduğunu **ölçerek**
biliyoruz, varsayarak değil — her düzeltmenin yanında ölçümü yazılıdır.

## Kural

`DarkMode::IsDark()` yanlışsa bu katmanın tamamı devre dışıdır. `/light` ve
`/classic` kipinde araç orijinaliyle birebir çizilir; karanlık mod bir
**eklentidir**, yeniden yazım değil.

## Kullanımı

```
git submodule add https://github.com/Shade-OS/shared.git shared
```

Araç projeleri `shared/shadeos.props` dosyasını içeri alır; include yolu
`$(MSBuildThisFileDirectory)` ile kendini bulur, elle ayar gerekmez.
