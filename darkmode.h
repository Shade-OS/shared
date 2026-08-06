/*++

    darkmode.h

    Windows 10/11 karanlik mod destegi.

    Windows'un karanlik mod anahtarlarinin buyuk kismi bugun hala BELGESIZDIR:
    uxtheme.dll bunlari yalnizca ordinal ile disa aktarir, ad vermez. Bu katman
    o ordinalleri surum kontrollu ve tamamen guvenli sekilde baglar -- ordinal
    yoksa ya da Windows surumu eskiyse sessizce acik moda duser, hicbir zaman
    cokmez.

    Desteklenen tabanlar:
        Windows 10 1809 (17763)  -> karanlik modun ilk surumu, ordinal 135 =
                                    AllowDarkModeForApp(BOOL)
        Windows 10 1903 (18362)  -> ordinal 135 = SetPreferredAppMode(enum)
        Windows 10 19H2 (18985)  -> DWMWA_USE_IMMERSIVE_DARK_MODE 19'dan 20'ye
                                    tasindi
        Windows 11 22H2 (22621)  -> DWMWA_SYSTEMBACKDROP_TYPE (Mica)

    Daha eski her surumde IsSupported() false doner ve uygulama acik temada
    calisir; boylece ayni ikili XP'den 11'e kadar sorunsuz calisabilir.

--*/

#ifndef SHADEOS_DARKMODE_H_
#define SHADEOS_DARKMODE_H_

#include <windows.h>

namespace DarkMode
{
    /*  Bir kez, pencere olusturmadan ONCE cagrilmali. uxtheme ordinallerini
        baglar, uygulama genelinde karanlik modu etkinlestirir ve sistem
        temasini okur. Birden fazla cagrilmasi zararsizdir. */
    void Initialize();

    /*  Bu Windows surumunde karanlik mod API'leri kullanilabilir mi? */
    bool IsSupported();

    /*  Su an karanlik tema mi etkin? (Yuksek kontrast acikken daima false --
        o durumda sistem kendi renklerini dayatir, biz karismayiz.) */
    bool IsDark();

    /*  WM_SETTINGCHANGE geldiginde tema degisimi mi diye bakar. */
    bool IsColorSchemeChange(UINT uMsg, LPARAM lParam);

    /*  Sistem temasini yeniden okur. Tema gercekten degistiyse true doner. */
    bool Refresh();

    /*  Modulun etkin temasini acikca belirler.

        Kullanicinin /dark veya /light ile yaptigi secim, sistem ayarindan
        farkli olabilir. Bu cagri yapilmazsa renk sorgulari sistem ayarini
        izler ve pencerenin bir kismi secilen temayla uyusmaz. */
    void SetDark(bool bDark);

    /*  Pencere ve tum alt denetimlerine karanlik tema uygular:
        AllowDarkModeForWindow + SetWindowTheme("DarkMode_*") + baslik cubugu. */
    void ApplyToWindow(HWND hWnd);
    void ApplyToControlTree(HWND hWndRoot);

    /*  Belirli bir uxtheme alt-temasini acikca atar.

        ApplyToWindow'un sinifa gore sectigi varsayilan her denetim icin dogru
        degil; olculdu (bkz. ..\re\tema\):

            SysListView32     -> "DarkMode_ItemsView"
                                 ("DarkMode_Explorer" metni siyah birakiyor)
            msctls_progress32 -> "DarkMode_DarkTheme"
                                 ("DarkMode_Explorer" beyaz kutu ciziyor)
            BS_GROUPBOX       -> "DarkMode_DarkTheme"
                                 ("DarkMode_Explorer" baslik metnini siyah yapiyor)

        pszTheme NULL ise tema sifirlanir (acik moda doner). */
    void ApplyTheme(HWND hWnd, LPCWSTR pszTheme);

    /*  Baslik cubugunu koyulastirir (DwmSetWindowAttribute). */
    bool SetTitleBarDark(HWND hWnd, bool bDark);

    /*  Windows 11 22H2+ arka plan malzemesi (Mica). Desteklenmiyorsa false. */
    bool SetMicaBackdrop(HWND hWnd, bool bEnable);

    /*  Pencere koseleri (Windows 11+). */
    bool SetRoundedCorners(HWND hWnd);

    /*  Aktif temanin renkleri. */
    COLORREF ColorBackground();
    COLORREF ColorSurface();
    COLORREF ColorText();
    COLORREF ColorTextSecondary();
    COLORREF ColorLink();
    COLORREF ColorBorder();

    /*  Arka plan fircasi. Sahipligi bu modulde kalir, silmeyin. */
    HBRUSH BrushBackground();
    HBRUSH BrushSurface();

    /*  Gercek (shim'lenmemis) yapi numarasi -- RtlGetVersion'dan. */
    DWORD BuildNumber();

    /*  Kaynaklari birakir. Proses sonunda cagrilmasi sart degildir. */
    void Shutdown();
}

#endif /* SHADEOS_DARKMODE_H_ */
