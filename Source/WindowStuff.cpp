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

    if(message == WM_RBUTTONDOWN)
    {
        int numItems = 0;
        if(id == ID_SCENES)
        {
            CallWindowProc(listboxProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);
            numItems = (int)SendMessage(hwnd, LB_GETCOUNT, 0, 0);
        }
        else
        {
            LVHITTESTINFO htInfo;
            int index;

            // Default behaviour of left/right click is to check/uncheck, we do not want to toggle items when right clicking above checkbox.

            numItems = ListView_GetItemCount(hwnd);

            GetCursorPos(&htInfo.pt);
            ScreenToClient(hwnd, &htInfo.pt);

            index = ListView_HitTestEx(hwnd, &htInfo);

            if(index != -1)
            {
                // Focus our control
                if(GetFocus() != hwnd)
                    SetFocus(hwnd);
                
                // Clear all selected items state and select/focus the item we've right-clicked if it wasn't previously selected.
                if(!(ListView_GetItemState(hwnd, index, LVIS_SELECTED) & LVIS_SELECTED))
                {
                    ListView_SetItemState(hwnd , -1 , 0, LVIS_SELECTED | LVIS_FOCUSED);
                
                    ListView_SetItemState(hwnd, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

                    ListView_SetSelectionMark(hwnd, index);
                }
            }
            else
            {
                ListView_SetItemState(hwnd , -1 , 0, LVIS_SELECTED | LVIS_FOCUSED)
                CallWindowProc(listviewProc, hwnd, WM_RBUTTONDOWN, wParam, lParam);
            }
        }

        HMENU hMenu = CreatePopupMenu();

        bool bSelected = true;

        if(id == ID_SCENES)
        {
            SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));

            for(UINT i=0; i<App->sceneClasses.Num(); i++)
            {
                if (App->sceneClasses[i].bDeprecated)
                    continue;

                String strAdd = Str("Listbox.Add");
                strAdd.FindReplace(TEXT("$1"), App->sceneClasses[i].strName);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_ADD+i, strAdd.Array());
            }
        }
        else if(id == ID_SOURCES)
        {
            if(!App->sceneElement)
                return 0;

            bSelected = ListView_GetSelectedCount(hwnd) != 0;
        }

        App->AppendModifyListbox(hwnd, hMenu, id, numItems, bSelected);

        POINT p;
        GetCursorPos(&p);

        int curSel = (id== ID_SOURCES)?(ListView_GetNextItem(hwnd, -1, LVNI_SELECTED)):((int)SendMessage(hwnd, LB_GETCURSEL, 0, 0));

        XElement *curSceneElement = App->sceneElement;

        if(id == ID_SCENES)
        {
            XElement *item = App->sceneElement;
            if(!item && numItems)
                return 0;

            ClassInfo *curClassInfo = NULL;
            if(numItems)
            {
                curClassInfo = App->GetSceneClass(item->GetString(TEXT("class")));
                if(!curClassInfo)
                {
                    curSceneElement->AddString(TEXT("class"), TEXT("Scene"));
                    curClassInfo = App->GetSceneClass(item->GetString(TEXT("class")));
                }

                if(curClassInfo && curClassInfo->configProc)
                {
                    AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                    AppendMenu(hMenu, MF_STRING, ID_LISTBOX_CONFIG, Str("Listbox.Config"));
                }
            }

            bool bDelete = false;

            int ret = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
            switch(ret)
            {
                default:
                    if(ret >= ID_LISTBOX_ADD && ret < ID_LISTBOX_COPYTO)
                    {
                        App->EnableSceneSwitching(false);

                        String strName = Str("Scene");
                        GetNewSceneName(strName);

                        if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            UINT classID = ret-ID_LISTBOX_ADD;
                            ClassInfo &ci = App->sceneClasses[classID];

                            XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                            XElement *newSceneElement = scenes->CreateElement(strName);

                            newSceneElement->SetString(TEXT("class"), ci.strClass);
                            if(ci.configProc)
                            {
                                if(!ci.configProc(newSceneElement, true))
                                {
                                    scenes->RemoveElement(newSceneElement);
                                    break;
                                }
                            }
                            
                            UINT newID = (UINT)SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)strName.Array());
                            PostMessage(hwnd, LB_SETCURSEL, newID, 0);
                            PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)hwnd);
                        }

                        App->EnableSceneSwitching(true);
                    }

                    if(ret >= ID_LISTBOX_COPYTO)
                    {
                        App->EnableSceneSwitching(false);

                        StringList sceneCollectionList;
                        App->GetSceneCollection(sceneCollectionList);

                        for(UINT i = 0; i < sceneCollectionList.Num(); i++)
                        {
                            if(sceneCollectionList[i] == App->GetCurrentSceneCollection())
                            {
                                sceneCollectionList.Remove(i);
                            }
                        }

                        UINT classID = ret - ID_LISTBOX_COPYTO;

                        String strScenesCopyToConfig;
                        strScenesCopyToConfig = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, sceneCollectionList[classID].Array());

                        if(!App->scenesCopyToConfig.Open(strScenesCopyToConfig))
                            CrashError(TEXT("Could not open '%s"), strScenesCopyToConfig.Array());

                        XElement *currentSceneCollection = App->scenesConfig.GetElement(TEXT("scenes"));

                        XElement *selectedScene = currentSceneCollection->GetElementByID(curSel);
                        XElement *copyToSceneCollection = App->scenesCopyToConfig.GetElement(TEXT("scenes"));

                        if(!copyToSceneCollection)
                            copyToSceneCollection = App->scenesCopyToConfig.CreateElement(TEXT("scenes"));

                        if(copyToSceneCollection)
                        {
                            XElement *foundGlobalSource = copyToSceneCollection->GetElement(selectedScene->GetName());
                            if(foundGlobalSource != NULL && selectedScene->GetName() != foundGlobalSource->GetName())
                            {
                                App->scenesCopyToConfig.Close();
                                App->EnableSceneSwitching(true);
                                String strExists = Str("CopyTo.SceneNameExists");
                                strExists.FindReplace(TEXT("$1"), selectedScene->GetName());
                                OBSMessageBox(hwnd, strExists, NULL, 0);
                                break;
                            }
                        }

                        XElement *newSceneElement = copyToSceneCollection->CopyElement(selectedScene, selectedScene->GetName());
                        newSceneElement->SetString(TEXT("class"), selectedScene->GetString(TEXT("class")));

                        bool globalSourceRefCheck = false;

                        XElement *newSceneSourcesGsRefCheck = newSceneElement->GetElement(TEXT("sources"));

                        if(newSceneSourcesGsRefCheck)
                        {
                            UINT numSources = newSceneSourcesGsRefCheck->NumElements();

                            for(int i = int(numSources - 1); i >= 0; i--)
                            {
                                XElement *sourceElement = newSceneSourcesGsRefCheck->GetElementByID(i);
                                String sourceClassName = sourceElement->GetString(TEXT("class"));

                                if(sourceClassName == "GlobalSource")
                                {
                                 globalSourceRefCheck = true;
                                }
                            }
                        }

                        if(globalSourceRefCheck)
                        {
                            if(OBSMessageBox(hwndMain, Str("CopyTo.CopyGlobalSourcesReferences"), Str("CopyTo.CopyGlobalSourcesReferences.Title"), MB_YESNO) == IDYES)
                            {
                                XElement *newSceneSources = newSceneElement->GetElement(TEXT("sources"));

                                if(newSceneSources)
                                {
                                    UINT numSources = newSceneSources->NumElements();

                                    for(int i = int(numSources - 1); i >= 0; i--)
                                    {
                                        XElement *sourceElement = newSceneSources->GetElementByID(i);
                                        String sourceClassName = sourceElement->GetString(TEXT("class"));

                                        if(sourceClassName == "GlobalSource")
                                        {
                                            XElement *data = sourceElement->GetElement(TEXT("data"));
//                                             if(data)
//                                             {
//                                                 CTSTR lpName = data->GetString(TEXT("name"));
//                                                 XElement *globalSourceName = App->GetGlobalSourceElement(lpName);
//                                                 if(globalSourceName)
//                                                 {
//                                                     XElement *importGlobalSources = App->scenesCopyToConfig.GetElement(TEXT("global sources"));
// 
//                                                     if(!importGlobalSources)
//                                                        importGlobalSources = App->scenesCopyToConfig.CreateElement(TEXT("global sources"));
// 
//                                                     if(importGlobalSources)
//                                                     {
//                                                         XElement *foundGlobalSource = importGlobalSources->GetElement(globalSourceName->GetName());
//                                                         if(foundGlobalSource != NULL && globalSourceName->GetName() != foundGlobalSource->GetName())
//                                                         {
//                                                             String strGsExists = Str("CopyTo.GlobalSourcesExists");
//                                                             strGsExists.FindReplace(TEXT("$1"), globalSourceName->GetName());
//                                                             OBSMessageBox(hwnd, strGsExists, NULL, 0);
//                                                         }
//                                                         else
//                                                         {
//                                                             XElement *newGlobalSources = importGlobalSources->CopyElement(globalSourceName, globalSourceName->GetName());
//                                                             newGlobalSources->SetString(TEXT("class"), globalSourceName->GetString(TEXT("class")));
//                                                         }
//                                                     }
//                                                 }
//                                             }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                XElement *newSceneSources = newSceneElement->GetElement(TEXT("sources"));

                                if(newSceneSources)
                                {
                                    UINT numSources = newSceneSources->NumElements();

                                    for(int i = int(numSources - 1); i >= 0; i--)
                                    {
                                        XElement *sourceElement = newSceneSources->GetElementByID(i);
                                        String sourceClassName = sourceElement->GetString(TEXT("class"));

                                        if(sourceClassName == "GlobalSource")
                                        {
                                            newSceneSources->RemoveElement(sourceElement);
                                        }
                                    }
                                }
                            }
                        }

                        String strCopied = Str("CopyTo.Success.Text");
                        strCopied.FindReplace(TEXT("$1"), selectedScene->GetName());
                        OBSMessageBox(hwnd, strCopied, Str("CopyTo.Success.Title"), 0);

                        App->EnableSceneSwitching(true);
                        App->scenesCopyToConfig.Close(true);
                    }
                    break;

                case ID_LISTBOX_REMOVE:
                    App->EnableSceneSwitching(false);

                    if(OBSMessageBox(hwndMain, Str("DeleteConfirm"), Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
                    {
                        DWORD hotkey = item->GetInt(TEXT("hotkey"));
                        if(hotkey)
                            App->RemoveSceneHotkey(hotkey);

                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        if(--numItems)
                        {
                            if(curSel == numItems)
                                curSel--;
                        }
                        else
                            curSel = LB_ERR;
                        
                        bDelete = true;
                    }

                    App->EnableSceneSwitching(true);
                    break;

                case ID_LISTBOX_RENAME:
                    {
                        App->EnableSceneSwitching(false);

                        String strName = item->GetName();
                        if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                            SendMessage(hwnd, LB_INSERTSTRING, curSel, (LPARAM)strName.Array());
                            SendMessage(hwnd, LB_SETCURSEL, curSel, 0);

                            item->SetName(strName);
                        }

                        App->EnableSceneSwitching(true);
                        break;
                    }
                case ID_LISTBOX_COPY:
                    {
                        App->EnableSceneSwitching(false);
                        
                        String strName = Str("Scene");
                        GetNewSceneName(strName);

                        if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            UINT classID = 0;   // ID_LISTBOX_ADD - ID_LISTBOX_ADD
                            ClassInfo &ci = App->sceneClasses[classID];

                            XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                            XElement *newSceneElement = scenes->CopyElement(item, strName);

                            newSceneElement->SetString(TEXT("class"), ci.strClass);
                            newSceneElement->SetInt(TEXT("hotkey"), 0);

                            if(ci.configProc)
                            {
                                if(!ci.configProc(newSceneElement, true))
                                {
                                    scenes->RemoveElement(newSceneElement);
                                    break;
                                }
                            }

                            UINT newID = (UINT)SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)strName.Array());
                            PostMessage(hwnd, LB_SETCURSEL, newID, 0);
                            PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)hwnd);
                        }

                        App->EnableSceneSwitching(true);

                        break;
                    }
                case ID_LISTBOX_CONFIG:
                    App->EnableSceneSwitching(false);

                    if(curClassInfo && curClassInfo->configProc(item, false))
                    {
                        if(App->bRunning)
                            App->scene->UpdateSettings();
                    }

                    App->EnableSceneSwitching(true);
                    break;

                case ID_LISTBOX_HOTKEY:
                    {
                        App->EnableSceneSwitching(false);

                        DWORD prevHotkey = item->GetInt(TEXT("hotkey"));

                        SceneHotkeyInfo hotkeyInfo;
                        hotkeyInfo.hotkey = prevHotkey;
                        hotkeyInfo.scene = item;

                        if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_SCENEHOTKEY), hwndMain, OBS::SceneHotkeyDialogProc, (LPARAM)&hotkeyInfo) == IDOK)
                        {
                            if(hotkeyInfo.hotkey)
                                hotkeyInfo.hotkeyID = API->CreateHotkey(hotkeyInfo.hotkey, SceneHotkey, 0);

                            item->SetInt(TEXT("hotkey"), hotkeyInfo.hotkey);

                            if(prevHotkey)
                                App->RemoveSceneHotkey(prevHotkey);

                            if(hotkeyInfo.hotkeyID)
                                App->sceneHotkeys << hotkeyInfo;
                        }

                        App->EnableSceneSwitching(true);
                        break;
                    }

                case ID_LISTBOX_MOVEUP:
                    if(curSel > 0)
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, curSel-1, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, curSel-1, 0);

                        curSel--;
                        item->MoveUp();
                    }
                    break;

                case ID_LISTBOX_MOVEDOWN:
                    if(curSel != (numItems-1))
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, curSel+1, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, curSel+1, 0);

                        curSel++;
                        item->MoveDown();
                    }
                    break;

                case ID_LISTBOX_MOVETOTOP:
                    if(curSel != 0)
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, 0, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, 0, 0);

                        curSel = 0;
                        item->MoveToTop();
                    }
                    break;

                case ID_LISTBOX_MOVETOBOTTOM:
                    if(curSel != numItems-1)
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, numItems-1, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, numItems-1, 0);

                        curSel = numItems-1;
                        item->MoveToBottom();
                    }
                    break;
            }

            if(curSel != LB_ERR)
            {
                SendMessage(hwnd, LB_SETCURSEL, curSel, 0);
                SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));

                if(bDelete)
                    item->GetParent()->RemoveElement(item);
            }
            else if(bDelete)
            {
                if(App->bRunning)
                {
                    OSEnterMutex(App->hSceneMutex);
                    delete App->scene;
                    App->scene = NULL;
                    OSLeaveMutex(App->hSceneMutex);
                }

                App->bChangingSources = true;
                ListView_DeleteAllItems(GetDlgItem(hwndMain, ID_SOURCES));
                App->bChangingSources = false;
                
                item->GetParent()->RemoveElement(item);
                App->sceneElement = NULL;
            }
        }
        else if(id == ID_SOURCES)
        {
            if(!App->sceneElement && numItems)
                return 0;

            List<SceneItem*> selectedSceneItems;
            if(App->scene)
                App->scene->GetSelectedItems(selectedSceneItems);

            if(selectedSceneItems.Num() < 1)
                nop();

            int ret = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
            App->TrackModifyListbox(hwnd, ret);
        }

        DestroyMenu(hMenu);

        return 0;
    }

    if(id == ID_SOURCES)
    {
        return CallWindowProc(listviewProc, hwnd, message, wParam, lParam);
    }
    else
    {
        return CallWindowProc(listboxProc, hwnd, message, wParam, lParam);
    }
}

