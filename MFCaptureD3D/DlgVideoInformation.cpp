
#include "MFCaptureD3D.h"
#include "resource.h"
#include "dialog.h"

/////////////////////////////////////////////////////////////////////

// Dialog functions

static void    OnInitDialog(HWND hwnd);


//-------------------------------------------------------------------
//  DlgProc
//
//  Dialog procedure for the "Select Device" dialog.
//-------------------------------------------------------------------

INT_PTR CALLBACK DlgInformationProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    switch (msg)
    {
    case WM_INITDIALOG:
        OnInitDialog(hwnd);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            EndDialog(hwnd, LOWORD(wParam));
            return TRUE;

        case IDCANCEL:
            EndDialog(hwnd, LOWORD(wParam));
            return TRUE;
        }
        break;
    }

    return FALSE;
}



//-------------------------------------------------------------------
//  OnInitDialog
//
//  Handles the WM_INITDIALOG message.
//-------------------------------------------------------------------

static void OnInitDialog(HWND hwnd)
{
    HRESULT hr = S_OK;

    // Populate the list with the friendly names of the devices.

    HWND hStatic1 = GetDlgItem(hwnd, IDC_COLORSPACE);

    HWND hStatic2 = GetDlgItem(hwnd, IDC_FORMAT);

    HWND hStatic3 = GetDlgItem(hwnd, IDC_INTERLACE);

    VideoAttribute * attr = g_pPreview->GetVideoAttribute();

    char str1[5] = { 0 };
    str1[0] = ((char *)(&attr->m_dwFormat))[0];
    str1[1] = ((char *)(&attr->m_dwFormat))[1];
    str1[2] = ((char *)(&attr->m_dwFormat))[2];
    str1[3] = ((char *)(&attr->m_dwFormat))[3];
    SetWindowTextA(hStatic1, str1);

    char str2[20];
    snprintf(str2,20,"%dx%dp%d", attr->m_uWidth,attr->m_uHeight,attr->m_uFps);
    SetWindowTextA(hStatic2, str2);

    char * str3;
    if (attr->m_bInterlace == MFVideoInterlace_Progressive)
    {
        str3 = "Progressive";
    } 
    else
    {
        str3 = "Interlace";
    }
    SetWindowTextA(hStatic3, str3);
}


//-------------------------------------------------------------------
//  OnVideoInformation
//
//  Show video information.
//
//  hwnd:    A handle to the application window.
/// bPrompt: If TRUE, prompt to user to select the device. Otherwise,
//           select the first device in the list.
//-------------------------------------------------------------------

void OnVideoInformation(HWND hwnd, BOOL bPrompt)
{
    ChooseDeviceParam param = { 0 };

    // NOTE: param.count might be zero.

    if (bPrompt)
    {

        INT_PTR result = DialogBoxParam(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDD_VIDEOINFORMATION),
            hwnd,
            DlgInformationProc,
            (LPARAM)&param
            );
    }

}
