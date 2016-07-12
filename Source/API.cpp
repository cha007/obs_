#include "Main.h"

#include <XInput.h>
#define XINPUT_GAMEPAD_LEFT_TRIGGER    0x0400
#define XINPUT_GAMEPAD_RIGHT_TRIGGER   0x0800


void OBS::RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated)
{
    if(!lpClassName || !*lpClassName)
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: No class name specified"));
        return;
    }

    if(!createProc)
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: No create procedure specified"));
        return;
    }

    if(GetSceneClass(lpClassName))
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: Tried to register '%s', but it already exists"), lpClassName);
        return;
    }

    ClassInfo *classInfo   = sceneClasses.CreateNew();
    classInfo->strClass    = lpClassName;
    classInfo->strName     = lpDisplayName;
    classInfo->createProc  = createProc;
    classInfo->configProc  = configProc;
    classInfo->bDeprecated = bDeprecated;
}

void OBS::RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated)
{
    if(!lpClassName || !*lpClassName)
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: No class name specified"));
        return;
    }

    if(!createProc)
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: No create procedure specified"));
        return;
    }

    if(GetImageSourceClass(lpClassName))
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: Tried to register '%s', but it already exists"), lpClassName);
        return;
    }

    ClassInfo *classInfo   = imageSourceClasses.CreateNew();
    classInfo->strClass    = lpClassName;
    classInfo->strName     = lpDisplayName;
    classInfo->createProc  = createProc;
    classInfo->configProc  = configProc;
    classInfo->bDeprecated = bDeprecated;
}

Scene* OBS::CreateScene(CTSTR lpClassName, XElement *data)
{
    for(UINT i=0; i<sceneClasses.Num(); i++)
    {
        if(sceneClasses[i].strClass.CompareI(lpClassName))
            return (Scene*)sceneClasses[i].createProc(data);
    }

    AppWarning(TEXT("OBS::CreateScene: Could not find scene class '%s'"), lpClassName);
    return NULL;
}

ImageSource* OBS::CreateImageSource(CTSTR lpClassName, XElement *data)
{
    for(UINT i=0; i<imageSourceClasses.Num(); i++)
    {
        if(imageSourceClasses[i].strClass.CompareI(lpClassName))
            return (ImageSource*)imageSourceClasses[i].createProc(data);
    }

    AppWarning(TEXT("OBS::CreateImageSource: Could not find image source class '%s'"), lpClassName);
    return NULL;
}

void OBS::ConfigureScene(XElement *element){
}

void OBS::ConfigureImageSource(XElement *element){
}

void OBS::InsertSourceItem(UINT index, LPWSTR name, bool checked)
{
    LVITEM lvI;
    // Initialize LVITEM members that are common to all items.
    lvI.mask      = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = 0;
    lvI.iSubItem  = 0;
    lvI.state     = 0;
    lvI.pszText = name;
    lvI.iItem = index;


    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

    ListView_InsertItem(hwndSources, &lvI);
    ListView_SetCheckState(hwndSources, index, checked);   
    ListView_SetColumnWidth(hwndSources, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hwndSources, 1, LVSCW_AUTOSIZE_USEHEADER);
}

