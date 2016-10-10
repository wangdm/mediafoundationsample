//////////////////////////////////////////////////////////////////////////
//
// preview.cpp: Manages video preview.
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
#include "BufferLock.h"
#include <shlwapi.h>
#include "VideoAttribute.h"

//-------------------------------------------------------------------
//  CreateInstance
//
//  Static class method to create the CPreview object.
//-------------------------------------------------------------------

HRESULT CPreview::CreateInstance(
    HWND hVideo,        // Handle to the video window.
    HWND hEvent,        // Handle to the window to receive notifications.
    CPreview **ppPlayer // Receives a pointer to the CPreview object.
    )
{
    assert(hVideo != NULL);
    assert(hEvent != NULL);

    if (ppPlayer == NULL)
    {
        return E_POINTER;
    }

    CPreview *pPlayer = new (std::nothrow) CPreview(hVideo, hEvent);

    // The CPlayer constructor sets the ref count to 1.

    if (pPlayer == NULL)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = pPlayer->Initialize();

    if (SUCCEEDED(hr))
    {
        *ppPlayer = pPlayer;
        (*ppPlayer)->AddRef();
    }

    SafeRelease(&pPlayer);
    return hr;
}


//-------------------------------------------------------------------
//  constructor
//-------------------------------------------------------------------

CPreview::CPreview(HWND hVideo, HWND hEvent) :
    m_pReader(NULL),
    m_hwndVideo(hVideo),
    m_hwndEvent(hEvent),
    m_nRefCount(1),
    m_pwszSymbolicLink(NULL),
    m_cchSymbolicLink(0),
    m_codec(NULL),
    m_codecContext(NULL),
    m_srcFrame(NULL),
    m_dstFrame(NULL),
    h264file(NULL),
    yuvfile(NULL)
{
    InitializeCriticalSection(&m_critsec);
}

//-------------------------------------------------------------------
//  destructor
//-------------------------------------------------------------------

CPreview::~CPreview()
{
    CloseDevice();

    m_draw.DestroyDevice();

    DeleteCriticalSection(&m_critsec);
}


//-------------------------------------------------------------------
//  Initialize
//
//  Initializes the object.
//-------------------------------------------------------------------

HRESULT CPreview::Initialize()
{
    HRESULT hr = S_OK;

    hr = m_draw.CreateDevice(m_hwndVideo);

    return hr;
}


//-------------------------------------------------------------------
//  CloseDevice
//
//  Releases all resources held by this object.
//-------------------------------------------------------------------

HRESULT CPreview::CloseDevice()
{
    EnterCriticalSection(&m_critsec);

    SafeRelease(&m_pReader);

    CoTaskMemFree(m_pwszSymbolicLink);
    m_pwszSymbolicLink = NULL;
    m_cchSymbolicLink = 0;

    UninitCodec();

    LeaveCriticalSection(&m_critsec);
    return S_OK;
}


/////////////// IUnknown methods ///////////////

//-------------------------------------------------------------------
//  AddRef
//-------------------------------------------------------------------

ULONG CPreview::AddRef()
{
    return InterlockedIncrement(&m_nRefCount);
}


//-------------------------------------------------------------------
//  Release
//-------------------------------------------------------------------

ULONG CPreview::Release()
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

HRESULT CPreview::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(CPreview, IMFSourceReaderCallback),
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

