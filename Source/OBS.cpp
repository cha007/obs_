#include "Main.h"
#include <intrin.h>

//primarily main window stuff an initialization/destruction code

typedef bool (*LOADPLUGINPROC)();
typedef bool (*LOADPLUGINEXPROC)(UINT);
typedef void (*UNLOADPLUGINPROC)();
typedef CTSTR (*GETPLUGINNAMEPROC)();

ImageSource* STDCALL CreateDesktopSource(XElement *data);
bool STDCALL ConfigureDesktopSource(XElement *data, bool bCreating);
bool STDCALL ConfigureWindowCaptureSource(XElement *data, bool bCreating);
bool STDCALL ConfigureMonitorCaptureSource(XElement *data, bool bCreating);
ImageSource* STDCALL CreateGlobalSource(XElement *data);

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);

APIInterface* CreateOBSApiInterface();


#define QuickClearHotkey(hotkeyID) \
    if(hotkeyID) \
{ \
    API->DeleteHotkey(hotkeyID); \
    hotkeyID = NULL; \
}

//----------------------------

WNDPROC listboxProc = NULL;
WNDPROC listviewProc = NULL;

//----------------------------

const float defaultBlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

//----------------------------

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, List<MonitorInfo> &monitors)
{
    monitors << MonitorInfo(hMonitor, lprcMonitor);
    return TRUE;
}

Scene* STDCALL CreateNormalScene(XElement *data)
{
    return new Scene;
}

BOOL IsWebrootLoaded()
{
    BOOL ret = FALSE;
    StringList moduleList;

    OSGetLoadedModuleList (GetCurrentProcess(), moduleList);

    HMODULE msIMG = GetModuleHandle(TEXT("MSIMG32"));
    if (msIMG)
    {
        FARPROC alphaBlend = GetProcAddress(msIMG, "AlphaBlend");
        if (alphaBlend)
        {
            if (!IsBadReadPtr(alphaBlend, 5))
            {
                BYTE opCode = *(BYTE *)alphaBlend;

                if (opCode == 0xE9)
                {
                    if (moduleList.HasValue(TEXT("wrusr.dll")))
                        ret = TRUE;
                }
            }
        }
    }

    return ret;
}

