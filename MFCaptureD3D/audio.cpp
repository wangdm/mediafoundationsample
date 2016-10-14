
#include "MFCaptureD3D.h"
#include <shlwapi.h>


CAudio::CAudio() :
    m_nRefCount(1),
    m_pReader(NULL),
    m_codec(NULL),
    m_codecContext(NULL),
    m_srcFrame(NULL),
    m_dstFrame(NULL),
    aacfile(NULL),
    pcmfile(NULL)
{
    InitializeCriticalSection(&m_critsec);
}


CAudio::~CAudio()
{

    DeleteCriticalSection(&m_critsec);
}


//-------------------------------------------------------------------
//  CreateInstance
//
//  Static class method to create the CPreview object.
//-------------------------------------------------------------------

HRESULT CAudio::CreateInstance(
    CAudio **ppAudio // Receives a pointer to the CAudio object.
    )
{

    if (ppAudio == NULL)
    {
        return E_POINTER;
    }

    CAudio *pAudio = new (std::nothrow) CAudio();

    // The CPlayer constructor sets the ref count to 1.

    if (pAudio == NULL)
    {
        return E_OUTOFMEMORY;
    }

    *ppAudio = pAudio;
    (*ppAudio)->AddRef();

    SafeRelease(&pAudio);
    return S_OK;
}


/////////////// IUnknown methods ///////////////

//-------------------------------------------------------------------
//  AddRef
//-------------------------------------------------------------------

ULONG CAudio::AddRef()
{
    return InterlockedIncrement(&m_nRefCount);
}


//-------------------------------------------------------------------
//  Release
//-------------------------------------------------------------------

ULONG CAudio::Release()
{
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    if (uCount == 0)
    {
        delete this;
    }
    // For thread safety, return a temporary variable.
    return uCount;
}



//-------------------------------------------------------------------
//  QueryInterface
//-------------------------------------------------------------------

HRESULT CAudio::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(CAudio, IMFSourceReaderCallback),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}


/////////////// IMFSourceReaderCallback methods ///////////////

//-------------------------------------------------------------------
// OnReadSample
//
// Called when the IMFMediaSource::ReadSample method completes.
//-------------------------------------------------------------------

HRESULT CAudio::OnReadSample(
    HRESULT hrStatus,
    DWORD /* dwStreamIndex */,
    DWORD /* dwStreamFlags */,
    LONGLONG /* llTimestamp */,
    IMFSample *pSample      // Can be NULL
    )
{
    HRESULT hr = S_OK;
    IMFMediaBuffer *pMediaBuffer = NULL;
    BYTE * pBuffer = NULL;

    static int count = 0;

    int ret = 0;

    EnterCriticalSection(&m_critsec);

    if (FAILED(hrStatus))
    {
        hr = hrStatus;
    }

    if (SUCCEEDED(hr))
    {
        if (pSample)
        {

            DWORD bufCount;
            pSample->GetBufferCount(&bufCount);
            // Get the audio sample buffer from the sample.
            hr = pSample->GetBufferByIndex(0, &pMediaBuffer);
            if (SUCCEEDED(hr))
            {
                DWORD bufSize;
                pMediaBuffer->Lock(&pBuffer, NULL, &bufSize);
                //LOG_DEBUG("read audio sample{count=%d, size=%d}\n", bufCount, bufSize);
                if (m_bPCMRecordStatus)
                {
                    pcmfile->write((char *)pBuffer, bufSize);
                }

                pMediaBuffer->Unlock();
            }
        }

    }

    // Request the next frame.
    if (SUCCEEDED(hr))
    {
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            NULL,   // actual
            NULL,   // flags
            NULL,   // timestamp
            NULL    // sample
            );
    }

    SafeRelease(&pMediaBuffer);

    LeaveCriticalSection(&m_critsec);
    return hr;
}


//-------------------------------------------------------------------
//  CloseDevice
//
//  Releases all resources held by this object.
//-------------------------------------------------------------------

HRESULT CAudio::CloseDevice()
{
    EnterCriticalSection(&m_critsec);

    UninitCodec();

    SafeRelease(&m_pReader);

//     CoTaskMemFree(m_pwszSymbolicLink);
//     m_pwszSymbolicLink = NULL;
//     m_cchSymbolicLink = 0;

    LeaveCriticalSection(&m_critsec);
    return S_OK;
}


