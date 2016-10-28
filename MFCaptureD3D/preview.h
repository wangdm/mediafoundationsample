//////////////////////////////////////////////////////////////////////////
//
// preview.h: Manages video preview.
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#pragma once

const UINT WM_APP_PREVIEW_ERROR = WM_APP + 1;    // wparam = HRESULT

class CPreview : public IMFSourceReaderCallback
{
public:
    static HRESULT CreateInstance(
        HWND hVideo, 
        HWND hEvent, 
        CPreview **ppPlayer
    );

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFSourceReaderCallback methods
    STDMETHODIMP OnReadSample(
        HRESULT hrStatus,
        DWORD dwStreamIndex,
        DWORD dwStreamFlags,
        LONGLONG llTimestamp,
        IMFSample *pSample
    );

    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *)
    {
        return S_OK;
    }

    STDMETHODIMP OnFlush(DWORD)
    {
        return S_OK;
    }

    HRESULT       SetDevice(IMFActivate *pActivate);
    HRESULT       InitCodec();
    void          UninitCodec();
	HRESULT       StartYUVRecord();
	HRESULT       StopYUVRecord();
	HRESULT       StartH264Record();
	HRESULT       StopH264Record();
	HRESULT       StartMP4Record();
	HRESULT       StopMP4Record();
    HRESULT       SetVideoAttribute(IMFMediaType *pType);
    VideoAttribute * GetVideoAttribute();
    HRESULT       CloseDevice();
    HRESULT       ResizeVideo(WORD width, WORD height);
    HRESULT       CheckDeviceLost(DEV_BROADCAST_HDR *pHdr, BOOL *pbDeviceLost);

protected:
    
    // Constructor is private. Use static CreateInstance method to create.
    CPreview(HWND hVideo, HWND hEvent);

    // Destructor is private. Caller should call Release.
    virtual ~CPreview();

    HRESULT Initialize();
    void    NotifyError(HRESULT hr) { PostMessage(m_hwndEvent, WM_APP_PREVIEW_ERROR, (WPARAM)hr, 0L); }
    HRESULT TryMediaType(IMFMediaType *pType);

    long                    m_nRefCount;        // Reference count.
    CRITICAL_SECTION        m_critsec;

    HWND                    m_hwndVideo;        // Video window.
    HWND                    m_hwndEvent;        // Application window to receive events. 

    IMFSourceReader         *m_pReader;

    DrawDevice              m_draw;             // Manages the Direct3D device.

    WCHAR                   *m_pwszSymbolicLink;
    UINT32                  m_cchSymbolicLink;

	VideoAttribute          m_videoAttribute;

	AVCodec					*m_codec;
	AVCodecContext          *m_codecContext;
    AVFrame                 *m_srcFrame;
    AVFrame                 *m_dstFrame;

    struct SwsContext       *m_swsContext;

    std::ofstream           *h264file;
    std::ofstream           *yuvfile;

	BOOL					m_bYUVRecordStatus = FALSE;
	BOOL					m_bH264RecordStatus = FALSE;
	BOOL					m_bMP4RecordStatus = FALSE;

	BufferPool				*m_videoPool;
};