// Checks the AppData path is writable and the disk has enough space
VOID CheckPermissionsAndDiskSpace()
{
    ULARGE_INTEGER   freeSpace;

    if (GetDiskFreeSpaceEx (lpAppDataPath, &freeSpace, NULL, NULL))
    {
        // 1MB ought to be enough for anybody...
        if (freeSpace.QuadPart < 1048576)
        {
            OBSMessageBox(OBSGetMainWindow(), Str("DiskFull"), NULL, MB_ICONERROR);
        }
    }

    HANDLE tempFile;
    String testPath;

    testPath = lpAppDataPath;
    testPath += TEXT("\\.test");

    tempFile = CreateFile(testPath.Array(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (tempFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED || err == ERROR_FILE_READ_ONLY)
            OBSMessageBox(OBSGetMainWindow(), Str("BadAppDataPermissions"), NULL, MB_ICONERROR);

        // TODO: extra handling for unknown errors (maybe some av returns weird codes?)
    }
    else
    {
        CloseHandle(tempFile);
    }
}

OBS::OBS()
{
	App = this;
    hSceneMutex = OSCreateMutex();
    hAuxAudioMutex = OSCreateMutex();
    hVideoEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    //-----------------------------------------------------
    // load classes
    RegisterSceneClass(TEXT("Scene"), Str("Scene"), (OBSCREATEPROC)CreateNormalScene, NULL, false);
  
    //-----------------------------------------------------
    // render frame class
    WNDCLASS wc;
    zero(&wc, sizeof(wc));
    wc.hInstance = hinstMain;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    //-----------------------------------------------------
    // main window class
    wc.lpszClassName = OBS_WINDOW_CLASS;
	wc.lpfnWndProc = (WNDPROC)OBSProc;
	wc.hIcon = NULL;//LoadIcon(hinstMain, MAKEINTRESOURCE(IDI_ICON1));
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName = NULL;//MAKEINTRESOURCE(IDR_MAINMENU);

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register main window class"));

    //-----------------------------------------------------
    // create main window

    int fullscreenX = GetSystemMetrics(SM_CXFULLSCREEN);
    int fullscreenY = GetSystemMetrics(SM_CYFULLSCREEN);

    borderXSize = borderYSize = 0;

    borderXSize += GetSystemMetrics(SM_CXSIZEFRAME)*2;
    borderYSize += GetSystemMetrics(SM_CYSIZEFRAME)*2;
    borderYSize += GetSystemMetrics(SM_CYMENU);
    borderYSize += GetSystemMetrics(SM_CYCAPTION);

    clientWidth  = GlobalConfig->GetInt(TEXT("General"), TEXT("Width"),  defaultClientWidth);
    clientHeight = GlobalConfig->GetInt(TEXT("General"), TEXT("Height"), defaultClientHeight);

    if(clientWidth < minClientWidth)
        clientWidth = minClientWidth;
    if(clientHeight < minClientHeight)
        clientHeight = minClientHeight;

    int maxCX = fullscreenX-borderXSize;
    int maxCY = fullscreenY-borderYSize;

    if(clientWidth > maxCX)
        clientWidth = maxCX;
    if(clientHeight > maxCY)
        clientHeight = maxCY;

    int cx = clientWidth  + borderXSize;
    int cy = clientHeight + borderYSize;

    int x = (fullscreenX/2)-(cx/2);
    int y = (fullscreenY/2)-(cy/2);

    int posX = GlobalConfig->GetInt(TEXT("General"), TEXT("PosX"), -9999);
    int posY = GlobalConfig->GetInt(TEXT("General"), TEXT("PosY"), -9999);

    hwndMain = CreateWindowEx(WS_EX_CONTROLPARENT|WS_EX_WINDOWEDGE|(LocaleIsRTL() ? WS_EX_LAYOUTRTL : 0), OBS_WINDOW_CLASS, GetApplicationName(),
        WS_OVERLAPPED | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, cx, cy, NULL, NULL, hinstMain, NULL);
    if(!hwndMain)
        CrashError(TEXT("Could not create main window"));

    hmenuMain = GetMenu(hwndMain);
    LocalizeMenu(hmenuMain);

    //-----------------------------------------------------
    // scenes listbox

    HWND hwndTemp;
    hwndTemp = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("LISTBOX"), NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LBS_HASSTRINGS|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    listboxProc = (WNDPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
    SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)OBS::ListboxHook);
   

    //-----------------------------------------------------
    // populate scenes
    hwndTemp = GetDlgItem(hwndMain, ID_SCENES);
    String collection = GetCurrentSceneCollection();

    if (!OSFileExists(String() << lpAppDataPath << L"\\sceneCollection\\" << collection << L".xconfig"))
        collection.Clear();

    if (collection.IsEmpty())
    {
        OSFindData ofd;
        HANDLE hFind = OSFindFirstFile(String() << lpAppDataPath << L"\\sceneCollection\\*.xconfig", ofd);
        if (hFind)
        {
            do
            {
                if (!ofd.bDirectory)
                {
                    collection = GetPathWithoutExtension(ofd.fileName);
                    break;
                }
            } while (OSFindNextFile(hFind, ofd));
            OSFindClose(hFind);
        }

        if (collection.IsEmpty())
        {
            CopyFile(String() << lpAppDataPath << L"\\scenes.xconfig", String() << lpAppDataPath << L"\\sceneCollection\\scenes.xconfig", true);
            collection = L"scenes";
            GlobalConfig->SetString(L"General", L"SceneCollection", collection);
        }
    }

    String strScenesConfig;
    strScenesConfig = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, collection.Array());

    if(!scenesConfig.Open(strScenesConfig))
        CrashError(TEXT("Could not open '%s'"), strScenesConfig.Array());

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
    if(!scenes)
        scenes = scenesConfig.CreateElement(TEXT("scenes"));

    UINT numScenes = scenes->NumElements();
    if(!numScenes)
    {
        XElement *scene = scenes->CreateElement(Str("Scene"));
        scene->SetString(TEXT("class"), TEXT("Scene"));
        numScenes++;
    }

    for(UINT i=0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        //scene->SetString(TEXT("class"), TEXT("Scene"));
        SendMessage(hwndTemp, LB_ADDSTRING, 0, (LPARAM)scene->GetName());
    }

    //-----------------------------------------------------
    // populate sources

    if(numScenes)
    {
        String strScene = AppConfig->GetString(TEXT("General"), TEXT("CurrentScene"));
        int id = (int)SendMessage(hwndTemp, LB_FINDSTRINGEXACT, -1, (LPARAM)strScene.Array());
        if(id == LB_ERR)
            id = 0;

        SendMessage(hwndTemp, LB_SETCURSEL, (WPARAM)id, 0);
        SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));
    }

    //-----------------------------------------------------

    hHotkeyMutex = OSCreateMutex();
    hStartupShutdownMutex = OSCreateMutex();

    //-----------------------------------------------------

    API = CreateOBSApiInterface();

    bDragResize = false;

     ReloadIniSettings();

    bAutoReconnect = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnect"), 1) != 0;
    reconnectTimeout = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnectTimeout"), 10);

    // TODO: Should these be stored in the config file?
    bRenderViewEnabled = GlobalConfig->GetInt(TEXT("General"), TEXT("PreviewEnabled"), 1) != 0;
    bForceRenderViewErase = false;
    renderFrameIn1To1Mode = false;

    if(GlobalConfig->GetInt(TEXT("General"), TEXT("ShowWebrootWarning"), TRUE) && IsWebrootLoaded())
        OBSMessageBox(hwndMain, TEXT("Webroot Secureanywhere appears to be active.  This product will cause problems with OBS as the security features block OBS from accessing Windows GDI functions.  It is highly recommended that you disable Secureanywhere and restart OBS.\r\n\r\nOf course you can always just ignore this message if you want, but it may prevent you from being able to stream certain things. Please do not report any bugs you may encounter if you leave Secureanywhere enabled."), TEXT("Just a slight issue you might want to be aware of"), MB_OK);

    CheckPermissionsAndDiskSpace();
    ShowWindow(hwndMain, SW_SHOW);

    renderFrameIn1To1Mode = !!GlobalConfig->GetInt(L"General", L"1to1Preview", false);

    if (bStreamOnStart)
        PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_STARTSTOP, 0), NULL);

	Start(true);
}


