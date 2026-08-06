/*++

    strutil.h

    Kucuk, bagimliliksiz dize karsilastirmalari.

    Neden CompareStringOrdinal degil?
      1) O API Vista ile geldi. Statik olarak baglandiginda ikili Windows XP'de
         hic acilmaz -- oysa bu programin calisan kismi XP'de de gecerlidir.
      2) Bu dosyadaki karsilastirmalarin tamami ASCII sabitlerine karsidir
         (pencere sinif adlari, komut satiri anahtarlari). ASCII'ye ozgu harf
         katlamasi hem yeterli hem de daha guvenli.

    Neden lstrcmpiW degil?
      lstrcmpiW YEREL AYARA duyarlidir ve Turkce sistemlerde klasik "noktasiz i"
      tuzagina duser: 'I' ve 'i' Turkce'de ayri harflerdir, dolayisiyla
      lstrcmpiW(L"CLASSIC", L"classic") Turkce yerel ayarda beklenmedik sonuc
      verebilir. Asagidaki katlama yalnizca A-Z araligini isler, yereli hic
      dikkate almaz -- yani her dilde ayni davranir.

--*/

#ifndef SHADEOS_STRUTIL_H_
#define SHADEOS_STRUTIL_H_

#include <windows.h>

/*  A-Z -> a-z. Baska hicbir karaktere dokunmaz. */
__inline WCHAR
ShadeFoldAscii(
    WCHAR ch
    )
{
    if (ch >= L'A' && ch <= L'Z')
    {
        return static_cast<WCHAR>(ch - L'A' + L'a');
    }
    return ch;
}


/*++
    Iki NUL-sonlu dizeyi ASCII buyuk/kucuk harf ayrimi yapmadan karsilastirir.
--*/
__inline bool
ShadeEqualsI(
    LPCWSTR pszA,
    LPCWSTR pszB
    )
{
    if (pszA == NULL || pszB == NULL)
    {
        return pszA == pszB;
    }

    for (;;)
    {
        WCHAR chA = ShadeFoldAscii(*pszA++);
        WCHAR chB = ShadeFoldAscii(*pszB++);

        if (chA != chB)
        {
            return false;
        }
        if (chA == L'\0')
        {
            return true;
        }
    }
}


/*++
    Uzunlugu verilmis (NUL ile bitmeyebilir) bir dizeyi, NUL-sonlu bir dizeyle
    karsilastirir. Komut satiri belirteclerini ayirirken kullanilir.
--*/
__inline bool
ShadeEqualsCountedI(
    LPCWSTR pszA,
    int     cchA,
    LPCWSTR pszB
    )
{
    if (pszA == NULL || pszB == NULL || cchA < 0)
    {
        return false;
    }

    int i = 0;
    for (; i < cchA; ++i)
    {
        WCHAR chB = pszB[i];
        if (chB == L'\0')
        {
            return false;       /* B daha kisa */
        }
        if (ShadeFoldAscii(pszA[i]) != ShadeFoldAscii(chB))
        {
            return false;
        }
    }

    return pszB[i] == L'\0';    /* B de tam burada bitmeli */
}

#endif /* SHADEOS_STRUTIL_H_ */
