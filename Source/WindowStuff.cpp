#include "Main.h"
#include <shellapi.h>
#include <ShlObj.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <MMSystem.h>

#include <memory>
#include <vector>

#define FREEZE_WND(hwnd)   SendMessage(hwnd, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM) 0);
#define THAW_WND(hwnd)     {SendMessage(hwnd, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM) 0); RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);}

extern WNDPROC listboxProc;
extern WNDPROC listviewProc;

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);

INT_PTR CALLBACK OBS::EnterSceneCollectionDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return false;
}

INT_PTR CALLBACK OBS::EnterProfileDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return false;
}

INT_PTR CALLBACK OBS::EnterSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return 0;
}

INT_PTR CALLBACK OBS::SceneHotkeyDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return 0;
}

INT_PTR CALLBACK OBS::EnterSceneNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return 0;
}

void OBS::GetNewSceneName(String &strScene)
{
    XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
    if(scenes)
    {
        String strTestName = strScene;

        UINT num = 1;
        while(scenes->GetElement(strTestName) != NULL)
            strTestName.Clear() << strScene << FormattedString(TEXT(" %u"), ++num);

        strScene = strTestName;
    }
}

void OBS::GetNewSourceName(String &strSource)
{
    XElement *sceneElement = API->GetSceneElement();
    if(sceneElement)
    {
        XElement *sources = sceneElement->GetElement(TEXT("sources"));
        if(!sources)
            sources = sceneElement->CreateElement(TEXT("sources"));

        String strTestName = strSource;

        UINT num = 1;
        while(sources->GetElement(strTestName) != NULL)
            strTestName.Clear() << strSource << FormattedString(TEXT(" %u"), ++num);

        strSource = strTestName;
    }
}

LRESULT CALLBACK OBS::ListboxHook(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UINT id = (UINT)GetWindowLongPtr(hwnd, GWL_ID);
	return CallWindowProc(listviewProc, hwnd, message, wParam, lParam); 
}

void OBS::TrackModifyListbox(HWND hwnd, int ret){
}

void OBS::AppendModifyListbox(HWND hwnd, HMENU hMenu, int id, int numItems, bool bSelected){
}

void OBS::DeleteItems(){
}

void OBS::SetSourceOrder(StringList &sourceNames){
}

void OBS::MoveSourcesUp(){
}

void OBS::MoveSourcesDown(){
}

void OBS::MoveSourcesToTop(){   
}

void OBS::MoveSourcesToBottom(){
}

void OBS::CenterItems(bool horizontal, bool vertical){
}

void OBS::MoveItemsToEdge(int horizontal, int vertical){
}

void OBS::MoveItemsByPixels(int dx, int dy){
}

void OBS::FitItemsToScreen(){
}

void OBS::ResetItemSizes(){
}

void OBS::ResetItemCrops(){
}

//----------------------------

INT_PTR CALLBACK OBS::EnterGlobalSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return 0;
}

INT_PTR CALLBACK OBS::GlobalSourcesProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    return FALSE;
}

INT_PTR CALLBACK OBS::GlobalSourcesImportProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
	return 0;
}

//----------------------------

struct ReconnectInfo
{
    UINT_PTR timerID;
    UINT secondsLeft;
};

INT_PTR CALLBACK OBS::ReconnectDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
   /* switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                ReconnectInfo *ri = new ReconnectInfo;
                ri->secondsLeft = App->reconnectTimeout;
                ri->timerID = 1;

                if(!SetTimer(hwnd, 1, 1000, NULL))
                {
                    App->bReconnecting = false;
                    EndDialog(hwnd, IDCANCEL);
                    delete ri;
                    return TRUE;
                }

                String strText;
                if(App->bReconnecting)
                    strText << Str("Reconnecting.Retrying") << UIntString(ri->secondsLeft);
                else
                    strText << Str("Reconnecting") << UIntString(ri->secondsLeft);

                SetWindowText(GetDlgItem(hwnd, IDC_RECONNECTING), strText);

                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)ri);
                return TRUE;
            }

        case WM_TIMER:
            {
                ReconnectInfo *ri = (ReconnectInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                if(wParam != 1)
                    break;

                if(!ri->secondsLeft)
                {
                    if (AppConfig->GetInt(TEXT("Publish"), TEXT("ExperimentalReconnectMode")) == 1 && AppConfig->GetInt(TEXT("Publish"), TEXT("Delay")) == 0)
                        App->RestartNetwork();
                    else
                        SendMessage(hwndMain, OBS_RECONNECT, 0, 0);
                    EndDialog(hwnd, IDOK);
                }
                else
                {
                    String strText;

                    ri->secondsLeft--;

                    if(App->bReconnecting)
                        strText << Str("Reconnecting.Retrying") << UIntString(ri->secondsLeft);
                    else
                        strText << Str("Reconnecting") << UIntString(ri->secondsLeft);

                    SetWindowText(GetDlgItem(hwnd, IDC_RECONNECTING), strText);
                }
                break;
            }

        case WM_COMMAND:
            if(LOWORD(wParam) == IDCANCEL)
            {
                App->bReconnecting = false;
                if (AppConfig->GetInt(TEXT("Publish"), TEXT("ExperimentalReconnectMode")) == 1 && AppConfig->GetInt(TEXT("Publish"), TEXT("Delay")) == 0)
                    App->Stop();
                EndDialog(hwnd, IDCANCEL);
            }
            break;

        case WM_CLOSE:
            App->bReconnecting = false;
            if (AppConfig->GetInt(TEXT("Publish"), TEXT("ExperimentalReconnectMode")) == 1 && AppConfig->GetInt(TEXT("Publish"), TEXT("Delay")) == 0)
                App->Stop();
            EndDialog(hwnd, IDCANCEL);
            break;

        case WM_DESTROY:
            {
                ReconnectInfo *ri = (ReconnectInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                KillTimer(hwnd, ri->timerID);
                delete ri;
            }
    }
*/
    return FALSE;
}

