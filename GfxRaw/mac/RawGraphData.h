#ifndef _RAW_GRAPH_DATA_H_
#define _RAW_GRAPH_DATA_H_

#pragma once


class Window;
typedef struct CGContext *CGContextRef;

class CRawGraphData {
public:
    CRawGraphData();
    virtual ~CRawGraphData();

    virtual bool create(int cx, int cy, WindowHandle handle, int nBitCount = 32);

    virtual void destroy();

    int width() const { return m_imageData.width; }
    int height() const { return m_imageData.height; }

    RawImageData *getRawBuff() { return &m_imageData; }
    uint8_t *getBuff() { return m_imageData.buff; }

    bool isValid() { return m_imageData.buff != nullptr; }

    void drawToWindow(int xdest, int ydest, int width, int height, int xsrc, int ysrc, float scaleFactor);

    CGContextRef getHandle() const { return m_context; }

protected:
    WindowHandle                m_windowHandle = 0;
    RawImageData                m_imageData;
    CGContextRef                m_context = nullptr;

};

#endif // _RAW_GRAPH_DATA_H_
