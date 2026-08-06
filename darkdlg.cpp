/*++

    darkdlg.cpp

    Aciklama icin darkdlg.h'ye bakin.

    Tasarim kurali: ACIK TEMADA BU DOSYADAKI HICBIR KOD CALISMAZ. Her giris
    noktasi DarkMode::IsDark() ile baslar; false ise hemen doner. Boylece
    /light ve /classic kipinde pencere, orijinal cleanmgr ile birebir ayni
    yollardan cizilir.

--*/

#include "darkdlg.h"
#include "darkmode.h"
#include "strutil.h"

/*  Yalnizca TIPLER icin (HTHEME). uxtheme.lib'e BAGLANMIYORUZ -- gereken
    islevler asagida GetProcAddress ile baglanir, boylece uxtheme'in
    bulunmadigi bir sistemde de ikili acilir. */
#include <uxtheme.h>

#include <strsafe.h>

#pragma comment(lib, "comctl32.lib")

namespace {

/*  Alt sinif kimlikleri -- her denetim turu icin ayri. */
const UINT_PTR kSubclassGroupBox = 1;
const UINT_PTR kSubclassTab      = 3;
const UINT_PTR kSubclassSheet    = 4;
const UINT_PTR kSubclassHeader   = 5;
const UINT_PTR kSubclassRadio    = 6;
const UINT_PTR kSubclassStaticFrame = 7;
const UINT_PTR kSubclassEdit        = 8;
const UINT_PTR kSubclassDlgNotify   = 9;


/*==========================================================================*
 *  Kucuk yardimcilar
 *==========================================================================*/

bool ClassIs(HWND hWnd, LPCWSTR pszClass)
{
    WCHAR szClass[64];
    if (GetClassNameW(hWnd, szClass, ARRAYSIZE(szClass)) == 0)
    {
        return false;
    }
    return ShadeEqualsI(szClass, pszClass);
}


HFONT ControlFont(HWND hWnd)
{
    HFONT hFont = reinterpret_cast<HFONT>(SendMessageW(hWnd, WM_GETFONT, 0, 0));
    if (hFont == NULL)
    {
        hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    return hFont;
}


/*  DPI'ya gore olcek. Manifest dpiAware=true oldugundan sistem DPI'si
    pencerenin gercek olceklemesidir. */
int Scale(int nBase)
{
    static int s_nDpi = 0;
    if (s_nDpi == 0)
    {
        HDC hdc = GetDC(NULL);
        s_nDpi = (hdc != NULL) ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
        if (hdc != NULL) { ReleaseDC(NULL, hdc); }
        if (s_nDpi <= 0) { s_nDpi = 96; }
    }
    return MulDiv(nBase, s_nDpi, 96);
}


/*==========================================================================*
 *  BS_GROUPBOX
 *==========================================================================*
 *
 *  Temanin karanlik varyanti grup kutusunu KAPSAMAZ: cerceve acik gri kalir
 *  ve baslik metni siyah cizilir (olculdu, ..\re\tema\). SetWindowTheme ile
 *  duzeltilemiyor, cunku cizim "Button" tema sinifinin GroupBox parcasindan
 *  geliyor ve o parcanin karanlik karsiligi yok.
 *
 *  Bu yuzden tamamini kendimiz ciziyoruz. Olculer temanin kendi oranlariyla
 *  ayni: cerceve, baslik metninin dikey ortasindan gecer; metin soldan 9 DLU
 *  iceridedir ve iki yanina birer bosluk birakilir.
 */
void PaintGroupBox(HWND hWnd, HDC hdcTarget)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    int cx = rcClient.right;
    int cy = rcClient.bottom;
    if (cx <= 0 || cy <= 0)
    {
        return;
    }

    /*  Cift tamponlama -- titreme olmasin. */
    HDC     hdcMem = CreateCompatibleDC(hdcTarget);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcTarget, cx, cy);
    HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);

    FillRect(hdcMem, &rcClient, DarkMode::BrushBackground());

    WCHAR szText[256];
    if (GetWindowTextW(hWnd, szText, ARRAYSIZE(szText)) == 0)
    {
        szText[0] = L'\0';
    }

    HGDIOBJ hFontOld = SelectObject(hdcMem, ControlFont(hWnd));
    SetBkMode(hdcMem, TRANSPARENT);

    SIZE szText2 = { 0, 0 };
    if (szText[0] != L'\0')
    {
        GetTextExtentPoint32W(hdcMem, szText, lstrlenW(szText), &szText2);
    }

    /*  Cerceve, metnin dikey ortasindan gecer. */
    int yFrame = szText2.cy / 2;

    HPEN    hPen    = CreatePen(PS_SOLID, 1, DarkMode::ColorBorder());
    HGDIOBJ hPenOld = SelectObject(hdcMem, hPen);
    HGDIOBJ hBrOld  = SelectObject(hdcMem, GetStockObject(NULL_BRUSH));

    int nRound = Scale(6);
    RoundRect(hdcMem, 0, yFrame, cx, cy, nRound, nRound);

    SelectObject(hdcMem, hBrOld);
    SelectObject(hdcMem, hPenOld);
    DeleteObject(hPen);

    if (szText[0] != L'\0')
    {
        int xText = Scale(9);
        int nPad  = Scale(3);

        /*  Cerceveyi metnin arkasindan sil. */
        RECT rcGap = { xText - nPad, yFrame, xText + szText2.cx + nPad, yFrame + 2 };
        FillRect(hdcMem, &rcGap, DarkMode::BrushBackground());

        SetTextColor(hdcMem, IsWindowEnabled(hWnd) ? DarkMode::ColorText()
                                                   : DarkMode::ColorTextSecondary());

        RECT rcText = { xText, 0, cx, szText2.cy };
        DrawTextW(hdcMem, szText, -1, &rcText,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);
    }

    SelectObject(hdcMem, hFontOld);

    BitBlt(hdcTarget, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}


