#pragma once

//
// ChooseDeviceParam structure
//
// Holds an array of IMFActivate pointers that represent video
// capture devices.
//

struct ChooseDeviceParam
{
    IMFActivate **ppDevices;    // Array of IMFActivate pointers.
    UINT32      count;          // Number of elements in the array.
    UINT32      selection;      // Selected device, by array index.
};

extern CPreview    *g_pPreview;

void ShowErrorMessage(PCWSTR format, HRESULT hrErr);

void OnChooseDevice(HWND hwnd, BOOL bPrompt);
void OnVideoInformation(HWND hwnd, BOOL bPrompt);