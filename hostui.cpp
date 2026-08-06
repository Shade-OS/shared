/*++

    hostui.cpp

    Aciklama icin hostui.h'ye bakin.

--*/

#include "hostui.h"
#include "darkmode.h"
#include "darkdlg.h"
#include "strutil.h"

#include <commctrl.h>
#include <cpl.h>
#include <strsafe.h>

namespace {

HHOOK g_hHook = NULL;

/*  PropertySheet cercevesinin sekme denetiminin sabit kimligi (prsht.h). */
const int kIdTabControl = 0x3020;


const UINT_PTR kSubclassHosted = 0x5D05;   /* "ShadeOS" */


/*++
    Barindirilan diyalogun alt sinifi.

    BU KATMAN OLMADAN TEMA YARIM KALIR ve sebebi sudur: kendi araclarimizda
    diyalog yordamini BIZ yazariz, dolayisiyla WM_CTLCOLOR* ailesini
    dogrudan karsilariz. Barindirilan bir arayuzde ise yordam BASKA MODULUN
    icindedir (sysdm.cpl) ve o, varsayilan ACIK fircalari dondurur.

    Olculdu (..\..\sysdm\re\tema): yalnizca DarkDlg::Apply cagrildiginda
    baslik cubugu ve sekme seridi koyu geliyor ama sayfa govdesi #F9F9F9,
    metin kutusu #FFFFFF kaliyordu -- cunku o iki renk denetimlerin degil,
    DIYALOGUN kendi yanitindan geliyor.

    Alt sinif, iletiyi asil yordama VARMADAN karsilar; boylece zemin bizim
    olur. Ayni cozum winver'da ShellAboutW icin de kullaniliyor.
--*/
struct SubtractCtx
{
    HWND hDlg;
    HRGN hrgn;
};


/*  Bolgeden bir alt pencerenin dikdortgenini cikarir. */
BOOL CALLBACK SubtractChildProc(HWND hWndChild, LPARAM lParam)
{
    SubtractCtx *pCtx = reinterpret_cast<SubtractCtx *>(lParam);

    /*  EnumChildWindows TORUNLARI da geziyor. Onlari atliyoruz: bir torunun
        dikdortgeni zaten ebeveyninin icindedir, ayrica koordinat esleme
        yanlis pencereye gore yapilirdi. */
    if (GetParent(hWndChild) != pCtx->hDlg || !IsWindowVisible(hWndChild))
    {
        return TRUE;
    }

    RECT rc;
    if (!GetWindowRect(hWndChild, &rc))
    {
        return TRUE;
    }

    MapWindowPoints(NULL, pCtx->hDlg, reinterpret_cast<POINT *>(&rc), 2);

    HRGN hrgnChild = CreateRectRgnIndirect(&rc);
    if (hrgnChild != NULL)
    {
        CombineRgn(pCtx->hrgn, pCtx->hrgn, hrgnChild, RGN_DIFF);
        DeleteObject(hrgnChild);
    }

    return TRUE;
}


LRESULT CALLBACK HostedDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                               UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        {
            INT_PTR hbr = DarkDlg::OnCtlColor(uMsg, wParam, lParam);
            if (hbr != 0)
            {
                return static_cast<LRESULT>(hbr);
            }
        }
        break;

    case WM_ERASEBKGND:
        {
            /*  Ozellik sayfalari zemini genelde tema dokusuyla boyar
                (EnableThemeDialogTexture); onu devralmazsak acik doku
                govdenin uzerine geri gelir. */
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(reinterpret_cast<HDC>(wParam), &rc, DarkMode::BrushBackground());
            return 1;
        }