bool OBS::SetScene(CTSTR lpScene)
{
    if(bDisableSceneSwitching)
        return false;

    HWND hwndScenes = GetDlgItem(hwndMain, ID_SCENES);
    UINT curSel = (UINT)SendMessage(hwndScenes, LB_GETCURSEL, 0, 0);

    //-------------------------

    if(curSel != LB_ERR)
    {
        UINT textLen = (UINT)SendMessage(hwndScenes, LB_GETTEXTLEN, curSel, 0);

        String strLBName;
        strLBName.SetLength(textLen);

        SendMessage(hwndScenes, LB_GETTEXT, curSel, (LPARAM)strLBName.Array());
        if(!strLBName.CompareI(lpScene))
        {
            UINT id = (UINT)SendMessage(hwndScenes, LB_FINDSTRINGEXACT, -1, (LPARAM)lpScene);
            if(id == LB_ERR)
                return false;

            SendMessage(hwndScenes, LB_SETCURSEL, id, 0);
        }
    }
    else
    {
        UINT id = (UINT)SendMessage(hwndScenes, LB_FINDSTRINGEXACT, -1, (LPARAM)lpScene);
        if(id == LB_ERR)
            return false;

        SendMessage(hwndScenes, LB_SETCURSEL, id, 0);
    }

    //-------------------------

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
    XElement *newSceneElement = scenes->GetElement(lpScene);
    if(!newSceneElement)
        return false;

    if(sceneElement == newSceneElement)
        return true;

    sceneElement = newSceneElement;

    CTSTR lpClass = sceneElement->GetString(TEXT("class"));
    if(!lpClass)
    {
        AppWarning(TEXT("OBS::SetScene: no class found for scene '%s'"), newSceneElement->GetName());
        return false;
    }

    DWORD sceneChangeStartTime;

    if(bRunning)
    {
        Log(TEXT("++++++++++++++++++++++++++++++++++++++++++++++++++++++"));
        Log(TEXT("  New Scene"));

        sceneChangeStartTime = OSGetTime();
    }

    XElement *sceneData = newSceneElement->GetElement(TEXT("data"));

    //-------------------------

    Scene *newScene = NULL;
    if(bRunning)
        newScene = CreateScene(lpClass, sceneData);

    //-------------------------

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

    SendMessage(hwndSources, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM) 0);

    App->scaleItem = NULL;

    bChangingSources = true;
    ListView_DeleteAllItems(hwndSources);

    XElement *sources = sceneElement->GetElement(TEXT("sources"));
    if(sources)
    {
        UINT numSources = sources->NumElements();
        ListView_SetItemCount(hwndSources, numSources);

        for(UINT i=0; i<numSources; i++)
        {
            XElement *sourceElement = sources->GetElementByID(i);
            String className = sourceElement->GetString(TEXT("class"));
        }

        for(UINT i=0; i<numSources; i++)
        {
            XElement *sourceElement = sources->GetElementByID(i);
            bool render = sourceElement->GetInt(TEXT("render"), 1) > 0;

            InsertSourceItem(i, (LPWSTR)sourceElement->GetName(), render);

            // Do not add image sources yet in case we're skipping the transition.
            // This fixes the issue where capture devices sources that used the
            // same device as one in the previous scene would just go blank
            // after switching.
            if(bRunning && newScene)
                newScene->AddImageSource(sourceElement);
        }
    }

    bChangingSources = false;
    SendMessage(hwndSources, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM) 0);
    RedrawWindow(hwndSources, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

    if(scene && newScene && newScene->HasMissingSources())
        OBSMessageBox(hwndMain, Str("Scene.MissingSources"), NULL, 0);

    if(bRunning)
    {
        //todo: cache scenes maybe?  undecided.  not really as necessary with global sources
        OSEnterMutex(hSceneMutex);

        UINT numSources;

        if (scene)
        {
            //shutdown previous scene, if any
            numSources = scene->sceneItems.Num();
            for(UINT i=0; i<numSources; i++)
            {
                XElement *source = scene->sceneItems[i]->GetElement();
                String className = source->GetString(TEXT("class"));
                if(scene->sceneItems[i]->bRender && className == "GlobalSource") {
                    XElement *globalSourceData = source->GetElement(TEXT("data"));
                    String globalSourceName = globalSourceData->GetString(TEXT("name"));
//                     if(App->GetGlobalSource(globalSourceName) != NULL) {
//                         App->GetGlobalSource(globalSourceName)->GlobalSourceLeaveScene();
//                     }
                }
            }

            scene->EndScene();
        }

        Scene *previousScene = scene;
        scene = newScene;

        if(newScene) {
            // If we're skipping the transition because of a non-global
            // DirectShow device, delete the scene here and add the
            // ImageSources at this point instead.
            delete previousScene;

            if(sources)
            {
                UINT numSources = sources->NumElements();

                for(UINT i=0; i<numSources; i++)
                {
                    XElement *sourceElement = sources->GetElementByID(i);

                    if(newScene)
                        newScene->AddImageSource(sourceElement);
                }
            }
        }

        scene->BeginScene();

        numSources = scene->sceneItems.Num();
        for(UINT i=0; i<numSources; i++)
        {
            XElement *source = scene->sceneItems[i]->GetElement();

            String className = source->GetString(TEXT("class"));
            if(scene->sceneItems[i]->bRender && className == "GlobalSource") {
                XElement *globalSourceData = source->GetElement(TEXT("data"));
                String globalSourceName = globalSourceData->GetString(TEXT("name"));
//                 if(App->GetGlobalSource(globalSourceName) != NULL) {
//                     App->GetGlobalSource(globalSourceName)->GlobalSourceEnterScene();
//                 }
            }
        }

        OSLeaveMutex(hSceneMutex);

        DWORD sceneChangeTime = OSGetTime() - sceneChangeStartTime;
        if (sceneChangeTime >= 500)
            Log(TEXT("PERFORMANCE WARNING: Scene change took %u ms, maybe some sources should be global sources?"), sceneChangeTime);
    }

    return true;
}

