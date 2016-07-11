/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"
#include <shellapi.h>
#include <ShlObj.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <MMSystem.h>

#include <memory>
#include <vector>


//hello, you've come into the file I hate the most.

#define FREEZE_WND(hwnd)   SendMessage(hwnd, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM) 0);
#define THAW_WND(hwnd)     {SendMessage(hwnd, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM) 0); RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);}

extern WNDPROC listboxProc;
extern WNDPROC listviewProc;

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);

enum
{
    ID_LISTBOX_REMOVE = 1,
    ID_LISTBOX_MOVEUP,
    ID_LISTBOX_MOVEDOWN,
    ID_LISTBOX_MOVETOTOP,
    ID_LISTBOX_MOVETOBOTTOM,
    ID_LISTBOX_CENTER,
    ID_LISTBOX_CENTERHOR,
    ID_LISTBOX_CENTERVER,
    ID_LISTBOX_MOVELEFT,
    ID_LISTBOX_MOVETOP,
    ID_LISTBOX_MOVERIGHT,
    ID_LISTBOX_MOVEBOTTOM,
    ID_LISTBOX_FITTOSCREEN,
    ID_LISTBOX_RESETSIZE,
    ID_LISTBOX_RESETCROP,
    ID_LISTBOX_RENAME,
    ID_LISTBOX_COPY,
    ID_LISTBOX_HOTKEY,
    ID_LISTBOX_CONFIG,

    // Render frame related.
    ID_TOGGLERENDERVIEW,
    ID_TOGGLEPANEL,
    ID_TOGGLEFULLSCREEN,
    ID_PREVIEWSCALETOFITMODE,
    ID_PREVIEW1TO1MODE,

    ID_LISTBOX_ADD,

    ID_LISTBOX_GLOBALSOURCE=5000,
    ID_PROJECTOR=6000,
    ID_LISTBOX_COPYTO=7000,
};

INT_PTR CALLBACK OBS::EnterSceneCollectionDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);

                return true;
            }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if (!str.Length())
                        {
                            OBSMessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        String strSceneCollectionPath;
                        strSceneCollectionPath << lpAppDataPath << TEXT("\\sceneCollection\\") << str << TEXT(".xconfig");

                        if (OSFileExists(strSceneCollectionPath))
                        {
                            String strExists = Str("NameExists");
                            strExists.FindReplace(TEXT("$1"), str);
                            OBSMessageBox(hwnd, strExists, NULL, 0);
                            break;
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return false;
}

INT_PTR CALLBACK OBS::EnterProfileDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);

                return true;
            }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if (!str.Length())
                        {
                            OBSMessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        String strProfilePath;
                        strProfilePath << lpAppDataPath << TEXT("\\profiles\\") << str << TEXT(".ini");

                        if (OSFileExists(strProfilePath))
                        {
                            String strExists = Str("NameExists");
                            strExists.FindReplace(TEXT("$1"), str);
                            OBSMessageBox(hwnd, strExists, NULL, 0);
                            break;
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return false;
}

INT_PTR CALLBACK OBS::EnterSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);

                //SetFocus(GetDlgItem(hwnd, IDC_NAME));
                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if(!str.Length())
                        {
                            OBSMessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        if(App->sceneElement)
                        {
                            XElement *sources = App->sceneElement->GetElement(TEXT("sources"));
                            if(!sources)
                                sources = App->sceneElement->CreateElement(TEXT("sources"));

                            XElement *foundSource = sources->GetElement(str);
                            if(foundSource != NULL && strOut != foundSource->GetName())
                            {
                                String strExists = Str("NameExists");
                                strExists.FindReplace(TEXT("$1"), str);
                                OBSMessageBox(hwnd, strExists, NULL, 0);
                                break;
                            }
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return 0;
}

INT_PTR CALLBACK OBS::SceneHotkeyDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                SceneHotkeyInfo *hotkeyInfo = (SceneHotkeyInfo*)lParam;
                SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_SETHOTKEY, hotkeyInfo->hotkey, 0);

                return TRUE;                    
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_CLEAR:
                    if(HIWORD(wParam) == BN_CLICKED)
                        SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_SETHOTKEY, 0, 0);
                    break;

                case IDOK:
                    {
                        SceneHotkeyInfo *hotkeyInfo = (SceneHotkeyInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        DWORD hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_GETHOTKEY, 0, 0);

                        if(hotkey == hotkeyInfo->hotkey)
                        {
                            EndDialog(hwnd, IDCANCEL);
                            break;
                        }

                        if(hotkey)
                        {
                            XElement *scenes = API->GetSceneListElement();
                            UINT numScenes = scenes->NumElements();
                            for(UINT i=0; i<numScenes; i++)
                            {
                                XElement *sceneElement = scenes->GetElementByID(i);
                                if(sceneElement->GetInt(TEXT("hotkey")) == hotkey)
                                {
                                    OBSMessageBox(hwnd, Str("Scene.Hotkey.AlreadyInUse"), NULL, 0);
                                    return 0;
                                }
                            }
                        }

                        hotkeyInfo->hotkey = hotkey;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return 0;
}