    case WM_PAINT:
        {
            /*  WM_ERASEBKGND'yi devralmak HER ZAMAN YETMIYOR.

                Bazi kabuk diyaloglari zemini iki tonlu cizer (ust "icerik"
                alani COLOR_WINDOW, alt dugme seridi COLOR_3DFACE) ve bunu
                SILME adiminda degil KENDI WM_PAINT'lerinde yapar. O zaman
                bizim koyu silmemizin uzerine acik renk geri geliyor.

                Olculdu (..\..\sysdm\re\tema\alt-olustur.png -- "Geri yukleme
                noktasi olustur"): metin kutusuyla dugme seridi arasindaki
                bant x=2..609 boyunca duz #FFFFFF. Diyalogda beyaz cizen bir
                DENETIM yok (alt pencereler: Static 1210, SysLink 1302,
                Edit 1303, SS_ETCHEDHORZ 1306, iki Button) -- beyaz, zeminin
                kendisi. Baslik ve metin kutusu alanlarinin koyu gorunmesinin
                sebebi, oralari denetimlerin ORTMESIYDI.

                Cozum: modul zemini cizsin, sonra denetimlerin ortmedigi
                alani biz yeniden boyayalim. Denetimlerin altini boyamiyoruz,
                bu yuzden titreme olmuyor ve modulun kendi cizimlerine de
                dokunmus olmuyoruz. */
            LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            HDC hdc = GetDC(hWnd);
            if (hdc != NULL)
            {
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);

                HRGN hrgn = CreateRectRgnIndirect(&rcClient);
                if (hrgn != NULL)
                {
                    SubtractCtx ctx;
                    ctx.hDlg = hWnd;
                    ctx.hrgn = hrgn;

                    EnumChildWindows(hWnd, SubtractChildProc,
                                     reinterpret_cast<LPARAM>(&ctx));
                    FillRgn(hdc, hrgn, DarkMode::BrushBackground());
                    DeleteObject(hrgn);
                }

                ReleaseDC(hWnd, hdc);
            }