OBS::~OBS()
{
    Stop(true, true);

    bShuttingDown = true;
    ClosePendingStreams();
    if (logDirectoryMonitor)
        OSMonitorDirectoryCallbackStop(logDirectoryMonitor);

    //DestroyWindow(hwndMain);

    // Remember window state for next launch
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement(hwndMain, &placement);
    RECT rect = { 0 };
    GetWindowRect(hwndMain, &rect);

    scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");
    scenesConfig.Close(true);

    for(UINT i=0; i<Icons.Num(); i++)
        DeleteObject(Icons[i].hIcon);
    Icons.Clear();

    for(UINT i=0; i<Fonts.Num(); i++)
    {
        DeleteObject(Fonts[i].hFont);
        Fonts[i].strFontFace.Clear();
    }
    Fonts.Clear();

    for(UINT i=0; i<sceneClasses.Num(); i++)
        sceneClasses[i].FreeData();
    for(UINT i=0; i<imageSourceClasses.Num(); i++)
        imageSourceClasses[i].FreeData();

    if (hVideoEvent)
        CloseHandle(hVideoEvent);

    if(hSceneMutex)
        OSCloseMutex(hSceneMutex);

    if(hAuxAudioMutex)
        OSCloseMutex(hAuxAudioMutex);

    delete API;
    API = NULL;

    if(hHotkeyMutex)
        OSCloseMutex(hHotkeyMutex);

    App = NULL;
}

/**
 * Controls which message should be displayed in the middle of the main window.
 */
void OBS::UpdateRenderViewMessage()
{
    if(bRunning){
        if(bRenderViewEnabled){
        }
        else{
        }
    }
    else{
    }
}

void OBS::ResizeRenderFrame(bool bRedrawRenderFrame)
{
    // Get output steam size and aspect ratio
    int curCX, curCY;
    float mainAspect;
    if(bRunning)
    {
        curCX = outputCX;
        curCY = outputCY;
        mainAspect = float(curCX)/float(curCY);
    }
    else
    {
        // Default to the monitor's resolution if the base size is undefined
        int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
        if(monitorID >= 1)
            monitorID = 0;
		int defCX = 1920;// screenRect.right - screenRect.left;
		int defCY = 1080;// screenRect.bottom - screenRect.top;

        // Calculate output size using the same algorithm that's in OBS::Start()
        float scale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
        curCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
        curCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);
        curCX = MIN(MAX(curCX, 128), 4096);
        curCY = MIN(MAX(curCY, 128), 4096);
        curCX = UINT(double(curCX) / double(scale));
        curCY = UINT(double(curCY) / double(scale));
        curCX = curCX & 0xFFFFFFFC; // Align width to 128bit for fast SSE YUV4:2:0 conversion
        curCY = curCY & 0xFFFFFFFE;

        mainAspect = float(curCX)/float(curCY);
    }

    // Get area to render in
    int x, y;
    UINT controlWidth  = clientWidth;
    UINT controlHeight = clientHeight;