HRESULT CPreview::OnReadSample(
    HRESULT hrStatus,
    DWORD /* dwStreamIndex */,
    DWORD /* dwStreamFlags */,
    LONGLONG /* llTimestamp */,
    IMFSample *pSample      // Can be NULL
    )
{
    HRESULT hr = S_OK;
    IMFMediaBuffer *pBuffer = NULL;

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
            // Get the video frame buffer from the sample.

            hr = pSample->GetBufferByIndex(0, &pBuffer);

            // Draw the frame.

            if (SUCCEEDED(hr))
            {
                BYTE * pbScanline0;
                LONG lStride;

                VideoBufferLock buffer(pBuffer);    // Helper object to lock the video buffer.

                // Lock the video buffer. This method returns a pointer to the first scan
                // line in the image, and the stride in bytes.
                hr = buffer.LockBuffer(m_videoAttribute.m_uStride, m_videoAttribute.m_uHeight, &pbScanline0, &lStride);

                if (SUCCEEDED(hr)) {
                    hr = m_draw.DrawFrame(pbScanline0, lStride);

                    av_image_fill_pointers(m_srcFrame->data, (AVPixelFormat)m_srcFrame->format, m_srcFrame->height, pbScanline0, m_srcFrame->linesize);

                    ret = sws_scale(m_swsContext, m_srcFrame->data, m_srcFrame->linesize, 0, m_srcFrame->height, m_dstFrame->data, m_dstFrame->linesize);


                    if (m_bYUVRecordStatus == TRUE)
                    {
                        int len = av_image_get_buffer_size((AVPixelFormat)m_dstFrame->format, m_dstFrame->width, m_dstFrame->height, 32);
                        yuvfile->write((char *)m_dstFrame->data[0], len);
                        count++;
                        LOG_INFO("write %d byte data\n",len);
                    }

					if (m_bH264RecordStatus == TRUE)
					{
						static BOOL first = TRUE;
						AVPacket pkt;
						av_init_packet(&pkt);
						pkt.data = NULL;    // packet data will be allocated by the encoder
						pkt.size = 0;
						int got_frame;
						ret = avcodec_encode_video2(m_codecContext, &pkt, m_dstFrame, &got_frame);
						if (ret != 0)
						{
							LOG_ERR("avcodec_encode_video2 error with %d !\n", ret);
						}

						if (got_frame)
						{
							if (first == TRUE)
							{
								if ((pkt.flags & AV_PKT_FLAG_KEY)) {
									first = FALSE;
									LOG_INFO("get first key frame\n");
									h264file->write((char *)pkt.data, pkt.size);
								}
							}
							else {
								h264file->write((char *)pkt.data, pkt.size);
							}

							LOG_DEBUG("pkt.pts=%lld pkt.dts=%lld pkt.size=%d !\n", pkt.pts, pkt.dts, pkt.size);
							av_packet_unref(&pkt);
						}

						m_dstFrame->pts++;
					}
                }

            }
        }

    }

    // Request the next frame.
    if (SUCCEEDED(hr))
    {
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            NULL,   // actual
            NULL,   // flags
            NULL,   // timestamp
            NULL    // sample
            );
    }

    if (FAILED(hr))
    {
        NotifyError(hr);
    }
    SafeRelease(&pBuffer);

    LeaveCriticalSection(&m_critsec);
    return hr;
}


//-------------------------------------------------------------------
// TryMediaType
//
// Test a proposed video format.
//-------------------------------------------------------------------

HRESULT CPreview::TryMediaType(IMFMediaType *pType)
{
    HRESULT hr = S_OK;

    BOOL bFound = FALSE;
    GUID subtype = { 0 };

    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

    if (FAILED(hr))
    {
        return hr;
    }

    // Do we support this type directly?
    if (m_draw.IsFormatSupported(subtype))
    {
        bFound = TRUE;
    }
    else
    {
        // Can we decode this media type to one of our supported
        // output formats?

        for (DWORD i = 0; ; i++)
        {
            // Get the i'th format.
            m_draw.GetFormat(i, &subtype);

            hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);

            if (FAILED(hr)) { break; }

            // Try to set this type on the source reader.
            hr = m_pReader->SetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                NULL,
                pType
                );

            if (SUCCEEDED(hr))
            {
                bFound = TRUE;
                break;
            }
        }
    }

    if (bFound)
    {

        hr = SetVideoAttribute(pType);
        hr = m_draw.SetVideoType(pType);
    }

    return hr;
}



//-------------------------------------------------------------------
// SetDevice
//
// Set up preview for a specified video capture device. 
//-------------------------------------------------------------------