//-------------------------------------------------------------------
// SetDevice
//
// Set up preview for a specified video capture device. 
//-------------------------------------------------------------------

HRESULT CAudio::SetDevice(IMFActivate *pActivate)
{
    HRESULT hr = S_OK;

    IMFMediaSource  *pSource = NULL;
    IMFAttributes   *pAttributes = NULL;
    IMFMediaType    *pType = NULL;

    EnterCriticalSection(&m_critsec);

    // Release the current device, if any.

    hr = CloseDevice();

    // Create the media source for the device.
    if (SUCCEEDED(hr))
    {
        hr = pActivate->ActivateObject(
            __uuidof(IMFMediaSource),
            (void**)&pSource
            );
    }

    // Get the symbolic link.
//     if (SUCCEEDED(hr))
//     {
//         hr = pActivate->GetAllocatedString(
//             MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
//             &m_pwszSymbolicLink,
//             &m_cchSymbolicLink
//             );
//     }

    IMFPresentationDescriptor* presentationDescriptor;
    hr = pSource->CreatePresentationDescriptor(&presentationDescriptor);
    if (SUCCEEDED(hr))
    {
        DWORD dwCount = 0;
        presentationDescriptor->GetStreamDescriptorCount(&dwCount);
        for (DWORD i = 0; i < dwCount; i++)
        {
            BOOL bSelect;
            IMFStreamDescriptor *pStreamDescriptor = NULL;
            hr = presentationDescriptor->GetStreamDescriptorByIndex(i, &bSelect, &pStreamDescriptor);
            if (SUCCEEDED(hr) && bSelect == TRUE)
            {
                IMFMediaTypeHandler *pMediaTypeHandler = NULL;
                hr = pStreamDescriptor->GetMediaTypeHandler(&pMediaTypeHandler);
                if (!SUCCEEDED(hr))
                {
                    SafeRelease(&pStreamDescriptor);
                }
                UINT32 maxFactor = 0;
                DWORD dwMediaTypeCount = 0;
                hr = pMediaTypeHandler->GetMediaTypeCount(&dwMediaTypeCount);
                for (DWORD j = 0; j < dwMediaTypeCount; j++)
                {
                    IMFMediaType * pMediaType = NULL;
                    hr = pMediaTypeHandler->GetMediaTypeByIndex(j, &pMediaType);
                    if (SUCCEEDED(hr))
                    {
                        UINT32 nChannels, nSamplesPerSec, nAvgBytesPerSec, nBlockAlign, wBitsPerSample, wSamplesPerBlock, uBitSize;
                        pMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nChannels);
                        pMediaType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &nAvgBytesPerSec);
                        pMediaType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &nBlockAlign);
                        pMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &wBitsPerSample);
                        pMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &nSamplesPerSec);
                        pMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_BLOCK, &wSamplesPerBlock);
                        pMediaType->GetUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, &uBitSize);

                    }
                    SafeRelease(&pMediaType);
                }
                SafeRelease(&pMediaTypeHandler);
            }
            SafeRelease(&pStreamDescriptor);
        }
    }
    SafeRelease(&presentationDescriptor);

    //
    // Create the source reader.
    //

    // Create an attribute store to hold initialization settings.

    if (SUCCEEDED(hr))
    {
        hr = MFCreateAttributes(&pAttributes, 2);
    }
    if (SUCCEEDED(hr))
    {
        hr = pAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
    }

    // Set the callback pointer.
    if (SUCCEEDED(hr))
    {
        hr = pAttributes->SetUnknown(
            MF_SOURCE_READER_ASYNC_CALLBACK,
            this
            );
    }

    if (SUCCEEDED(hr))
    {
        hr = MFCreateSourceReaderFromMediaSource(
            pSource,
            pAttributes,
            &m_pReader
            );
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pReader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            &pType
            );

        UINT32 nChannels, nSamplesPerSec, nAvgBytesPerSec, nBlockAlign, wBitsPerSample, wSamplesPerBlock, uBitSize;
        pType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nChannels);
        pType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &nAvgBytesPerSec);
        pType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &nBlockAlign);
        pType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &wBitsPerSample);
        pType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &nSamplesPerSec);
        pType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_BLOCK, &wSamplesPerBlock);
        pType->GetUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, &uBitSize);

        m_audioAttribute.m_uChannel = nChannels;
        m_audioAttribute.m_uSampleBit = wBitsPerSample;
        m_audioAttribute.m_uSampleRate = nSamplesPerSec;
    }

    InitCodec();

    if (SUCCEEDED(hr))
    {
        // Ask for the first sample.
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            NULL,
            NULL,
            NULL,
            NULL
            );
    }

    if (FAILED(hr))
    {
        if (pSource)
        {
            pSource->Shutdown();

            // NOTE: The source reader shuts down the media source
            // by default, but we might not have gotten that far.
        }
        CloseDevice();
    }

    SafeRelease(&pSource);
    SafeRelease(&pAttributes);
    SafeRelease(&pType);

    LeaveCriticalSection(&m_critsec);
    return hr;
}

