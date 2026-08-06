/*++

    darkdlg.h

    Diyalog denetimleri icin karanlik mod boyama katmani.

    darkmode.cpp uygulama GENELINDEKI karanlik modu acar (uxtheme ordinalleri,
    baslik cubugu, palet). Ama bu, denetimlerin dogru gorunmesine yetmez:
    Windows'un tema motoru karanlik varyanti yalnizca bir kismi icin tasir.
    Olculdu (bkz. ..\re\tema\):

        dogru gelen        : push dugme, kaydirma cubugu, metin kutusu,
                             acilir kutu -- SetWindowTheme ile
        YANLIS gelen       : BS_GROUPBOX cercevesi ve basligi,
                             ListView'in onay kutulari (durum goruntu listesi),
                             ilerleme cubugunun oluk rengi,
                             sekme denetiminin govdesi,
                             SS_ICON statiklerinin zemini

    Bu modul yanlis gelenleri kendisi cizer. Kural sudur:

        ACIK TEMADA HICBIR SEYE DOKUNULMAZ.

    Yani /light ve /classic kipinde bu modulun tamami devre disidir ve pencere
    orijinalle birebir ayni cizilir. Karanlik mod bir EKLENTIDIR, orijinalin
    yerine gecen bir yeniden yazim degil.

--*/

#ifndef SHADEOS_DARKDLG_H_
#define SHADEOS_DARKDLG_H_

#include <windows.h>
#include <commctrl.h>

namespace DarkDlg
{
    /*  Diyaloga ve tum alt denetimlerine karanlik temayi uygular; boyanmasi
        gerekenleri alt siniflar (subclass). Acik temada hicbir sey yapmaz.

        Diyalogun WM_INITDIALOG'unda, denetimler olustuktan SONRA cagrilmali. */
    void Apply(HWND hDlg);

    /*  WM_CTLCOLOR* ailesinin tamami. Ilgilenmiyorsa 0 doner; o zaman cagiran
        taraf FALSE dondurup varsayilan islemeye birakmalidir.

        Donen deger dogrudan diyalog yordamindan SetWindowLongPtr(DWLP_MSGRESULT)
        ile dondurulecek HBRUSH'tir. */
    INT_PTR OnCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam);

    /*  Grup kutularinin ve sekmelerin NM_CUSTOMDRAW bildirimlerini karsilar.
        Ilgilendiyse true doner ve *plResult'i doldurur. */
    bool OnNotify(HWND hDlg, LPARAM lParam, LRESULT *plResult);

    /*  ListView'i karanlik zemine hazirlar: renkler, kaydirma cubugu temasi ve
        onay kutulari icin ozel durum goruntu listesi.
        Goruntu listesinin sahipligi ListView'e gecer (LVS_EX_CHECKBOXES ile
        olusan varsayilani degistirir). */
    void SetupListView(HWND hList);

    /*  SysTreeView32.

        Tema adi agacin ZEMININI degistirmez; ListView'de oldugu gibi renkler
        acikca verilmelidir (TVM_SETBKCOLOR / TVM_SETTEXTCOLOR). Olculdu:
        yalnizca SetWindowTheme uygulandiginda "Gorsel Efektler" listesi
        bembeyaz kaliyordu (..\..\sysdm\re\tema\alt-performans.png). */
    void SetupTreeView(HWND hTree);

    /*  SysHeader32 -- ListView'in sutun basligi.

        ListView'e uygulanan tema basligi KAPSAMAZ; ayri ele alinmali, yoksa
        koyu bir listenin ustunde bembeyaz bir baslik satiri kalir (olculdu:
        Ortam Degiskenleri ve Kullanici Profilleri kutulari). */
    void SetupHeader(HWND hHeader);

    /*  PropertySheet'in sekme denetimi. Sekme govdesinin zemini ve sekme
        basliklarinin metni tema tarafindan karanliga cevrilmiyor. */
    void SetupTabControl(HWND hTab);

    /*  PropertySheet CERCEVESI.

        Cerceve, comctl32'nin KENDI diyalogudur; bizim sayfa yordamlarimiz onun
        iletilerini hic gormez. Bu yuzden sayfalar koyu olsa bile cercevenin
        zemini acik gri (#F0F0F0) kalir ve sekme seridinin ustunde/yaninda
        beyaz bir bant olarak gorunur -- olculdu (bkz. ..\re\tema\zoom-sekme-serit.png).

        Duzeltme icin cerceveyi alt siniflayip WM_ERASEBKGND ve WM_CTLCOLOR*
        ailesini devraliyoruz. PSCB_INITIALIZED geri cagrisindan cagrilmali. */
    void ApplyToSheet(HWND hSheet);

    /*  Tema degistiginde (WM_SETTINGCHANGE) yeniden uygulanir. */
    void Refresh(HWND hDlg);

    /*  Alt siniflari kaldirir. Diyalog kapanirken cagrilmasi sart degildir --
        alt sinif pencere ile birlikte olur -- ama duzenli olmak iyidir. */
    void Detach(HWND hDlg);
}

#endif /* SHADEOS_DARKDLG_H_ */