//     if(bPanelVisible)
//         controlHeight -= totalControlAreaHeight + controlPadding;
    UINT newRenderFrameWidth, newRenderFrameHeight;
    if(renderFrameIn1To1Mode)
    {
        newRenderFrameWidth  = (UINT)curCX;
        newRenderFrameHeight = (UINT)curCY;
        x = (int)controlWidth / 2 - curCX / 2;
        y = (int)controlHeight / 2 - curCY / 2;
    }
    else
    { // Scale to fit
        Vect2 renderSize = Vect2(float(controlWidth), float(controlHeight));
        float renderAspect = renderSize.x/renderSize.y;

        if(renderAspect > mainAspect)
        {
            renderSize.x = renderSize.y*mainAspect;
            x = int((float(controlWidth)-renderSize.x)*0.5f);
            y = 0;
        }
        else
        {
            renderSize.y = renderSize.x/mainAspect;
            x = 0;
            y = int((float(controlHeight)-renderSize.y)*0.5f);
        }

        // Round and ensure even size
        newRenderFrameWidth  = int(renderSize.x+0.5f)&0xFFFFFFFE;
        newRenderFrameHeight = int(renderSize.y+0.5f)&0xFFFFFFFE;
    }

    // Fill the majority of the window with the 3D scene. We'll render everything in DirectX
    SetWindowPos(hwndRenderFrame, NULL, 0, 0, controlWidth, controlHeight, SWP_NOOWNERZORDER);

    //----------------------------------------------

    renderFrameX = x;
    renderFrameY = y;
    renderFrameWidth  = newRenderFrameWidth;
    renderFrameHeight = newRenderFrameHeight;
    renderFrameCtrlWidth  = controlWidth;
    renderFrameCtrlHeight = controlHeight;
    if(!bRunning)
    {
        oldRenderFrameCtrlWidth = renderFrameCtrlWidth;
        oldRenderFrameCtrlHeight = renderFrameCtrlHeight;
        InvalidateRect(hwndRenderMessage, NULL, true); // Repaint text
    }
    else if(bRunning && bRedrawRenderFrame)
    {
        oldRenderFrameCtrlWidth = renderFrameCtrlWidth;
        oldRenderFrameCtrlHeight = renderFrameCtrlHeight;
        bResizeRenderView = true;
    }
}

void OBS::SetFullscreenMode(bool fullscreen){
}

void OBS::GetBaseSize(UINT &width, UINT &height) const
{
    if(bRunning)
    {
        width = baseCX;
        height = baseCY;
    }
    else
    {
        int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
        if(monitorID >= 1)
            monitorID = 0;

		int defCX = 1920;
		int defCY = 1080;

		width  = 1920;
		height = 1080;
    }
}

void OBS::ResizeWindow(bool bRedrawRenderFrame){
}

void OBS::GetProfiles(StringList &profileList)
{
    String strProfilesWildcard;
    OSFindData ofd;
    HANDLE hFind;

    profileList.Clear();
    String profileDir(FormattedString(L"%s/profiles/", OBSGetAppDataPath()));
    strProfilesWildcard << profileDir << "*.ini";

    if(hFind = OSFindFirstFile(strProfilesWildcard, ofd))
    {
        do
        {
            String profile(GetPathWithoutExtension(ofd.fileName));
            String profilePath(FormattedString(L"%s%s.ini", profileDir.Array(), profile.Array()));
            if(ofd.bDirectory || !OSFileExists(profilePath) || profileList.HasValue(profile)) continue;
            profileList << profile;
        } while(OSFindNextFile(hFind, ofd));

        OSFindClose(hFind);
    }
}

void OBS::GetSceneCollection(StringList &sceneCollectionList)
{
    String strSceneCollectionWildcard;
    OSFindData ofd;
    HANDLE hFind;

    sceneCollectionList.Clear();

    String sceneCollectionDir(FormattedString(L"%s/sceneCollection/", OBSGetAppDataPath()));
    strSceneCollectionWildcard << sceneCollectionDir << "*.xconfig";
    if (hFind = OSFindFirstFile(strSceneCollectionWildcard, ofd))
    {
        do
        {
            String sceneCollection(GetPathWithoutExtension(ofd.fileName));
            String sceneCollectionPath(FormattedString(L"%s%s.xconfig", sceneCollectionDir.Array(), sceneCollection.Array()));
            if (ofd.bDirectory || !OSFileExists(sceneCollectionPath) || sceneCollectionList.HasValue(sceneCollection)) continue;
            sceneCollectionList << sceneCollection;
        } while (OSFindNextFile(hFind, ofd));
        OSFindClose(hFind);
    }
}

