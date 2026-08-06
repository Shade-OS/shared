/*++

    sysres.cpp

    Aciklama icin sysres.h'ye bakin.

--*/

#include "sysres.h"

#include <strsafe.h>

#ifndef SHADEOS_PREFER_SYSTEM_RES
#define SHADEOS_PREFER_SYSTEM_RES  1
#endif

namespace {

HMODULE g_hSystem       = NULL;
UINT    g_uOwnBase      = 0;
bool    g_bInitialized  = false;


/*++
    Sistemdeki karsilik modulu yalnizca kaynak icin acar.

    Yol GetSystemDirectoryW ile bulunur; sabit "C:\Windows\System32" YAZILMAZ.
    32-bit bir yapinin 64-bit Windows'ta calismasi durumunda WOW64
    yonlendirmesi bizi SysWOW64'e goturur -- oradaki ikili de ayni kaynaklari
    tasidigi icin sonuc degismez.
--*/
HMODULE OpenSystemModule(LPCWSTR pszModule, UINT uProbeString, UINT uProbeDialog)
{
    WCHAR szPath[MAX_PATH];

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

    /*  AS_DATAFILE: kod calistirilmaz, yalnizca kaynak bolumu eslenir.
        MUI cozumlemesi bu bayrakla da yapilir (olculdu). */
    HMODULE hModule = LoadLibraryExW(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hModule == NULL)
    {
        return NULL;
    }

    /*  Beklenen kaynaklar gercekten var mi? Cok farkli bir surumle
        karsilasirsak gomulu yedeklere donebilelim. */
    bool bOk = true;

    if (uProbeString != 0)
    {
        WCHAR szProbe[64];
        bOk = bOk && (LoadStringW(hModule, uProbeString, szProbe, ARRAYSIZE(szProbe)) > 0);
    }

    if (uProbeDialog != 0)
    {
        bOk = bOk && (FindResourceW(hModule, MAKEINTRESOURCEW(uProbeDialog),
                                    RT_DIALOG) != NULL);
    }

    if (!bOk)
    {
        FreeLibrary(hModule);
        return NULL;
    }

    return hModule;
}

} /* anonim ad alani */


namespace SysRes {

bool Initialize(LPCWSTR pszModule, UINT uOwnStringBase,
                UINT uProbeString, UINT uProbeDialog)
{
    if (g_bInitialized)
    {
        return g_hSystem != NULL;
    }
    g_bInitialized = true;
    g_uOwnBase     = uOwnStringBase;

#if SHADEOS_PREFER_SYSTEM_RES
    if (pszModule != NULL && pszModule[0] != L'\0')
    {
        g_hSystem = OpenSystemModule(pszModule, uProbeString, uProbeDialog);
    }
#else
    UNREFERENCED_PARAMETER(pszModule);
    UNREFERENCED_PARAMETER(uProbeString);
    UNREFERENCED_PARAMETER(uProbeDialog);
#endif

    return g_hSystem != NULL;
}


bool Available(void)
{
    return g_hSystem != NULL;
}


int LoadStr(UINT uId, LPWSTR pszBuf, int cchBuf)
{
    if (pszBuf == NULL || cchBuf <= 0)
    {
        return 0;
    }
    pszBuf[0] = L'\0';

    /*  Esigin uzerindeki kimlikler klona ozgudur; sistem modulunde ayni
        kimlikte baska bir metin bulunabilecegi icin oraya hic bakmiyoruz. */
    bool bAskSystem = (g_hSystem != NULL) &&
                      (g_uOwnBase == 0 || uId < g_uOwnBase);

    if (bAskSystem)
    {
        int n = LoadStringW(g_hSystem, uId, pszBuf, cchBuf);
        if (n > 0)
        {
            return n;
        }
    }

    return LoadStringW(GetModuleHandleW(NULL), uId, pszBuf, cchBuf);
}


const DLGTEMPLATE *LoadDialogTemplate(UINT uId)
{
    if (g_hSystem == NULL)
    {
        return NULL;
    }

    HRSRC hRes = FindResourceW(g_hSystem, MAKEINTRESOURCEW(uId), RT_DIALOG);
    if (hRes == NULL)
    {
        return NULL;
    }

    HGLOBAL hGlob = LoadResource(g_hSystem, hRes);
    if (hGlob == NULL)
    {
        return NULL;
    }

    return static_cast<const DLGTEMPLATE *>(LockResource(hGlob));
}


HICON LoadIcon(UINT uId, int cx, int cy)
{
    HICON hIcon = NULL;

    if (g_hSystem != NULL)
    {
        hIcon = static_cast<HICON>(LoadImageW(g_hSystem, MAKEINTRESOURCEW(uId),
                                              IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
        if (hIcon != NULL)
        {
            return hIcon;
        }
    }

    return static_cast<HICON>(LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(uId),
                                         IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
}


int FormatMsg(UINT uId, const DWORD_PTR *pArgs, LPWSTR pszBuf, int cchBuf)
{
    if (pszBuf == NULL || cchBuf <= 0)
    {
        return 0;
    }
    pszBuf[0] = L'\0';

    if (g_hSystem == NULL)
    {
        /*  Ileti tablosunun gomulu bir yedegi yok; cagiran taraf sablondaki
            metni oldugu gibi birakir. */
        return 0;
    }

    DWORD dwFlags = FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY;

    DWORD cch = FormatMessageW(dwFlags, g_hSystem, uId, 0, pszBuf,
                               static_cast<DWORD>(cchBuf),
                               reinterpret_cast<va_list *>(const_cast<DWORD_PTR *>(pArgs)));
    if (cch == 0)
    {
        pszBuf[0] = L'\0';
        return 0;
    }

    /*  Ileti tablosu girdileri "%0" ile biter; yine de sondaki CR/LF ve
        bosluklari temizliyoruz -- denetim metnine kacan bir satir sonu
        yerlesimi bozar. */
    while (cch > 0 && (pszBuf[cch - 1] == L'\r' || pszBuf[cch - 1] == L'\n' ||
                       pszBuf[cch - 1] == L' '))
    {
        pszBuf[--cch] = L'\0';
    }

    return static_cast<int>(cch);
}


void Shutdown(void)
{
    if (g_hSystem != NULL)
    {
        FreeLibrary(g_hSystem);
        g_hSystem = NULL;
    }
    g_bInitialized = false;
}

} /* namespace SysRes */
