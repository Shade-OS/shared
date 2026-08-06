/*++

    hostui.h

    SISTEMIN KENDI ARAYUZUNU kendi surecimizde barindirip karanliga boyar.

    Neden bu var?
    -------------
    ShadeOS'ta iki farkli klonlama durumu cikti:

      1. Motor DISARIDA (cleanmgr). Dosyalari kayit defterindeki COM
         isleyicileri siliyordu; ayni API'yi cagirinca davranis bedavaya
         birebir geldi. Orada pencereyi kendimiz kurduk.

      2. Motor ICERIDE (winver'in ShellAboutW'si, sysdm.cpl'nin Sistem
         Ozellikleri sayfalari). Mantik o modulun kendi govdesinde; disaridan
         cagirabilecegimiz bir arayuz yok. Sayfalari yeniden yazmak, her
         ayari (ortam degiskenleri, sanal bellek, DEP, gorsel efektler...)
         bastan uygulamak demekti -- ve her Windows surumunde yeniden
         dogrulamak gerekirdi.

    Ikinci durumda cok daha iyi bir yol var: o modulu KENDI SURECIMIZE
    yukleyip disa aktardigi giris noktasini cagirmak. Pencere bizim
    surecimizde, bizim is parcacigimizda olusur -- dolayisiyla uygulama
    genelinde actigimiz karanlik mod ve asagidaki kanca dogrudan uzerinde
    calisir.

    Sonuc: davranis %100 orijinaldir (zaten ORIJINAL KODUN kendisi calisir),
    ustune karanlik mod gelir.

    Nasil calisir
    -------------
    Bu giris noktalari modaldir: cagirildiklari anda kendi ileti dongulerini
    calistirir ve pencere kapanana kadar donmezler. "Once cagir, sonra
    pencereyi bul" ise yaramaz. Cozum, cagridan HEMEN ONCE kendi is
    parcacigimiza bir WH_CALLWNDPROCRET kancasi kurmak: her iletisim kutusu
    olustugunda WM_INITDIALOG bizim uzerimizden geçer ve tam o anda tema
    uygulanir.

    WH_CALLWNDPROCRET, ileti pencere yordami tarafindan ISLENDIKTEN SONRA
    cagrilir; WM_INITDIALOG icin bu onemlidir, cunku o noktada tum alt
    denetimler olusmus ve yerlesmistir.

    Kanca YALNIZCA kendi is parcacigimiza kurulur (dwThreadId = kendi
    kimligimiz). Baska hicbir prosese hicbir sey enjekte edilmez.

--*/

#ifndef SHADEOS_HOSTUI_H_
#define SHADEOS_HOSTUI_H_

#include <windows.h>

namespace HostUI
{
    /*  Kancayi kurar. Karanlik mod kapaliysa hicbir sey yapmaz ve true
        doner -- o zaman barindirilan arayuz orijinaliyle birebir cizilir. */
    bool Install();

    /*  Kancayi kaldirir. Proses sonunda cagrilmasi sart degildir. */
    void Remove();

    /*  Sistemdeki bir modulu kaynak icin DEGIL, CALISTIRMAK icin yukler ve
        disa aktardigi giris noktasini cagirir:

            void WINAPI Entry(LPCWSTR pszArg)

        DIKKAT -- BU "rundll32 BICIMI" DEGILDIR. Ilk surumde oyle sanilmis ve

            void CALLBACK Entry(HWND, HINSTANCE, LPWSTR, int)

        imzasiyla cagrilmisti. sysdm.cpl!DisplaySYSDMCPL disassemble edildi
        (dumpbin /disasm, RVA 0x18150) ve gercek sozlesme olculdu:

            mov  qword ptr [rsp+8],rbx
            push rdi
            sub  rsp,20h
            xor  r9d,r9d      <- 4. arguman OKUNMADAN siliniyor
            mov  rbx,rcx      <- TEK okunan parametre
            xor  r8d,r8d      <- 3. arguman OKUNMADAN siliniyor

        Yani fonksiyon tek parametrelidir ve onu RCX'ten okur. rundll32
        imzasiyla cagirildiginda sayfa dizesi R8'e konuyordu ve fonksiyonun
        ucuncu komutunda siliniyordu; RCX ise NULL kaliyordu. Sonuc: sayfa
        argumani ne verilirse verilsin her zaman ilk sekme aciliyordu.

        Fazladan arguman GECMEK zararsizdir (x64'te temizligi cagiran yapar);
        hata argumanin YANLIS REGISTER'A konmasiydi.

        Modul bulunamaz ya da giris noktasi yoksa false doner; cagiran taraf
        kullaniciya anlamli bir sey soyleyebilir. */
    bool RunModuleEntry(LPCWSTR pszModule, LPCSTR pszEntry, LPCWSTR pszArg);


    /*  Bir Denetim Masasi ogesini (.cpl) KENDI SURECIMIZDE acar.

        sysdm.cpl bir istisnadir: kendine ozel DisplaySYSDMCPL disa aktarimi
        vardir. Geri kalan klasik ogeler ORTAK bir sozlesme kullanir --
        System32'deki 18 .cpl dosyasi tarandi, 14'u "CPlApplet" disa
        aktariyor (sysdm.cpl, appwiz.cpl, hdwwiz.cpl ve wscui.cpl aktarmiyor).

        Sozlesme (cpl.h):

            LONG CALLBACK CPlApplet(HWND, UINT uMsg, LPARAM lp1, LPARAM lp2)

        ve su sirayla surulur: CPL_INIT -> CPL_GETCOUNT -> CPL_INQUIRE ->
        CPL_DBLCLK (arayuzu ACAN ve pencere kapanana kadar donmeyen cagri)
        -> CPL_STOP -> CPL_EXIT.

        Boylece her klasik oge YENI BIR PROJE degil, bir satir olur.

        iApplet: Bir .cpl birden fazla oge barindirabilir (ornegin main.cpl
                 hem Fare hem Klavye'yi tasir). Sifir tabanli indistir.
        pszParams: Varsa CPL_STARTWPARMSW ile gecirilir; oge onu islemezse
                 CPL_DBLCLK'e dusulur. NULL/bos gecilebilir.  */
    bool RunControlPanelApplet(LPCWSTR pszModule, int iApplet, LPCWSTR pszParams);


    /*  Bir .cpl icindeki ogeleri sayar ve adlarini okur; "hangi indis?"
        sorusunu tahminle degil olcumle cevaplamak icin.

        pfnYaz'a her oge icin (indis, ad, aciklama) verilir. Modul acilamazsa
        ya da CPlApplet yoksa false doner. */
    typedef void (*PFN_AppletYaz)(int iApplet, LPCWSTR pszAd, LPCWSTR pszAciklama, void *pvUser);

    bool ListControlPanelApplets(LPCWSTR pszModule, PFN_AppletYaz pfnYaz, void *pvUser);
}

#endif /* SHADEOS_HOSTUI_H_ */