void OBS::TrackModifyListbox(HWND hwnd, int ret)
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (ListView_GetSelectedCount(hwndSources));
    XElement *selectedElement = NULL;
    ClassInfo *curClassInfo = NULL;
    if(numSelected == 1)
    {
        UINT selectedID = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
        XElement *sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        selectedElement = sourcesElement->GetElementByID(selectedID);
        curClassInfo = App->GetImageSourceClass(selectedElement->GetString(TEXT("class")));
    }

    switch(ret)
    {
        // General render frame stuff above here
        case ID_TOGGLERENDERVIEW:
            App->bRenderViewEnabled = !App->bRenderViewEnabled;
            App->bForceRenderViewErase = !App->bRenderViewEnabled;
            App->UpdateRenderViewMessage();
            break;
        case ID_PREVIEWSCALETOFITMODE:
            App->renderFrameIn1To1Mode = false;
            App->ResizeRenderFrame(true);
            break;
        case ID_PREVIEW1TO1MODE:
            App->renderFrameIn1To1Mode = true;
            App->ResizeRenderFrame(true);
            break;

        // Sources below here
        default:
            if (ret >= ID_PROJECTOR)
            {
                UINT monitorID = ret-ID_PROJECTOR;
                if (monitorID == 0)
                    App->bPleaseDisableProjector = true;
            }
            else if(ret >= ID_LISTBOX_ADD)
            {
                App->EnableSceneSwitching(false);

                ClassInfo *ci;
                if(ret >= ID_LISTBOX_GLOBALSOURCE)
                    ci = App->GetImageSourceClass(TEXT("GlobalSource"));
                else
                {
                    UINT classID = ret-ID_LISTBOX_ADD;
                    ci = App->imageSourceClasses+classID;
                }

                String strName;
                if(ret >= ID_LISTBOX_GLOBALSOURCE){
                }
                else
                    strName = ci->strName;

                GetNewSourceName(strName);

                if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                {
                    XElement *curSceneElement = App->sceneElement;
                    XElement *sources = curSceneElement->GetElement(TEXT("sources"));
                    if(!sources)
                        sources = curSceneElement->CreateElement(TEXT("sources"));
                    XElement *newSourceElement = sources->InsertElement(0, strName);
                    newSourceElement->SetInt(TEXT("render"), 1);

                    if(ret >= ID_LISTBOX_GLOBALSOURCE){

                    }
                    else
                    {
                        newSourceElement->SetString(TEXT("class"), ci->strClass);
                        if(ci->configProc)
                        {
                            if(!ci->configProc(newSourceElement, true))
                            {
                                sources->RemoveElement(newSourceElement);
                                App->EnableSceneSwitching(true);
                                break;
                            }
                        }
                    }

                    if(App->sceneElement == curSceneElement)
                    {
                        if(App->bRunning)
                        {
                            App->EnterSceneMutex();
                            App->scene->InsertImageSource(0, newSourceElement);
                            App->LeaveSceneMutex();
                        }

                        UINT numSources = sources->NumElements();

                        // clear selection/focus for all items before adding the new item
                        ListView_SetItemState(hwndSources , -1 , 0, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_SetItemCount(hwndSources, numSources);
                                
                        App->bChangingSources = true;
                        App->InsertSourceItem(0, (LPWSTR)strName.Array(), true);
                        App->bChangingSources = false;

                        SetFocus(hwndSources);
                                
                        // make sure the added item is visible/selected/focused and selection mark moved to it.
                        ListView_EnsureVisible(hwndSources, 0, false);
                        ListView_SetItemState(hwndSources, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_SetSelectionMark(hwndSources, 0);
                    }
                }

                App->EnableSceneSwitching(true);
            }
            break;

        case ID_LISTBOX_REMOVE:
            App->DeleteItems();
            break;

        case ID_LISTBOX_RENAME:
            {
                if (!selectedElement)
                    break;

                App->EnableSceneSwitching(false);

                String strName = selectedElement->GetName();
                TSTR oldStrName = sdup(strName.Array());
                if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                {
                    int curSel = (int)SendMessage(hwndSources, LB_GETCURSEL, 0, 0);
                    ListView_SetItemText(hwndSources, curSel, 0, strName.Array());
                    selectedElement->SetName(strName);
                            
                    Free((void*)oldStrName);
                            
                    ListView_SetColumnWidth(hwndSources, 0, LVSCW_AUTOSIZE_USEHEADER);
                    ListView_SetColumnWidth(hwndSources, 1, LVSCW_AUTOSIZE_USEHEADER);
                }

                App->EnableSceneSwitching(true);
                break;
            }

        case ID_LISTBOX_CONFIG:
            {
                App->EnableSceneSwitching(false);

                List<SceneItem*> selectedSceneItems;

                if(App->scene)
                    App->scene->GetSelectedItems(selectedSceneItems);

                ImageSource *source = NULL;
                Vect2 multiple;

                if(App->bRunning && selectedSceneItems.Num())
                {
                    source = selectedSceneItems[0]->GetSource();
                    if(source)
                    {
                        Vect2 curSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                        Vect2 baseSize = source->GetSize();

                        multiple = curSize/baseSize;
                    }
                }

                if(curClassInfo && curClassInfo->configProc && curClassInfo->configProc(selectedElement, false))
                {

                    if(App->bRunning && selectedSceneItems.Num())
                    {
                        App->EnterSceneMutex();

                        if(source)
                        {
                            Vect2 newSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                            newSize *= multiple;

                            selectedElement->SetFloat(TEXT("cx"), newSize.x);
                            selectedElement->SetFloat(TEXT("cy"), newSize.y);

                            selectedSceneItems[0]->GetSource()->UpdateSettings();
                        }

                        selectedSceneItems[0]->Update();

                        App->LeaveSceneMutex();
                    }
                }

                App->EnableSceneSwitching(true);
            }
            break;

        case ID_LISTBOX_MOVEUP:
            App->MoveSourcesUp();
            break;

        case ID_LISTBOX_MOVEDOWN:
            App->MoveSourcesDown();
            break;

        case ID_LISTBOX_MOVETOTOP:
            App->MoveSourcesToTop();
            break;

        case ID_LISTBOX_MOVETOBOTTOM:
            App->MoveSourcesToBottom();
            break;

        case ID_LISTBOX_CENTER:
            App->CenterItems(true, true);
            break;

        case ID_LISTBOX_CENTERHOR:
            App->CenterItems(true, false);
            break;

        case ID_LISTBOX_CENTERVER:
            App->CenterItems(false, true);
            break;

        case ID_LISTBOX_MOVELEFT:
            App->MoveItemsToEdge(-1, 0);
            break;

        case ID_LISTBOX_MOVETOP:
            App->MoveItemsToEdge(0, -1);
            break;

        case ID_LISTBOX_MOVERIGHT:
            App->MoveItemsToEdge(1, 0);
            break;

        case ID_LISTBOX_MOVEBOTTOM:
            App->MoveItemsToEdge(0, 1);
            break;

        case ID_LISTBOX_FITTOSCREEN:
            App->FitItemsToScreen();
            break;

        case ID_LISTBOX_RESETSIZE:
            App->ResetItemSizes();
            break;

        case ID_LISTBOX_RESETCROP:
            App->ResetItemCrops();
            break;
    }
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

void OBS::ResetApplicationName()
{
    SetWindowText(hwndMain, GetApplicationName());
}

//----------------------------

LRESULT CALLBACK OBS::OBSProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_COMMAND:
            switch(LOWORD(wParam)){
                case ID_GLOBALSOURCES:
                    OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_GLOBAL_SOURCES), hwnd, (DLGPROC)OBS::GlobalSourcesProc);
                    break;

                case ID_TOGGLERECORDING:
                    App->ToggleRecording();
                    break;

                case ID_REFRESH_LOGS:
                    App->ResetLogUploadMenu();
                    break;

                case ID_SETTINGS_OPENCONFIGFOLDER:
                    {
                        String strAppPath = API->GetAppDataPath();
                        ShellExecute(NULL, TEXT("open"), strAppPath, 0, 0, SW_SHOWNORMAL);
                    }
                    break;
// 
//                 case ID_MICVOLUME:
//                     if(HIWORD(wParam) == VOLN_ADJUSTING || HIWORD(wParam) == VOLN_FINALVALUE)
//                     {
//                         if(IsWindowEnabled((HWND)lParam))
//                         {
//                             App->micVol = GetVolumeControlValue((HWND)lParam);
// 
//                             bool finalValue = HIWORD(wParam) == VOLN_FINALVALUE;
// 
//                             if(App->micVol < EPSILON)
//                                 App->micVol = 0.0f;
// 
//                             if(finalValue)
//                             {
//                                 AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), App->micVol);
//                                 if (App->micVol < EPSILON)
//                                     AppConfig->SetFloat(TEXT("Audio"), TEXT("MicMutedVolume"), GetVolumeControlMutedVal((HWND)lParam));
//                             }
//                         }
//                     }
//                     break;
//                 case ID_DESKTOPVOLUME:
//                     if(HIWORD(wParam) == VOLN_ADJUSTING || HIWORD(wParam) == VOLN_FINALVALUE)
//                     {
//                         if(IsWindowEnabled((HWND)lParam))
//                         {
//                             App->desktopVol = GetVolumeControlValue((HWND)lParam);
//                             
//                             bool finalValue = HIWORD(wParam) == VOLN_FINALVALUE;
// 
//                             if(App->desktopVol < EPSILON)
//                                 App->desktopVol = 0.0f;
// 
//                             if(finalValue)
//                             {
//                                 AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), App->desktopVol);
//                                 if (App->desktopVol < EPSILON)
//                                     AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopMutedVolume"), GetVolumeControlMutedVal((HWND)lParam));
//                             }
//                         }
//                     }
//                     break;
                case ID_SCENEEDITOR:
                    if(HIWORD(wParam) == BN_CLICKED)
                        App->bEditMode = !App->bEditMode;
                    break;

                case ID_SCENES:
                    if(HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        HWND hwndScenes = (HWND)lParam;
                        UINT id = (UINT)SendMessage(hwndScenes, LB_GETCURSEL, 0, 0);
                        if(id != LB_ERR)
                        {
                            String strName;
                            strName.SetLength((UINT)SendMessage(hwndScenes, LB_GETTEXTLEN, id, 0));
                            SendMessage(hwndScenes, LB_GETTEXT, id, (LPARAM)strName.Array());
                            App->SetScene(strName);
                        }
                    }
                    break;

                case ID_SCENECOLLECTION_NEW:
                    App->AddSceneCollection(SceneCollectionAction::Add);
                    break;
                case ID_SCENECOLLECTION_CLONE:
                    App->AddSceneCollection(SceneCollectionAction::Clone);
                    break;
                case ID_SCENECOLLECTION_RENAME:
                    App->AddSceneCollection(SceneCollectionAction::Rename);
                    break;
                case ID_SCENECOLLECTION_REMOVE:
                    App->RemoveSceneCollection();
                    break;
                case ID_SCENECOLLECTION_IMPORT:
                    App->ImportSceneCollection();
                    break;
                case ID_SCENECOLLECTION_EXPORT:
                    App->ExportSceneCollection();
                    break;

                case ID_PROFILE_NEW:
                    App->AddProfile(ProfileAction::Add);
                    break;
                case ID_PROFILE_CLONE:
                    App->AddProfile(ProfileAction::Clone);
                    break;
                case ID_PROFILE_RENAME:
                    App->AddProfile(ProfileAction::Rename);
                    break;
                case ID_PROFILE_REMOVE:
                    App->RemoveProfile();
                    break;
                case ID_PROFILE_IMPORT:
                    App->ImportProfile();
                    break;
                case ID_PROFILE_EXPORT:
                    App->ExportProfile();
                    break;

                case ID_TESTSTREAM:
                    //App->bTestStream = true;
                    //App->ToggleCapturing();
                    break;

                case ID_STARTSTOP:
                    App->ToggleCapturing();
                    break;

                case ID_MINIMIZERESTORE:
                    {
                        bool bMinimizeToNotificationArea = AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0;
                        if (bMinimizeToNotificationArea)
                        {
                            if (IsWindowVisible(hwnd))
                            {
                                ShowWindow(hwnd, SW_MINIMIZE);
                                ShowWindow(hwnd, SW_HIDE);
                            }
                            else
                            {
                                ShowWindow(hwnd, SW_SHOW);
                                ShowWindow(hwnd, SW_RESTORE);
                            }
                        }
                        else
                        {
                            if (IsIconic(hwnd))
                                ShowWindow(hwnd, SW_RESTORE);
                            else
                                ShowWindow(hwnd, SW_MINIMIZE);
                        }
                    }
                    break;

                case IDA_SOURCE_DELETE:
                    App->DeleteItems();
                    break;

                case IDA_SOURCE_CENTER:
                    App->CenterItems(true, true);
                    break;

                case IDA_SOURCE_CENTER_VER:
                    App->CenterItems(false, true);
                    break;

                case IDA_SOURCE_CENTER_HOR:
                    App->CenterItems(true, false);
                    break;

                case IDA_SOURCE_LEFT_CANVAS:
                    App->MoveItemsToEdge(-1, 0);
                    break;

                case IDA_SOURCE_TOP_CANVAS:
                    App->MoveItemsToEdge(0, -1);
                    break;

                case IDA_SOURCE_RIGHT_CANVAS:
                    App->MoveItemsToEdge(1, 0);
                    break;

                case IDA_SOURCE_BOTTOM_CANVAS:
                    App->MoveItemsToEdge(0, 1);
                    break;

                case IDA_SOURCE_FITTOSCREEN:
                    App->FitItemsToScreen();
                    break;

                case IDA_SOURCE_RESETSIZE:
                    App->ResetItemSizes();
                    break;

                case IDA_SOURCE_RESETCROP:
                    App->ResetItemCrops();
                    break;

                case IDA_SOURCE_MOVEUP:
                    App->MoveSourcesUp();
                    break;

                case IDA_SOURCE_MOVEDOWN:
                    App->MoveSourcesDown();
                    break;

                case IDA_SOURCE_MOVETOTOP:
                    App->MoveSourcesToTop();
                    break;

                case IDA_SOURCE_MOVETOBOTTOM:
                    App->MoveSourcesToBottom();
                    break;

                default:
                    {
                        UINT id = LOWORD(wParam);
                        if (id >= ID_SWITCHPROFILE && 
                            id <= ID_SWITCHPROFILE_END)
                        {
                            if (App->bRunning)
                                break;

                            MENUITEMINFO mii;
                            zero(&mii, sizeof(mii));
                            mii.cbSize = sizeof(mii);
                            mii.fMask = MIIM_STRING;

                            HMENU hmenuMain = GetMenu(hwndMain);
                            HMENU hmenuProfiles = GetSubMenu(hmenuMain, 2);
                            GetMenuItemInfo(hmenuProfiles, id, FALSE, &mii);

                            String strProfile;
                            strProfile.SetLength(mii.cch++);
                            mii.dwTypeData = strProfile.Array();

                            GetMenuItemInfo(hmenuProfiles, id, FALSE, &mii);

                            if(!strProfile.CompareI(GetCurrentProfile()))
                            {
                                String strProfilePath;
                                strProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

                                if(!AppConfig->Open(strProfilePath))
                                {
                                    OBSMessageBox(hwnd, TEXT("Error - unable to open ini file"), NULL, 0);
                                    break;
                                }

                                GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);
                                App->ReloadIniSettings();
                                ResetProfileMenu();
                                ResetApplicationName();
                            }
                        }
                        else if (id >= ID_SWITCHSCENECOLLECTION &&
                            id <= ID_SWITCHSCENECOLLECTION_END)
                        {
                            if (App->bRunning)
                                break;

                            MENUITEMINFO mii;
                            zero(&mii, sizeof(mii));
                            mii.cbSize = sizeof(mii);
                            mii.fMask = MIIM_STRING;

                            HMENU hmenuMain = GetMenu(hwndMain);
                            HMENU hmenuSceneCollection = GetSubMenu(hmenuMain, 3);
                            GetMenuItemInfo(hmenuSceneCollection, id, FALSE, &mii);

                            String strSceneCollection;
                            strSceneCollection.SetLength(mii.cch++);
                            mii.dwTypeData = strSceneCollection.Array();

                            GetMenuItemInfo(hmenuSceneCollection, id, FALSE, &mii);

                            if (!strSceneCollection.CompareI(GetCurrentSceneCollection()))
                            {
                                if (!App->SetSceneCollection(strSceneCollection))
                                {
                                    OBSMessageBox(hwnd, TEXT("Error - unable to open xconfig file"), NULL, 0);
                                    break;
                                }
                            }
                        }
                        else if (id >= ID_VIEW_LOG && id <= ID_VIEW_LOG_END)
                        {
                            String log = GetLogUploadMenuItem(id);
                            if (log.IsEmpty())
                                break;

                            String tar = FormattedString(L"%s\\logs\\%s", OBSGetAppDataPath(), log.Array());

                            HINSTANCE result = ShellExecute(nullptr, L"edit", tar.Array(), 0, 0, SW_SHOWNORMAL);
                            if (result > (HINSTANCE)32)
                                break;
                            
                            result = ShellExecute(nullptr, nullptr, tar.Array(), 0, 0, SW_SHOWNORMAL);
                            if (result > (HINSTANCE)32)
                                break;

                            if (result == (HINSTANCE)SE_ERR_NOASSOC || result == (HINSTANCE)SE_ERR_ASSOCINCOMPLETE)
                            {
                                OPENASINFO info;
                                info.pcszFile = tar.Array();
                                info.pcszClass = nullptr;
                                info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_EXEC;
                                if (SHOpenWithDialog(nullptr, &info) == S_OK)
                                    break;
                            }

                            String error = Str("Sources.TextSource.FileNotFound");
                            OBSMessageBox(hwndMain, error.FindReplace(L"$1", tar.Array()).Array(), nullptr, MB_ICONERROR);
                        }
                    }
            }
            break;

        case WM_NOTIFY:
            {
                NMHDR nmh = *(LPNMHDR)lParam;

                switch(wParam)
                {
                    case ID_SOURCES:
                        if(nmh.code == NM_CUSTOMDRAW)
                        {
                            LPNMLVCUSTOMDRAW  lplvcd = (LPNMLVCUSTOMDRAW)lParam;
                            switch( lplvcd->nmcd.dwDrawStage)
                            {
                                case CDDS_ITEMPREPAINT:

                                    int state, bkMode;
                                    BOOL checkedState;
                                    RECT iconRect, textRect, itemRect, gsVIRect;
                                    COLORREF oldTextColor;

                                    String itemText;

                                    HDC hdc = lplvcd->nmcd.hdc;
                                    int itemId = (int)lplvcd->nmcd.dwItemSpec;

                                    bool isGlobalSource = false;

                                    XElement *sources, *sourcesElement = NULL;
                                    
                                    sources = App->sceneElement->GetElement(TEXT("sources"));
                                    
                                    if(sources)
                                    {
                                        sourcesElement = sources->GetElementByID(itemId);

                                        if(sourcesElement)
                                            if(scmpi(sourcesElement->GetString(TEXT("class")), TEXT("GlobalSource")) == 0)
                                                isGlobalSource = true;
                                    }

                                    ListView_GetItemRect(nmh.hwndFrom,itemId, &itemRect, LVIR_BOUNDS);
                                    ListView_GetItemRect(nmh.hwndFrom,itemId, &textRect, LVIR_LABEL);

                                    iconRect.right = textRect.left - 1;
                                    iconRect.left = iconRect.right - (textRect.bottom - textRect.top);
                                    iconRect.top  = textRect.top + 2;
                                    iconRect.bottom = textRect.bottom - 2;

                                    gsVIRect.left = itemRect.left + 1;
                                    gsVIRect.right = itemRect.left + 2;
                                    gsVIRect.top = iconRect.top + 1;
                                    gsVIRect.bottom = iconRect.bottom - 1;

                                    state = ListView_GetItemState(nmh.hwndFrom, itemId, LVIS_SELECTED);
                                    checkedState = ListView_GetCheckState(nmh.hwndFrom, itemId);

                                    oldTextColor = GetTextColor(hdc);

                                    if(state&LVIS_SELECTED)
                                    {
                                        FillRect(hdc, &itemRect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                                        SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                                    }
                                    else
                                    {
                                        FillRect(hdc, &itemRect, (HBRUSH)(COLOR_WINDOW + 1));
                                        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
                                    }
                                   

                                    HTHEME hTheme = OpenThemeData(hwnd, TEXT("BUTTON"));
                                    if(hTheme)
                                    {
                                        if(checkedState)
                                            DrawThemeBackground(hTheme, hdc, BP_CHECKBOX, (state&LVIS_SELECTED)?CBS_CHECKEDPRESSED:CBS_CHECKEDNORMAL, &iconRect, NULL);
                                        else
                                            DrawThemeBackground(hTheme, hdc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, &iconRect, NULL);
                                        CloseThemeData(hTheme);
                                    }
                                    else
                                    {
                                        iconRect.right = iconRect.left + iconRect.bottom - iconRect.top;
                                        if(checkedState)
                                            DrawFrameControl(hdc,&iconRect, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_FLAT);
                                        else
                                            DrawFrameControl(hdc,&iconRect, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_FLAT);
                                    }

                                    if(isGlobalSource)
                                        FillRect(hdc, &gsVIRect, (state&LVIS_SELECTED) ? (HBRUSH)(COLOR_HIGHLIGHTTEXT + 1) : (HBRUSH)(COLOR_WINDOWTEXT + 1));
                                    
                                    textRect.left += 2;

                                    if(sourcesElement)
                                    {
                                        itemText = sourcesElement->GetName();
                                        if(itemText.IsValid())
                                        {
                                            bkMode = SetBkMode(hdc, TRANSPARENT);
                                            DrawText(hdc, itemText, slen(itemText), &textRect, DT_LEFT | DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
                                            SetBkMode(hdc, bkMode);
                                        }
                                    }

                                    SetTextColor(hdc, oldTextColor);

                                    return CDRF_SKIPDEFAULT;
                            }

                            return CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW;
                        }

                        if(nmh.code == LVN_ITEMCHANGED && !App->bChangingSources)
                        {
                            NMLISTVIEW pnmv = *(LPNMLISTVIEW)lParam;
                            if((pnmv.uOldState & LVIS_SELECTED) != (pnmv.uNewState & LVIS_SELECTED))
                            {
                                /*item selected*/
                                App->SelectSources();
                            }
                            if((pnmv.uOldState & LVIS_STATEIMAGEMASK) != (pnmv.uNewState & LVIS_STATEIMAGEMASK))
                            {
                                //checks changed
                                App->CheckSources();
                            }
                        }
                        else if(nmh.code == NM_DBLCLK)
                        {
                            NMITEMACTIVATE itemActivate = *(LPNMITEMACTIVATE)lParam;
                            UINT selectedID = itemActivate.iItem;

                            /* check to see if the double click is on top of the checkbox
                               if so, forget about it */
                            LVHITTESTINFO hitInfo;
                            hitInfo.pt = itemActivate.ptAction;
                            ListView_HitTestEx(nmh.hwndFrom, &hitInfo);
                            if(hitInfo.flags & LVHT_ONITEMSTATEICON)
                                break;
                            
                            if(!App->sceneElement)
                                break;

                            XElement *sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
                            if(!sourcesElement)
                                break;

                            XElement *selectedElement = sourcesElement->GetElementByID(selectedID);
                            if(!selectedElement)
                                break;

                            ClassInfo *curClassInfo = App->GetImageSourceClass(selectedElement->GetString(TEXT("class")));
                            if(curClassInfo && curClassInfo->configProc)
                            {
                                Vect2 multiple;
                                ImageSource *source;

                                App->EnableSceneSwitching(false);

                                if(App->bRunning && App->scene)
                                {
                                    SceneItem* selectedItem = App->scene->GetSceneItem(selectedID);
                                    source = selectedItem->GetSource();
                                    if(source)
                                    {
                                        Vect2 curSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                                        Vect2 baseSize = source->GetSize();

                                        multiple = curSize/baseSize;
                                    }
                                }

                                if(curClassInfo->configProc(selectedElement, false))
                                {
                                    if(App->bRunning)
                                    {
                                        App->EnterSceneMutex();

                                        if(App->scene)
                                        {
                                            Vect2 newSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                                            newSize *= multiple;

                                            selectedElement->SetFloat(TEXT("cx"), newSize.x);
                                            selectedElement->SetFloat(TEXT("cy"), newSize.y);

                                            SceneItem* selectedItem = App->scene->GetSceneItem(selectedID);
                                            if(selectedItem->GetSource())
                                                selectedItem->GetSource()->UpdateSettings();
                                            selectedItem->Update();
                                        }

                                        App->LeaveSceneMutex();
                                    }
                                }

                                App->EnableSceneSwitching(true);
                            }
                        }
                        break;

                        case ID_TOGGLERECORDING:
                            if (nmh.code == BCN_DROPDOWN)
                            {
                                LPNMBCDROPDOWN drop = (LPNMBCDROPDOWN)lParam;

                                HMENU menu = CreatePopupMenu();
                                AppendMenu(menu, MF_STRING, 1, App->bRecordingReplayBuffer ? Str("MainWindow.StopReplayBuffer") : Str("MainWindow.StartReplayBuffer"));
                                AppendMenu(menu, MF_STRING | (App->bRecording == !App->bRecordingReplayBuffer ? MF_DISABLED : 0), 2,
                                    App->bRecording ? Str("MainWindow.StopRecordingAndReplayBuffer") : Str("MainWindow.StartRecordingAndReplayBuffer"));

                                POINT p;
                                p.x = drop->rcButton.right;
                                p.y = drop->rcButton.bottom;
                                ClientToScreen(GetDlgItem(hwndMain, ID_TOGGLERECORDING), &p);

                                switch (TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTALIGN, p.x, p.y, 0, GetDlgItem(hwndMain, ID_TOGGLERECORDING), nullptr))
                                {
                                case 1:
                                    //App->ToggleReplayBuffer();
                                    break;
                                case 2:
                                    App->ToggleRecording();
                                    //App->ToggleReplayBuffer();
                                    break;
                                case 0:
                                    break;
                                }
                            }
                }
            }
            break;

        case WM_ENTERSIZEMOVE:
            App->bDragResize = true;
            break;

        case WM_EXITSIZEMOVE:
            if(App->bSizeChanging)
            {
                RECT client;
                GetClientRect(hwnd, &client);
                App->ResizeWindow(true);
                App->bSizeChanging = false;
            }
            App->bDragResize = false;
            break;

        case WM_DRAWITEM:
//             if(wParam == ID_STATUS)
//             {
//                 DRAWITEMSTRUCT &dis = *(DRAWITEMSTRUCT*)lParam; //don't dis me bro
//                 App->DrawStatusBar(dis);
//             }
            break;

        case WM_SIZING:
            {
                RECT &screenSize = *(RECT*)lParam;

                int newWidth  = MAX(screenSize.right  - screenSize.left, minClientWidth+App->borderXSize);
                int newHeight = MAX(screenSize.bottom - screenSize.top , minClientHeight+App->borderYSize);

                /*int maxCX = GetSystemMetrics(SM_CXFULLSCREEN);
                int maxCY = GetSystemMetrics(SM_CYFULLSCREEN);

                if(newWidth > maxCX)
                    newWidth = maxCX;
                if(newHeight > maxCY)
                    newHeight = maxCY;*/

                if(wParam == WMSZ_LEFT || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT)
                    screenSize.left = screenSize.right - newWidth;
                else
                    screenSize.right = screenSize.left + newWidth;

                if(wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT)
                    screenSize.bottom = screenSize.top + newHeight;
                else
                    screenSize.top = screenSize.bottom - newHeight;

                return TRUE;
            }

        case WM_SIZE:
            {
                RECT client;
                GetClientRect(hwnd, &client);

                if(wParam != SIZE_MINIMIZED && (App->clientWidth != client.right || App->clientHeight != client.bottom))
                {
                    App->clientWidth  = client.right;
                    App->clientHeight = client.bottom;
                    App->bSizeChanging = true;

                    if(wParam == SIZE_MAXIMIZED)
                        App->ResizeWindow(true);
                    else
                        App->ResizeWindow(!App->bDragResize);

                    if(!App->bDragResize)
                        App->bSizeChanging = false;
                }
                else if (wParam == SIZE_MINIMIZED && AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0)
                {
                    ShowWindow(hwnd, SW_HIDE);
                }
                break;
            }

        case OBS_REQUESTSTOP:
            if (!App->IsRunning())
                break;

            App->Stop();

            if((App->bFirstConnect && App->totalStreamTime < 30000) || !App->bAutoReconnect)
            {
				OBSMessageBox(hwnd, Str("Connection.Disconnected"), NULL, MB_ICONEXCLAMATION);
            }
            else if(wParam == 0)
            {
                //FIXME: show some kind of non-modal notice to the user explaining why they got disconnected
                //status bar would be nice, but that only renders when we're streaming.
                PlaySound((LPCTSTR)SND_ALIAS_SYSTEMASTERISK, NULL, SND_ALIAS_ID | SND_ASYNC);

                if (!App->reconnectTimeout)
                {
                    //fire immediately
                    PostMessage(hwndMain, OBS_RECONNECT, 0, 0);
                }
                else
                {
                    App->bReconnecting = false;
                    OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwnd, OBS::ReconnectDialogProc);
                }
            }

            break;

        case OBS_NETWORK_FAILED:
            if ((App->bFirstConnect && App->totalStreamTime < 30000) || !App->bAutoReconnect)
            {
                //no reconnect, or the connection died very early in the stream
                App->Stop();
				OBSMessageBox(hwnd, Str("Connection.Disconnected"), NULL, MB_ICONEXCLAMATION);
            }
            else
            {
                PlaySound((LPCTSTR)SND_ALIAS_SYSTEMASTERISK, NULL, SND_ALIAS_ID | SND_ASYNC);

                if (!App->reconnectTimeout)
                {
                    //fire immediately
                    App->RestartNetwork();
                }
                else
                {
                    OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwnd, OBS::ReconnectDialogProc);
                }
            }
            break;

        case OBS_CALLHOTKEY:
            App->CallHotkey((DWORD)lParam, wParam != 0);
            break;

        case OBS_RECONNECT:
            App->bReconnecting = true;
            App->Start();
            break;

        case OBS_SETSCENE:
            {
                TSTR lpScene = (TSTR)lParam;
                App->SetScene(lpScene);
                Free(lpScene);
                break;
            }
        case OBS_SETSOURCEORDER:
            {
                StringList *order = (StringList*)lParam;
                App->SetSourceOrder(*order);
                delete(order);
                break;
            }
        case OBS_SETSOURCERENDER:
            {
                bool render = lParam > 0;
                CTSTR sourceName = (CTSTR) wParam;
                App->SetSourceRender(sourceName, render);
                Free((void *)sourceName);
                break;
            }
        case OBS_UPDATESTATUSBAR:
            //App->SetStatusBarData();
            break;

        case WM_CLOSE:
            if (App->bRunning)
            {
                if (App->bRecording)
                {
                    if (OBSMessageBox(hwnd, Str("CloseWhileActiveWarning.Message"), Str("CloseWhileActiveWarning.Title"), MB_ICONQUESTION | MB_YESNO) == IDYES)
                    {
                        PostQuitMessage(0);
                    }
                }
                else if (App->bStreaming)
                {
                    if (OBSMessageBox(hwnd, Str("CloseWhileActiveWarning.Message"), Str("CloseWhileActiveWarning.Title"), MB_ICONQUESTION | MB_YESNO) == IDYES)
                    {
                        PostQuitMessage(0);
                    }
                }
            }
            else
                PostQuitMessage(0);
            break;

        case WM_ENDSESSION:
            if (wParam == TRUE)
            {
                // AppConfig / GlobalConfig should already save most important changes. A few UI
                // things like last window size / position and other UI state gets lost though.
                App->scenesConfig.Save();
                return TRUE;
            }

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
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