static int check_sample_fmt(AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

static int check_sample_rate(AVCodec *codec, int sample_rate)
{
    const int *p = codec->supported_samplerates;

    while (*p != 0) {
        if (*p == sample_rate)
            return 1;
        p++;
    }
    return 0;
}

HRESULT CAudio::InitCodec() {

    HRESULT hr = S_OK;

    //m_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    m_codec = avcodec_find_encoder_by_name("libfdk_aac");
    if (m_codec == NULL)
    {
        return !S_OK;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);

    m_codecContext->channels = m_audioAttribute.m_uChannel;
    m_codecContext->sample_rate = m_audioAttribute.m_uSampleRate;
    m_codecContext->sample_fmt = AV_SAMPLE_FMT_FLT;
    m_codecContext->channel_layout = av_get_default_channel_layout(m_codecContext->channels);

    m_codecContext->bit_rate = 64000;

    if (!check_sample_fmt(m_codec, m_codecContext->sample_fmt)) {
        LOG_ERR("Encoder does not support sample format %s",
            av_get_sample_fmt_name(m_codecContext->sample_fmt));
    }

    if (!check_sample_rate(m_codec, m_codecContext->sample_rate)) {
        LOG_ERR("Encoder does not support sample rate %d",
            m_codecContext->sample_rate);
    }

    int ret = avcodec_open2(m_codecContext, m_codec, NULL);
    if (ret < 0) {
        char strerr[100];
        av_strerror(ret, strerr, 100);
        LOG_ERR("open codex failed with %s!", strerr);
    }

    int sample_size = av_samples_get_buffer_size(NULL, m_codecContext->channels, m_codecContext->frame_size, m_codecContext->sample_fmt, 0);

    m_dstFrame = av_frame_alloc();
    if (m_dstFrame) {
    }

    m_srcFrame = av_frame_alloc();
    if (m_srcFrame) {
    }

    return hr;
}


void CAudio::UninitCodec() {

    if (m_dstFrame)
    {
        if (m_dstFrame->data[0])
        {
            av_freep(&m_dstFrame->data[0]);
        }
        av_frame_free(&m_dstFrame);
    }
    if (m_codecContext)
    {
        avcodec_close(m_codecContext);
        av_free(m_codecContext);
        m_codecContext = NULL;
    }

    if (aacfile)
    {
        aacfile->flush();
        aacfile->close();
        delete aacfile;
        aacfile = NULL;
    }

    if (pcmfile)
    {
        pcmfile->flush();
        pcmfile->close();
        delete pcmfile;
        pcmfile = NULL;
    }
}


HRESULT CAudio::StartPCMRecord() {
    LOG_INFO("PCM Record Starting...\n");

    pcmfile = new std::ofstream("audio.pcm", std::ios::binary);

    m_bPCMRecordStatus = TRUE;

    return S_OK;
}

HRESULT CAudio::StopPCMRecord() {

    LOG_INFO("PCM Record Stopping...\n");

    m_bPCMRecordStatus = FALSE;

    if (pcmfile)
    {
        pcmfile->flush();
        pcmfile->close();
        delete pcmfile;
        pcmfile = NULL;
    }

    return S_OK;
}

HRESULT CAudio::StartAACRecord() {

    LOG_INFO("AAC Record Starting...\n");

    aacfile = new std::ofstream("audio.aac", std::ios::binary);

    m_bAACRecordStatus = TRUE;

    return S_OK;
}

HRESULT CAudio::StopAACRecord() {

    LOG_INFO("AAC Record Stopping...\n");

    m_bAACRecordStatus = FALSE;

    if (aacfile)
    {
        aacfile->flush();
        aacfile->close();
        delete aacfile;
        aacfile = NULL;
    }

    return S_OK;
}