bool OBS::SetSceneCollection(CTSTR lpCollection) {
    if (bRunning)
        return false;

    App->scenesConfig.Save();
    CTSTR collection = GetCurrentSceneCollection();
    String strSceneCollectionPath;
    strSceneCollectionPath = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, collection);

    if (!App->scenesConfig.Open(strSceneCollectionPath))
    {
        return false;
    }

    GlobalConfig->SetString(TEXT("General"), TEXT("SceneCollection"), lpCollection);
    App->scenesConfig.Close();
    App->ReloadSceneCollection();
    ResetSceneCollectionMenu();
    ResetApplicationName();
//    App->UpdateNotificationAreaIcon();
    App->scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");

    return true;
}

struct HotkeyInfo
{
    DWORD hotkeyID;
    DWORD hotkey;
    OBSHOTKEYPROC hotkeyProc;
    UPARAM param;
    bool bModifiersDown, bHotkeyDown, bDownSent;
};

class OBSAPIInterface : public APIInterface
{
    friend class OBS;

    List<HotkeyInfo> hotkeys;
    DWORD curHotkeyIDVal;

    void HandleHotkeys();
	
    virtual void SetChangedSettings(bool isModified) {App->SetChangedSettings(isModified);}
    virtual void SetAbortApplySettings(bool abort) { App->SetAbortApplySettings(abort); }
    virtual void SetCanOptimizeSettings(bool canOptimize) override { App->SetCanOptimizeSettings(canOptimize); }
	
public:
    virtual void EnterSceneMutex() {App->EnterSceneMutex();}
    virtual void LeaveSceneMutex() {App->LeaveSceneMutex();}

