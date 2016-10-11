#pragma once

//
// ChooseDeviceParam structure
//
// Holds an array of IMFActivate pointers that represent video
// capture devices.
//

struct ChooseDeviceParam
{
    IMFActivate **ppVideoDevices;    // Array of IMFActivate pointers.
    UINT32      videoCount;          // Number of elements in the array.
    UINT32      videoSelection;      // Selected device, by array index.
    IMFActivate **ppAudioDevices;    // Array of IMFActivate pointers.
    UINT32      audioCount;          // Number of elements in the array.
    UINT32      audioSelection;      // Selected device, by array index.
};

extern CPreview    *g_pPreview;

void ShowErrorMessage(PCWSTR format, HRESULT hrErr);

void OnChooseDevice(HWND hwnd, BOOL bPrompt);
void OnVideoInformation(HWND hwnd, BOOL bPrompt);