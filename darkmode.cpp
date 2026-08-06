/*++

    darkmode.cpp

    Karanlik mod API katmani. Ayrintili aciklama icin darkmode.h'ye bakin.

    Tasarim kurali: BU MODULDEKI HICBIR SEY ZORUNLU DEGILDIR. Her API dinamik
    baglanir; uxtheme ordinali yoksa, dwmapi yoksa, Windows eskiyse ilgili
    ozellik sessizce devre disi kalir ve uygulama acik temada calismaya devam
    eder. Boylece tek ikili Windows XP'den Windows 11'e kadar calisir.

--*/

#include "darkmode.h"
#include "strutil.h"

#include <dwmapi.h>

namespace {

/*==========================================================================*
 *  Surum esikleri
 *==========================================================================*/

const DWORD kBuild_1809 = 17763;    /* karanlik modun ilk surumu             */
const DWORD kBuild_1903 = 18362;    /* ordinal 135 imzasi degisti            */
const DWORD kBuild_DarkTitleBar = 18985; /* DWMWA 19 -> 20                   */
const DWORD kBuild_Win11 = 22000;   /* yuvarlak koseler                      */
const DWORD kBuild_Win11_22H2 = 22621; /* Mica / SYSTEMBACKDROP_TYPE         */

/* dwmapi'nin eski SDK'larda bulunmayan sabitleri. */
const DWORD kDwmwaUseImmersiveDarkModeOld = 19;
const DWORD kDwmwaUseImmersiveDarkModeNew = 20;
const DWORD kDwmwaWindowCornerPreference  = 33;
const DWORD kDwmwaSystemBackdropType      = 38;

const DWORD kDwmwcpRound        = 2;   /* DWMWCP_ROUND        */
const DWORD kDwmsbtMainWindow   = 2;   /* DWMSBT_MAINWINDOW = Mica */

/*==========================================================================*
 *  Belgesiz uxtheme ordinalleri
 *==========================================================================*
 *
 *  Bu fonksiyonlar uxtheme.dll'de yalnizca ordinal ile disa aktarilir, ad
 *  tasimazlar. Imzalari Windows'un kendi bilesenlerinin kullanimindan
 *  cikarilmistir. Buradaki `bool` C++ bool'udur (1 bayt) -- BOOL degil.
 */

enum PreferredAppMode
{
    AppMode_Default    = 0,
    AppMode_AllowDark  = 1,   /* sistem ayarina uy  <-- dogru davranis */
    AppMode_ForceDark  = 2,
    AppMode_ForceLight = 3
};

typedef void (WINAPI *PFN_RefreshImmersiveColorPolicyState)(void);      /* 104 */
typedef bool (WINAPI *PFN_ShouldAppsUseDarkMode)(void);                 /* 132 */
typedef bool (WINAPI *PFN_AllowDarkModeForWindow)(HWND, bool);          /* 133 */
typedef bool (WINAPI *PFN_AllowDarkModeForApp)(bool);                   /* 135 @1809 */
typedef int  (WINAPI *PFN_SetPreferredAppMode)(int);                    /* 135 @1903+ */
typedef void (WINAPI *PFN_FlushMenuThemes)(void);                       /* 136 */

typedef HRESULT (WINAPI *PFN_SetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

/* ntdll: shim'lenmemis gercek surum. GetVersionEx uyumluluk katmanina takilir. */
typedef struct _SHADEOS_OSVERSIONINFOW
{
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
} SHADEOS_OSVERSIONINFOW;

typedef LONG (WINAPI *PFN_RtlGetVersion)(SHADEOS_OSVERSIONINFOW *);

/*==========================================================================*
 *  Modul durumu
 *==========================================================================*/

struct State
{
    bool     bInitialized;
    bool     bSupported;
    bool     bDark;
    DWORD    dwBuild;

    HMODULE  hUxTheme;
    HMODULE  hDwmApi;

    PFN_RefreshImmersiveColorPolicyState pfnRefreshImmersiveColorPolicyState;
    PFN_ShouldAppsUseDarkMode            pfnShouldAppsUseDarkMode;
    PFN_AllowDarkModeForWindow           pfnAllowDarkModeForWindow;
    PFN_AllowDarkModeForApp              pfnAllowDarkModeForApp;
    PFN_SetPreferredAppMode              pfnSetPreferredAppMode;
    PFN_FlushMenuThemes                  pfnFlushMenuThemes;
    PFN_SetWindowTheme                   pfnSetWindowTheme;
    PFN_DwmSetWindowAttribute            pfnDwmSetWindowAttribute;

    HBRUSH   hbrBackground;
    HBRUSH   hbrSurface;
    COLORREF crBackgroundCached;
    COLORREF crSurfaceCached;
};

State g_state = { 0 };

/*--------------------------------------------------------------------------*/

DWORD QueryRealBuildNumber(void)
{
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (hNtDll != NULL)
    {
        PFN_RtlGetVersion pfn = reinterpret_cast<PFN_RtlGetVersion>(
            reinterpret_cast<void *>(GetProcAddress(hNtDll, "RtlGetVersion")));
        if (pfn != NULL)
        {
            SHADEOS_OSVERSIONINFOW vi;
            ZeroMemory(&vi, sizeof(vi));
            vi.dwOSVersionInfoSize = sizeof(vi);
            if (pfn(&vi) == 0)
            {
                return vi.dwBuildNumber;
            }
        }
    }

    /* RtlGetVersion yoksa (NT4 vb.) karanlik mod zaten soz konusu degil. */
    return 0;
}

bool IsHighContrastActive(void)
{
    HIGHCONTRASTW hc;
    ZeroMemory(&hc, sizeof(hc));
    hc.cbSize = sizeof(hc);

    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0))
    {
        return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
    return false;
}

/*++
    Kullanicinin uygulama temasi tercihini okur.

    Kayit defteri, ShouldAppsUseDarkMode()'dan daha guvenilirdir: ordinal 132
    bazi yapilarda yuksek kontrast durumunda beklenmedik sonuc verir. Kayit
    defteri okunamazsa ordinale duseriz.
--*/
bool QuerySystemDarkMode(void)
{
    if (IsHighContrastActive())
    {
        /* Yuksek kontrastta sistem kendi paletini dayatir; karismayiz. */
        return false;
    }

    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD dwValue = 1;
        DWORD dwType  = 0;
        DWORD cbData  = sizeof(dwValue);

        LSTATUS lStatus = RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, &dwType,
                                           reinterpret_cast<LPBYTE>(&dwValue), &cbData);
        RegCloseKey(hKey);