//----------------------------

void OBS::AddProfilesToMenu(HMENU menu)
{
    StringList profileList;
    GetProfiles(profileList);

    for(UINT i=0; i<profileList.Num(); i++)
    {
        String &strProfile = profileList[i];

        UINT flags = MF_STRING;
        if(strProfile.CompareI(GetCurrentProfile()))
            flags |= MF_CHECKED;

        AppendMenu(menu, flags, ID_SWITCHPROFILE+i, strProfile.Array());
    }
}

void OBS::AddSceneCollectionToMenu(HMENU menu)
{
    StringList sceneCollectionList;
    GetSceneCollection(sceneCollectionList);

    for (UINT i = 0; i < sceneCollectionList.Num(); i++)
    {
        String &strSceneCollection = sceneCollectionList[i];

        UINT flags = MF_STRING;
        if (strSceneCollection.CompareI(GetCurrentSceneCollection()))
            flags |= MF_CHECKED;

        AppendMenu(menu, flags, ID_SWITCHSCENECOLLECTION+i, strSceneCollection.Array());
    }
}

//----------------------------

void OBS::AddSceneCollection(SceneCollectionAction action){
}

void OBS::RemoveSceneCollection(){
}

void OBS::ImportSceneCollection(){
}

void OBS::ExportSceneCollection(){
}

void OBS::AddProfile(ProfileAction action){
}

void OBS::RemoveProfile(){
}

void OBS::ImportProfile(){
}

void OBS::ExportProfile(){
}

void OBS::ResetSceneCollectionMenu(){
}

void OBS::ResetProfileMenu(){
}

void OBS::DisableMenusWhileStreaming(bool disable){
}

void LogUploadMonitorCallback(){
    PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_REFRESH_LOGS, 0), 0);
}

static HMENU FindParent(HMENU root, UINT id, String *name=nullptr){
    return nullptr;
}

void OBS::ResetLogUploadMenu(){
}

String GetLogUploadMenuItem(UINT item){
    return String();
}

namespace
{
    struct HLOCALDeleter
    {
        void operator()(void *h) { GlobalFree(h); }
    };

    struct MemUnlocker
    {
        void operator()(void *m) { GlobalUnlock(m); }
    };

    struct ClipboardHelper
    {
        ClipboardHelper(HWND owner) { OpenClipboard(owner); }
        ~ClipboardHelper() { CloseClipboard(); }
        bool Insert(String &str)
        {
            using namespace std;

            if (!EmptyClipboard()) return false;

            unique_ptr<void, HLOCALDeleter> h(GlobalAlloc(LMEM_MOVEABLE, (str.Length() + 1) * sizeof TCHAR));
            if (!h) return false;

            unique_ptr<void, MemUnlocker> mem(GlobalLock(h.get()));
            if (!mem) return false;

            tstr_to_wide(str.Array(), (wchar_t*)mem.get(), str.Length() + 1);

            if (!SetClipboardData(CF_UNICODETEXT, mem.get())) return false;

            h.release();
            return true;
        }

        bool Contains(String &str)
        {
            using namespace std;

            HANDLE h = GetClipboardData(CF_UNICODETEXT);
            if (!h) return false;

            unique_ptr<void, MemUnlocker> mem(GlobalLock(h));
            if (!mem) return false;

            return !!str.Compare((CTSTR)mem.get());
        }
    };
}

//----------------------------