    virtual void StartStopStream()
    {
        PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_STARTSTOP, 0), 0);
    }

    virtual void StartStopPreview()
    {
        //PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_TESTSTREAM, 0), 0);
    }

    virtual void StartStopRecording() override
    {
        PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_TOGGLERECORDING, 0), 0);
    }

    virtual void StartStopRecordingReplayBuffer() override
    {
        PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_TOGGLERECORDINGREPLAYBUFFER, 0), 0);
    }

    virtual void SaveReplayBuffer() override
    {
        //::SaveReplayBuffer(App->replayBuffer, (DWORD)(App->GetVideoTime() - App->firstFrameTimestamp));
    }

    virtual bool GetStreaming()
    {
        return App->bRunning;
    }

    virtual bool GetPreviewOnly()
    {
		return false;
    }

    virtual bool GetRecording() const override
    {
        return App->bRecording;
    }

    virtual bool GetRecordingReplayBuffer() const override
    {
        return App->bRecordingReplayBuffer;
    }

    virtual bool GetKeepRecording() const override
    {
        return App->bKeepRecording;
    }
    
    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
    {
        App->RegisterSceneClass(lpClassName, lpDisplayName, createProc, configProc, false);
    }

    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
    {
        App->RegisterImageSourceClass(lpClassName, lpDisplayName, createProc, configProc, false);
    }

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data)
    {
        return App->CreateImageSource(lpClassName, data);
    }

    virtual XElement* GetSceneListElement()         {return App->scenesConfig.GetElement(TEXT("scenes"));}
    virtual XElement* GetGlobalSourceListElement()  {return App->scenesConfig.GetElement(TEXT("global sources"));}

    virtual void SetSourceOrder(StringList &sourceNames)
    {
        StringList* order = new StringList();
        order->CopyList(sourceNames);
        PostMessage(hwndMain, OBS_SETSOURCEORDER, 0, (LPARAM) order);
    }
    
    virtual void SetSourceRender(CTSTR lpSource, bool render)
    {
        if(!lpSource || !*lpSource)
            return;

        PostMessage(hwndMain, OBS_SETSOURCERENDER, (WPARAM)sdup(lpSource), (LPARAM) render);
    }
    
    virtual bool SetScene(CTSTR lpScene, bool bPost)
    {
        assert(lpScene && *lpScene);

        if(!lpScene || !*lpScene)
            return false;

        if(bPost)
        {
            PostMessage(hwndMain, OBS_SETSCENE, 0, (LPARAM)sdup(lpScene));
            return true;
        }

        return App->SetScene(lpScene);
    }
    virtual Scene* GetScene() const             {return App->scene;}

    virtual CTSTR GetSceneName() const          {return App->GetSceneElement()->GetName();}
    virtual XElement* GetSceneElement()         {return App->GetSceneElement();}

    virtual UINT CreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param);
    virtual void DeleteHotkey(UINT hotkeyID);

    virtual Vect2 GetBaseSize() const               {return Vect2(float(App->baseCX), float(App->baseCY));}
    virtual Vect2 GetRenderFrameSize() const        {return Vect2(float(App->renderFrameWidth), float(App->renderFrameHeight));}
    virtual Vect2 GetRenderFrameOffset() const      {return Vect2(float(App->renderFrameX), float(App->renderFrameY));}
    virtual Vect2 GetRenderFrameControlSize() const {return Vect2(float(App->renderFrameCtrlWidth), float(App->renderFrameCtrlHeight));}
    virtual Vect2 GetOutputSize() const             {return Vect2(float(App->outputCX), float(App->outputCY));}

    virtual void GetBaseSize(UINT &width, UINT &height) const               {App->GetBaseSize(width, height);}
    virtual void GetRenderFrameSize(UINT &width, UINT &height) const        {App->GetRenderFrameSize(width, height);}
    virtual void GetRenderFrameOffset(UINT &x, UINT &y) const               {App->GetRenderFrameOffset(x, y);}
    virtual void GetRenderFrameControlSize(UINT &width, UINT &height) const {App->GetRenderFrameControlSize(width, height);}
    virtual void GetOutputSize(UINT &width, UINT &height) const             {App->GetOutputSize(width, height);}

    virtual Vect2 MapWindowToFramePos(Vect2 mousePos) const     {return App->MapWindowToFramePos(mousePos);}
    virtual Vect2 MapFrameToWindowPos(Vect2 framePos) const     {return App->MapFrameToWindowPos(framePos);}
    virtual Vect2 MapWindowToFrameSize(Vect2 windowSize) const  {return App->MapWindowToFrameSize(windowSize);}
    virtual Vect2 MapFrameToWindowSize(Vect2 frameSize) const   {return App->MapFrameToWindowSize(frameSize);}
    virtual Vect2 GetWindowToFrameScale() const                 {return App->GetWindowToFrameScale();}
    virtual Vect2 GetFrameToWindowScale() const                 {return App->GetFrameToWindowScale();}

    virtual UINT GetMaxFPS() const                  {return App->bRunning ? App->fps : AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);}
    virtual bool GetRenderFrameIn1To1Mode() const   {return App->renderFrameIn1To1Mode;}

    virtual CTSTR GetLanguage() const           {return App->strLanguage;}

    virtual CTSTR GetAppDataPath() const        {return lpAppDataPath;}
    virtual String GetPluginDataPath() const    {return String() << lpAppDataPath << TEXT("\\pluginData");}

    virtual HWND GetMainWindow() const          {return hwndMain;}

    virtual UINT AddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority)           {return App->AddStreamInfo(lpInfo, priority);}
    virtual void SetStreamInfo(UINT infoID, CTSTR lpInfo)                           {App->SetStreamInfo(infoID, lpInfo);}
    virtual void SetStreamInfoPriority(UINT infoID, StreamInfoPriority priority)    {App->SetStreamInfoPriority(infoID, priority);}
    virtual void RemoveStreamInfo(UINT infoID)                                      {App->RemoveStreamInfo(infoID);}

    virtual bool UseMultithreadedOptimizations() const {return App->bUseMultithreadedOptimizations;}

    virtual void AddAudioSource(AudioSource *source) {App->AddAudioSource(source);}
    virtual void RemoveAudioSource(AudioSource *source) {App->RemoveAudioSource(source);}

    virtual QWORD GetAudioTime() const          {return App->GetAudioTime();}

    virtual CTSTR GetAppPath() const            {return lpAppPath;}

    virtual void SetDesktopVolume(float val, bool finalValue)        {App->SetDesktopVolume(val, finalValue);}
    virtual float GetDesktopVolume()                                 {return App->GetDesktopVolume();}
    virtual void ToggleDesktopMute()                                 {App->ToggleDesktopMute();}
    virtual bool GetDesktopMuted()                                   {return App->GetDesktopMuted();}

    virtual void SetMicVolume(float val, bool finalValue)            {App->SetMicVolume(val, finalValue);}
    virtual float GetMicVolume()                                     {return App->GetMicVolume();}
    virtual void ToggleMicMute()                                     {App->ToggleMicMute();}
    virtual bool GetMicMuted()                                       {return App->GetMicMuted();}

    virtual DWORD GetOBSVersion() const {return OBS_VERSION;}

