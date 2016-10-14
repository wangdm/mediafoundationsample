//////////////////////////////////////////////////////////////////////////
//
// winmain.cpp : Application entry-point
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#include "MFCaptureD3D.h"
#include "resource.h"
#include "dialog.h"

// Include the v6 common controls in the manifest
#pragma comment(linker, \
    "\"/manifestdependency:type='Win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")


BOOL    InitializeApplication();
BOOL    InitializeWindow(HWND *pHwnd);
void    CleanUp();
INT     MessageLoop(HWND hwnd);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void    ShowErrorMessage(PCWSTR format, HRESULT hr);

// Window message handlers
BOOL    OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
void    OnClose(HWND hwnd);
void    OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
void    OnSize(HWND hwnd, UINT state, int cx, int cy);
void    OnDeviceChange(HWND hwnd, DEV_BROADCAST_HDR *pHdr);

// Command handlers
void    OnChooseDevice(HWND hwnd, BOOL bPrompt);
void    OnVideoInformation(HWND hwnd, BOOL bPrompt);

void    OnStartYUVRecord();
void    OnStopYUVRecord();
void    OnStartH264Record();
void    OnStopH264Record();
void    OnStartMP4Record();
void    OnStopMP4Record();
void    OnStartPCMRecord();
void    OnStopPCMRecord();
void    OnStartAACRecord();
void    OnStopAACRecord();


// Constants 
const WCHAR CLASS_NAME[]  = L"MFCapture Window Class";
const WCHAR WINDOW_NAME[] = L"MFCapture Sample Application";


// Global variables

CPreview    *g_pPreview = NULL;
CAudio      *g_pAudio = NULL;
HDEVNOTIFY  g_hdevnotify = NULL;

BOOL g_YUVRecordStatus = FALSE;
BOOL g_H264RecordStatus = FALSE;
BOOL g_MP4RecordStatus = FALSE;
BOOL g_PCMRecordStatus = FALSE;
BOOL g_AACRecordStatus = FALSE;


//-------------------------------------------------------------------
// WinMain
//
// Application entry-point. 
//-------------------------------------------------------------------

INT WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,INT)
{
    HWND hwnd = 0;

    (void)HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    if (InitializeApplication() && InitializeWindow(&hwnd))
    {
        MessageLoop(hwnd);
    }

    CleanUp();

    return 0;
}


//-------------------------------------------------------------------
//  WindowProc
//
//  Window procedure.
//-------------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_CLOSE,  OnClose);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_SIZE,    OnSize);

    case WM_APP_PREVIEW_ERROR:
        ShowErrorMessage(L"Error", (HRESULT)wParam);
        break;

    case WM_DEVICECHANGE:
        OnDeviceChange(hwnd, (PDEV_BROADCAST_HDR)lParam);
        break;

    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


//-------------------------------------------------------------------
// InitializeApplication
//
// Initializes the application.
//-------------------------------------------------------------------

BOOL InitializeApplication()
{
    HRESULT hr = S_OK;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr))
	{
		hr = MFStartup(MF_VERSION);
	}

	avcodec_register_all();

    return (SUCCEEDED(hr));
}

//-------------------------------------------------------------------
// CleanUp
//
// Releases resources.
//-------------------------------------------------------------------

void CleanUp()
{
    if (g_hdevnotify)
    {
        UnregisterDeviceNotification(g_hdevnotify);
    }

    if (g_pPreview)
    {
        g_pPreview->CloseDevice();
    }

    SafeRelease(&g_pPreview);

    MFShutdown();
    CoUninitialize();
}


//-------------------------------------------------------------------
// InitializeWindow
//
// Creates the application window.
//-------------------------------------------------------------------

BOOL InitializeWindow(HWND *pHwnd)
{
    WNDCLASS wc = {0};

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU1);

    if (!RegisterClass(&wc))
    {
        return FALSE;
    }

    HWND hwnd = CreateWindow(
        CLASS_NAME,
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
        );

    if (!hwnd)
    {
        return FALSE;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    *pHwnd = hwnd;

    return TRUE;
}


