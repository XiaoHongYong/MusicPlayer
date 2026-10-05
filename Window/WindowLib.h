#pragma once

#ifndef _HEADER_WIDGET_H_
#define _HEADER_WIDGET_H_

#include "WindowTypes.h"
#include "../Utils/Utils.h"
#include "../GfxRaw/GfxRaw.h"
#include "Cursor.h"

#ifdef WIN32
#include "win32/Window.h"
#endif

#ifdef _LINUX_GTK2
#endif

#ifdef _MAC_OS
#include "mac/Window.h"
#endif

#include "Menu.h"
#include "FolderDialog.h"
#include "FileSaveDlg.h"
#include "FileOpenDlg.h"
#include "DlgChooseColor.h"
#include "DlgChooseFont.h"

#include "WndResizer.h"
#include "WndDrag.h"
#include "Desktop.h"
#include "SkinOs.h"


#endif // _HEADER_WIDGET_H_
