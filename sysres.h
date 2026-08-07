/*++

    sysres.h

    Sistemdeki KARSILIK modulun kaynaklarini okur.

    ShadeOS araclarinin ortak sorunu sudur: klonladigimiz programlarin
    metinleri ve diyalog sablonlari kendi govdelerinde degil, dil uydularinda
    durur:

        %SystemRoot%\System32\<dil>\winver.exe.mui
        %SystemRoot%\System32\<dil>\cleanmgr.exe.mui

    Bir klon 38 dilin MUI paketini tasiyamaz. Metni kendimiz cevirseydik
    "birebir ayni" sarti yalnizca iki dilde tutardi. Bunun yerine kaynagi TAM
    OLARAK ORIJINALIN OKUDUGU YERDEN okuyoruz: sistemin kendi dosyasindan.
    Boylece arayuz dili ne olursa olsun hem metinler hem de -- diyalog
    sablonlari da buradan geldigi icin -- DENETIM YERLESIMI orijinalle ayni
    cikar.

    LOAD_LIBRARY_AS_DATAFILE ile aciliyor: hicbir kod calistirilmaz, yalnizca
    kaynak bolumu eslenir. MUI dil cozumlemesi bu bayrakla da yapilir
    (winver klonunda olculdu).

    Sistem dosyasi bulunamazsa her sey aracin KENDI gomulu kaynaklarina duser.

--*/

#ifndef SHADEOS_SYSRES_H_
#define SHADEOS_SYSRES_H_

#include <windows.h>

namespace SysRes
{
    /*  Sistemdeki karsilik modulu kaynak icin acar.

        pszModule       "winver.exe" / "cleanmgr.exe" gibi. System32 icinde
                        aranir; sabit yol YAZILMAZ, GetSystemDirectoryW
                        kullanilir.

        uOwnStringBase  Bu kimlik ve UZERI dizeler DAIMA aracin kendi
                        kaynagindan okunur -- onlar klona ozgudur ve sistem
                        modulunde ayni kimlikte baska bir metin bulunabilir.
                        0 verilirse boyle bir esik yoktur.

        uProbeString /  Acilis dogrulamasi: bu kimlikler sistem modulunde
        uProbeDialog    gercekten var mi? Cok farkli bir surumle karsilasirsak
                        gomulu kaynaklara donebilmek icin. 0 = sinama yok.

        Birden fazla cagrilmasi zararsizdir. */
    bool Initialize(LPCWSTR pszModule, UINT uOwnStringBase,
                    UINT uProbeString, UINT uProbeDialog);

    /*  Sistem modulu gercekten kullanilabildi mi? (Raporlama icin.) */
    bool Available();

    /*  Dize: once sistem, sonra kendi kaynagimiz. */
    int LoadStr(UINT uId, LPWSTR pszBuf, int cchBuf);

    /*  Diyalog sablonunun HAM baytlari (DLGTEMPLATE / DLGTEMPLATEEX).

        Donen isaretci kaynak bolumune aittir; serbest birakilmaz -- Windows
        kaynak sayfalarini modul kapanana kadar tutar. Bulunamazsa NULL. */
    const DLGTEMPLATE *LoadDialogTemplate(UINT uId);

    /*  Ikon: once sistem, sonra kendi kaynagimiz.
        Cagiran DestroyIcon ile birakmali. */
    HICON LoadIcon(UINT uId, int cx, int cy);

    /*  Menuyu SISTEM modulunden yukler.

        Sablon ve ikonla ayni gerekce: menu metinleri de o modulun MUI'sinde
        durur, dolayisiyla her arayuz dilinde dogru gelir ve klonun icine
        tek bir menu metni gomulmez.

        Donen HMENU cagirana aittir; pencereye SetMenu ile verildiginde
        pencere sahiplenir. Bulunamazsa NULL doner. */
    HMENU LoadMenu(UINT uId);

    /*  ILETI TABLOSU (RT_MESSAGETABLE).

        Bazi programlarin DEGISKEN iceren metinleri STRINGTABLE'da degil ileti
        tablosundadir; LoadStringW onlari hic gormez. cleanmgr'da pencere
        basligi, ust bilgi satiri ve tarama metni oradan gelir -- ve
        sablonlardaki yazilar yalnizca tasarim yer tutucusudur.

        Bicimlendirme FormatMessageW ile yapilir; "%2!c!" gibi belirtecler
        yalnizca o API tarafindan anlasilir.

        pArgs, FORMAT_MESSAGE_ARGUMENT_ARRAY dizisidir: dize argumanlari
        LPCWSTR, "%n!c!" argumanlari WCHAR degeri olarak konur.
        Basarisizlikta 0 doner ve tampon bos kalir. */
    int FormatMsg(UINT uId, const DWORD_PTR *pArgs, LPWSTR pszBuf, int cchBuf);

    void Shutdown();
}

#endif /* SHADEOS_SYSRES_H_ */
