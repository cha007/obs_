#include "Main.h"

#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>

#include <intrin.h>
#include <inttypes.h>

#include <gdiplus.h>


//----------------------------

HWND        hwndMain        = NULL;
HWND        hwndRenderFrame = NULL;
HINSTANCE   hinstMain       = NULL;
ConfigFile  *GlobalConfig   = NULL;
ConfigFile  *AppConfig      = NULL;
OBS         *App            = NULL;
bool        bStreamOnStart  = false;
TCHAR       lpAppPath[MAX_PATH];
TCHAR       lpAppDataPath[MAX_PATH];
//----------------------------
void InitSockets();
void TerminateSockets();

void SetupIni(CTSTR profile)
{
    //first, find out which profile we're using
    String strProfile = profile ? profile : GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
    DWORD lastVersion = GlobalConfig->GetInt(TEXT("General"), TEXT("LastAppVersion"));
    String strIni;

	if (profile)
		GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), profile);

    //--------------------------------------------
    // try to find and open the file, otherwise use the first one available

    bool bFoundProfile = false;

    if(strProfile.IsValid())
    {
        strIni << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");
        bFoundProfile = OSFileExists(strIni) != 0;
    }

    if(!bFoundProfile)
    {
        OSFindData ofd;

        strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\*.ini");
        HANDLE hFind = OSFindFirstFile(strIni, ofd);
        if(hFind)
        {
            do
            {
                if(ofd.bDirectory) continue;

                strProfile = GetPathWithoutExtension(ofd.fileName);
                GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);
                bFoundProfile = true;

                break;

            } while(OSFindNextFile(hFind, ofd));

            OSFindClose(hFind);
        }
    }

    //--------------------------------------------
    // open, or if no profile found, create one

    if(bFoundProfile)
    {
        strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

        if(AppConfig->Open(strIni))
            return;
    }

    strProfile = TEXT("Untitled");
    GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);

    strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

    if(!AppConfig->Create(strIni))
        CrashError(TEXT("Could not create '%s'"), strIni.Array());

    AppConfig->SetString(TEXT("Audio"),          TEXT("Device"),        TEXT("Default"));
    AppConfig->SetFloat (TEXT("Audio"),          TEXT("MicVolume"),     1.0f);
    AppConfig->SetFloat (TEXT("Audio"),          TEXT("DesktopVolume"), 1.0f);

    AppConfig->SetInt   (TEXT("Video"),          TEXT("Monitor"),       0);
    AppConfig->SetInt   (TEXT("Video"),          TEXT("FPS"),           30);
    AppConfig->SetFloat (TEXT("Video"),          TEXT("Downscale"),     1.0f);
    AppConfig->SetInt   (TEXT("Video"),          TEXT("DisableAero"),   0);

    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("BufferSize"),    1000);
    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"),    1000);
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"),        TEXT("veryfast"));
    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("Quality"),       8);

    AppConfig->SetInt   (TEXT("Audio Encoding"), TEXT("Format"),        1);
    AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Bitrate"),       TEXT("128"));
    AppConfig->SetInt   (TEXT("Audio Encoding"), TEXT("isStereo"),      1);

    AppConfig->SetInt   (TEXT("Publish"),        TEXT("Service"),       0);
    AppConfig->SetInt   (TEXT("Publish"),        TEXT("Mode"),          0);
};

void LoadGlobalIni()
{
    GlobalConfig = new ConfigFile;

    String strGlobalIni;
    strGlobalIni << lpAppDataPath << TEXT("\\global.ini");

    if(!GlobalConfig->Open(strGlobalIni))
    {
        if(!GlobalConfig->Create(strGlobalIni))
            CrashError(TEXT("Could not create '%s'"), strGlobalIni.Array());

        //----------------------
        // first, try to set the app to the system language, defaulting to english if the language doesn't exist

        DWORD bufSize = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, NULL, 0);

        String str639Lang;
        str639Lang.SetLength(bufSize);

        GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, str639Lang.Array(), bufSize+1);

        String strLangFile;
        strLangFile << TEXT("locale/") << str639Lang << TEXT(".txt");

        if(!OSFileExists(strLangFile))
            str639Lang = TEXT("en");

        //----------------------

        GlobalConfig->SetString(TEXT("General"), TEXT("Language"), str639Lang);
        GlobalConfig->SetInt(TEXT("General"), TEXT("MaxLogs"), 20);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    LPWSTR sceneCollection = NULL;
    //------------------------------------------------------------

    hinstMain = hInstance;
    if(InitXT(NULL, TEXT("FastAlloc")))
    {
        InitSockets();
        CoInitialize(0);

		String strDirectory;
		UINT dirSize = GetCurrentDirectory(0, 0);
		strDirectory.SetLength(dirSize);
		GetCurrentDirectory(dirSize, strDirectory.Array());
		scpy(lpAppPath, strDirectory);

		TSTR lpAllocator = NULL;
		SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, lpAppDataPath);
		scat_n(lpAppDataPath, TEXT("\\OBS"), 4);

		if (!OSFileExists(lpAppDataPath) && !OSCreateDirectory(lpAppDataPath))
			CrashError(TEXT("Couldn't create directory '%s'"), lpAppDataPath);

		LoadGlobalIni();

        AppConfig = new ConfigFile;
        SetupIni(NULL);
        
        App = new OBS;
        MSG msg;
        while(GetMessage(&msg, NULL, 0, 0)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);           
        }
		
        delete App;
        delete AppConfig;
        delete GlobalConfig;
        TerminateSockets();
    }

    TerminateXT();
    return 0;
}