#ifdef OBS_TEST_BUILD
    virtual bool IsTestVersion() const {return 1;}
#else
    virtual bool IsTestVersion() const {return 0;}
#endif

    virtual UINT NumAuxAudioSources() const
    {
        return App->auxAudioSources.Num();
    }

    virtual AudioSource* GetAuxAudioSource(UINT id)
    {
        if(App->auxAudioSources.Num() > id)
            return App->auxAudioSources[id];

        AppWarning(TEXT("Tried to get an aux audio source that doesn't exist!"));
        return NULL;
    }

    virtual AudioSource* GetDesktopAudioSource()    {return App->desktopAudio;}
    virtual AudioSource* GetMicAudioSource()        {return App->micAudio;}

    virtual void GetCurDesktopVolumeStats(float *rms, float *max, float *peak) const
    {
        *rms = App->desktopMag;
        *max = App->desktopMax;
        *peak = App->desktopPeak;
    }

    virtual void GetCurMicVolumeStats(float *rms, float *max, float *peak) const
    {
        *rms = App->micMag;
        *max = App->micMax;
        *peak = App->micPeak;
    }
	
    virtual UINT GetSampleRateHz() const {return App->GetSampleRateHz();}    
    virtual UINT GetCaptureFPS() const        {return App->captureFPS;}
    virtual UINT GetTotalFrames() const       {return App->network ? App->network->NumTotalVideoFrames() : 0;}
    virtual UINT GetFramesDropped() const     {return App->curFramesDropped;}
    virtual UINT GetTotalStreamTime() const   {return App->totalStreamTime;}
    virtual UINT GetBytesPerSec() const       {return App->bytesPerSec;}

    virtual bool SetSceneCollection(CTSTR lpCollection, CTSTR lpScene)
    {
        assert(lpCollection && *lpCollection);

        if (!lpCollection || !*lpCollection)
            return false;

        bool success = App->SetSceneCollection(lpCollection);
        if (lpScene != NULL && success)
        {
            SetScene(lpScene, true);
        }

        return success;
    }
    virtual CTSTR GetSceneCollectionName() const { return App->GetCurrentSceneCollection(); }
    virtual void GetSceneCollectionNames(StringList &list) const { return App->GetSceneCollection(list); }
    virtual void DisableTransitions()          { }
    virtual void EnableTransitions()           { }
	virtual bool TransitionsEnabled() const    { return 0; }
};

APIInterface* CreateOBSApiInterface()
{
    return new OBSAPIInterface;
}


void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(!bDown) return;

    XElement *scenes = API->GetSceneListElement();
    if(scenes)
    {
        UINT numScenes = scenes->NumElements();
        for(UINT i=0; i<numScenes; i++)
        {
            XElement *scene = scenes->GetElementByID(i);
            DWORD sceneHotkey = (DWORD)scene->GetInt(TEXT("hotkey"));
            if(sceneHotkey == hotkey)
            {
                App->SetScene(scene->GetName());
                return;
            }
        }
    }
}

DWORD STDCALL OBS::HotkeyThread(LPVOID lpUseless){
    return 0;
}

void OBS::CallHotkey(DWORD hotkeyID, bool bDown){
}