String OBS::GetApplicationName()
{
    String name;

    // we hide the bit version on 32 bit to avoid confusing users who have a 64
    // bit pc unncessarily asking for the 64 bit version under the assumption
    // that the 32 bit version doesn't work or something.
    name << "Profile: " << App->GetCurrentProfile() << " - " << "Scenes: " << App->GetCurrentSceneCollection() << L" - " OBS_VERSION_STRING
#ifdef _WIN64
    L" - 64bit";
#else
    L"";
#endif
    return name;
}

//----------------------------

void OBS::ResetApplicationName(){
    SetWindowText(hwndMain, GetApplicationName());
}

//----------------------------
LRESULT CALLBACK OBS::OBSProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
	return DefWindowProc(hwnd, message, wParam, lParam);;
}

ItemModifyType GetItemModifyType(const Vect2 &mousePos, const Vect2 &itemPos, const Vect2 &itemSize, const Vect4 &crop, const Vect2 &scaleVal)
{
    Vect2 lowerRight = itemPos+itemSize;
    float epsilon = 10.0f;

    Vect2 croppedItemPos = itemPos + Vect2(crop.x / scaleVal.x, crop.y / scaleVal.y);
    Vect2 croppedLowerRight = lowerRight - Vect2(crop.w / scaleVal.x, crop.z / scaleVal.y);

    if( mousePos.x < croppedItemPos.x    ||
        mousePos.y < croppedItemPos.y    ||
        mousePos.x > croppedLowerRight.x ||
        mousePos.y > croppedLowerRight.y )
    {
        return ItemModifyType_None;
    }    

    // Corner sizing
    if(mousePos.CloseTo(croppedItemPos, epsilon))
        return ItemModifyType_ScaleTopLeft;
    else if(mousePos.CloseTo(croppedLowerRight, epsilon))
        return ItemModifyType_ScaleBottomRight;
    else if(mousePos.CloseTo(Vect2(croppedLowerRight.x, croppedItemPos.y), epsilon))
        return ItemModifyType_ScaleTopRight;
    else if(mousePos.CloseTo(Vect2(croppedItemPos.x, croppedLowerRight.y), epsilon))
        return ItemModifyType_ScaleBottomLeft;

    epsilon = 4.0f;

    // Edge sizing
    if(CloseFloat(mousePos.x, croppedItemPos.x, epsilon))
        return ItemModifyType_ScaleLeft;
    else if(CloseFloat(mousePos.x, croppedLowerRight.x, epsilon))
        return ItemModifyType_ScaleRight;
    else if(CloseFloat(mousePos.y, croppedItemPos.y, epsilon))
        return ItemModifyType_ScaleTop;
    else if(CloseFloat(mousePos.y, croppedLowerRight.y, epsilon))
        return ItemModifyType_ScaleBottom;


    return ItemModifyType_Move;
}

/**
 * Maps a point in window coordinates to frame coordinates.
 */
Vect2 OBS::MapWindowToFramePos(Vect2 mousePos)
{
    if(App->renderFrameIn1To1Mode)
        return (mousePos - App->GetRenderFrameOffset()) * (App->GetBaseSize() / App->GetOutputSize());
    return (mousePos - App->GetRenderFrameOffset()) * (App->GetBaseSize() / App->GetRenderFrameSize());
}

/**
 * Maps a point in frame coordinates to window coordinates.
 */
Vect2 OBS::MapFrameToWindowPos(Vect2 framePos)
{
    if(App->renderFrameIn1To1Mode)
        return framePos * (App->GetOutputSize() / App->GetBaseSize()) + App->GetRenderFrameOffset();
    return framePos * (App->GetRenderFrameSize() / App->GetBaseSize()) + App->GetRenderFrameOffset();
}

/**
 * Maps a size in window coordinates to frame coordinates.
 */

Vect2 OBS::MapWindowToFrameSize(Vect2 windowSize)
{
    if(App->renderFrameIn1To1Mode)
        return windowSize * (App->GetBaseSize() / App->GetOutputSize());
    return windowSize * (App->GetBaseSize() / App->GetRenderFrameSize());
}

/**
 * Maps a size in frame coordinates to window coordinates.
 */
Vect2 OBS::MapFrameToWindowSize(Vect2 frameSize)
{
    if(App->renderFrameIn1To1Mode)
        return frameSize * (App->GetOutputSize() / App->GetBaseSize());
    return frameSize * (App->GetRenderFrameSize() / App->GetBaseSize());
}

/**
 * Returns the scale of the window relative to the actual frame size. E.g.
 * if the window is twice the size of the frame this will return "0.5".
 */
Vect2 OBS::GetWindowToFrameScale()
{
    return MapWindowToFrameSize(Vect2(1.0f, 1.0f));
}

/**
 * Returns the scale of the frame relative to the window size. E.g.
 * if the window is twice the size of the frame this will return "2.0".
 */
Vect2 OBS::GetFrameToWindowScale()
{
    return MapFrameToWindowSize(Vect2(1.0f, 1.0f));
}