INT_PTR CALLBACK OBS::EnterSceneNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if(!str.Length())
                        {
                            OBSMessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                        XElement *foundScene = scenes->GetElement(str);
                        if(foundScene != NULL && strOut != foundScene->GetName())
                        {
                            String strExists = Str("NameExists");
                            strExists.FindReplace(TEXT("$1"), str);
                            OBSMessageBox(hwnd, strExists, NULL, 0);
                            break;
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }
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

INT_PTR CALLBACK OBS::ReconnectDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
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

void OBS::AddSceneCollection(SceneCollectionAction action)
{
    if (App->bRunning)
        return;

    String strCurSceneCollection = GlobalConfig->GetString(TEXT("General"), TEXT("SceneCollection"));

    String strSceneCollection;
    if (action == SceneCollectionAction::Rename)
        strSceneCollection = strCurSceneCollection;

    if (OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneCollectionDialogProc, (LPARAM)&strSceneCollection) != IDOK)
        return;

    App->scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");
    App->scenesConfig.Save();

    String strCurSceneCollectionPath;
    strCurSceneCollectionPath = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, strCurSceneCollection.Array());

    String strSceneCollectionPath;
    strSceneCollectionPath << lpAppDataPath << TEXT("\\sceneCollection\\") << strSceneCollection << TEXT(".xconfig");

    if ((action != SceneCollectionAction::Rename || !strSceneCollectionPath.CompareI(strCurSceneCollectionPath)) && OSFileExists(strSceneCollectionPath))
        OBSMessageBox(hwndMain, Str("Settings.General.ScenesExists"), NULL, 0);
    else
    {
        bool success = true;
        App->scenesConfig.Close(true);

        if (action == SceneCollectionAction::Rename)
        {
            if (!MoveFile(strCurSceneCollectionPath, strSceneCollectionPath))
                success = false;
        }
        else if (action == SceneCollectionAction::Clone)
        {
            if (!CopyFileW(strCurSceneCollectionPath, strSceneCollectionPath, TRUE))
                success = false;
        }
        else
        {
            if (!App->scenesConfig.Open(strSceneCollectionPath))
            {
                OBSMessageBox(hwndMain, TEXT("Error - unable to create new Scene Collection, could not create file"), NULL, 0);
                success = false;
            }
        }

        if (!success)
        {
            App->scenesConfig.Open(strCurSceneCollectionPath);
            return;
        }

        GlobalConfig->SetString(TEXT("General"), TEXT("SceneCollection"), strSceneCollection);

        App->ReloadSceneCollection();
        App->ResetSceneCollectionMenu();
        App->ResetApplicationName();
    }
}