LRESULT CALLBACK GroupBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                              UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;               /* tamami WM_PAINT'te ciziliyor */

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintGroupBox(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

    case WM_ENABLE:
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, GroupBoxProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*==========================================================================*
 *  ListView onay kutulari
 *==========================================================================*
 *
 *  LVS_EX_CHECKBOXES etkinlestirildiginde comctl32, onay kutularini kendi
 *  urettigi bir DURUM goruntu listesinden cizer ve o goruntuler ACIK tema
 *  parcalarindan uretilir. Karanlik zeminde beyaz kareler olarak kaliyorlar.
 *
 *  Belgelenmis cozum, durum goruntu listesini kendi listemizle degistirmek:
 *  ListView, durum dizini N icin goruntu N-1'i kullanir; yani 0 = isaretsiz,
 *  1 = isaretli.
 *
 *  Kutuyu tema motoruna cizdirmek yerine kendimiz ciziyoruz. Sebep: karanlik
 *  onay kutusunun tema sinifi adi BELGESIZDIR ve Windows surumleri arasinda
 *  degisiyor; elle cizim her surumde ayni sonucu verir ve hicbir sey
 *  varsaymaz.
 */
/*==========================================================================*
 *  uxtheme -- dinamik baglama
 *==========================================================================*
 *
 *  Onay kutusunu ELLE cizmek yerine tema motoruna cizdirmek, Windows'un
 *  kendi kutusuyla birebir ayni sonucu verir (kose yaricapi, cizgi kalinligi,
 *  kenar yumusatmasi). Elle cizilen tik "net degil" geri bildirimini aldi;
 *  tema cizimi o sorunu tamamen ortadan kaldiriyor.
 *
 *  Sinif adi "DarkMode_Explorer::Button" BELGESIZDIR; bulunamazsa asagidaki
 *  elle cizim devreye girer -- projenin genel kurali: hicbir sey zorunlu degil.
 */

typedef HTHEME  (WINAPI *PFN_OpenThemeData)(HWND, LPCWSTR);
typedef HRESULT (WINAPI *PFN_CloseThemeData)(HTHEME);
typedef HRESULT (WINAPI *PFN_DrawThemeBackground)(HTHEME, HDC, int, int, const RECT *, const RECT *);
typedef HRESULT (WINAPI *PFN_GetThemePartSize)(HTHEME, HDC, int, int, const RECT *, int, SIZE *);

PFN_OpenThemeData       g_pfnOpenThemeData       = NULL;
PFN_CloseThemeData      g_pfnCloseThemeData      = NULL;
PFN_DrawThemeBackground g_pfnDrawThemeBackground = NULL;
PFN_GetThemePartSize    g_pfnGetThemePartSize    = NULL;
bool                    g_bThemeBound            = false;

/*  vssym32.h sabitleri -- eski SDK'larda bulunmayabilir diye elle. */
const int kBP_CHECKBOX          = 3;
const int kCBS_UNCHECKEDNORMAL  = 1;
const int kCBS_CHECKEDNORMAL    = 5;
const int kTS_TRUE              = 1;   /* THEMESIZE: TS_TRUE */

void BindTheme(void)
{
    if (g_bThemeBound)
    {
        return;
    }
    g_bThemeBound = true;

    HMODULE h = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (h == NULL)
    {
        return;
    }

    g_pfnOpenThemeData = reinterpret_cast<PFN_OpenThemeData>(
        reinterpret_cast<void *>(GetProcAddress(h, "OpenThemeData")));
    g_pfnCloseThemeData = reinterpret_cast<PFN_CloseThemeData>(
        reinterpret_cast<void *>(GetProcAddress(h, "CloseThemeData")));
    g_pfnDrawThemeBackground = reinterpret_cast<PFN_DrawThemeBackground>(
        reinterpret_cast<void *>(GetProcAddress(h, "DrawThemeBackground")));
    g_pfnGetThemePartSize = reinterpret_cast<PFN_GetThemePartSize>(
        reinterpret_cast<void *>(GetProcAddress(h, "GetThemePartSize")));
}


/*++
    Onay kutusunu tema motoruna cizdirir. Basarisizsa false doner.

    TUZAK: OpenThemeData'ya, uzerinde SetWindowTheme ile alt-tema adi
    atanmis bir pencere verilirse "App::Class" bicimindeki sinif adi SESSIZCE
    NULL doner. Bu yuzden hWnd olarak NULL geciyoruz.
--*/
bool DrawThemedCheck(HDC hdc, const RECT *prc, bool bChecked)
{
    BindTheme();

    if (g_pfnOpenThemeData == NULL || g_pfnDrawThemeBackground == NULL ||
        g_pfnCloseThemeData == NULL)
    {
        return false;
    }

    HTHEME hTheme = g_pfnOpenThemeData(NULL, L"DarkMode_Explorer::Button");
    if (hTheme == NULL)
    {
        return false;
    }

    int nState = bChecked ? kCBS_CHECKEDNORMAL : kCBS_UNCHECKEDNORMAL;

    /*  Kutunun kendi dogal boyutu; hucrenin ortasina oturtuyoruz. */
    RECT rcBox = *prc;

    SIZE szPart;
    if (g_pfnGetThemePartSize != NULL &&
        SUCCEEDED(g_pfnGetThemePartSize(hTheme, hdc, kBP_CHECKBOX, nState, NULL,
                                        kTS_TRUE, &szPart)) &&
        szPart.cx > 0 && szPart.cy > 0)
    {
        int cx = prc->right - prc->left;
        int cy = prc->bottom - prc->top;

        if (szPart.cx <= cx && szPart.cy <= cy)
        {
            rcBox.left   = prc->left + (cx - szPart.cx) / 2;
            rcBox.top    = prc->top  + (cy - szPart.cy) / 2;
            rcBox.right  = rcBox.left + szPart.cx;
            rcBox.bottom = rcBox.top  + szPart.cy;
        }
    }

    HRESULT hr = g_pfnDrawThemeBackground(hTheme, hdc, kBP_CHECKBOX, nState, &rcBox, NULL);

    g_pfnCloseThemeData(hTheme);

    return SUCCEEDED(hr);
}


/*  Tema yoksa kullanilan elle cizim. */
void DrawCheckGlyph(HDC hdc, const RECT *prc, bool bChecked)
{
    if (DrawThemedCheck(hdc, prc, bChecked))
    {
        return;
    }

    int cx = prc->right - prc->left;
    int cy = prc->bottom - prc->top;

    /*  Kutu, hucrenin ortasinda; kenarlarda birer piksel pay. */
    int nBox = (cx < cy ? cx : cy) - Scale(3);
    if (nBox < 8) { nBox = (cx < cy ? cx : cy); }

    RECT rcBox;
    rcBox.left   = prc->left + (cx - nBox) / 2;
    rcBox.top    = prc->top  + (cy - nBox) / 2;
    rcBox.right  = rcBox.left + nBox;
    rcBox.bottom = rcBox.top  + nBox;

    int nRound = Scale(4);

    if (bChecked)
    {
        /*  Dolu kutu + beyaz onay isareti. */
        HBRUSH hbr = CreateSolidBrush(DarkMode::ColorLink());
        HPEN   hpn = CreatePen(PS_SOLID, 1, DarkMode::ColorLink());

        HGDIOBJ hbrOld = SelectObject(hdc, hbr);
        HGDIOBJ hpnOld = SelectObject(hdc, hpn);
        RoundRect(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, nRound, nRound);
        SelectObject(hdc, hpnOld);
        SelectObject(hdc, hbrOld);
        DeleteObject(hpn);
        DeleteObject(hbr);

        /*  Onay isareti: iki cizgi. Segoe Fluent glifine bagimli olmamak
            icin bilerek elle ciziliyor -- yazi tipi her sistemde yok. */
        int nThick = (nBox >= 16) ? 2 : 1;
        HPEN hpnMark = CreatePen(PS_SOLID, nThick, RGB(0, 0, 0));

        /*  Koyu tema vurgusu acik renkli oldugundan isaret KOYU olmali;
            aksi halde kontrast kalmaz. */
        HGDIOBJ hpnMarkOld = SelectObject(hdc, hpnMark);

        POINT pts[3];
        pts[0].x = rcBox.left + MulDiv(nBox, 25, 100);
        pts[0].y = rcBox.top  + MulDiv(nBox, 52, 100);
        pts[1].x = rcBox.left + MulDiv(nBox, 43, 100);
        pts[1].y = rcBox.top  + MulDiv(nBox, 70, 100);
        pts[2].x = rcBox.left + MulDiv(nBox, 75, 100);
        pts[2].y = rcBox.top  + MulDiv(nBox, 30, 100);
        Polyline(hdc, pts, 3);

        SelectObject(hdc, hpnMarkOld);
        DeleteObject(hpnMark);
    }
    else
    {
        /*  Bos kutu: yalnizca cerceve. Ton, Windows 11'in karanlik onay
            kutusuyla ayni (#8A8A8A); daha acigi koyu zeminde parlak bir
            kare gibi duruyordu. */
        HPEN    hpn    = CreatePen(PS_SOLID, 1, RGB(0x8A, 0x8A, 0x8A));
        HGDIOBJ hpnOld = SelectObject(hdc, hpn);
        HGDIOBJ hbrOld = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, nRound, nRound);
        SelectObject(hdc, hbrOld);
        SelectObject(hdc, hpnOld);
        DeleteObject(hpn);
    }
}


/*++
    ListView icin karanlik durum (onay kutusu) goruntu listesi uretir.

    ONEMLI AYRINTI -- indis kaymasi
    -------------------------------
    ListView, LVIS_STATEIMAGEMASK'teki N degeri icin goruntu listesindeki
    N-1'inci goruntuyu cizer; N = 0 "goruntu yok" demektir. Yani mantiken
    iki goruntu (isaretsiz, isaretli) yetmeli.

    Ancak comctl32'nin LVS_EX_CHECKBOXES ile KENDI urettigi durum listesinde
    goruntu sayisi bu varsayimla uyusmayabilir; uyusmazsa isaretli ogeler
    isaretsiz gorunur -- ilk denemede tam olarak bu oldu (bkz. ..\re\tema\).

    Bu yuzden tahmin etmiyoruz: comctl32'nin kurdugu listenin goruntu sayisini
    OKUYUP ayni sayida goruntu uretiyoruz ve isaretsiz/isaretli ciftini ayni
    indislere koyuyoruz. Boylece hangi surumde hangi sozlesme gecerliyse ona
    uyuyoruz.
--*/
HIMAGELIST MakeCheckImageList(HWND hCtl, HIMAGELIST himlOld, int cImagesDefault)
{
    /*  Onay kutusu hucresinin boyutu, kucuk ikon olcusuyle ayni olmali;
        denetim satir yuksekligini buna gore ayarliyor. */
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    /*  Varsa comctl32'nin kendi listesindeki goruntu sayisi -- sozlesmeyi o
        belirler. Liste henuz kurulmamissa cagiranin verdigi varsayilan. */
    int cImages = (himlOld != NULL) ? ImageList_GetImageCount(himlOld)
                                    : cImagesDefault;
    if (cImages < 2) { cImages = 2; }
    if (cImages > 8) { cImages = 8; }

    /*  Isaretsiz ve isaretli goruntulerin indisleri.

        cImages == 2  -> durum 1 -> goruntu 0, durum 2 -> goruntu 1
        cImages >= 3  -> comctl32 bos bir goruntuyu 0'a koymus demektir;
                         durum 1 -> goruntu 1, durum 2 -> goruntu 2
        Ikisinde de "durum N -> goruntu N-1" kurali gecerli olmadigi icin
        indisleri sayidan turetiyoruz. */
    int iUnchecked = (cImages >= 3) ? 1 : 0;
    int iChecked   = iUnchecked + 1;

    HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR32, cImages, 0);
    if (himl == NULL)
    {
        return NULL;
    }

    HDC hdcRef = GetDC(hCtl);
    if (hdcRef == NULL)
    {
        ImageList_Destroy(himl);
        return NULL;
    }

    COLORREF crBk = DarkMode::ColorSurface();

    for (int i = 0; i < cImages; ++i)
    {
        HDC     hdcMem = CreateCompatibleDC(hdcRef);
        HBITMAP hbm    = CreateCompatibleBitmap(hdcRef, cx, cy);
        HGDIOBJ hbmOld = SelectObject(hdcMem, hbm);

        RECT   rc  = { 0, 0, cx, cy };
        HBRUSH hbr = CreateSolidBrush(crBk);
        FillRect(hdcMem, &rc, hbr);
        DeleteObject(hbr);

        /*  Yalnizca iki anlamli goruntu ciziyoruz; kalanlar duz zemin kalir
            (comctl32 onlari zaten istemez). */
        if (i == iUnchecked || i == iChecked)
        {
            DrawCheckGlyph(hdcMem, &rc, (i == iChecked));
        }

        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);

        ImageList_Add(himl, hbm, NULL);
        DeleteObject(hbm);
    }

    ReleaseDC(hCtl, hdcRef);
    return himl;
}