void OBS::ReloadSceneCollection()
{
    HWND hwndTemp;
    hwndTemp = GetDlgItem(hwndMain, ID_SCENES);

    CTSTR collection = GetCurrentSceneCollection();
    String strScenesConfig = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, collection);

    if (!scenesConfig.Open(strScenesConfig))
        CrashError(TEXT("Could not open '%s'"), strScenesConfig.Array());

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));

    if (!scenes)
        scenes = scenesConfig.CreateElement(TEXT("scenes"));

    SendMessage(hwndTemp, LB_RESETCONTENT, 0, 0);

    App->sceneElement = NULL;
    
    UINT numScenes = scenes->NumElements();
    if (!numScenes)
    {
        XElement *scene = scenes->CreateElement(Str("Scene"));
        scene->SetString(TEXT("class"), TEXT("Scene"));
        numScenes++;
    }

    for (UINT i = 0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        SendMessage(hwndTemp, LB_ADDSTRING, 0, (LPARAM)scene->GetName());
    }
    //-----------------------------------------------------
    // populate sources

    if (numScenes)
    {
        String strScene = AppConfig->GetString(TEXT("General"), TEXT("CurrentScene"));
        int id = (int)SendMessage(hwndTemp, LB_FINDSTRINGEXACT, -1, (LPARAM)strScene.Array());
        if (id == LB_ERR)
            id = 0;

        SendMessage(hwndTemp, LB_SETCURSEL, (WPARAM)id, 0);
        SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));
    }

    //-----------------------------------------------------

    for (UINT i = 0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        DWORD hotkey = scene->GetInt(TEXT("hotkey"));
        if (hotkey)
        {
            SceneHotkeyInfo hotkeyInfo;
            hotkeyInfo.hotkey = hotkey;
            hotkeyInfo.scene = scene;
            hotkeyInfo.hotkeyID = API->CreateHotkey(hotkey, SceneHotkey, 0);

            if (hotkeyInfo.hotkeyID)
                sceneHotkeys << hotkeyInfo;
        }
    }
}

void OBS::ReloadIniSettings()
{
    HWND hwndTemp;

    //-------------------------------------------
    // mic volume data
    hwndTemp = GetDlgItem(hwndMain, ID_MICVOLUME);

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("MicVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), 1.0f);

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING, false, true);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    EnableWindow(hwndTemp, !strDevice.CompareI(TEXT("Disable")));

    //-------------------------------------------
    // desktop volume
    hwndTemp = GetDlgItem(hwndMain, ID_DESKTOPVOLUME);

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("DesktopVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);

    //-------------------------------------------
    // desktop boost
    DWORD desktopBoostMultiple = GlobalConfig->GetInt(TEXT("Audio"), TEXT("DesktopBoostMultiple"), 1);
    if(desktopBoostMultiple < 1)
        desktopBoostMultiple = 1;
    else if(desktopBoostMultiple > 20)
        desktopBoostMultiple = 20;
    desktopBoost = float(desktopBoostMultiple);

    //-------------------------------------------
    // mic boost
    DWORD micBoostMultiple = AppConfig->GetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), 1);
    if(micBoostMultiple < 1)
        micBoostMultiple = 1;
    else if(micBoostMultiple > 20)
        micBoostMultiple = 20;
    micBoost = float(micBoostMultiple);

    //-------------------------------------------
    // hotkeys
    QuickClearHotkey(pushToTalkHotkeyID);
    QuickClearHotkey(pushToTalkHotkey2ID);
    QuickClearHotkey(muteMicHotkeyID);
    QuickClearHotkey(muteDesktopHotkeyID);
    QuickClearHotkey(stopStreamHotkeyID);
    QuickClearHotkey(startStreamHotkeyID);
    QuickClearHotkey(stopRecordingHotkeyID);
    QuickClearHotkey(startRecordingHotkeyID);
    QuickClearHotkey(stopReplayBufferHotkeyID);
    QuickClearHotkey(startReplayBufferHotkeyID);
    QuickClearHotkey(saveReplayBufferHotkeyID);
    QuickClearHotkey(recordFromReplayBufferHotkeyID);

    DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));
    DWORD hotkey2 = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"));

    bKeepRecording = AppConfig->GetInt(TEXT("Publish"), TEXT("KeepRecording")) != 0;

    if (/*!minimizeToIcon &&*/ !IsWindowVisible(hwndMain))
        ShowWindow(hwndMain, SW_SHOW);

    //--------------------------------------------
    // Update old config, transition old encoder selection
    //int qsv = AppConfig->GetInt(L"Video Encoding", L"UseQSV", -1);
    int nvenc = AppConfig->GetInt(L"Video Encoding", L"UseNVENC", -1);
    if (/*qsv != -1 ||*/ nvenc != -1)
    {
        AppConfig->SetString(L"Video Encoding", L"Encoder", (nvenc > 0) ? L"NVENC" : L"x264");
        AppConfig->Remove(L"Video Encoding", L"UseQSV");
        AppConfig->Remove(L"Video Encoding", L"UseNVENC");

        int custom = AppConfig->GetInt(L"Video Encoding", L"UseCustomSettings", -1);
        int customQSV = AppConfig->GetInt(L"Video Encoding", L"QSVUseVideoEncoderSettings", -1);
        if (custom > 0 && customQSV > 0)
            AppConfig->SetString(L"Video Encoding", L"CustomQSVSettings", AppConfig->GetString(L"Video Encoding", L"CustomSettings"));
    }
}