//-------------------------------------------------------------------
// MessageLoop 
//
// Implements the window message loop.
//-------------------------------------------------------------------

INT MessageLoop(HWND hwnd)
{
    MSG msg = {0};

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hwnd);

    return INT(msg.wParam);
}


//-------------------------------------------------------------------
// OnCreate
//
// Handles the WM_CREATE message.
//-------------------------------------------------------------------

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT)
{
    HRESULT hr = S_OK;

    // Register this window to get device notification messages.

    DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
    di.dbcc_size = sizeof(di);
    di.dbcc_devicetype  = DBT_DEVTYP_DEVICEINTERFACE;
    di.dbcc_classguid  = KSCATEGORY_CAPTURE; 

    g_hdevnotify = RegisterDeviceNotification(
        hwnd,
        &di,
        DEVICE_NOTIFY_WINDOW_HANDLE
        );

    if (g_hdevnotify == NULL)
    {
        ShowErrorMessage(L"RegisterDeviceNotification failed.", HRESULT_FROM_WIN32(GetLastError()));
        return FALSE;
    }

    // Create the object that manages video preview. 
    hr = CPreview::CreateInstance(hwnd, hwnd, &g_pPreview);
    if (FAILED(hr))
    {
        ShowErrorMessage(L"CPreview::CreateInstance failed.", hr);
        return FALSE;
    }

    // Create the object that manages video preview. 
    hr = CAudio::CreateInstance(&g_pAudio);
    if (FAILED(hr))
    {
        ShowErrorMessage(L"CAudio::CreateInstance failed.", hr);
        return FALSE;
    }

    // Select the first available device (if any).
    OnChooseDevice(hwnd, FALSE);

    return TRUE;
}



//-------------------------------------------------------------------
// OnClose
//
// Handles WM_CLOSE messages.
//-------------------------------------------------------------------

void OnClose(HWND /*hwnd*/)
{
    PostQuitMessage(0);
}



//-------------------------------------------------------------------
// OnSize
//
// Handles WM_SIZE messages.
//-------------------------------------------------------------------

void OnSize(HWND hwnd, UINT /*state */, int cx, int cy)
{
    if (g_pPreview)
    {
        g_pPreview->ResizeVideo((WORD)cx, (WORD)cy);

        InvalidateRect(hwnd, NULL, FALSE);
    }
}


//-------------------------------------------------------------------
// OnCommand 
//
// Handles WM_COMMAND messages
//-------------------------------------------------------------------

