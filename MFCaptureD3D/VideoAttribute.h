
#pragma once


class VideoAttribute
{
public:

	DWORD  m_dwFmtName = 0;
    INT    m_iPixFmt;
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