/*==========================================================================*
 *  TVS_CHECKBOXES OLMADAN cizilen onay kutulari
 *==========================================================================*
 *
 *  Bazi moduller (sysdm.cpl bunlardan biri) agac onay kutularini comctl32'ye
 *  birakmaz; kutu goruntulerini KENDILERI uretip agacin NORMAL goruntu
 *  listesine koyar ve her ogenin iImage'ini o goruntulere yoneltir. Bu,
 *  TVS_CHECKBOXES'tan onceki klasik yontemdir.
 *
 *  Sonucu sudur: TVM_SETBKCOLOR zemini koyulastirir, ama kutular acik temaya
 *  gore uretildigi icin oldugu gibi kalir. Olculdu (..\..\sysdm\re\tema\
 *  perf-0-gorsel.png, "Gorsel Efektler"):
 *
 *      isaretsiz : 20x20, duz #F3F3F3 dolgu + #626262 cerceve
 *      isaretli  : 20x20, #005FB8 dolgu + #DFEBF6 onay isareti
 *      disi      : saydam (agac zemini #2B2B2B goruluyor)
 *
 *  HANGI INDISIN NE OLDUGUNU VARSAYMIYORUZ. Modulden module -- hatta surumden
 *  surume -- degisebilir ve yanlis varsayim, isaretli ogeleri isaretsiz
 *  gostermek gibi sessiz bir hataya doner (ayni tuzaga cleanmgr'in ListView
 *  onay kutularinda bir kez dusuldu). Bunun yerine her goruntuyu bilinen bir
 *  zemine cizip PIKSELLERINDEN siniflandiriyoruz:
 *
 *      cok sayida vurgu (accent) pikseli  -> isaretli
 *      cogunlukla duz acik dolgu          -> isaretsiz
 *      digeri                             -> onay kutusu degil, ELLENMEZ
 */
enum CheckKind
{
    kNotACheck = 0,
    kUncheckedImage,
    kCheckedImage
};


CheckKind ClassifyCheckImage(HIMAGELIST himl, int iImage, int cx, int cy, HDC hdcRef)
{
    const COLORREF crProbe = RGB(255, 0, 255);   /* magenta: saydamlik olcusu */

    HDC     hdcMem = CreateCompatibleDC(hdcRef);
    HBITMAP hbm    = CreateCompatibleBitmap(hdcRef, cx, cy);

    if (hdcMem == NULL || hbm == NULL)
    {
        if (hbm != NULL)    { DeleteObject(hbm); }
        if (hdcMem != NULL) { DeleteDC(hdcMem); }
        return kNotACheck;
    }

    HGDIOBJ hbmOld = SelectObject(hdcMem, hbm);

    RECT   rc  = { 0, 0, cx, cy };
    HBRUSH hbr = CreateSolidBrush(crProbe);
    FillRect(hdcMem, &rc, hbr);
    DeleteObject(hbr);

    ImageList_Draw(himl, iImage, hdcMem, 0, 0, ILD_TRANSPARENT);

    int cOpaque = 0;
    int cLight  = 0;
    int cAccent = 0;

    for (int y = 0; y < cy; ++y)
    {
        for (int x = 0; x < cx; ++x)
        {
            COLORREF c = GetPixel(hdcMem, x, y);
            if (c == crProbe)
            {
                continue;               /* saydam kaldi */
            }

            ++cOpaque;

            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);

            if (r > 0xD0 && g > 0xD0 && b > 0xD0)
            {
                ++cLight;
            }
            if (b > 0x90 && (b - r) > 40 && (b - g) > 20)
            {
                ++cAccent;
            }
        }
    }

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);

    /*  Onay kutusu hucreyi doldurur; buyuk olcude saydam bir goruntu baska
        bir seydir (gercek bir ikon gibi) ve ona dokunmuyoruz. */
    if (cOpaque < (cx * cy) / 2)
    {
        return kNotACheck;
    }
    if (cAccent >= cOpaque / 4)
    {
        return kCheckedImage;
    }
    if (cLight >= cOpaque / 2)
    {
        return kUncheckedImage;
    }
    return kNotACheck;
}


/*++
    Agacin NORMAL goruntu listesindeki onay kutusu goruntulerini karanlik
    olanlarla degistirir. Onay kutusu OLMAYAN goruntuler oldugu gibi kopyalanir.

    Eski liste YOK EDILMEZ: o, comctl32'nin degil, barindirdigimiz modulun
    malidir ve baska yerde de tutuyor olabilir. Proses omru boyunca duran
    kucuk bir goruntu listesi, baskasinin nesnesini serbest birakmaktan cok
    daha ucuzdur.
--*/
void RethemeImageListChecks(HWND hTree)
{
    HIMAGELIST himlOld = reinterpret_cast<HIMAGELIST>(
        SendMessageW(hTree, TVM_GETIMAGELIST, TVSIL_NORMAL, 0));

    if (himlOld == NULL)
    {
        return;
    }

    int cImages = ImageList_GetImageCount(himlOld);
    int cx = 0;
    int cy = 0;

    if (cImages <= 0 || cImages > 64 ||
        !ImageList_GetIconSize(himlOld, &cx, &cy) || cx <= 0 || cy <= 0)
    {
        return;
    }

    HDC hdcRef = GetDC(hTree);
    if (hdcRef == NULL)
    {
        return;
    }

    HIMAGELIST himlNew = ImageList_Create(cx, cy, ILC_COLOR32, cImages, 0);
    if (himlNew == NULL)
    {
        ReleaseDC(hTree, hdcRef);
        return;
    }

    int cReplaced = 0;

    for (int i = 0; i < cImages; ++i)
    {
        CheckKind kind = ClassifyCheckImage(himlOld, i, cx, cy, hdcRef);

        HDC     hdcMem = CreateCompatibleDC(hdcRef);
        HBITMAP hbm    = CreateCompatibleBitmap(hdcRef, cx, cy);
        HGDIOBJ hbmOld = SelectObject(hdcMem, hbm);

        /*  Zemin, agacin zemin rengiyle DOLU (saydam degil).

            Maskeli bir liste daha zarif gorunurdu ama olmaz: glifin
            yumusatilmis kenarlari maske rengiyle karisir ve cevresinde o
            rengin hayaleti kalir. Olcum zaten saydamliga gerek olmadigini
            gosteriyor -- kutu hucrenin tamamini kapliyor (20x20 hucrede
            20x20 glif), yani "disarisi" diye bir yer yok. */
        RECT   rc  = { 0, 0, cx, cy };
        HBRUSH hbr = CreateSolidBrush(DarkMode::ColorSurface());
        FillRect(hdcMem, &rc, hbr);
        DeleteObject(hbr);

        if (kind == kNotACheck)
        {
            ImageList_Draw(himlOld, i, hdcMem, 0, 0, ILD_TRANSPARENT);
        }
        else
        {
            DrawCheckGlyph(hdcMem, &rc, (kind == kCheckedImage));
            ++cReplaced;
        }

        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);

        ImageList_Add(himlNew, hbm, NULL);
        DeleteObject(hbm);
    }

    ReleaseDC(hTree, hdcRef);

    if (cReplaced == 0)
    {
        /*  Onay kutusu bulunmadi -- burasi bizim isimiz degil. */
        ImageList_Destroy(himlNew);
        return;
    }

    SendMessageW(hTree, TVM_SETIMAGELIST, TVSIL_NORMAL,
                 reinterpret_cast<LPARAM>(himlNew));
}


/*==========================================================================*
 *  BS_AUTORADIOBUTTON
 *==========================================================================*
 *
 *  Olculdu: "DarkMode_Explorer" temasiyla ONAY KUTUSU metni #FFFFFF gelirken
 *  RADYO DUGMESI metni #000000 kaliyor. Ayni tema sinifinin farkli parcalari
 *  ve radyo parcasinin karanlik metin rengi tanimli degil.
 *
 *  Glifi tema motoruna cizdiriyoruz (yerli gorunum), METNI kendimiz.
 */
const int kBP_RADIOBUTTON     = 2;
const int kRBS_UNCHECKEDNORMAL = 1;
const int kRBS_CHECKEDNORMAL   = 5;
const int kRBS_UNCHECKEDDISABLED = 4;
const int kRBS_CHECKEDDISABLED   = 8;

