
#include "MFCaptureD3D.h"
#include "resource.h"
#include "dialog.h"

/////////////////////////////////////////////////////////////////////

// Dialog functions

static void    OnInitDialog(HWND hwnd, ChooseDeviceParam *pParam);
static HRESULT OnOK(HWND hwnd, ChooseDeviceParam *pParam);


//-------------------------------------------------------------------
//  DlgProc
//
//  Dialog procedure for the "Select Device" dialog.
//-------------------------------------------------------------------

INT_PTR CALLBACK DlgChooseDeviceProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static ChooseDeviceParam *pParam = NULL;

    switch (msg)
    {
    case WM_INITDIALOG:
        pParam = (ChooseDeviceParam*)lParam;
        OnInitDialog(hwnd, pParam);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            OnOK(hwnd, pParam);
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

static void OnInitDialog(HWND hwnd, ChooseDeviceParam *pParam)
{
    HRESULT hr = S_OK;

    // Populate the list with the friendly names of the devices.

    HWND hList = GetDlgItem(hwnd, IDC_DEVICE_LIST);

    for (DWORD i = 0; i < pParam->count; i++)
    {
        WCHAR *szFriendlyName = NULL;

        hr = pParam->ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &szFriendlyName,
            NULL
            );

        if (FAILED(hr))
        {
            break;
        }


        int index = ListBox_AddString(hList, szFriendlyName);

        ListBox_SetItemData(hList, index, i);

        CoTaskMemFree(szFriendlyName);
    }

    // Assume no selection for now.
    pParam->selection = (UINT32)-1;

    if (pParam->count == 0)
    {
        // If there are no devices, disable the "OK" button.
        EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
    }
}


static HRESULT OnOK(HWND hwnd, ChooseDeviceParam *pParam)
{
    HWND hList = GetDlgItem(hwnd, IDC_DEVICE_LIST);

    int sel = ListBox_GetCurSel(hList);

    if (sel != LB_ERR)
    {
        pParam->selection = (UINT32)ListBox_GetItemData(hList, sel);
    }

    return S_OK;
}



//-------------------------------------------------------------------
//  OnChooseDevice
//
//  Select a video capture device.
//
//  hwnd:    A handle to the application window.
/// bPrompt: If TRUE, prompt to user to select the device. Otherwise,
//           select the first device in the list.
//-------------------------------------------------------------------

void OnChooseDevice(HWND hwnd, BOOL bPrompt)
{
    HRESULT hr = S_OK;
    ChooseDeviceParam param = { 0 };

    UINT iDevice = 0;   // Index into the array of devices
    BOOL bCancel = FALSE;

    IMFAttributes *pAttributes = NULL;

    // Initialize an attribute store to specify enumeration parameters.

    hr = MFCreateAttributes(&pAttributes, 1);

    if (FAILED(hr)) { goto done; }

    // Ask for source type = video capture devices.

    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        );

    if (FAILED(hr)) { goto done; }

    // Enumerate devices.
    hr = MFEnumDeviceSources(pAttributes, &param.ppDevices, &param.count);

    if (FAILED(hr)) { goto done; }

    // NOTE: param.count might be zero.

    if (bPrompt)
    {
        // Ask the user to select a device.

        INT_PTR result = DialogBoxParam(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDD_CHOOSE_DEVICE),
            hwnd,
            DlgChooseDeviceProc,
            (LPARAM)&param
            );

        if (result == IDOK)
        {
            iDevice = param.selection;
        }
        else
        {
            bCancel = TRUE; // User cancelled
        }
    }

    if (!bCancel && (param.count > 0))
    {
        // Give this source to the CPlayer object for preview.
        hr = g_pPreview->SetDevice(param.ppDevices[iDevice]);
    }

done:

    SafeRelease(&pAttributes);

    for (DWORD i = 0; i < param.count; i++)
    {
        SafeRelease(&param.ppDevices[i]);
    }
    CoTaskMemFree(param.ppDevices);

    if (FAILED(hr))
    {
        ShowErrorMessage(L"Cannot create a video capture device", hr);
    }
}