void OBS::SelectSources()
{
    if(scene)
        scene->DeselectAll();

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = ListView_GetSelectedCount(hwndSources);

    if(numSelected)
    {
        List<UINT> selectedItems;
        selectedItems.SetSize(numSelected);
        //SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedItems.Array());

        if(scene)
        {
            int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
            while (iPos != -1)
            {
                SceneItem *sceneItem = scene->GetSceneItem(iPos);
                sceneItem->bSelected = true;
                
                iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
            }
        }
    }
}

void OBS::CheckSources()
{
    XElement *curSceneElement = App->sceneElement;
    XElement *sources = curSceneElement->GetElement(TEXT("sources"));

    if(!sources)
        return;

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

    UINT numSources = ListView_GetItemCount(hwndSources);
    for(UINT i = 0; i < numSources; i++)
    {
        bool checked = ListView_GetCheckState(hwndSources, i) > 0;
        XElement *source =sources->GetElementByID(i);
        bool curRender = source->GetInt(TEXT("render"), 0) > 0;
        if(curRender != checked)
        {
            source->SetInt(TEXT("render"), (checked)?1:0);
            if(scene && i < scene->NumSceneItems())
            {
                SceneItem *sceneItem = scene->GetSceneItem(i);
                sceneItem->bRender = checked;
                sceneItem->SetRender(checked);
            }
        }
    }
}

void OBS::SetSourceRender(CTSTR sourceName, bool render)
{
    XElement *curSceneElement = App->sceneElement;
    XElement *sources = curSceneElement->GetElement(TEXT("sources"));

    if(!sources)
        return;

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

    UINT numSources = ListView_GetItemCount(hwndSources);
    for(UINT i = 0; i < numSources; i++)
    {
        bool checked = ListView_GetCheckState(hwndSources, i) > 0;
        XElement *source =sources->GetElementByID(i);
        if(scmp(source->GetName(), sourceName) == 0 && checked != render)
        {
            if(scene && i < scene->NumSceneItems())
            {
                SceneItem *sceneItem = scene->GetSceneItem(i);
                sceneItem->SetRender(render);
            }
            else
            {
                source->SetInt(TEXT("render"), (render)?1:0);
            }
            App->bChangingSources = true;
            ListView_SetCheckState(hwndSources, i, render);
            App->bChangingSources = false;

            break;
        }
    }
}

void OBS::GetThreadHandles (HANDLE *videoThread, HANDLE *encodeThread)
{
    if (hVideoThread)
        *videoThread = hVideoThread;

    if (hEncodeThread)
        *encodeThread = hEncodeThread;
}

NetworkStream* CreateRTMPPublisher();
NetworkStream* CreateNullNetwork();

void OBS::RestartNetwork()
{
    OSEnterMutex(App->hStartupShutdownMutex);

    //delete the old one
    App->network.reset();

    //start up a new one
    App->bSentHeaders = false;
    App->network.reset(CreateRTMPPublisher());

    OSLeaveMutex(App->hStartupShutdownMutex);
}
