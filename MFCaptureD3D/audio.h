#pragma once

class CAudio : public IMFSourceReaderCallback
{
public:
    CAudio();
    ~CAudio();

    static HRESULT CreateInstance(
        CAudio **ppAudio
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
    HRESULT       CloseDevice();
    HRESULT       InitCodec();
    void          UninitCodec();
    HRESULT       StartPCMRecord();
    HRESULT       StopPCMRecord();
    HRESULT       StartAACRecord();
    HRESULT       StopAACRecord();

protected:

    long                    m_nRefCount;        // Reference count.
    CRITICAL_SECTION        m_critsec;

    IMFSourceReader         *m_pReader;

    AudioAttribute          m_audioAttribute;

    AVCodec					*m_codec;
    AVCodecContext          *m_codecContext;
    AVFrame                 *m_srcFrame;
    AVFrame                 *m_dstFrame;

    std::ofstream           *aacfile;
    std::ofstream           *pcmfile;

    BOOL					m_bAACRecordStatus = FALSE;
    BOOL					m_bPCMRecordStatus = FALSE;
};