        if (lStatus == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            return (dwValue == 0);
        }
    }

    if (g_state.pfnShouldAppsUseDarkMode != NULL)
    {
        return g_state.pfnShouldAppsUseDarkMode();
    }

    return false;
}

void ReleaseBrushes(void)
{
    if (g_state.hbrBackground != NULL) { DeleteObject(g_state.hbrBackground); g_state.hbrBackground = NULL; }
    if (g_state.hbrSurface    != NULL) { DeleteObject(g_state.hbrSurface);    g_state.hbrSurface    = NULL; }
}

} /* anonim ad alani */


/*==========================================================================*
 *  Ortak arayuz
 *==========================================================================*/

namespace DarkMode {

void Initialize(void)
{
    if (g_state.bInitialized)
    {
        return;
    }
    g_state.bInitialized = true;

    g_state.dwBuild = QueryRealBuildNumber();

    /* 1809 oncesinde karanlik mod API'leri hic yok. */
    if (g_state.dwBuild < kBuild_1809)
    {
        g_state.bSupported = false;
        g_state.bDark      = false;
        return;
    }

    g_state.hUxTheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    g_state.hDwmApi  = LoadLibraryExW(L"dwmapi.dll",  NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (g_state.hUxTheme == NULL)
    {
        g_state.bSupported = false;
        return;
    }

    /* Ordinal ile bagla. MAKEINTRESOURCEA, ordinali isim yerine gecirir. */
    g_state.pfnRefreshImmersiveColorPolicyState =
        reinterpret_cast<PFN_RefreshImmersiveColorPolicyState>(
            reinterpret_cast<void *>(GetProcAddress(g_state.hUxTheme, MAKEINTRESOURCEA(104))));

    g_state.pfnShouldAppsUseDarkMode =
        reinterpret_cast<PFN_ShouldAppsUseDarkMode>(
            reinterpret_cast<void *>(GetProcAddress(g_state.hUxTheme, MAKEINTRESOURCEA(132))));

    g_state.pfnAllowDarkModeForWindow =
        reinterpret_cast<PFN_AllowDarkModeForWindow>(
            reinterpret_cast<void *>(GetProcAddress(g_state.hUxTheme, MAKEINTRESOURCEA(133))));

    g_state.pfnFlushMenuThemes =
        reinterpret_cast<PFN_FlushMenuThemes>(
            reinterpret_cast<void *>(GetProcAddress(g_state.hUxTheme, MAKEINTRESOURCEA(136))));

    /*  Ordinal 135'in imzasi 1903'te degisti:
            1809      : bool AllowDarkModeForApp(bool)
            1903+     : PreferredAppMode SetPreferredAppMode(PreferredAppMode)
        Yanlis olani cagirmak yigin bozulmasina yol acabilir, bu yuzden
        yapi numarasina gore ayiriyoruz. */
    FARPROC pfn135 = GetProcAddress(g_state.hUxTheme, MAKEINTRESOURCEA(135));
    if (pfn135 != NULL)
    {
        if (g_state.dwBuild >= kBuild_1903)
        {
            g_state.pfnSetPreferredAppMode =
                reinterpret_cast<PFN_SetPreferredAppMode>(reinterpret_cast<void *>(pfn135));
        }
        else
        {
            g_state.pfnAllowDarkModeForApp =
                reinterpret_cast<PFN_AllowDarkModeForApp>(reinterpret_cast<void *>(pfn135));
        }
    }

    /* SetWindowTheme adiyla disa aktarilir, ordinale gerek yok. */
    g_state.pfnSetWindowTheme =
        reinterpret_cast<PFN_SetWindowTheme>(
            reinterpret_cast<void *>(GetProcAddress(g_state.hUxTheme, "SetWindowTheme")));

    if (g_state.hDwmApi != NULL)
    {
        g_state.pfnDwmSetWindowAttribute =
            reinterpret_cast<PFN_DwmSetWindowAttribute>(
                reinterpret_cast<void *>(GetProcAddress(g_state.hDwmApi, "DwmSetWindowAttribute")));
    }

    g_state.bSupported = (g_state.pfnAllowDarkModeForWindow != NULL) &&
                         (g_state.pfnSetPreferredAppMode != NULL ||
                          g_state.pfnAllowDarkModeForApp != NULL);

    if (!g_state.bSupported)
    {
        return;
    }

    /*  AllowDark: "sistem ayarina uy". ForceDark KULLANMIYORUZ -- kullanici
        acik tema secmisse uygulamanin da acik olmasi dogrusudur. */
    if (g_state.pfnSetPreferredAppMode != NULL)
    {
        g_state.pfnSetPreferredAppMode(AppMode_AllowDark);
    }
    else if (g_state.pfnAllowDarkModeForApp != NULL)
    {
        g_state.pfnAllowDarkModeForApp(true);
    }

    if (g_state.pfnRefreshImmersiveColorPolicyState != NULL)
    {
        g_state.pfnRefreshImmersiveColorPolicyState();
    }

    g_state.bDark = QuerySystemDarkMode();
}

bool IsSupported(void)
{
    return g_state.bSupported;
}

bool IsDark(void)
{
    return g_state.bSupported && g_state.bDark;
}

DWORD BuildNumber(void)
{
    return g_state.dwBuild;
}

bool IsColorSchemeChange(UINT uMsg, LPARAM lParam)
{
    if (uMsg != WM_SETTINGCHANGE || lParam == 0)
    {
        return false;
    }

    /*  Tema degisiminde sistem WM_SETTINGCHANGE'i lParam = L"ImmersiveColorSet"
        ile yayinlar. */
    LPCWSTR pszArea = reinterpret_cast<LPCWSTR>(lParam);
    return ShadeEqualsI(pszArea, L"ImmersiveColorSet");
}

bool Refresh(void)
{
    if (!g_state.bSupported)
    {
        return false;
    }

    if (g_state.pfnRefreshImmersiveColorPolicyState != NULL)
    {
        g_state.pfnRefreshImmersiveColorPolicyState();
    }

    bool bNewDark = QuerySystemDarkMode();
    if (bNewDark == g_state.bDark)
    {
        return false;
    }

    g_state.bDark = bNewDark;
    ReleaseBrushes();   /* renkler degisti, fircalari yeniden uret */

    if (g_state.pfnFlushMenuThemes != NULL)
    {
        g_state.pfnFlushMenuThemes();
    }
    return true;
}

void SetDark(bool bDark)
{
    /*  Desteklenmiyorsa karanlik olamaz; sessizce acik kalir. */
    bool bEffective = bDark && g_state.bSupported;

    /*  UYGULAMA GENELINDEKI kipi de zorluyoruz.

        Yalnizca g_state.bDark'i degistirmek YETMEZ: Initialize,
        SetPreferredAppMode(AllowDark) cagirmistir ve o "sistem ayarina uy"
        demektir. Sistem KARANLIK iken /light verildiginde denetimler yine
        koyu cizilirdi -- olculdu (bkz. ..\re\tema\klon2-light-1.png:
        pencere /light'a ragmen koyu cikmisti).

        ForceDark / ForceLight ise tercihi acikca dayatir. Bu cagri yalnizca
        kullanici /dark veya /light verdiginde yapilir; anahtarsiz calismada
        Initialize'in AllowDark'i gecerli kalir ve pencere sistem temasini
        canli izlemeye devam eder. */
    if (g_state.bSupported)
    {
        if (g_state.pfnSetPreferredAppMode != NULL)
        {
            g_state.pfnSetPreferredAppMode(bEffective ? AppMode_ForceDark
                                                      : AppMode_ForceLight);
        }
        else if (g_state.pfnAllowDarkModeForApp != NULL)
        {
            g_state.pfnAllowDarkModeForApp(bEffective);
        }

        if (g_state.pfnRefreshImmersiveColorPolicyState != NULL)
        {
            g_state.pfnRefreshImmersiveColorPolicyState();
        }
        if (g_state.pfnFlushMenuThemes != NULL)
        {
            g_state.pfnFlushMenuThemes();
        }
    }

    if (bEffective == g_state.bDark)
    {
        return;
    }

    g_state.bDark = bEffective;
    ReleaseBrushes();   /* renkler degisti, fircalari yeniden uret */
}

bool SetTitleBarDark(HWND hWnd, bool bDark)
{
    if (g_state.pfnDwmSetWindowAttribute == NULL || g_state.dwBuild < kBuild_1809)
    {
        return false;
    }

    /*  Oznitelik numarasi 18985'te 19'dan 20'ye tasindi. Once dogru olani
        deneriz; hata donerse digerini deneriz (Insider yapilarinda esikler
        tam oturmayabiliyor). */
    BOOL bValue = bDark ? TRUE : FALSE;

    DWORD dwFirst  = (g_state.dwBuild >= kBuild_DarkTitleBar)
                         ? kDwmwaUseImmersiveDarkModeNew : kDwmwaUseImmersiveDarkModeOld;
    DWORD dwSecond = (dwFirst == kDwmwaUseImmersiveDarkModeNew)
                         ? kDwmwaUseImmersiveDarkModeOld : kDwmwaUseImmersiveDarkModeNew;

    if (SUCCEEDED(g_state.pfnDwmSetWindowAttribute(hWnd, dwFirst, &bValue, sizeof(bValue))))
    {
        return true;
    }
    return SUCCEEDED(g_state.pfnDwmSetWindowAttribute(hWnd, dwSecond, &bValue, sizeof(bValue)));
}

bool SetMicaBackdrop(HWND hWnd, bool bEnable)
{
    if (g_state.pfnDwmSetWindowAttribute == NULL || g_state.dwBuild < kBuild_Win11_22H2)
    {
        return false;
    }

    DWORD dwType = bEnable ? kDwmsbtMainWindow : 1 /* DWMSBT_NONE */;
    if (FAILED(g_state.pfnDwmSetWindowAttribute(hWnd, kDwmwaSystemBackdropType,
                                                &dwType, sizeof(dwType))))
    {
        return false;
    }

    /*  Malzemenin ISTEMCI alaninda da gorunmesi icin cerceveyi tum pencereye
        genislet. Bu olmadan Mica yalnizca baslik cubugunda kalir. */
    if (bEnable && g_state.hDwmApi != NULL)
    {
        typedef HRESULT (WINAPI *PFN_DwmExtendFrameIntoClientArea)(HWND, const MARGINS *);

        PFN_DwmExtendFrameIntoClientArea pfnExtend =
            reinterpret_cast<PFN_DwmExtendFrameIntoClientArea>(
                reinterpret_cast<void *>(
                    GetProcAddress(g_state.hDwmApi, "DwmExtendFrameIntoClientArea")));

        if (pfnExtend != NULL)
        {
            MARGINS mg = { -1, -1, -1, -1 };
            pfnExtend(hWnd, &mg);
        }
    }

    return true;
}

bool SetRoundedCorners(HWND hWnd)
{
    if (g_state.pfnDwmSetWindowAttribute == NULL || g_state.dwBuild < kBuild_Win11)
    {
        return false;
    }

    DWORD dwPref = kDwmwcpRound;
    return SUCCEEDED(g_state.pfnDwmSetWindowAttribute(hWnd, kDwmwaWindowCornerPreference,
                                                      &dwPref, sizeof(dwPref)));
}

/*++
    Tek bir denetime karanlik tema uygular.

    SetWindowTheme'e verilen ad, denetimin hangi tema sinifini kullanacagini
    belirler. Windows'un karanlik varyantlari:
        "DarkMode_Explorer" -> dugme, liste, agac, kaydirma cubugu
        "DarkMode_CFD"      -> metin kutusu, acilir kutu (Common File Dialog)
        "DarkMode_ItemsView"-> liste/detay gorunumu
--*/
void ApplyToWindow(HWND hWnd)
{
    if (!g_state.bSupported || hWnd == NULL)
    {
        return;
    }

    if (g_state.pfnAllowDarkModeForWindow != NULL)
    {
        g_state.pfnAllowDarkModeForWindow(hWnd, g_state.bDark);
    }

    if (g_state.pfnSetWindowTheme == NULL)
    {
        return;
    }

    WCHAR szClass[64];
    if (GetClassNameW(hWnd, szClass, ARRAYSIZE(szClass)) == 0)
    {
        szClass[0] = L'\0';
    }

    LPCWSTR pszTheme = NULL;

    if (g_state.bDark)
    {
        if (ShadeEqualsI(szClass, L"Edit") || ShadeEqualsI(szClass, L"ComboBox"))
        {
            pszTheme = L"DarkMode_CFD";
        }
        else
        {
            pszTheme = L"DarkMode_Explorer";
        }
    }

    /*  pszTheme == NULL ise tema sifirlanir, yani acik moda doner. */
    g_state.pfnSetWindowTheme(hWnd, pszTheme, NULL);
}

void ApplyTheme(HWND hWnd, LPCWSTR pszTheme)
{
    if (!g_state.bSupported || hWnd == NULL || g_state.pfnSetWindowTheme == NULL)
    {
        return;
    }

    if (g_state.pfnAllowDarkModeForWindow != NULL)
    {
        g_state.pfnAllowDarkModeForWindow(hWnd, g_state.bDark);
    }

    /*  Acik temada tema adini sifirliyoruz; aksi halde denetim koyu kalirdi. */
    g_state.pfnSetWindowTheme(hWnd, g_state.bDark ? pszTheme : NULL, NULL);
}

namespace {

BOOL CALLBACK ApplyChildProc(HWND hWndChild, LPARAM /*lParam*/)
{
    DarkMode::ApplyToWindow(hWndChild);
    return TRUE;
}

} /* anonim */

void ApplyToControlTree(HWND hWndRoot)
{
    if (!g_state.bSupported || hWndRoot == NULL)
    {
        return;
    }

    ApplyToWindow(hWndRoot);
    SetTitleBarDark(hWndRoot, g_state.bDark);
    EnumChildWindows(hWndRoot, ApplyChildProc, 0);
}

/*==========================================================================*
 *  Palet
 *==========================================================================*
 *
 *  Karanlik degerler Windows 11'in kendi yuzey paletiyle hizalidir. Acik
 *  degerler sistem renklerinden okunur, boylece ozel temalarda da dogru olur.
 */

COLORREF ColorBackground(void)
{
    return IsDark() ? RGB(32, 32, 32) : GetSysColor(COLOR_3DFACE);
}

COLORREF ColorSurface(void)
{
    return IsDark() ? RGB(43, 43, 43) : GetSysColor(COLOR_WINDOW);
}

COLORREF ColorText(void)
{
    return IsDark() ? RGB(243, 243, 243) : GetSysColor(COLOR_WINDOWTEXT);
}

COLORREF ColorTextSecondary(void)
{
    return IsDark() ? RGB(170, 170, 170) : GetSysColor(COLOR_GRAYTEXT);
}

COLORREF ColorLink(void)
{
    /* #4CC2FF: Windows 11'in karanlik temadaki baglanti rengi. */
    return IsDark() ? RGB(76, 194, 255) : RGB(0, 102, 204);
}

COLORREF ColorBorder(void)
{
    return IsDark() ? RGB(56, 56, 56) : RGB(217, 217, 217);
}

HBRUSH BrushBackground(void)
{
    COLORREF cr = ColorBackground();
    if (g_state.hbrBackground == NULL || g_state.crBackgroundCached != cr)
    {
        if (g_state.hbrBackground != NULL)
        {
            DeleteObject(g_state.hbrBackground);
        }
        g_state.hbrBackground = CreateSolidBrush(cr);
        g_state.crBackgroundCached = cr;
    }
    return g_state.hbrBackground;
}

HBRUSH BrushSurface(void)
{
    COLORREF cr = ColorSurface();
    if (g_state.hbrSurface == NULL || g_state.crSurfaceCached != cr)
    {
        if (g_state.hbrSurface != NULL)
        {
            DeleteObject(g_state.hbrSurface);
        }
        g_state.hbrSurface = CreateSolidBrush(cr);
        g_state.crSurfaceCached = cr;
    }
    return g_state.hbrSurface;
}

void Shutdown(void)
{
    ReleaseBrushes();

    if (g_state.hUxTheme != NULL) { FreeLibrary(g_state.hUxTheme); g_state.hUxTheme = NULL; }
    if (g_state.hDwmApi  != NULL) { FreeLibrary(g_state.hDwmApi);  g_state.hDwmApi  = NULL; }

    g_state.bInitialized = false;
    g_state.bSupported   = false;
}

} /* namespace DarkMode */
