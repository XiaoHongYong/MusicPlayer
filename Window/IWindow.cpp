//
//  IWindow.cpp
//  MusicPlayer
//
//  Created by HongyongXiao on 2021/11/13.
//

#include "IWindow.h"
#include "../GfxRaw/GfxRaw.h"


IWindow::IWindow() {
    m_pmemGraph = nullptr;
}


IWindow::~IWindow() {
    if (m_pmemGraph) {
        delete m_pmemGraph;
        m_pmemGraph = nullptr;
    }
}

void IWindow::onCreate() {
    onLanguageChanged();
}

void IWindow::onResized(int width, int height) {
    if (m_wndSize.cx == width && m_wndSize.cy == height && m_pmemGraph) {
        return;
    }
    m_wndSize.cx = width;
    m_wndSize.cy = height;

    recreateMemGraphics();

    onSize(width, height);
}

void IWindow::onScaleFactorChanged(float scaleFactor) {
    (void)scaleFactor;
    if (m_wndSize.cx <= 0 || m_wndSize.cy <= 0) {
        return;
    }
    recreateMemGraphics();
}

CRawGraph *IWindow::getMemGraphics() {
    //    assert(m_pmemGraph);
    if (!m_pmemGraph) {
        recreateMemGraphics();
    }
    if (!m_pmemGraph) {
        return nullptr;
    }

    m_pmemGraph->resetClipBoundBox(CRect(0, 0, m_pmemGraph->width(), m_pmemGraph->height()));
    m_pmemGraph->resetOrigin(CPoint(0, 0));

    return m_pmemGraph;
}

bool IWindow::recreateMemGraphics() {
    if (m_wndSize.cx <= 0 || m_wndSize.cy <= 0) {
        return false;
    }

    if (m_pmemGraph) {
        delete m_pmemGraph;
        m_pmemGraph = nullptr;
    }

    CRawGraph *memCanvas = new CRawGraph(getScaleFactor());
    if (!memCanvas->create(m_wndSize.cx, m_wndSize.cy, getHandle())) {
        delete memCanvas;
        return false;
    }
    m_pmemGraph = memCanvas;

    return true;
}