            return lr;
        }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, HostedDlgProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*++
    Yeni olusan bir iletisim kutusuna temayi uygular.

    CERCEVE mi SAYFA mi? Ikisi de "#32770" sinifindadir, ama yalnizca
    CERCEVEDE 0x3020 kimlikli sekme denetimi bulunur. Ayrimi yapmak sart:
    cerceve WM_ERASEBKGND'yi de devralmalidir, yoksa sekme seridinin
    cevresinde acik gri bir bant kalir (cleanmgr'da olculdu).
--*/
/*++
    Diyalog bir DirectUI kabugunun icinde mi?

    Bazi sihirbazlar Win32 diyalogu DEGILDIR. Olculdu (netplwiz'in "Etki Alani
    veya Calisma Grubuna Katil" sihirbazi):

        NativeHWNDHost                 <- ust duzey pencere
          DirectUIHWND
            CtrlNotifySink / Button    '&Sonraki'
            CtrlNotifySink / Button    'Iptal'
            CtrlNotifySink / #32770    <- yalnizca BURASI gercek diyalog

    Baslik, icerik alani ve komut seridi ayri pencereler degil; hepsini tek
    bir DirectUIHWND'nin icine DUI70 kendi motoruyla ciziyor. Renkleri de
    modulun kendi UIFILE stil kaynaklarindan aliyor. Dolayisiyla ne
    WM_CTLCOLOR*, ne WM_ERASEBKGND/WM_PAINT devralma, ne NM_CUSTOMDRAW, ne de
    bir tema sinifi ise yariyor -- AEROWIZARD'in da karanlik varyanti yok
    (olculdu: AEROWIZARD, DarkMode_Explorer::AeroWizard, DarkMode::AeroWizard
    -- hepsi baslik ve icerik alani icin #FFFFFF).

    Ic sayfayi TEK BASINA koyulastirmak sonucu iyilestirmiyor, KOTULESTIRIYOR:
    bembeyaz bir sihirbazin ortasinda koyu bir kutu kaliyor. Bu modulun kurali
    acik: karanlik mod bir EKLENTIDIR, yarim birakilmis bir yeniden yazim
    degil. Boyayamadigimiz yere hic dokunmuyoruz; sihirbaz orijinaliyle
    birebir cizilir.
--*/
bool IsInsideDirectUI(HWND hDlg)
{
    HWND h = hDlg;

    /*  Derinlik sinirli: GetParent ust duzey pencerelerde SAHIBI dondurur,
        yani zincir diyalogun disina cikabilir. Sinir, dongu ihtimalini de
        kapatiyor. */
    for (int i = 0; i < 16 && h != NULL; ++i)
    {
        WCHAR szClass[32];

        if (GetClassNameW(h, szClass, ARRAYSIZE(szClass)) != 0 &&
            (ShadeEqualsI(szClass, L"NativeHWNDHost") ||
             ShadeEqualsI(szClass, L"DirectUIHWND")))
        {
            return true;
        }

        h = GetParent(h);
    }

    return false;
}


void ThemeDialog(HWND hDlg)
{
    if (hDlg == NULL || !DarkMode::IsDark())
    {
        return;
    }

    if (IsInsideDirectUI(hDlg))
    {
        return;
    }

    /*  ONCE alt sinif: zemin ve denetim renkleri artik bize gelsin. */
    SetWindowSubclass(hDlg, HostedDlgProc, kSubclassHosted, 0);

    HWND hTab = GetDlgItem(hDlg, kIdTabControl);

    if (hTab != NULL)
    {
        DarkDlg::ApplyToSheet(hDlg);
        DarkDlg::SetupTabControl(hTab);
        DarkMode::SetRoundedCorners(hDlg);
    }
    else
    {
        DarkDlg::Apply(hDlg);
    }

    RedrawWindow(hDlg, NULL, NULL,
                 RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
}


/*++
    Kendi is parcacigimizdaki pencere olusumlarini izler.

    WH_CALLWNDPROCRET, ileti pencere yordami tarafindan ISLENDIKTEN SONRA
    cagrilir. WM_INITDIALOG icin bu onemlidir: o noktada tum alt denetimler
    olusmus ve yerlesmistir, dolayisiyla hepsine tema uygulanabilir.
--*/
LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && lParam != 0)
    {
        const CWPRETSTRUCT *pRet = reinterpret_cast<const CWPRETSTRUCT *>(lParam);

        if (pRet->message == WM_INITDIALOG)
        {
            WCHAR szClass[16];

            /*  "#32770" tum iletisim kutularinin sinif adidir. */
            if (GetClassNameW(pRet->hwnd, szClass, ARRAYSIZE(szClass)) != 0 &&
                ShadeEqualsI(szClass, L"#32770"))
            {
                ThemeDialog(pRet->hwnd);
            }
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

/*++
    Sistemdeki bir modulu CALISTIRMAK icin yukler.

    Yol System32'den kurulur; sabit "C:\Windows\System32" YAZILMAZ. Verilen ad
    zaten mutlak bir yol ise oldugu gibi kullanilir -- boylece kullanici kendi
    .cpl dosyasini da acabilir.

    LOAD_WITH_ALTERED_SEARCH_PATH: modulun kendi bagimliliklarini kendi
    dizininden bulmasi icin. Denetim Masasi ogeleri yan modullere dayaniyor
    (olculdu: sysdm.cpl'nin "Uzak" sayfasi remotepg.dll'den geliyor).
--*/
HMODULE LoadSystemModule(LPCWSTR pszModule)
{
    if (pszModule == NULL || pszModule[0] == L'\0')
    {
        return NULL;
    }

    WCHAR szPath[MAX_PATH];

    /*  Mutlak yol mu? ("C:\..." ya da "\\sunucu\...") */
    bool bAbsolute = (pszModule[1] == L':') ||
                     (pszModule[0] == L'\\' && pszModule[1] == L'\\');

    if (bAbsolute)
    {
        if (FAILED(StringCchCopyW(szPath, ARRAYSIZE(szPath), pszModule)))
        {
            return NULL;
        }
    }
    else
    {
        UINT cch = GetSystemDirectoryW(szPath, ARRAYSIZE(szPath));
        if (cch == 0 || cch >= ARRAYSIZE(szPath))
        {
            return NULL;
        }
        if (FAILED(StringCchCatW(szPath, ARRAYSIZE(szPath), L"\\")) ||
            FAILED(StringCchCatW(szPath, ARRAYSIZE(szPath), pszModule)))
        {
            return NULL;
        }
    }

    return LoadLibraryExW(szPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}


/*  CPlApplet sozlesmesi (cpl.h). */
typedef LONG (CALLBACK *PFN_CPlApplet)(HWND, UINT, LPARAM, LPARAM);


PFN_CPlApplet GetCPlApplet(HMODULE hModule)
{
    return reinterpret_cast<PFN_CPlApplet>(
        reinterpret_cast<void *>(GetProcAddress(hModule, "CPlApplet")));
}

} /* anonim ad alani */


namespace HostUI {

bool Install(void)
{
    if (!DarkMode::IsDark())
    {
        /*  Acik/klasik temada hicbir sey yapmiyoruz; barindirilan arayuz
            orijinaliyle birebir cizilir. */
        return true;
    }

    if (g_hHook != NULL)
    {
        return true;
    }

    /*  Yalnizca KENDI is parcacigimiz. Baska prosese enjeksiyon yok. */
    g_hHook = SetWindowsHookExW(WH_CALLWNDPROCRET, CallWndRetProc,
                                NULL, GetCurrentThreadId());
    return g_hHook != NULL;
}


void Remove(void)
{
    if (g_hHook != NULL)
    {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
}


bool RunModuleEntry(LPCWSTR pszModule, LPCSTR pszEntry, LPCWSTR pszArg)
{
    if (pszModule == NULL || pszEntry == NULL)
    {
        return false;
    }

    HMODULE hModule = LoadSystemModule(pszModule);
    if (hModule == NULL)
    {
        return false;
    }

    /*  Tek parametreli sozlesme -- gerekcesi ve olcumu hostui.h'de. */
    typedef void (WINAPI *PFN_ArgW)(LPCWSTR);

    PFN_ArgW pfn = reinterpret_cast<PFN_ArgW>(
        reinterpret_cast<void *>(GetProcAddress(hModule, pszEntry)));

    if (pfn == NULL)
    {
        FreeLibrary(hModule);
        return false;
    }

    /*  Bos dize ile NULL ayni degildir: sysdm.cpl once NULL denetimi yapar,
        sonra bos dizeyi de ayni kabul eder -- ama baska moduller icin bunu
        varsaymiyoruz, "sayfa verilmedi"yi NULL olarak geciriyoruz. */
    pfn((pszArg != NULL && pszArg[0] != L'\0') ? pszArg : NULL);

    /*  Modulu birakmiyoruz: barindirdigimiz arayuz kapandiktan sonra bile
        gecikmeli temizlik yapabilir. Proses zaten hemen sonra sonlaniyor. */
    return true;
}


/*++
    Denetim Masasi ogesini surer.

    SIRA ONEMLIDIR ve belgelenmistir: CPL_INIT ile baslanir, CPL_GETCOUNT ile
    oge sayisi ogrenilir, CPL_INQUIRE ile oge tanitilir, CPL_DBLCLK arayuzu
    ACAR (modaldir, pencere kapanana kadar donmez), sonra CPL_STOP ve
    CPL_EXIT ile duzgunce kapatilir.

    CPL_INQUIRE'i ATLAMIYORUZ: bazi ogeler kendilerini yalnizca o adimda
    hazirlar ve dogrudan CPL_DBLCLK gonderilirse eksik durumla acilir. Ucuz
    bir cagri, atlamanin getirisi yok.
--*/
bool RunControlPanelApplet(LPCWSTR pszModule, int iApplet, LPCWSTR pszParams)
{
    if (iApplet < 0)
    {
        iApplet = 0;
    }

    HMODULE hModule = LoadSystemModule(pszModule);
    if (hModule == NULL)
    {
        return false;
    }

    PFN_CPlApplet pfn = GetCPlApplet(hModule);
    if (pfn == NULL)
    {
        FreeLibrary(hModule);
        return false;
    }

    if (pfn(NULL, CPL_INIT, 0, 0) == 0)
    {
        /*  Oge kendini baslatamadi; CPL_EXIT yine de gonderilir. */
        pfn(NULL, CPL_EXIT, 0, 0);
        return false;
    }

    LONG cApplets = pfn(NULL, CPL_GETCOUNT, 0, 0);
    if (cApplets <= 0 || iApplet >= cApplets)
    {
        pfn(NULL, CPL_EXIT, 0, 0);
        return false;
    }

    CPLINFO info;
    ZeroMemory(&info, sizeof(info));
    pfn(NULL, CPL_INQUIRE, iApplet, reinterpret_cast<LPARAM>(&info));

    bool bShown = false;

    /*  Ek parametre verildiyse once onu deneriz: CPL_STARTWPARMSW islenirse
        sifirdan farkli doner. Islenmezse normal acilisa duseriz -- her oge
        bu iletiyi desteklemiyor. */
    if (pszParams != NULL && pszParams[0] != L'\0')
    {
        if (pfn(NULL, CPL_STARTWPARMSW, iApplet,
                reinterpret_cast<LPARAM>(pszParams)) != 0)
        {
            bShown = true;
        }
    }

    if (!bShown)
    {
        pfn(NULL, CPL_DBLCLK, iApplet, info.lData);
    }

    pfn(NULL, CPL_STOP, iApplet, info.lData);
    pfn(NULL, CPL_EXIT, 0, 0);

    /*  Modul birakilmiyor; gerekcesi RunModuleEntry'dekiyle ayni. */
    return true;
}


bool ListControlPanelApplets(LPCWSTR pszModule, PFN_AppletYaz pfnYaz, void *pvUser)
{
    if (pfnYaz == NULL)
    {
        return false;
    }

    HMODULE hModule = LoadSystemModule(pszModule);
    if (hModule == NULL)
    {
        return false;
    }

    PFN_CPlApplet pfn = GetCPlApplet(hModule);
    if (pfn == NULL)
    {
        FreeLibrary(hModule);
        return false;
    }

    if (pfn(NULL, CPL_INIT, 0, 0) == 0)
    {
        pfn(NULL, CPL_EXIT, 0, 0);
        return false;
    }

    LONG cApplets = pfn(NULL, CPL_GETCOUNT, 0, 0);

    for (LONG i = 0; i < cApplets; ++i)
    {
        WCHAR szAd[256]  = L"";
        WCHAR szAcik[256] = L"";

        /*  IKI YOL VAR ve ikisi de gerekli.

            CPL_NEWINQUIRE dizeleri DOGRUDAN verir; CPL_INQUIRE ise yalnizca
            KAYNAK KIMLIKLERI verir ve dizeleri modulden bizim okumamiz
            gerekir. Ogeler hangisini destekledigini soylemez, bu yuzden once
            NEWINQUIRE denenir, bos donerse INQUIRE'a duseriz. */
        NEWCPLINFO ni;
        ZeroMemory(&ni, sizeof(ni));
        ni.dwSize = sizeof(ni);

        pfn(NULL, CPL_NEWINQUIRE, i, reinterpret_cast<LPARAM>(&ni));

        if (ni.szName[0] != L'\0')
        {
            StringCchCopyW(szAd,   ARRAYSIZE(szAd),   ni.szName);
            StringCchCopyW(szAcik, ARRAYSIZE(szAcik), ni.szInfo);
        }
        else
        {
            CPLINFO ci;
            ZeroMemory(&ci, sizeof(ci));
            pfn(NULL, CPL_INQUIRE, i, reinterpret_cast<LPARAM>(&ci));

            if (ci.idName != 0)
            {
                LoadStringW(hModule, static_cast<UINT>(ci.idName), szAd, ARRAYSIZE(szAd));
            }
            if (ci.idInfo != 0)
            {
                LoadStringW(hModule, static_cast<UINT>(ci.idInfo), szAcik, ARRAYSIZE(szAcik));
            }
        }

        pfnYaz(static_cast<int>(i), szAd, szAcik, pvUser);
    }

    pfn(NULL, CPL_EXIT, 0, 0);
    return true;
}

} /* namespace HostUI */
