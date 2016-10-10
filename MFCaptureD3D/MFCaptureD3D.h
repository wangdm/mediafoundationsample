//////////////////////////////////////////////////////////////////////////
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>

#include <new>
#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <strsafe.h>
#include <assert.h>

#include <ks.h>
#include <ksmedia.h>
#include <Dbt.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

#define LOG_ERR(var, ...) \
    do{ \
        char str[260]; \
        snprintf(str, 260,"[ERROR]%s:%d -- "var, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        OutputDebugStringA(str); \
    } while (0);

#define LOG_INFO(var, ...) \
    do{ \
        char str[260]; \
        snprintf(str, 260,"[INFO]%s:%d -- "var, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        OutputDebugStringA(str); \
    } while (0);

#define LOG_DEBUG(var, ...) \
    do{ \
        char str[260]; \
        snprintf(str, 260,"[DEBUG]%s:%d -- "var, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        OutputDebugStringA(str); \
    } while (0);

#include "VideoAttribute.h"

#include "device.h"
#include "preview.h"



