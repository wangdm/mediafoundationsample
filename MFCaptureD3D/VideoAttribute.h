
#pragma once


class VideoAttribute
{
public:

	DWORD  m_dwFormat = 0;
	UINT   m_uWidth;
	UINT   m_uHeight;
	LONG   m_uStride;
	UINT   m_uFps;
	BOOL   m_bInterlace;

	void SetVideoSize(UINT width, UINT height) {
		m_uWidth  = width;
		m_uHeight = height;
	}

protected:

private:

};