HRESULT CPreview::SetDevice(IMFActivate *pActivate)
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
    if (SUCCEEDED(hr))
    {
        hr = pActivate->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &m_pwszSymbolicLink,
            &m_cchSymbolicLink
            );
    }

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
                        UINT32 uWidth, uHeight, uNummerator, uDenominator;
                        char formatName[5] = { 0 };
                        GUID subType;
                        pMediaType->GetGUID(MF_MT_SUBTYPE, &subType);
                        formatName[0] = ((char *)(&subType.Data1))[0];
                        formatName[1] = ((char *)(&subType.Data1))[1];
                        formatName[2] = ((char *)(&subType.Data1))[2];
                        formatName[3] = ((char *)(&subType.Data1))[3];
                        MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &uWidth, &uHeight);
                        MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &uNummerator, &uDenominator);
                        UINT32 factor = uWidth * uHeight * uNummerator;
                        if (factor > maxFactor)
                        {
                            maxFactor = factor;
                            pMediaTypeHandler->SetCurrentMediaType(pMediaType);
                        }
                        else if ((factor == maxFactor) && m_draw.IsFormatSupported(subType))
                        {
                            pMediaTypeHandler->SetCurrentMediaType(pMediaType);
                        }

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

    // Try to find a suitable output type.
    if (SUCCEEDED(hr))
    {
        for (DWORD i = 0; ; i++)
        {
            //             hr = m_pReader->GetNativeMediaType(
            //                 (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            //                 i,
            //                 &pType
            //                 );
            hr = m_pReader->GetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                &pType
                );


            if (FAILED(hr)) { break; }

            hr = TryMediaType(pType);

            SafeRelease(&pType);

            if (SUCCEEDED(hr))
            {
                // Found an output type.
                break;
            }
        }
    }
    //Init Codec
    InitCodec();

    if (SUCCEEDED(hr))
    {
        // Ask for the first sample.
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
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

VideoAttribute * CPreview::GetVideoAttribute() {

    return &m_videoAttribute;
}

HRESULT CPreview::SetVideoAttribute(IMFMediaType *pType) {

    HRESULT hr = S_OK;
    GUID subtype = { 0 };
    MFRatio PAR = { 0 };
    LONG lStride = 0;

    // Get the frame format.
    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    m_videoAttribute.m_dwFmtName = (DWORD)subtype.Data1;
    if (subtype.Data1 == MFVideoFormat_RGB32.Data1)
    {
        m_videoAttribute.m_iPixFmt = AV_PIX_FMT_RGB32;
    }else if (subtype.Data1 == MFVideoFormat_RGB24.Data1)
    {
        m_videoAttribute.m_iPixFmt = AV_PIX_FMT_RGB24;
    }else if (subtype.Data1 == MFVideoFormat_YUY2.Data1)
    {
        m_videoAttribute.m_iPixFmt = AV_PIX_FMT_YUYV422;
    }else if (subtype.Data1 == MFVideoFormat_NV12.Data1)
    {
        m_videoAttribute.m_iPixFmt = AV_PIX_FMT_NV12;
    }else if (subtype.Data1 == MFVideoFormat_I420.Data1)
    {
        m_videoAttribute.m_iPixFmt = AV_PIX_FMT_YUV420P;
    }

    // Get the frame size.
    hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &m_videoAttribute.m_uWidth, &m_videoAttribute.m_uHeight);

    // Get the interlace mode. Default: assume progressive.
    m_videoAttribute.m_bInterlace = MFGetAttributeUINT32(pType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, (UINT32*)&PAR.Numerator, (UINT32*)&PAR.Denominator);
    if (SUCCEEDED(hr))
    {
        m_videoAttribute.m_uFps = PAR.Numerator / PAR.Denominator;
    }


    // Try to get the default stride from the media type.
    hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
    if (FAILED(hr))
    {
        // Attribute not set. Try to calculate the default stride.
        hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, m_videoAttribute.m_uWidth, &lStride);
    }
    if (SUCCEEDED(hr))
    {
        m_videoAttribute.m_uStride = lStride;
    }

    return hr;
}


