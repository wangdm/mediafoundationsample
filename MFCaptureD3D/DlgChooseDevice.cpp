
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

    HWND hVideoList = GetDlgItem(hwnd, IDC_COMBO_VIDEO);

    for (DWORD i = 0; i < pParam->videoCount; i++)
    {
        WCHAR *szFriendlyName = NULL;

        hr = pParam->ppVideoDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &szFriendlyName,
            NULL
            );

        if (FAILED(hr))
        {
            break;
        }

        int index = ComboBox_AddString(hVideoList, szFriendlyName);
        ComboBox_SetItemData(hVideoList, index, i);

        CoTaskMemFree(szFriendlyName);
    }


    HWND hAudioList = GetDlgItem(hwnd, IDC_COMBO_AUDIO);

    for (DWORD i = 0; i < pParam->audioCount; i++)
    {
        WCHAR *szFriendlyName = NULL;

        hr = pParam->ppAudioDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &szFriendlyName,
            NULL
            );

        if (FAILED(hr))
        {
            break;
        }

        int index = ComboBox_AddString(hAudioList, szFriendlyName);
        ComboBox_SetItemData(hAudioList, index, i);

        CoTaskMemFree(szFriendlyName);
    }

    // Assume no selection for now.
    pParam->videoSelection = (UINT32)-1;

    if (pParam->videoCount == 0)
    {
        // If there are no devices, disable the "OK" button.
        EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
    }
}


static HRESULT OnOK(HWND hwnd, ChooseDeviceParam *pParam)
{
    HWND hList = GetDlgItem(hwnd, IDC_COMBO_VIDEO);

    int sel = ComboBox_GetCurSel(hList);

    if (sel != LB_ERR)
    {
        pParam->videoSelection = (UINT32)ListBox_GetItemData(hList, sel);
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

    UINT iVideoDevice = 0;   // Index into the array of devices
    UINT iAudioDevice = 0;   // Index into the array of devices
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

    // Enumerate video devices.
    hr = MFEnumDeviceSources(pAttributes, &param.ppVideoDevices, &param.videoCount);

    if (FAILED(hr)) { goto done; }

    // Ask for source type = audio capture devices.

    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID
        );

    if (FAILED(hr)) { goto done; }

    // Enumerate audio devices.
    hr = MFEnumDeviceSources(pAttributes, &param.ppAudioDevices, &param.audioCount);

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
            iVideoDevice = param.videoSelection;
            iAudioDevice = param.audioSelection;
        }
        else
        {
            bCancel = TRUE; // User cancelled
        }
    }

    if (!bCancel && (param.videoCount > 0))
    {
        // Give this source to the CPlayer object for preview.
        hr = g_pPreview->SetDevice(param.ppVideoDevices[iVideoDevice]);
    }

    if (!bCancel && (param.audioCount > 0))
    {
        // Give this source to the CPlayer object for preview.
        hr = g_pAudio->SetDevice(param.ppAudioDevices[iAudioDevice]);
    }

done:

    SafeRelease(&pAttributes);

    for (DWORD i = 0; i < param.videoCount; i++)
    {
        SafeRelease(&param.ppVideoDevices[i]);
    }
    CoTaskMemFree(param.ppVideoDevices);

    if (FAILED(hr))
    {
        ShowErrorMessage(L"Cannot create a video capture device", hr);
    }
}