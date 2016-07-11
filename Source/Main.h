#pragma once

#define WINVER         0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT   0x0600
#define NTDDI_VERSION  NTDDI_VISTASP1
#define WIN32_LEAN_AND_MEAN
#define ISOLATION_AWARE_ENABLED 1
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <math.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#define USE_AAC 1

#pragma warning(default : 4265)

//-------------------------------------------
// direct3d 10.1

#include <D3D10_1.h>
#include <D3DX10.h>
#include <DXGI.h>

#define USE_DXGI1_2 1

#ifdef USE_DXGI1_2
#include <dxgi1_2.h>
#endif


//-------------------------------------------
// API DLL

#include "OBSApi.h"


//-------------------------------------------
// application globals

class OBS;
extern HWND         hwndMain;
extern HWND         hwndRenderFrame;
extern HINSTANCE    hinstMain;
extern ConfigFile   *GlobalConfig;
extern ConfigFile   *AppConfig;
extern OBS          *App;
extern bool         bStreamOnStart;
extern TCHAR        lpAppPath[MAX_PATH];
extern TCHAR        lpAppDataPath[MAX_PATH];

// this gets defined by the automatic building process
#ifndef OBS_VERSION_SUFFIX
#define OBS_VERSION_SUFFIX ""
#endif

#define OBS_VERSION             0x006507 //version number is 0xMMmmtt (super-major.major.minor - hex)
#define OBS_VERSION_STRING_RAW  "7d-vision"
#define OBS_VERSION_STRING_ANSI OBS_VERSION_STRING_RAW OBS_VERSION_SUFFIX
#define OBS_VERSION_STRING      TEXT(OBS_VERSION_STRING_RAW) TEXT(OBS_VERSION_SUFFIX)

#if defined(_WIN64)
#define OBS_CONFIG_UPDATE_KEY TEXT("LastUpdateCheck64")
#else
#define OBS_CONFIG_UPDATE_KEY TEXT("LastUpdateCheck")
#endif

#define OBS_WINDOW_CLASS         TEXT("OBSWindowClass")

inline UINT ConvertMSTo100NanoSec(UINT ms)
{
    return ms*1000*10; //1000 microseconds, then 10 "100nanosecond" segments
}

void WINAPI ProcessEvents();

//-------------------------------------------
// application headers

#define VOL_MIN -96
#define VOL_MAX 0
#define VOLN_METERED  0x302

#include "../resource.h"
#include "VolumeControl.h"
#include "OBS.h"