HRESULT CPreview::InitCodec() {

    HRESULT hr = S_OK;

    m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (m_codec == NULL)
    {
        return !S_OK;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);

    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->profile = FF_PROFILE_H264_HIGH;

    m_codecContext->width = (int)m_videoAttribute.m_uWidth;
    m_codecContext->height = (int)m_videoAttribute.m_uHeight;
    m_codecContext->framerate.num = (int)m_videoAttribute.m_uFps;
    m_codecContext->framerate.den = (int)1;
    m_codecContext->time_base.num = (int)1;
    m_codecContext->time_base.den = (int)m_videoAttribute.m_uFps;
    m_codecContext->gop_size = (int)m_videoAttribute.m_uFps;

    m_codecContext->max_b_frames = 1;

    av_opt_set(m_codecContext->priv_data, "preset", "slow", 0);
    m_codecContext->bit_rate = 4000000;
    m_codecContext->bit_rate_tolerance = 4000000;
    m_codecContext->rc_max_rate = 4000000;
    m_codecContext->rc_min_rate = 4000000;

    m_codecContext->qmin = 12;
    m_codecContext->qmax = 34;
    m_codecContext->max_qdiff = 8;

    int ret = avcodec_open2(m_codecContext, m_codec, NULL);
    if (ret < 0) {
        OutputDebugStringA("open codex failed!");
    }

    m_dstFrame = av_frame_alloc();
    if (m_dstFrame) {
        m_dstFrame->format = m_codecContext->pix_fmt;
        m_dstFrame->width = m_codecContext->width;
        m_dstFrame->height = m_codecContext->height;
    }
    ret = av_image_alloc(m_dstFrame->data, m_dstFrame->linesize, m_dstFrame->width, m_dstFrame->height,
        (AVPixelFormat)m_dstFrame->format, 32);

    m_srcFrame = av_frame_alloc();
    if (m_srcFrame) {
        m_srcFrame->format = (AVPixelFormat)m_videoAttribute.m_iPixFmt;
        m_srcFrame->width = m_codecContext->width;
        m_srcFrame->height = m_codecContext->height;
    }
    av_image_fill_linesizes(m_srcFrame->linesize, (AVPixelFormat)m_srcFrame->format, m_srcFrame->width);

    m_swsContext = sws_getContext(m_srcFrame->width, m_srcFrame->height, (AVPixelFormat)m_srcFrame->format,
        m_dstFrame->width, m_dstFrame->height, (AVPixelFormat)m_dstFrame->format,
        SWS_BILINEAR, NULL, NULL, NULL);

    h264file = new std::ofstream("video.h264", std::ios::binary);

    yuvfile = new std::ofstream("video.yuv", std::ios::binary);

    return hr;
}


void CPreview::UninitCodec() {

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

    if (h264file)
    {
        h264file->flush();
        h264file->close();
        delete h264file;
        h264file = NULL;
    }

    if (yuvfile)
    {
        yuvfile->flush();
        yuvfile->close();
        delete yuvfile;
        yuvfile = NULL;
    }
}




//-------------------------------------------------------------------
//  ResizeVideo
//  Resizes the video rectangle.
//
//  The application should call this method if the size of the video
//  window changes; e.g., when the application receives WM_SIZE.
//-------------------------------------------------------------------

HRESULT CPreview::ResizeVideo(WORD /*width*/, WORD /*height*/)
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critsec);

    hr = m_draw.ResetDevice();

    if (FAILED(hr))
    {
        MessageBox(NULL, L"ResetDevice failed!", NULL, MB_OK);
    }

    LeaveCriticalSection(&m_critsec);

    return hr;
}


//-------------------------------------------------------------------
//  CheckDeviceLost
//  Checks whether the current device has been lost.
//
//  The application should call this method in response to a
//  WM_DEVICECHANGE message. (The application must register for 
//  device notification to receive this message.)
//-------------------------------------------------------------------

HRESULT CPreview::CheckDeviceLost(DEV_BROADCAST_HDR *pHdr, BOOL *pbDeviceLost)
{
    DEV_BROADCAST_DEVICEINTERFACE *pDi = NULL;

    if (pbDeviceLost == NULL)
    {
        return E_POINTER;
    }

    *pbDeviceLost = FALSE;

    if (pHdr == NULL)
    {
        return S_OK;
    }

    if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
    {
        return S_OK;
    }

    pDi = (DEV_BROADCAST_DEVICEINTERFACE*)pHdr;


    EnterCriticalSection(&m_critsec);

    if (m_pwszSymbolicLink)
    {
        if (_wcsicmp(m_pwszSymbolicLink, pDi->dbcc_name) == 0)
        {
            *pbDeviceLost = TRUE;
        }
    }

    LeaveCriticalSection(&m_critsec);

    return S_OK;
}


HRESULT CPreview::StartYUVRecord() {
	LOG_INFO("YUV Record Starting...\n");

	return S_OK;
}

HRESULT CPreview::StopYUVRecord() {

	LOG_INFO("YUV Record Stopping...\n");

	return S_OK;
}

HRESULT CPreview::StartH264Record() {

	LOG_INFO("H264 Record Starting...\n");

	return S_OK;
}

HRESULT CPreview::StopH264Record() {

	LOG_INFO("H264 Record Stopping...\n");

	return S_OK;
}

HRESULT CPreview::StartMP4Record() {

	LOG_INFO("MP4 Record Starting...\n");

	return S_OK;
}

HRESULT CPreview::StopMP4Record() {

	LOG_INFO("MP4 Record Stopping...\n");

	return S_OK;
}