UINT OBSAPIInterface::CreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param)
{
    if(!hotkey)
        return 0;

    //FIXME: vk and fsModifiers aren't used?
    DWORD vk = LOBYTE(hotkey);
    DWORD modifier = HIBYTE(hotkey);
    DWORD fsModifiers = 0;

    if(modifier & HOTKEYF_ALT)
        fsModifiers |= MOD_ALT;
    if(modifier & HOTKEYF_CONTROL)
        fsModifiers |= MOD_CONTROL;
    if(modifier & HOTKEYF_SHIFT)
        fsModifiers |= MOD_SHIFT;

    OSEnterMutex(App->hHotkeyMutex);
    HotkeyInfo &hi      = *hotkeys.CreateNew();
    hi.hotkeyID         = ++curHotkeyIDVal;
    hi.hotkey           = hotkey;
    hi.hotkeyProc       = hotkeyProc;
    hi.param            = param;
    hi.bModifiersDown   = false;
    hi.bHotkeyDown      = false;
    OSLeaveMutex(App->hHotkeyMutex);

    return curHotkeyIDVal;
}

void OBSAPIInterface::DeleteHotkey(UINT hotkeyID)
{
    OSEnterMutex(App->hHotkeyMutex);
    for(UINT i=0; i<hotkeys.Num(); i++)
    {
        if(hotkeys[i].hotkeyID == hotkeyID)
        {
            hotkeys.Remove(i);
            break;
        }
    }
    OSLeaveMutex(App->hHotkeyMutex);
}

void OBSAPIInterface::HandleHotkeys(){
}

UINT OBS::AddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority){
	return 0;
}

void OBS::SetStreamInfo(UINT infoID, CTSTR lpInfo){
}

void OBS::SetStreamInfoPriority(UINT infoID, StreamInfoPriority priority){
}

void OBS::RemoveStreamInfo(UINT infoID){
}

//todo: get rid of this and use some sort of info window.  this is a really dumb design.  what was I thinking?
String OBS::GetMostImportantInfo(StreamInfoPriority &priority){
    return String();
}

void OBS::SetDesktopVolume(float val, bool finalValue)
{
    val = min(1.0f, max(0, val));
    HWND desktop = GetDlgItem(hwndMain, ID_DESKTOPVOLUME);
    
	/* float in lParam hack */
    LPARAM temp;
	float* tempFloatPointer = (float*)&temp;
	*tempFloatPointer = val;
    
    /*Send message to desktop volume control and have it handle it*/
    PostMessage(desktop, WM_COMMAND, 
        MAKEWPARAM(ID_DESKTOPVOLUME, finalValue?VOLN_FINALVALUE:VOLN_ADJUSTING),
        (LPARAM)temp);
}

float OBS::GetDesktopVolume()
{
    HWND desktop = GetDlgItem(hwndMain, ID_DESKTOPVOLUME);
    return GetVolumeControlValue(desktop);
}

void OBS::ToggleDesktopMute()
{
    HWND desktop = GetDlgItem(hwndMain, ID_DESKTOPVOLUME);
    
    /*Send message to desktop volume control and have it handle it*/
    PostMessage(desktop, WM_COMMAND, MAKEWPARAM(ID_DESKTOPVOLUME, VOLN_TOGGLEMUTE), 0);
}

bool OBS::GetDesktopMuted()
{
    return GetDesktopVolume() < VOLN_MUTELEVEL;
}

void OBS::SetMicVolume(float val, bool finalValue)
{
    val = min(1.0f, max(0, val));
    HWND mic = GetDlgItem(hwndMain, ID_MICVOLUME);
    
	/* float in lParam hack */
    LPARAM temp;
	float* tempFloatPointer = (float*)&temp;
	*tempFloatPointer = val;

    /*Send message to microphone volume control and have it handle it*/
    PostMessage(mic, WM_COMMAND ,
        MAKEWPARAM(ID_MICVOLUME, finalValue?VOLN_FINALVALUE:VOLN_ADJUSTING),
        (LPARAM)temp);
}

float OBS::GetMicVolume()
{
    HWND mic = GetDlgItem(hwndMain, ID_MICVOLUME);
    return GetVolumeControlValue(mic);
}

void OBS::ToggleMicMute()
{
    HWND mic = GetDlgItem(hwndMain, ID_MICVOLUME);
    
    /*Send message to microphone volume control and have it handle it*/
    PostMessage(mic, WM_COMMAND, MAKEWPARAM(ID_MICVOLUME, VOLN_TOGGLEMUTE), 0);
}

bool OBS::GetMicMuted()
{
    return GetMicVolume() < VOLN_MUTELEVEL;
}