void OnCommand(HWND hwnd, int id, HWND /*hwndCtl*/, UINT /*codeNotify*/)
{
    switch (id)
    {
        case ID_FILE_CHOOSEDEVICE:
            OnChooseDevice(hwnd, TRUE);
            break;
        case ID_FILE_VIDEOINFORMATION:
            OnVideoInformation(hwnd, TRUE);
			break;
		case ID_REC_YUV:
			if (g_YUVRecordStatus==TRUE)
			{
				OnStopYUVRecord();
				HMENU hMenu = GetMenu(hwnd);
				ModifyMenuA(hMenu, ID_REC_YUV, MF_BYCOMMAND, ID_REC_YUV, "Start YUV Record");
				g_YUVRecordStatus = FALSE;
			} 
			else
			{
				OnStartYUVRecord();
				HMENU hMenu = GetMenu(hwnd);
				ModifyMenuA(hMenu, ID_REC_YUV, MF_BYCOMMAND, ID_REC_YUV, "Stop YUV Record");
				g_YUVRecordStatus = TRUE;
			}
			break;
		case ID_REC_H264:
			if (g_H264RecordStatus == TRUE)
			{
				OnStopH264Record();
				HMENU hMenu = GetMenu(hwnd);
				ModifyMenuA(hMenu, ID_REC_H264, MF_BYCOMMAND, ID_REC_H264, "Start H264 Record");
				g_H264RecordStatus = FALSE;
			}
			else
			{
				OnStartH264Record();
				HMENU hMenu = GetMenu(hwnd);
				ModifyMenuA(hMenu, ID_REC_H264, MF_BYCOMMAND, ID_REC_H264, "Stop H264 Record");
				g_H264RecordStatus = TRUE;
			}
			break;
		case ID_REC_MP4:
			if (g_MP4RecordStatus = TRUE)
			{
				OnStopMP4Record();
				HMENU hMenu = GetMenu(hwnd);
				ModifyMenuA(hMenu, ID_REC_MP4, MF_BYCOMMAND, ID_REC_MP4, "Start MP4 Record");
				g_MP4RecordStatus = FALSE;
			}
			else
			{
				OnStartMP4Record();
				HMENU hMenu = GetMenu(hwnd);
				ModifyMenuA(hMenu, ID_REC_MP4, MF_BYCOMMAND, ID_REC_MP4, "Stop MP4 Record");
				g_MP4RecordStatus = TRUE;
			}
            break;
        case ID_REC_PCM:
            if (g_PCMRecordStatus == TRUE)
            {
                OnStopPCMRecord();
                HMENU hMenu = GetMenu(hwnd);
                ModifyMenuA(hMenu, ID_REC_PCM, MF_BYCOMMAND, ID_REC_PCM, "Start PCM Record");
                g_PCMRecordStatus = FALSE;
            }
            else
            {
                OnStartPCMRecord();
                HMENU hMenu = GetMenu(hwnd);
                ModifyMenuA(hMenu, ID_REC_PCM, MF_BYCOMMAND, ID_REC_PCM, "Stop PCM Record");
                g_PCMRecordStatus = TRUE;
            }
            break;
        case ID_REC_AAC:
            if (g_AACRecordStatus == TRUE)
            {
                OnStopAACRecord();
                HMENU hMenu = GetMenu(hwnd);
                ModifyMenuA(hMenu, ID_REC_AAC, MF_BYCOMMAND, ID_REC_AAC, "Start AAC Record");
                g_AACRecordStatus = FALSE;
            }
            else
            {
                OnStartAACRecord();
                HMENU hMenu = GetMenu(hwnd);
                ModifyMenuA(hMenu, ID_REC_AAC, MF_BYCOMMAND, ID_REC_AAC, "Stop AAC Record");
                g_AACRecordStatus = TRUE;
            }
            break;
    }
}

//-------------------------------------------------------------------
//  OnDeviceChange
//
//  Handles WM_DEVICECHANGE messages.
//-------------------------------------------------------------------

void OnDeviceChange(HWND hwnd, DEV_BROADCAST_HDR *pHdr)
{
    if (g_pPreview == NULL || pHdr == NULL)
    {
        return;
    }

    HRESULT hr = S_OK;
    BOOL bDeviceLost = FALSE;

    // Check if the current device was lost.

    hr = g_pPreview->CheckDeviceLost(pHdr, &bDeviceLost);

    if (FAILED(hr) || bDeviceLost)
    {
        g_pPreview->CloseDevice();

        MessageBox(hwnd, L"Lost the capture device.", WINDOW_NAME, MB_OK);
    }
}


void ShowErrorMessage(PCWSTR format, HRESULT hrErr)
{
    HRESULT hr = S_OK;
    WCHAR msg[MAX_PATH];

    hr = StringCbPrintf(msg, sizeof(msg), L"%s (hr=0x%X)", format, hrErr);

    if (SUCCEEDED(hr))
    {
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
    }
    else
    {
        DebugBreak();
    }
}

void OnStartYUVRecord() {
	g_pPreview->StartYUVRecord();
}

void OnStopYUVRecord() {
	g_pPreview->StopYUVRecord();
}

void OnStartH264Record() {
	g_pPreview->StartH264Record();
}

void OnStopH264Record() {
	g_pPreview->StopH264Record();
}

void OnStartMP4Record() {
	g_pPreview->StartMP4Record();
}

void OnStopMP4Record() {
	g_pPreview->StopMP4Record();
}

void OnStartPCMRecord() {
    g_pAudio->StartPCMRecord();
}

void OnStopPCMRecord() {
    g_pAudio->StopPCMRecord();
}

void OnStartAACRecord() {
    g_pAudio->StartAACRecord();
}

void OnStopAACRecord() {
    g_pAudio->StopAACRecord();
}