void PaintRadio(HWND hWnd, HDC hdcTarget)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    int cx = rcClient.right;
    int cy = rcClient.bottom;
    if (cx <= 0 || cy <= 0)
    {
        return;
    }

    bool bChecked  = (SendMessageW(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool bEnabled  = (IsWindowEnabled(hWnd) != FALSE);

    HDC     hdcMem = CreateCompatibleDC(hdcTarget);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcTarget, cx, cy);
    HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);

    FillRect(hdcMem, &rcClient, DarkMode::BrushBackground());

    /*  Glif kutusu. Yedek yol icin makul bir baslangic; tema varsa asagida
        DOGAL boyutla degistirilecek. */
    int nGlyph = Scale(13);
    if (nGlyph > cy) { nGlyph = cy; }

    RECT rcGlyph;
    rcGlyph.left   = 0;
    rcGlyph.top    = (cy - nGlyph) / 2;
    rcGlyph.right  = nGlyph;
    rcGlyph.bottom = rcGlyph.top + nGlyph;

    bool bDrawn = false;

    BindTheme();
    if (g_pfnOpenThemeData != NULL && g_pfnDrawThemeBackground != NULL &&
        g_pfnCloseThemeData != NULL)
    {
        HTHEME hTheme = g_pfnOpenThemeData(NULL, L"DarkMode_Explorer::Button");
        if (hTheme != NULL)
        {
            int nState = bEnabled
                ? (bChecked ? kRBS_CHECKEDNORMAL   : kRBS_UNCHECKEDNORMAL)
                : (bChecked ? kRBS_CHECKEDDISABLED : kRBS_UNCHECKEDDISABLED);

            /*  GLIFI TEMANIN DOGAL BOYUTUNDA CIZ.

                Ilk surumde kutu GetSystemMetrics(SM_CYMENUCHECK)'ten
                aliniyordu -- o menu onay isaretinin olcusudur, radyo
                dugmesininki degil. Boyutlar tutmayinca DrawThemeBackground
                glifi OLCEKLIYOR ve kenarlar basamakli cikiyor; %500
                buyutmede acikca goruldu (..\..\sysdm\re\tema\zoom-radyo.png).

                TS_TRUE, parcanin bozulmadan cizilecegi gercek olcusunu verir.
                Onu alip dikeyde ortaliyoruz; artik olcekleme yok, kenarlar
                Windows'un kendi cizimi kadar puruzsuz. */
            SIZE szPart;
            if (g_pfnGetThemePartSize != NULL &&
                SUCCEEDED(g_pfnGetThemePartSize(hTheme, hdcMem, kBP_RADIOBUTTON,
                                                nState, NULL, kTS_TRUE, &szPart)) &&
                szPart.cx > 0 && szPart.cy > 0 && szPart.cy <= cy)
            {
                rcGlyph.left   = 0;
                rcGlyph.top    = (cy - szPart.cy) / 2;
                rcGlyph.right  = szPart.cx;
                rcGlyph.bottom = rcGlyph.top + szPart.cy;
                nGlyph         = szPart.cx;
            }

            bDrawn = SUCCEEDED(g_pfnDrawThemeBackground(hTheme, hdcMem,
                                                        kBP_RADIOBUTTON, nState,
                                                        &rcGlyph, NULL));
            g_pfnCloseThemeData(hTheme);
        }
    }

    if (!bDrawn)
    {
        /*  Tema yoksa elle: halka + secili ise ic nokta. */
        HPEN    hpn    = CreatePen(PS_SOLID, 1, RGB(0x8A, 0x8A, 0x8A));
        HGDIOBJ hpnOld = SelectObject(hdcMem, hpn);
        HGDIOBJ hbrOld = SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
        Ellipse(hdcMem, rcGlyph.left + 1, rcGlyph.top + 1,
                rcGlyph.right - 1, rcGlyph.bottom - 1);
        SelectObject(hdcMem, hbrOld);
        SelectObject(hdcMem, hpnOld);
        DeleteObject(hpn);

        if (bChecked)
        {
            int nInset = nGlyph / 3;
            HBRUSH hbrDot = CreateSolidBrush(DarkMode::ColorLink());
            HGDIOBJ hbrDotOld = SelectObject(hdcMem, hbrDot);
            HPEN hpnDot = CreatePen(PS_SOLID, 1, DarkMode::ColorLink());
            HGDIOBJ hpnDotOld = SelectObject(hdcMem, hpnDot);
            Ellipse(hdcMem, rcGlyph.left + nInset, rcGlyph.top + nInset,
                    rcGlyph.right - nInset, rcGlyph.bottom - nInset);
            SelectObject(hdcMem, hpnDotOld);
            SelectObject(hdcMem, hbrDotOld);
            DeleteObject(hpnDot);
            DeleteObject(hbrDot);
        }
    }

    /*  Metin -- asil duzeltilen sey bu. */
    WCHAR szText[512];
    if (GetWindowTextW(hWnd, szText, ARRAYSIZE(szText)) > 0)
    {
        HGDIOBJ hFontOld = SelectObject(hdcMem, ControlFont(hWnd));
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, bEnabled ? DarkMode::ColorText()
                                      : DarkMode::ColorTextSecondary());

        RECT rcText = rcClient;
        rcText.left = rcGlyph.right + Scale(5);

        LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);

        UINT uFormat = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
        if ((style & BS_MULTILINE) != 0)
        {
            uFormat = DT_LEFT | DT_TOP | DT_WORDBREAK;
        }

        DrawTextW(hdcMem, szText, -1, &rcText, uFormat);
        SelectObject(hdcMem, hFontOld);
    }

    BitBlt(hdcTarget, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}


LRESULT CALLBACK RadioProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                           UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintRadio(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

    /*  Secim degisince kendimiz yeniden cizmeliyiz. */
    case BM_SETCHECK:
    case WM_ENABLE:
        {
            LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            InvalidateRect(hWnd, NULL, FALSE);
            return lr;
        }

    case WM_LBUTTONUP:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        {
            LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            /*  Ayni gruptaki diger radyolar da degisti; hepsini tazele. */
            HWND hParent = GetParent(hWnd);
            if (hParent != NULL)
            {
                InvalidateRect(hParent, NULL, TRUE);
            }
            return lr;
        }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, RadioProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*==========================================================================*
 *  SysHeader32  (ListView sutun basligi)
 *==========================================================================*
 *
 *  Listeye verilen tema adi basligi KAPSAMAZ; koyu bir listenin ustunde
 *  bembeyaz bir satir kalir (olculdu: Ortam Degiskenleri ve Kullanici
 *  Profilleri kutulari).
 *
 *  "DarkMode_ItemsView::Header" tema adini atamak da YETMEDI -- olculdu,
 *  baslik yine #FFFFFF kaldi. Bu yuzden grup kutusu ve sekme denetiminde
 *  oldugu gibi tamamini kendimiz ciziyoruz; o iki yerde bu yol kesin sonuc
 *  verdi.
 */
void PaintHeader(HWND hWnd, HDC hdcTarget)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    int cx = rcClient.right;
    int cy = rcClient.bottom;
    if (cx <= 0 || cy <= 0)
    {
        return;
    }

    HDC     hdcMem = CreateCompatibleDC(hdcTarget);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcTarget, cx, cy);
    HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);

    /*  Zemin: listenin yuzeyinden bir tik koyu, boylece baslik ayirt edilir. */
    HBRUSH hbrBk = CreateSolidBrush(RGB(0x25, 0x25, 0x25));
    FillRect(hdcMem, &rcClient, hbrBk);
    DeleteObject(hbrBk);

    HGDIOBJ hFontOld = SelectObject(hdcMem, ControlFont(hWnd));
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, DarkMode::ColorText());

    HBRUSH hbrLine = CreateSolidBrush(DarkMode::ColorBorder());

    int cItems = static_cast<int>(SendMessageW(hWnd, HDM_GETITEMCOUNT, 0, 0));

    for (int i = 0; i < cItems; ++i)
    {
        RECT rcItem;
        if (SendMessageW(hWnd, HDM_GETITEMRECT, static_cast<WPARAM>(i),
                         reinterpret_cast<LPARAM>(&rcItem)) == 0)
        {
            continue;
        }

        WCHAR szText[128];
        szText[0] = L'\0';

        HDITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask       = HDI_TEXT | HDI_FORMAT;
        item.pszText    = szText;
        item.cchTextMax = ARRAYSIZE(szText);

        UINT uFormat = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;

        if (SendMessageW(hWnd, HDM_GETITEMW, static_cast<WPARAM>(i),
                         reinterpret_cast<LPARAM>(&item)) != 0)
        {
            /*  Sutunun kendi hizalamasina uy. */
            if ((item.fmt & HDF_RIGHT) != 0)       { uFormat |= DT_RIGHT;  }
            else if ((item.fmt & HDF_CENTER) != 0) { uFormat |= DT_CENTER; }
            else                                   { uFormat |= DT_LEFT;   }
        }

        RECT rcText = rcItem;
        rcText.left  += Scale(6);
        rcText.right -= Scale(6);

        if (szText[0] != L'\0')
        {
            DrawTextW(hdcMem, szText, -1, &rcText, uFormat);
        }

        /*  Sutun ayirici. */
        RECT rcSep = { rcItem.right - 1, rcItem.top + Scale(4),
                       rcItem.right,     rcItem.bottom - Scale(4) };
        FillRect(hdcMem, &rcSep, hbrLine);
    }

    /*  Basligin alt kenari. */
    RECT rcBottom = { 0, cy - 1, cx, cy };
    FillRect(hdcMem, &rcBottom, hbrLine);

    DeleteObject(hbrLine);
    SelectObject(hdcMem, hFontOld);

    BitBlt(hdcTarget, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}