void OBS::RemoveSceneCollection()
{
    if (App->bRunning)
        return;

    String strCurSceneCollection = GlobalConfig->GetString(TEXT("General"), TEXT("SceneCollection"));
    String strCurSceneCollectionFile = strCurSceneCollection + L".xconfig";
    String strCurSceneCollectionDir;
    strCurSceneCollectionDir << lpAppDataPath << TEXT("\\sceneCollection\\");

    OSFindData ofd;
    HANDLE hFind = OSFindFirstFile(strCurSceneCollectionDir + L"*.xconfig", ofd);

    if (!hFind)
    {
        Log(L"Find failed for scene collections");
        return;
    }

    String nextFile;

    do
    {
        if (scmpi(ofd.fileName, strCurSceneCollectionFile) != 0)
        {
            nextFile = ofd.fileName;
            break;
        }
    } while (OSFindNextFile(hFind, ofd));
    OSFindClose(hFind);

    if (nextFile.IsEmpty())
        return;

    String strConfirm = Str("Settings.General.ConfirmDelete");
    strConfirm.FindReplace(TEXT("$1"), strCurSceneCollection);
    if (OBSMessageBox(hwndMain, strConfirm, Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
    {
        String strCurSceneCollectionPath;
        strCurSceneCollectionPath << strCurSceneCollectionDir << strCurSceneCollection << TEXT(".xconfig");
        OSDeleteFile(strCurSceneCollectionPath);
        App->scenesConfig.Close();

        GlobalConfig->SetString(L"General", L"SceneCollection", GetPathWithoutExtension(nextFile));

        App->ReloadSceneCollection();
        App->ResetSceneCollectionMenu();
    }
}

void OBS::ImportSceneCollection()
{
    if (OBSMessageBox(hwndMain, Str("ImportCollectionReplaceWarning.Text"), Str("ImportCollectionReplaceWarning.Title"), MB_YESNO) == IDNO)
        return;

    TCHAR lpFile[MAX_PATH+1];
    zero(lpFile, sizeof(lpFile));

    OPENFILENAME ofn;
    zero(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = lpFile;
    ofn.hwndOwner = hwndMain;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT("Scene Files (*.xconfig)\0*.xconfig\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = GlobalConfig->GetString(L"General", L"LastImportExportPath");

    TCHAR curDirectory[MAX_PATH+1];
    GetCurrentDirectory(MAX_PATH, curDirectory);

    BOOL bOpenFile = GetOpenFileName(&ofn);
    SetCurrentDirectory(curDirectory);

    if (!bOpenFile)
        return;

    if (GetPathExtension(lpFile).IsEmpty())
        scat(lpFile, L".xconfig");

    GlobalConfig->SetString(L"General", L"LastImportExportPath", GetPathDirectory(lpFile));

    String strCurSceneCollection = GlobalConfig->GetString(TEXT("General"), TEXT("SceneCollection"));
    String strCurSceneCollectionFile;
    strCurSceneCollectionFile << lpAppDataPath << TEXT("\\sceneCollection\\") << strCurSceneCollection << L".xconfig";

    scenesConfig.Close();
    CopyFile(lpFile, strCurSceneCollectionFile, false);
    App->ReloadSceneCollection();
}

void OBS::ExportSceneCollection()
{
    TCHAR lpFile[MAX_PATH+1];
    zero(lpFile, sizeof(lpFile));

    OPENFILENAME ofn;
    zero(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = lpFile;
    ofn.hwndOwner = hwndMain;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT("Scene Files (*.xconfig)\0*.xconfig\0");
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrInitialDir = GlobalConfig->GetString(L"General", L"LastImportExportPath");

    TCHAR curDirectory[MAX_PATH+1];
    GetCurrentDirectory(MAX_PATH, curDirectory);

    BOOL bSaveFile = GetSaveFileName(&ofn);
    SetCurrentDirectory(curDirectory);

    if (!bSaveFile)
        return;

    if (GetPathExtension(lpFile).IsEmpty())
        scat(lpFile, L".xconfig");

    GlobalConfig->SetString(L"General", L"LastImportExportPath", GetPathDirectory(lpFile));

    scenesConfig.SaveTo(lpFile);
}

//----------------------------

void OBS::AddProfile(ProfileAction action)
{
    if (App->bRunning)
        return;

    String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));

    String strProfile;
    if (action == ProfileAction::Rename)
        strProfile = strCurProfile;

    if (OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterProfileDialogProc, (LPARAM)&strProfile) != IDOK)
        return;

    String strCurProfilePath;
    strCurProfilePath = FormattedString(L"%s\\profiles\\%s.ini", lpAppDataPath, strCurProfile.Array());

    String strProfilePath;
    strProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

    if ((action != ProfileAction::Rename || !strProfilePath.CompareI(strCurProfilePath)) && OSFileExists(strProfilePath))
        OBSMessageBox(hwndMain, Str("MainMenu.Profiles.ProfileExists"), NULL, 0);
    else
    {
        bool success = true;

        if (action == ProfileAction::Rename)
        {
            if (!MoveFile(strCurProfilePath, strProfilePath))
                success = false;
            AppConfig->SetFilePath(strProfilePath);
        }
        else if (action == ProfileAction::Clone)
        {
            if (!CopyFileW(strCurProfilePath, strProfilePath, TRUE))
                success = false;
        }
        else
        {
            if(!AppConfig->Create(strProfilePath))
            {
                OBSMessageBox(hwndMain, TEXT("Error - unable to create new profile, could not create file"), NULL, 0);
                return;
            }
        }

        if (!success)
        {
            AppConfig->Open(strCurProfilePath);
            return;
        }

        GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);

        App->ReloadIniSettings();
        App->ResetProfileMenu();
        App->ResetApplicationName();
    }
}

void OBS::RemoveProfile()
{
    if (App->bRunning)
        return;

    String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));

    String strCurProfileFile = strCurProfile + L".ini";
    String strCurProfileDir;
    strCurProfileDir << lpAppDataPath << TEXT("\\profiles\\");

    OSFindData ofd;
    HANDLE hFind = OSFindFirstFile(strCurProfileDir + L"*.ini", ofd);

    if (!hFind)
    {
        Log(L"Find failed for profile");
        return;
    }

    String nextFile;

    do
    {
        if (scmpi(ofd.fileName, strCurProfileFile) != 0)
        {
            nextFile = ofd.fileName;
            break;
        }
    } while (OSFindNextFile(hFind, ofd));
    OSFindClose(hFind);

    if (nextFile.IsEmpty())
        return;

    String strConfirm = Str("Settings.General.ConfirmDelete");
    strConfirm.FindReplace(TEXT("$1"), strCurProfile);
    if (OBSMessageBox(hwndMain, strConfirm, Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
    {
        String strCurProfilePath;
        strCurProfilePath << strCurProfileDir << strCurProfile << TEXT(".ini");
        OSDeleteFile(strCurProfilePath);

        GlobalConfig->SetString(L"General", L"Profile", GetPathWithoutExtension(nextFile));

        strCurProfilePath.Clear();
        strCurProfilePath << strCurProfileDir << nextFile;
        if (!AppConfig->Open(strCurProfilePath))
        {
            OBSMessageBox(hwndMain, TEXT("Error - unable to open ini file"), NULL, 0);
            return;
        }

        App->ReloadIniSettings();
        App->ResetApplicationName();
        App->ResetProfileMenu();
    }
}

void OBS::ImportProfile()
{
    if (OBSMessageBox(hwndMain, Str("ImportProfileReplaceWarning.Text"), Str("ImportProfileReplaceWarning.Title"), MB_YESNO) == IDNO)
        return;

    TCHAR lpFile[MAX_PATH+1];
    zero(lpFile, sizeof(lpFile));

    OPENFILENAME ofn;
    zero(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = lpFile;
    ofn.hwndOwner = hwndMain;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT("Profile Files (*.ini)\0*.ini\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = GlobalConfig->GetString(L"General", L"LastImportExportPath");

    TCHAR curDirectory[MAX_PATH+1];
    GetCurrentDirectory(MAX_PATH, curDirectory);

    BOOL bOpenFile = GetOpenFileName(&ofn);
    SetCurrentDirectory(curDirectory);

    if (!bOpenFile)
        return;

    if (GetPathExtension(lpFile).IsEmpty())
        scat(lpFile, L".ini");

    GlobalConfig->SetString(L"General", L"LastImportExportPath", GetPathDirectory(lpFile));

    String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
    String strCurProfileFile;
    strCurProfileFile << lpAppDataPath << TEXT("\\profiles\\") << strCurProfile << L".ini";

    CopyFile(lpFile, strCurProfileFile, false);

    if(!AppConfig->Open(strCurProfileFile))
    {
        OBSMessageBox(hwndMain, TEXT("Error - unable to open ini file"), NULL, 0);
        return;
    }

    App->ReloadIniSettings();
}

void OBS::ExportProfile()
{
    TCHAR lpFile[MAX_PATH+1];
    zero(lpFile, sizeof(lpFile));

    OPENFILENAME ofn;
    zero(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = lpFile;
    ofn.hwndOwner = hwndMain;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT("Profile Files (*.ini)\0*.ini\0");
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrInitialDir = GlobalConfig->GetString(L"General", L"LastImportExportPath");

    TCHAR curDirectory[MAX_PATH+1];
    GetCurrentDirectory(MAX_PATH, curDirectory);

    BOOL bSaveFile = GetSaveFileName(&ofn);
    SetCurrentDirectory(curDirectory);

    if (!bSaveFile)
        return;

    if (GetPathExtension(lpFile).IsEmpty())
        scat(lpFile, L".ini");

    String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
    String strCurProfileFile;
    strCurProfileFile << lpAppDataPath << TEXT("\\profiles\\") << strCurProfile << L".ini";

    GlobalConfig->SetString(L"General", L"LastImportExportPath", GetPathDirectory(lpFile));

    CopyFile(strCurProfileFile, lpFile,  false);
}


//----------------------------

void OBS::ResetSceneCollectionMenu()
{
    HMENU hmenuMain = GetMenu(hwndMain);
    HMENU hmenuSceneCollection = GetSubMenu(hmenuMain, 3);
    while (DeleteMenu(hmenuSceneCollection, 8, MF_BYPOSITION));
    AddSceneCollectionToMenu(hmenuSceneCollection);
}

void OBS::ResetProfileMenu()
{
    HMENU hmenuMain = GetMenu(hwndMain);
    HMENU hmenuProfiles = GetSubMenu(hmenuMain, 2);
    while (DeleteMenu(hmenuProfiles, 8, MF_BYPOSITION));
    AddProfilesToMenu(hmenuProfiles);
}

//----------------------------

void OBS::DisableMenusWhileStreaming(bool disable)
{
    HMENU hmenuMain = GetMenu(hwndMain);

    EnableMenuItem(hmenuMain, 2, (!disable ? MF_ENABLED : MF_DISABLED) | MF_BYPOSITION);
    EnableMenuItem(hmenuMain, 3, (!disable ? MF_ENABLED : MF_DISABLED) | MF_BYPOSITION);

    EnableMenuItem(hmenuMain, ID_HELP_UPLOAD_CURRENT_LOG, (!disable ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hmenuMain, ID_HELP_ANALYZE_CURRENT_LOG, (!disable ? MF_ENABLED : MF_DISABLED));

    DrawMenuBar(hwndMain);
}

//----------------------------

void LogUploadMonitorCallback()
{
    PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_REFRESH_LOGS, 0), 0);
}

//----------------------------

static HMENU FindParent(HMENU root, UINT id, String *name=nullptr)
{
    MENUITEMINFO info;
    zero(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_SUBMENU | (name ? MIIM_STRING : 0);

    MENUITEMINFO verifier;
    zero(&verifier, sizeof(verifier));
    verifier.cbSize = sizeof(verifier);

    bool found = false;

    int count = GetMenuItemCount(root);
    for (int i = 0; i < count; i++)
    {
        info.cch = 0;
        if (!GetMenuItemInfo(root, i, true, &info))
            continue;

        if (!info.hSubMenu)
            continue;

        HMENU submenu = info.hSubMenu;
        if (!GetMenuItemInfo(submenu, id, false, &verifier))
            continue;

        if (name)
        {
            name->SetLength(info.cch++);
            info.dwTypeData = name->Array();
            GetMenuItemInfo(root, i, true, &info);
            info.dwTypeData = nullptr;
        }

        found = true;

        root = submenu;
        i = 0;
        count = GetMenuItemCount(root);
    }

    return found ? root : nullptr;
}

void OBS::ResetLogUploadMenu()
{
    String logfilePattern = FormattedString(L"%s/logs/*.log", OBSGetAppDataPath());

    std::vector<decltype(App->logFiles.cbegin())> validLogs;

    OSFindData ofd;
    HANDLE finder;
    if (!App->logDirectoryMonitor)
    {
        App->logDirectoryMonitor = OSMonitorDirectoryCallback(String(OBSGetAppDataPath()) << L"/logs/", LogUploadMonitorCallback);

        if (!(finder = OSFindFirstFile(logfilePattern, ofd)))
            return;

        char *contents = (char*)Allocate(1024 * 8);

        do
        {
            if (ofd.bDirectory) continue;

            String filename = GetPathFileName(ofd.fileName, true);
            auto iter = App->logFiles.emplace(filename.Array(), false).first;

            XFile f(String(OBSGetAppDataPath()) << L"/logs/" << filename, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING);
            if (!f.IsOpen())
                continue;

            DWORD nRead = f.Read(contents, 1024*8 - 1);
            contents[nRead] = 0;

            bool validLog = (strstr(contents, "Open Broadcaster Software") != nullptr);

            if (!validLog)
                continue;

            iter->second = true;
            validLogs.push_back(iter);
        } while (OSFindNextFile(finder, ofd));

        Free(contents);
    }
    else
    {
        if (finder = OSFindFirstFile(logfilePattern, ofd))
        {
            auto previous = std::move(App->logFiles);

            App->logFiles.clear();

            do
            {
                if (ofd.bDirectory) continue;

                std::wstring log = GetPathFileName(ofd.fileName, true);
                if (previous.find(log) == previous.end())
                    continue;

                if (!(App->logFiles[log] = previous[log]))
                    continue;

                validLogs.push_back(App->logFiles.find(log));
            } while (OSFindNextFile(finder, ofd));
        }
        else
            App->logFiles.clear();
    }

    HMENU hmenuMain = GetMenu(hwndMain);
    HMENU hmenuUpload = FindParent(hmenuMain, ID_HELP_UPLOAD_CURRENT_LOG);
    if (!hmenuUpload)
        return;

    while (DeleteMenu(hmenuUpload, 2, MF_BYPOSITION));

    if (validLogs.empty())
        return;

    AppendMenu(hmenuUpload, MF_SEPARATOR, 0, nullptr);

    AppendMenu(hmenuUpload, MF_STRING, ID_UPLOAD_ANALYZE_LOG, Str("MainMenu.Help.AnalyzeLastLog"));
    AppendMenu(hmenuUpload, MF_STRING, ID_UPLOAD_LOG, Str("MainMenu.Help.UploadLastLog"));

    AppendMenu(hmenuUpload, MF_SEPARATOR, 0, nullptr);

    unsigned i = 0;
    for (auto iter = validLogs.rbegin(); iter != validLogs.rend(); i++, iter++)
    {
        HMENU items = CreateMenu();
        AppendMenu(items, MF_STRING, ID_UPLOAD_ANALYZE_LOG + i, Str("LogUpload.Analyze"));
        AppendMenu(items, MF_STRING, ID_UPLOAD_LOG + i, Str("LogUpload.Upload"));
        AppendMenu(items, MF_STRING, ID_VIEW_LOG + i, Str("LogUpload.View"));

        AppendMenu(hmenuUpload, MF_STRING | MF_POPUP, (UINT_PTR)items, (*iter)->first.c_str());
    }
}

//----------------------------

String GetLogUploadMenuItem(UINT item)
{
    HMENU hmenuMain = GetMenu(hwndMain);

    String log;
    FindParent(hmenuMain, item, &log);

    return log;
}

//----------------------------

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