LRESULT CALLBACK HeaderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                            UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintHeader(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, HeaderProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*==========================================================================*
 *  SysTabControl32  (PropertySheet sekmeleri)
 *==========================================================================*
 *
 *  Sekme denetiminin karanlik varyanti yok: hem sekme basliklari hem de
 *  sayfalarin arkasindaki GOVDE acik renk kaliyor. Govde, sayfalarin altinda
 *  kaldigi icin yalnizca kenarlarda birkac piksel gorunur ama koyu bir
 *  pencerede o birkac piksel hemen goze carpar.
 *
 *  Tamamini kendimiz ciziyoruz: govde, sekme dikdortgenleri ve metinler.
 */
void PaintTabControl(HWND hWnd, HDC hdcTarget)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    int cx = rcClient.right;
    int cy = rcClient.bottom;
    if (cx <= 0 || cy <= 0)
    {
        return;
    }

    HDC     hdcMem = CreateCompatibleDC(hdcTarget);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcTarget, cx, cy);
    HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);

    FillRect(hdcMem, &rcClient, DarkMode::BrushBackground());

    /*  Govde: sekme seridinin altindaki alan. */
    RECT rcBody = rcClient;
    SendMessageW(hWnd, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&rcBody));

    RECT rcPanel = rcClient;
    rcPanel.top = rcBody.top - Scale(2);
    if (rcPanel.top < 0) { rcPanel.top = 0; }

    FillRect(hdcMem, &rcPanel, DarkMode::BrushBackground());

    HBRUSH hbrEdge = CreateSolidBrush(DarkMode::ColorBorder());
    FrameRect(hdcMem, &rcPanel, hbrEdge);
    DeleteObject(hbrEdge);

    /*  Sekmeler. */
    HGDIOBJ hFontOld = SelectObject(hdcMem, ControlFont(hWnd));
    SetBkMode(hdcMem, TRANSPARENT);

    int cTabs = static_cast<int>(SendMessageW(hWnd, TCM_GETITEMCOUNT, 0, 0));
    int iSel  = static_cast<int>(SendMessageW(hWnd, TCM_GETCURSEL, 0, 0));

    for (int i = 0; i < cTabs; ++i)
    {
        RECT rcTab;
        if (SendMessageW(hWnd, TCM_GETITEMRECT, static_cast<WPARAM>(i),
                         reinterpret_cast<LPARAM>(&rcTab)) == 0)
        {
            continue;
        }

        bool bSelected = (i == iSel);

        /*  Secili sekme govdeyle birlesir; digerleri bir tik asagida durur --
            temanin kendi davranisiyla ayni. */
        if (!bSelected)
        {
            rcTab.top += Scale(2);
        }

        HBRUSH hbrTab = CreateSolidBrush(bSelected ? DarkMode::ColorBackground()
                                                   : RGB(0x2B, 0x2B, 0x2B));
        FillRect(hdcMem, &rcTab, hbrTab);
        DeleteObject(hbrTab);

        HBRUSH hbrLine = CreateSolidBrush(DarkMode::ColorBorder());
        FrameRect(hdcMem, &rcTab, hbrLine);
        DeleteObject(hbrLine);

        if (bSelected)
        {
            /*  Secili sekmenin alt kenarini sil ki govdeyle birlessin. */
            RECT rcJoin = { rcTab.left + 1, rcTab.bottom - 1,
                            rcTab.right - 1, rcTab.bottom };
            FillRect(hdcMem, &rcJoin, DarkMode::BrushBackground());
        }

        WCHAR szText[128];
        szText[0] = L'\0';

        TCITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask       = TCIF_TEXT;
        item.pszText    = szText;
        item.cchTextMax = ARRAYSIZE(szText);

        if (SendMessageW(hWnd, TCM_GETITEMW, static_cast<WPARAM>(i),
                         reinterpret_cast<LPARAM>(&item)) != 0 && szText[0] != L'\0')
        {
            SetTextColor(hdcMem, bSelected ? DarkMode::ColorText()
                                           : DarkMode::ColorTextSecondary());
            DrawTextW(hdcMem, szText, -1, &rcTab,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    SelectObject(hdcMem, hFontOld);

    BitBlt(hdcTarget, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}


LRESULT CALLBACK TabProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                         UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintTabControl(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

    /*  Sekme degistiginde tum serit yeniden cizilmeli. */
    case TCM_SETCURSEL:
        {
            LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            InvalidateRect(hWnd, NULL, FALSE);
            return lr;
        }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, TabProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*==========================================================================*
 *  PropertySheet cercevesi
 *==========================================================================*
 *
 *  Cerceve comctl32'nin kendi diyalogudur; sayfalarimizin yordamlari onun
 *  iletilerini gormez. Alt siniflamadan cercevenin zemini acik gri kalir ve
 *  sekme seridinin ustunde beyaz bir bant olusur.
 */
LRESULT CALLBACK SheetProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                           UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(reinterpret_cast<HDC>(wParam), &rc, DarkMode::BrushBackground());
            return 1;
        }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        {
            INT_PTR hbr = DarkDlg::OnCtlColor(uMsg, wParam, lParam);
            if (hbr != 0)
            {
                return static_cast<LRESULT>(hbr);
            }
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, SheetProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*==========================================================================*
 *  Cerceve / ayrac STATIC denetimleri
 *==========================================================================*
 *
 *  SS_ETCHEDHORZ, SS_ETCHEDVERT, SS_ETCHEDFRAME, SS_BLACKFRAME, SS_GRAYFRAME
 *  ve SS_WHITEFRAME, denetimi tema motoruyla degil SISTEM 3B RENKLERIYLE
 *  cizer (COLOR_3DSHADOW / COLOR_3DHIGHLIGHT). O renkler karanlik modda
 *  degismez, dolayisiyla ayrac cizgileri koyu zeminde acik gri kalir.
 *
 *  Olculdu (..\..\sysdm\re\tema\ana-3-koruma.png): "Sistem Geri Yukleme" ve
 *  "Koruma Ayarlari" basliklarinin yanindaki cizgiler #A0A0A0. Sablondaki
 *  denetim: Static, style 0x50001008 (SS_GRAYFRAME | SS_SUNKEN),
 *  exstyle 0x00020004 (WS_EX_STATICEDGE), yuksekligi 3 piksel.
 *
 *  Cizgiyi kendimiz ciziyoruz. WS_EX_STATICEDGE / WS_EX_CLIENTEDGE de
 *  kaldirilmali: onlar PENCERE DISI alanda cizilir ve WM_PAINT'i devralmak
 *  onlara yetismez.
 */
LRESULT CALLBACK StaticFrameProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                 UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            FillRect(hdc, &rc, DarkMode::BrushBackground());

            HBRUSH hbrLine = CreateSolidBrush(DarkMode::ColorBorder());

            int cx = rc.right - rc.left;
            int cy = rc.bottom - rc.top;

            if (cy <= Scale(4))
            {
                /*  Yatay ayrac: tek piksellik cizgi, dikeyde ortalanmis.
                    Orijinal iki piksellik "oyulmus" gorunum kullaniyor ama
                    Windows 11'in kendi ayraclari tek piksel; koyu zeminde
                    de dogru duran bu. */
                RECT rcLine = { rc.left, rc.top + cy / 2, rc.right, rc.top + cy / 2 + 1 };
                FillRect(hdc, &rcLine, hbrLine);
            }
            else if (cx <= Scale(4))
            {
                RECT rcLine = { rc.left + cx / 2, rc.top, rc.left + cx / 2 + 1, rc.bottom };
                FillRect(hdc, &rcLine, hbrLine);
            }
            else
            {
                FrameRect(hdc, &rc, hbrLine);
            }

            DeleteObject(hbrLine);
            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, StaticFrameProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*==========================================================================*
 *  WS_EX_CLIENTEDGE'li METIN KUTULARI
 *==========================================================================*
 *
 *  Metin kutusunun ZEMINI WM_CTLCOLOREDIT ile koyulasir, ama KENARLIGI
 *  pencere disi (non-client) alanda cizilir ve oraya erisimimiz yoktur.
 *
 *  "DarkMode_CFD" temasi da kurtarmiyor. Dogrudan tema motoruna soruldu:
 *
 *      GetThemeColor("DarkMode_CFD::Edit", EP_EDITBORDER_NOSCROLL,
 *                    ETS_NORMAL,  TMT_BORDERCOLOR) -> #ABADB3
 *      GetThemeColor("Edit",              ... ayni parca ...)  -> #ABADB3
 *
 *  Yani karanlik sinif, kenarlik icin ACIK temanin renginin AYNISINI
 *  veriyor -- bu parca icin karanlik varyant hic tanimlanmamis. Sonuc
 *  olculdu (..\..\sysdm\re\tema\ana-0-bilgisayaradi.png): kutu odaktayken
 *  cercevesi #FFFFFF, odak baskasindayken #9B9B9B.
 *
 *  Radyo dugmesi metninde oldugu gibi, temanin susmasi bizim cizmemiz
 *  gerektigi anlamina gelir.
 */
LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
    case WM_NCPAINT:
        {
            /*  DefSubclassProc CAGRILMAZ: onu cagirmak acik client edge'i
                geri getirirdi. Kenarligi tamamen biz ciziyoruz. */
            HDC hdc = GetWindowDC(hWnd);
            if (hdc != NULL)
            {
                RECT rc;
                GetWindowRect(hWnd, &rc);
                OffsetRect(&rc, -rc.left, -rc.top);

                bool bFocus = (GetFocus() == hWnd);

                /*  Dis cerceve: odaktayken Windows 11'in vurgu rengi,
                    normalde sessiz bir ayrac tonu. */
                HBRUSH hbrEdge = CreateSolidBrush(bFocus ? DarkMode::ColorLink()
                                                         : DarkMode::ColorBorder());
                FrameRect(hdc, &rc, hbrEdge);
                DeleteObject(hbrEdge);

                /*  Client edge iki piksel yer ayirir; kalan ic bandi kutunun
                    kendi zemin rengiyle dolduruyoruz ki metin ile cerceve
                    arasinda dogru bosluk kalsin. */
                InflateRect(&rc, -1, -1);
                if (rc.right > rc.left && rc.bottom > rc.top)
                {
                    HBRUSH hbrIn = CreateSolidBrush(
                        IsWindowEnabled(hWnd) ? DarkMode::ColorSurface()
                                              : DarkMode::ColorBackground());
                    FrameRect(hdc, &rc, hbrIn);
                    DeleteObject(hbrIn);
                }

                ReleaseDC(hWnd, hdc);
            }
        }
        return 0;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        {
            /*  Cerceve rengi odaga bagli; degistiginde yeniden cizdirilmeli. */
            LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            RedrawWindow(hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
            return lr;
        }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, EditProc, uIdSubclass);
        break;

    default:
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*++
    Diyalogun kendi uzerine kurulan ince bir katman: yalnizca WM_NOTIFY.

    NEDEN ALT SINIF DEGIL DE BURASI: SysLink, NM_CUSTOMDRAW'unu KENDINE
    degil EBEVEYNINE gonderir. Denetimi alt siniflamak o bildirimi gormemizi
    saglamaz.

    Ve neden her diyaloga otomatik takiliyor: aksi halde her diyalog
    yordamanin ayri ayri DarkDlg::OnNotify cagirmasi gerekirdi. Bu, unutulmasi
    kolay ve unutuldugunda sessizce yanlis calisan bir sozlesme olurdu --
    barindirdigimiz modullerin yordamlarina zaten hic dokunamiyoruz.
--*/
LRESULT CALLBACK DlgNotifyProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                               UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    if (uMsg == WM_NOTIFY)
    {
        LRESULT lr = 0;

        if (DarkDlg::OnNotify(hWnd, lParam, &lr))
        {
            /*  Bildirimi gonderen denetim, SendMessage'in donus degerini
                okur; pencere yordamindan donen deger odur. DWLP_MSGRESULT
                de ayarlaniyor ki diyalog sozlesmesi bozulmasin. */
            SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, static_cast<LONG_PTR>(lr));
            return lr;
        }
    }
    else if (uMsg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hWnd, DlgNotifyProc, uIdSubclass);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/*  Static'in govdesi bir cerceve/ayrac mi? */
bool IsFrameStatic(LONG_PTR style)
{
    LONG_PTR type = style & SS_TYPEMASK;

    return type == SS_BLACKFRAME  || type == SS_GRAYFRAME  || type == SS_WHITEFRAME ||
           type == SS_ETCHEDHORZ  || type == SS_ETCHEDVERT || type == SS_ETCHEDFRAME;
}


/*==========================================================================*
 *  Denetim agacini gezme
 *==========================================================================*/

BOOL CALLBACK ApplyChildProc(HWND hWndChild, LPARAM /*lParam*/)
{
    if (ClassIs(hWndChild, L"Static") &&
        IsFrameStatic(GetWindowLongPtrW(hWndChild, GWL_STYLE)))
    {
        LONG_PTR exStyle = GetWindowLongPtrW(hWndChild, GWL_EXSTYLE);
        LONG_PTR exWant  = exStyle & ~static_cast<LONG_PTR>(WS_EX_STATICEDGE |
                                                            WS_EX_CLIENTEDGE |
                                                            WS_EX_WINDOWEDGE);
        if (exWant != exStyle)
        {
            SetWindowLongPtrW(hWndChild, GWL_EXSTYLE, exWant);
            SetWindowPos(hWndChild, NULL, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        SetWindowSubclass(hWndChild, StaticFrameProc, kSubclassStaticFrame, 0);
        InvalidateRect(hWndChild, NULL, TRUE);
    }
    else if (ClassIs(hWndChild, L"Edit"))
    {
        DarkMode::ApplyToWindow(hWndChild);

        /*  Kenarligi yalnizca client edge VARSA ciziyoruz; yoksa cizilecek
            bir pencere disi alan da yoktur (salt okunur bilgi alanlari
            boyledir ve orijinalinde de cercevesizdirler). */
        if ((GetWindowLongPtrW(hWndChild, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) != 0)
        {
            SetWindowSubclass(hWndChild, EditProc, kSubclassEdit, 0);
            RedrawWindow(hWndChild, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
        }
    }
    else if (ClassIs(hWndChild, L"Button"))
    {
        LONG_PTR style = GetWindowLongPtrW(hWndChild, GWL_STYLE);
        LONG_PTR type  = style & BS_TYPEMASK;

        if (type == BS_GROUPBOX)
        {
            SetWindowSubclass(hWndChild, GroupBoxProc, kSubclassGroupBox, 0);
        }
        else if (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON)
        {
            /*  RADYO DUGMELERI AYRI ELE ALINMALI.

                Olculdu (..\..\sysdm\re\tema\alt-performans.png): ayni
                "DarkMode_Explorer" temasiyla ONAY KUTUSU metni #FFFFFF
                gelirken RADYO DUGMESI metni #000000 kaliyor -- koyu zeminde
                tamamen okunmaz. Iki denetim ayni tema sinifinin farkli
                parcalarini kullaniyor ve radyo parcasinin karanlik metin
                rengi tanimli degil.

                Metni kendimiz ciziyoruz; glif icin tema motorunu
                kullanmaya devam ediyoruz, boylece gorunum yerli kaliyor. */
            SetWindowSubclass(hWndChild, RadioProc, kSubclassRadio, 0);
            DarkMode::ApplyToWindow(hWndChild);
        }
        else
        {
            /*  Push dugmeler ve onay kutulari "DarkMode_Explorer" ile dogru
                geliyor -- olculdu, ek mudahale gerekmiyor. */
            DarkMode::ApplyToWindow(hWndChild);
        }
    }
    else if (ClassIs(hWndChild, PROGRESS_CLASSW))
    {
        /*  Tema adi "DarkMode_DarkTheme" OLMALI: yatak #131313, dolgu
            #6CCB5F. "DarkMode_Explorer" beyaz bir kutu ciziyor.

            PBM_SETBKCOLOR / PBM_SETBARCOLOR KULLANILMAZ: o iletiler temayi
            tamamen kapatir ve cubuk klasik duz dikdortgene doner. */
        DarkMode::ApplyTheme(hWndChild, L"DarkMode_DarkTheme");
    }
    else if (ClassIs(hWndChild, WC_TABCONTROLW))
    {
        SetWindowSubclass(hWndChild, TabProc, kSubclassTab, 0);
        DarkMode::ApplyToWindow(hWndChild);
    }
    else if (ClassIs(hWndChild, WC_LISTVIEWW))
    {
        DarkDlg::SetupListView(hWndChild);
    }
    else if (ClassIs(hWndChild, WC_TREEVIEWW))
    {
        DarkDlg::SetupTreeView(hWndChild);
    }
    else if (ClassIs(hWndChild, WC_HEADERW))
    {
        /*  ListView'in sutun basligi. ApplyChildProc onu ayrica da gezer
            (ListView'in alt penceresidir), ama SetupListView zaten ele
            aliyor; burada genel dalin uzerine yazmasini onluyoruz. */
        DarkDlg::SetupHeader(hWndChild);
    }
    else
    {
        DarkMode::ApplyToWindow(hWndChild);
    }

    return TRUE;
}

} /* anonim ad alani */


/*==========================================================================*
 *  Ortak arayuz
 *==========================================================================*/

namespace DarkDlg {

void Apply(HWND hDlg)
{
    if (!DarkMode::IsDark() || hDlg == NULL)
    {
        return;
    }

    DarkMode::ApplyToWindow(hDlg);
    DarkMode::SetTitleBarDark(hDlg, true);

    /*  SysLink'in NM_CUSTOMDRAW'unu karsilamak icin (bkz. DlgNotifyProc). */
    SetWindowSubclass(hDlg, DlgNotifyProc, kSubclassDlgNotify, 0);

    EnumChildWindows(hDlg, ApplyChildProc, 0);

    InvalidateRect(hDlg, NULL, TRUE);
}


INT_PTR OnCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!DarkMode::IsDark())
    {
        return 0;
    }

    HDC hdc = reinterpret_cast<HDC>(wParam);

    switch (uMsg)
    {
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        SetTextColor(hdc, DarkMode::ColorText());
        SetBkColor(hdc, DarkMode::ColorBackground());
        SetBkMode(hdc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(DarkMode::BrushBackground());

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        {
            /*  Salt okunur metin kutusu (201'deki aciklama alani) statik gibi
                davranmali; duzenlenebilir olan yuzey rengini alir. */
            HWND hCtl = reinterpret_cast<HWND>(lParam);
            LONG_PTR style = (hCtl != NULL) ? GetWindowLongPtrW(hCtl, GWL_STYLE) : 0;

            if ((style & ES_READONLY) != 0)
            {
                SetTextColor(hdc, DarkMode::ColorText());
                SetBkColor(hdc, DarkMode::ColorBackground());
                SetBkMode(hdc, TRANSPARENT);
                return reinterpret_cast<INT_PTR>(DarkMode::BrushBackground());
            }

            SetTextColor(hdc, DarkMode::ColorText());
            SetBkColor(hdc, DarkMode::ColorSurface());
            return reinterpret_cast<INT_PTR>(DarkMode::BrushSurface());
        }

    default:
        break;
    }

    return 0;
}


bool OnNotify(HWND /*hDlg*/, LPARAM lParam, LRESULT *plResult)
{
    /*  Grup kutusu, ilerleme cubugu ve sekme denetimi alt sinif yoluyla
        cizildigi icin onlara NM_CUSTOMDRAW gerekmiyor. Geriye bir denetim
        kaliyor: SysLink.

        BAGLANTI RENGI TEMADAN ALINAMAZ. Dogrudan tema motoruna soruldu,
        TEXT_HYPERLINKTEXT / HLS_NORMAL / TMT_TEXTCOLOR:

            TEXTSTYLE                    -> #0066CC
            DarkMode_Explorer::TextStyle -> #0066CC
            DarkMode_CFD::TextStyle      -> #0066CC
            DarkMode::TextStyle          -> #0066CC
            Explorer::TextStyle          -> #0066CC

        Hicbir karanlik varyant yok; hepsi acik temanin rengini veriyor.
        Yani SetWindowTheme ne verilirse verilsin bu duzelmez -- olculdu
        (..\..\sysdm\re\tema\ana-4-uzak.png: bagalanti metni #0066CC,
        zemin #202020). Radyo metni ve metin kutusu kenarliginda oldugu
        gibi, tema susuyorsa cizmek bize duser.

        SysLink, kendi WM_PAINT'i sirasinda EBEVEYNINE NM_CUSTOMDRAW
        gonderir; bu yuzden burasi -- alt sinif degil -- dogru yer. */
    if (!DarkMode::IsDark() || lParam == 0 || plResult == NULL)
    {
        return false;
    }

    const NMHDR *pHdr = reinterpret_cast<const NMHDR *>(lParam);

    if (pHdr->code != NM_CUSTOMDRAW || pHdr->hwndFrom == NULL)
    {
        return false;
    }

    const NMCUSTOMDRAW *pCd = reinterpret_cast<const NMCUSTOMDRAW *>(lParam);

    /*  KAYDIRICI (msctls_trackbar32).

        Kanal (oluk) sistem renkleriyle cizilir ve karanlik modda acik kalir.
        Olculdu (..\..\sysdm\re\tema\alt-yapilandir.png, "Disk Alani
        Kullanimi"): koyu diyalogun ortasinda bembeyaz bir cubuk. Kaydiricinin
        KENDISI (thumb) vurgu renginde dogru geliyor, ona dokunmuyoruz.  */
    if (ClassIs(pHdr->hwndFrom, TRACKBAR_CLASSW))
    {
        const int kTBCD_CHANNEL = 3;

        switch (pCd->dwDrawStage)
        {
        case CDDS_PREPAINT:
            *plResult = CDRF_NOTIFYITEMDRAW;
            return true;

        case CDDS_ITEMPREPAINT:
            if (pCd->dwItemSpec == kTBCD_CHANNEL)
            {
                RECT rc = pCd->rc;

                HBRUSH hbrFill = CreateSolidBrush(DarkMode::ColorSurface());
                FillRect(pCd->hdc, &rc, hbrFill);
                DeleteObject(hbrFill);

                HBRUSH hbrEdge = CreateSolidBrush(DarkMode::ColorBorder());
                FrameRect(pCd->hdc, &rc, hbrEdge);
                DeleteObject(hbrEdge);

                *plResult = CDRF_SKIPDEFAULT;
                return true;
            }
            *plResult = CDRF_DODEFAULT;
            return true;

        default:
            break;
        }

        return false;
    }

    /*  Sinif adi iki turlu olabiliyor: comctl32 sinifi "SysLink" adiyla
        kaydeder ama bu makinede olculen ad "Link Window". WC_LINK sabitine
        guvenmeyip ikisini de kabul ediyoruz. */
    if (!ClassIs(pHdr->hwndFrom, L"SysLink") &&
        !ClassIs(pHdr->hwndFrom, L"Link Window"))
    {
        return false;
    }

    switch (pCd->dwDrawStage)
    {
    case CDDS_PREPAINT:
        *plResult = CDRF_NOTIFYITEMDRAW;
        return true;

    case CDDS_ITEMPREPAINT:
        {
            COLORREF cr;

            if ((pCd->uItemState & CDIS_DISABLED) != 0)
            {
                cr = DarkMode::ColorTextSecondary();
            }
            else if ((pCd->uItemState & (CDIS_HOT | CDIS_SELECTED)) != 0)
            {
                /*  Acik temada baglanti fare altinda #0066CC'den #3399FF'e,
                    yani ACILIYOR. Karanlikta da ayni yonu koruyoruz. */
                cr = RGB(0x99, 0xD9, 0xFF);
            }
            else
            {
                cr = DarkMode::ColorLink();
            }

            SetTextColor(pCd->hdc, cr);

            /*  CDRF_NEWFONT: "DC'yi degistirdim, oyle kullan" demektir.
                CDRF_DODEFAULT donulurse denetim DC'yi kendi rengiyle
                yeniden kurar ve degisiklik kaybolur. */
            *plResult = CDRF_NEWFONT;
        }
        return true;

    default:
        break;
    }

    return false;
}


void SetupListView(HWND hList)
{
    if (!DarkMode::IsDark() || hList == NULL)
    {
        return;
    }

    /*  Tema adi "DarkMode_Explorer" OLMALI -- KAYDIRMA CUBUGU yuzunden.

        Olculdu (..\re\tema\klon4-dark-1.png, x=502..527 -> #F0F0F0):
        "DarkMode_ItemsView" listenin govdesini koyulastirir ama kaydirma
        cubugunu HIC etkilemez; koyu pencerenin sag kenarinda bembeyaz bir
        serit kalir. Kaydirma cubugunun karanlik varyantini yalnizca
        "DarkMode_Explorer" tasiyor.

        "DarkMode_Explorer"in tek yan etkisi liste metnini siyah birakmasidir
        -- ama metin ve zemin renklerini asagida LVM_SETTEXTCOLOR /
        LVM_SETBKCOLOR ile zaten acikca veriyoruz, dolayisiyla o yan etki
        hicbir zaman gorunmuyor. */
    DarkMode::ApplyTheme(hList, L"DarkMode_Explorer");

    /*  WS_EX_CLIENTEDGE'i KALDIRIYORUZ.

        Cokuk 3B kenar, tema motorundan degil GetSysColor(COLOR_3DSHADOW /
        COLOR_3DHIGHLIGHT) degerlerinden cizilir; karanlik modda bu renkler
        degismedigi icin listenin cevresinde ACIK GRI bir cerceve kaliyor
        (olculdu, ..\re\tema\zoom-liste-sol-ust.png).

        Sablonda WS_BORDER de var; istemci kenari kalkinca o ince, koyu
        cerceve devreye giriyor ve kutu koyu zeminde dogru duruyor.
        SWP_FRAMECHANGED sart: olmadan stil degisikligi gecerli olmuyor. */
    LONG_PTR exStyle = GetWindowLongPtrW(hList, GWL_EXSTYLE);
    if ((exStyle & WS_EX_CLIENTEDGE) != 0)
    {
        SetWindowLongPtrW(hList, GWL_EXSTYLE, exStyle & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE));
        SetWindowPos(hList, NULL, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    COLORREF crBk   = DarkMode::ColorSurface();
    COLORREF crText = DarkMode::ColorText();

    SendMessageW(hList, LVM_SETBKCOLOR,     0, static_cast<LPARAM>(crBk));
    SendMessageW(hList, LVM_SETTEXTBKCOLOR, 0, static_cast<LPARAM>(crBk));
    SendMessageW(hList, LVM_SETTEXTCOLOR,   0, static_cast<LPARAM>(crText));

    /*  Sutun basligi ayri bir denetimdir ve listeye verilen tema onu
        kapsamaz; koyu bir listenin ustunde bembeyaz kaliyordu (olculdu). */
    {
        HWND hHeader = reinterpret_cast<HWND>(SendMessageW(hList, LVM_GETHEADER, 0, 0));
        if (hHeader != NULL)
        {
            SetupHeader(hHeader);
        }
    }

    /*  Onay kutulari: comctl32'nin urettigi ACIK goruntuleri kendi
        listemizle degistiriyoruz. LVS_EX_CHECKBOXES'in ONCEDEN acilmis
        olmasi gerekir; cagiran diyalog bunu WM_INITDIALOG'da yapiyor. */
    LONG_PTR lExStyle = SendMessageW(hList, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
    if ((lExStyle & LVS_EX_CHECKBOXES) != 0)
    {
        HIMAGELIST himlCur = reinterpret_cast<HIMAGELIST>(
            SendMessageW(hList, LVM_GETIMAGELIST, LVSIL_STATE, 0));

        HIMAGELIST himl = MakeCheckImageList(hList, himlCur, 2);
        if (himl != NULL)
        {
            HIMAGELIST himlOld = reinterpret_cast<HIMAGELIST>(
                SendMessageW(hList, LVM_SETIMAGELIST, LVSIL_STATE,
                             reinterpret_cast<LPARAM>(himl)));

            /*  Eski durum listesi comctl32'ye aitti; artik kimse kullanmiyor. */
            if (himlOld != NULL && himlOld != himl)
            {
                ImageList_Destroy(himlOld);
            }
        }
    }
}


void SetupTreeView(HWND hTree)
{
    if (!DarkMode::IsDark() || hTree == NULL)
    {
        return;
    }

    /*  Kaydirma cubugu ve secim vurgusu icin Gezgin temasi. */
    DarkMode::ApplyTheme(hTree, L"DarkMode_Explorer");

    /*  Renkler ACIKCA verilmeli -- tema adi agacin zeminini degistirmiyor.
        Olculdu: yalnizca SetWindowTheme ile "Gorsel Efektler" listesi
        bembeyaz kaliyordu. */
    SendMessageW(hTree, TVM_SETBKCOLOR,   0, static_cast<LPARAM>(DarkMode::ColorSurface()));
    SendMessageW(hTree, TVM_SETTEXTCOLOR, 0, static_cast<LPARAM>(DarkMode::ColorText()));
    SendMessageW(hTree, TVM_SETLINECOLOR, 0, static_cast<LPARAM>(DarkMode::ColorBorder()));

    /*  Cokuk 3B kenar sistem renklerinden cizilir ve koyu zeminde acik gri
        kalir -- ListView'de oldugu gibi kaldiriyoruz. */
    LONG_PTR exStyle = GetWindowLongPtrW(hTree, GWL_EXSTYLE);
    if ((exStyle & WS_EX_CLIENTEDGE) != 0)
    {
        SetWindowLongPtrW(hTree, GWL_EXSTYLE,
                          exStyle & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE));
        SetWindowPos(hTree, NULL, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    /*  TVS_CHECKBOXES onay kutulari.

        ListView'deki sorunun AYNISI agacta da var ve ayri ele alinmali:
        TVM_SETBKCOLOR zemini koyulastirir ama durum goruntu listesini
        DEGISTIRMEZ. O listeyi comctl32 ACIK temaya gore uretir; sonuc,
        koyu bir agacin uzerinde bembeyaz isaretsiz kutulardir. Olculdu
        (..\..\sysdm\re\tema\perf-0-gorsel.png): isaretsiz kutu #F3F3F3,
        agac zemini #2B2B2B. Isaretli kutu accent mavisi oldugu icin dogru
        gorunuyordu ve kusur yalnizca isaretsizlerde ortaya cikiyordu.

        Not: agacta durum indisleri 1 tabanlidir (INDEXTOSTATEIMAGEMASK),
        yani 0 numarali goruntu kullanilmaz -- comctl32 UC goruntu kurar.
        MakeCheckImageList bunu goruntu sayisindan turettigi icin ayri bir
        kod yoluna gerek yok; liste henuz kurulmadiysa varsayilani 3 veriyoruz. */
    if ((GetWindowLongPtrW(hTree, GWL_STYLE) & TVS_CHECKBOXES) != 0)
    {
        HIMAGELIST himlCur = reinterpret_cast<HIMAGELIST>(
            SendMessageW(hTree, TVM_GETIMAGELIST, TVSIL_STATE, 0));

        HIMAGELIST himl = MakeCheckImageList(hTree, himlCur, 3);
        if (himl != NULL)
        {
            HIMAGELIST himlOld = reinterpret_cast<HIMAGELIST>(
                SendMessageW(hTree, TVM_SETIMAGELIST, TVSIL_STATE,
                             reinterpret_cast<LPARAM>(himl)));

            if (himlOld != NULL && himlOld != himl)
            {
                ImageList_Destroy(himlOld);
            }
        }
    }
    else
    {
        /*  TVS_CHECKBOXES YOKKEN de onay kutusu gorunuyorsa, kutulari
            barindirdigimiz modul kendi NORMAL goruntu listesiyle ciziyordur
            (TVS_CHECKBOXES'tan onceki klasik yontem). sysdm.cpl tam olarak
            bunu yapiyor: olculdu, style=0x50030014 (TVS_CHECKBOXES yok),
            TVSIL_STATE = NULL, TVSIL_NORMAL = dolu.

            O listedeki goruntuleri ACIK temaya gore uretmis; koyu zeminde
            isaretsiz kutu #F3F3F3 kaliyor. Hangi indisin ne anlama geldigini
            VARSAYMIYORUZ -- her goruntuyu okuyup siniflandiriyoruz. */
        RethemeImageListChecks(hTree);
    }

    InvalidateRect(hTree, NULL, TRUE);
}


void SetupHeader(HWND hHeader)
{
    if (!DarkMode::IsDark() || hHeader == NULL)
    {
        return;
    }

    /*  DENENDI VE YETMEDI: DarkMode::ApplyTheme(hHeader,
        L"DarkMode_ItemsView::Header") -- baslik olcumde yine #FFFFFF kaldi.
        Bu yuzden grup kutusu ve sekmede oldugu gibi tamamini kendimiz
        ciziyoruz. */
    SetWindowSubclass(hHeader, HeaderProc, kSubclassHeader, 0);
    InvalidateRect(hHeader, NULL, TRUE);
}


void SetupTabControl(HWND hTab)
{
    if (!DarkMode::IsDark() || hTab == NULL)
    {
        return;
    }

    SetWindowSubclass(hTab, TabProc, kSubclassTab, 0);
    DarkMode::ApplyTheme(hTab, L"DarkMode_DarkTheme");
    InvalidateRect(hTab, NULL, TRUE);
}


void ApplyToSheet(HWND hSheet)
{
    if (!DarkMode::IsDark() || hSheet == NULL)
    {
        return;
    }

    SetWindowSubclass(hSheet, SheetProc, kSubclassSheet, 0);

    DarkMode::ApplyToWindow(hSheet);
    DarkMode::SetTitleBarDark(hSheet, true);

    EnumChildWindows(hSheet, ApplyChildProc, 0);

    InvalidateRect(hSheet, NULL, TRUE);
}


void Refresh(HWND hDlg)
{
    if (hDlg == NULL)
    {
        return;
    }

    /*  Tema degistiyse alt siniflarin kendisi ayni kalir; yalnizca renkler
        yeniden okunur. Karanliktan aciga geciste alt siniflar zararsizdir:
        her cizim yordami DarkMode::IsDark() sorgulayan renk fonksiyonlarini
        kullanir... ancak grup kutusu ve sekme icin ACIK temada varsayilan
        cizim daha dogru oldugundan alt siniflari kaldiriyoruz. */
    if (!DarkMode::IsDark())
    {
        Detach(hDlg);
    }
    else
    {
        Apply(hDlg);
    }

    InvalidateRect(hDlg, NULL, TRUE);
}


namespace {

BOOL CALLBACK DetachChildProc(HWND hWndChild, LPARAM /*lParam*/)
{
    RemoveWindowSubclass(hWndChild, GroupBoxProc,    kSubclassGroupBox);
    RemoveWindowSubclass(hWndChild, TabProc,         kSubclassTab);
    RemoveWindowSubclass(hWndChild, HeaderProc,      kSubclassHeader);
    RemoveWindowSubclass(hWndChild, RadioProc,       kSubclassRadio);
    RemoveWindowSubclass(hWndChild, StaticFrameProc, kSubclassStaticFrame);
    RemoveWindowSubclass(hWndChild, EditProc,        kSubclassEdit);
    DarkMode::ApplyToWindow(hWndChild);
    return TRUE;
}

} /* anonim */


void Detach(HWND hDlg)
{
    if (hDlg == NULL)
    {
        return;
    }
    EnumChildWindows(hDlg, DetachChildProc, 0);
}

} /* namespace DarkDlg */
