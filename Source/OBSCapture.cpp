#include "Main.h"
#include <time.h>
#include <Avrt.h>

VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);
VideoEncoder* CreateNVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, String &errors);
AudioEncoder* CreateAACEncoder(UINT bitRate);

AudioSource* CreateAudioSource(bool bMic, CTSTR lpID);

NetworkStream* CreateRTMPPublisher();
NetworkStream* CreateBandwidthAnalyzer();

void StartBlankSoundPlayback(CTSTR lpDevice);
void StopBlankSoundPlayback();

VideoFileStream* CreateMP4FileStream(CTSTR lpFile);

void OBS::ToggleRecording()
{
    if (!bRecording)
    {
        if (!bRunning && !bStreaming) Start(true);
        else StartRecording(true);
    }
    else
        StopRecording();
}

void OBS::ToggleCapturing()
{
    if(!bRunning || (!bStreaming && (bRecording || bRecordingReplayBuffer)))
        Start();
    else
        Stop();
}

String ExpandRecordingFilename(String filename)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    filename.FindReplace(L"$Y", UIntString(st.wYear).Array());
    filename.FindReplace(L"$M", UIntString(st.wMonth).Array());
    filename.FindReplace(L"$0M", FormattedString(L"%02u", st.wMonth).Array());
    filename.FindReplace(L"$D", UIntString(st.wDay).Array());
    filename.FindReplace(L"$0D", FormattedString(L"%02u", st.wDay).Array());
    filename.FindReplace(L"$h", UIntString(st.wHour).Array());
    filename.FindReplace(L"$0h", FormattedString(L"%02u", st.wHour).Array());
    filename.FindReplace(L"$m", UIntString(st.wMinute).Array());
    filename.FindReplace(L"$0m", FormattedString(L"%02u", st.wMinute).Array());
    filename.FindReplace(L"$s", UIntString(st.wSecond).Array());
    filename.FindReplace(L"$0s", FormattedString(L"%02u", st.wSecond).Array());

    filename.FindReplace(L"$T", FormattedString(L"%u-%02u-%02u-%02u%02u-%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond).Array());
    return filename;
}

String GetExpandedRecordingDirectoryBase(String path)
{
    if (GetPathExtension(path))
        path = GetPathDirectory(path);

    String expanded = path;
    do
    {
        expanded = ExpandRecordingFilename(path);

        if (expanded == path)
            break;

        if (OSFileIsDirectory(expanded))
            break;

        path = GetPathDirectory(path);
    } while (expanded != path);

    CreatePath(expanded);

    return expanded;
}

String GetOutputFilename(bool replayBuffer=false)
{
    String path = OSGetDefaultVideoSavePath(replayBuffer ? L"\\Replay-$T.flv" : L"\\$T.flv");
    String strOutputFile = AppConfig->GetString(TEXT("Publish"), replayBuffer ? L"ReplayBufferSavePath" : L"SavePath", path.IsValid() ? path.Array() : nullptr);
    strOutputFile.FindReplace(TEXT("\\"), TEXT("/"));

    OSFindData ofd;
    HANDLE hFind = NULL;
    bool bUseDateTimeName = true;
    bool bOverwrite = GlobalConfig->GetInt(L"General", L"OverwriteRecordings", false) != 0;
    
    strOutputFile = ExpandRecordingFilename(strOutputFile);

    CreatePath(GetPathDirectory(strOutputFile));

    if (!bOverwrite && (hFind = OSFindFirstFile(strOutputFile, ofd)))
    {
        String strFileExtension = GetPathExtension(strOutputFile);
        String strFileWithoutExtension = GetPathWithoutExtension(strOutputFile);

        if (strFileExtension.IsValid() && !ofd.bDirectory)
        {
            String strNewFilePath;
            UINT curFile = 0;

            do
            {
                strNewFilePath.Clear() << strFileWithoutExtension << TEXT(" (") << FormattedString(TEXT("%02u"), ++curFile) << TEXT(").") << strFileExtension;
            } while (OSFileExists(strNewFilePath));

            strOutputFile = strNewFilePath;

            bUseDateTimeName = false;
        }

        if (ofd.bDirectory)
            strOutputFile.AppendChar('/');

        OSFindClose(hFind);
    }

    if (bUseDateTimeName)
    {
        String strFileName = GetPathFileName(strOutputFile);

        if (!strFileName.IsValid() || !IsSafeFilename(strFileName))
        {
            SYSTEMTIME st;
            GetLocalTime(&st);

            String strDirectory = GetPathDirectory(strOutputFile);
            String file = strOutputFile.Right(strOutputFile.Length() - strDirectory.Length());
            String extension;

            if (!file.IsEmpty())
                extension = GetPathExtension(file.Array());

            if (extension.IsEmpty())
                extension = TEXT("flv");
            strOutputFile = FormattedString(TEXT("%s/%u-%02u-%02u-%02u%02u-%02u.%s"), strDirectory.Array(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, extension.Array());
        }
    }

    return strOutputFile;
}

VideoFileStream *CreateFileStream(String strOutputFile)
{
    String strFileExtension = GetPathExtension(strOutputFile);
	
    if (strFileExtension.CompareI(TEXT("mp4")))
        return CreateMP4FileStream(strOutputFile);

    return nullptr;
}

bool OBS::StartRecording(bool force)
{
    if (!bRunning || bRecording) return true;
    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
    bool saveToFile = AppConfig->GetInt(L"Publish", L"SaveToFile") != 0;

    bWriteToFile = force || networkMode == 1 || saveToFile;

    // Don't request a keyframe while everything is starting up for the first time
    if(!bStartingUp) videoEncoder->RequestKeyframe();

    String strOutputFile;
    if (bWriteToFile)
        strOutputFile = GetOutputFilename();

    bool success = true;
    if(bWriteToFile && strOutputFile.IsValid())
    {
        fileStream.reset(CreateFileStream(strOutputFile));

        if(!fileStream)
        {
            Log(TEXT("Warning - OBSCapture::Start: Unable to create the file stream. Check the file path in Broadcast Settings."));
            OBSMessageBox(hwndMain, Str("Capture.Start.FileStream.Warning"), Str("Capture.Start.FileStream.WarningCaption"), MB_OK | MB_ICONWARNING);        
            bRecording = false;
            success = false;
        }
        else {
            bRecording = true;
        }
    }
    return success;
}

void OBS::StopRecording(bool immediate)
{
    if (!bStreaming && !bRecordingReplayBuffer && bRunning && bRecording) Stop(true);

    if(!bRecording) return;

    // Prevent the encoder thread from trying to write to fileStream while it's closing

    if (!immediate && fileStreamStop.func)
        return;

    auto shutdown = [this]()
    {
        AddPendingStream(fileStream.release());

        bRecording = false;

        if (!bStreaming && !bRecordingReplayBuffer && bRunning && !bRecording) PostStopMessage(true);
    };

    if (immediate)
        return shutdown();

    fileStreamStop.func = shutdown;

    fileStreamStop.time = (DWORD)(GetVideoTime() - firstFrameTimestamp);
}

void OBS::Start(bool recordingOnly, bool replayBufferOnly)
{
    if(bRunning && !bRecording && !bRecordingReplayBuffer) return;

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
	DWORD delayTime =  (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("Delay"));

    if (bRecording && networkMode != 0) return;

    if (!bRunning && !bStreamFlushed && !recordingOnly && !replayBufferOnly) return;

    if((bRecording || bRecordingReplayBuffer) && networkMode == 0 && delayTime == 0 && !recordingOnly && !replayBufferOnly && bStreamFlushed) {
        bFirstConnect = !bReconnecting;
         
        network.reset(CreateRTMPPublisher());

        Log(TEXT("=====Stream Start (while recording): %s============================="), CurrentDateTimeString().Array());

        bSentHeaders = false;
        bStreaming = true;
        return;
    }

    bStartingUp = true;

    OSEnterMutex(hStartupShutdownMutex);

    DisableMenusWhileStreaming(true);

    scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");
    scenesConfig.Save();

    //-------------------------------------------------------------

    fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
    frameTime = 1000/fps;

    OSCheckForBuggyDLLs();

    //-------------------------------------------------------------
retryHookTest:
    bool alreadyWarnedAboutModules = false;
    if (OSIncompatibleModulesLoaded())
    {
        Log(TEXT("Incompatible modules (pre-D3D) detected."));
        int ret = OBSMessageBox(hwndMain, Str("IncompatibleModules"), NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE);
        if (ret == IDABORT)
        {
            DisableMenusWhileStreaming(false);
            OSLeaveMutex (hStartupShutdownMutex);
            bStartingUp = false;
            return;
        }
        else if (ret == IDRETRY)
        {
            goto retryHookTest;
        }

        alreadyWarnedAboutModules = true;
    }

    String strPatchesError;
    if (OSIncompatiblePatchesLoaded(strPatchesError))
    {
        DisableMenusWhileStreaming(false);
        OSLeaveMutex (hStartupShutdownMutex);
        OBSMessageBox(hwndMain, strPatchesError.Array(), NULL, MB_ICONERROR);
        Log(TEXT("Incompatible patches detected."));
        bStartingUp = false;
        return;
    }

    //check the user isn't trying to stream or record with no sources which is typically
    //a configuration error

        bool foundSource = false;
        XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
        if (scenes)
        {
            UINT numScenes = scenes->NumElements();

            for (UINT i = 0; i<numScenes; i++)
            {
                XElement *sceneElement = scenes->GetElementByID(i);
                XElement *sources = sceneElement->GetElement(TEXT("sources"));
                if (sources && sources->NumElements())
                {
                    foundSource = true;
                    break;
                }
            }
        }

        if (!foundSource)
        {
            if (OBSMessageBox(hwndMain, Str("NoSourcesFound"), NULL, MB_ICONWARNING|MB_YESNO) == IDNO)
            {
                DisableMenusWhileStreaming(false);
                OSLeaveMutex(hStartupShutdownMutex);
                bStartingUp = false;
                return;
            }
        }
    

    //-------------------------------------------------------------

    String processPriority = AppConfig->GetString(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
    if (!scmp(processPriority, TEXT("Idle")))
        SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    else if (!scmp(processPriority, TEXT("Above Normal")))
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    else if (!scmp(processPriority, TEXT("High")))
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
	delayTime = 0;// (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("Delay"));

    String strError;

    bFirstConnect = !bReconnecting;
    network.reset(CreateRTMPPublisher());
    

    if(!network)
    {
        DisableMenusWhileStreaming(false);
        OSLeaveMutex (hStartupShutdownMutex);

        if(!bReconnecting)
            OBSMessageBox(hwndMain, strError, NULL, MB_ICONERROR);
        else
            OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwndMain, OBS::ReconnectDialogProc);
        bStartingUp = false;
        return;
    }

    bReconnecting = false;

    //-------------------------------------------------------------

    Log(TEXT("=====Stream Start: %s==============================================="), CurrentDateTimeString().Array());

    //-------------------------------------------------------------

    bEnableProjectorCursor = GlobalConfig->GetInt(L"General", L"EnableProjectorCursor", 1) != 0;
    bPleaseEnableProjector = bPleaseDisableProjector = false;

    int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
    if(monitorID >= 1)
        monitorID = 0;

	int defCX = 1920;//
	int defCY = 1080;//

    downscaleType = AppConfig->GetInt(TEXT("Video"), TEXT("Filter"), 0);
    downscale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
    baseCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
    baseCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);

    baseCX = MIN(MAX(baseCX, 128), 4096);
    baseCY = MIN(MAX(baseCY, 128), 4096);

    scaleCX = UINT(double(baseCX) / double(downscale));
    scaleCY = UINT(double(baseCY) / double(downscale));

    //align width to 128bit for fast SSE YUV4:2:0 conversion
    outputCX = scaleCX & 0xFFFFFFFC;
    outputCY = scaleCY & 0xFFFFFFFE;

    bUseMultithreadedOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
    Log(TEXT("  Multithreaded optimizations: %s"), (CTSTR)(bUseMultithreadedOptimizations ? TEXT("On") : TEXT("Off")));

    encoderSkipThreshold = GlobalConfig->GetInt(TEXT("Video"), TEXT("EncoderSkipThreshold"), fps/4);
    if (encoderSkipThreshold < 1)
        encoderSkipThreshold = 1;

    //------------------------------------------------------------------

    Log(TEXT("  Base resolution: %ux%u"), baseCX, baseCY);
    Log(TEXT("  Output resolution: %ux%u"), outputCX, outputCY);
    Log(TEXT("------------------------------------------"));

    //------------------------------------------------------------------

//     GS = new D3D10System;
//     GS->Init();

    //Thanks to ASUS OSD hooking the goddamn user mode driver framework (!!!!), we have to re-check for dangerous
    //hooks after initializing D3D.
retryHookTestV2:
    if (!alreadyWarnedAboutModules)
    {
        if (OSIncompatibleModulesLoaded())
        {
            Log(TEXT("Incompatible modules (post-D3D) detected."));
            int ret = OBSMessageBox(hwndMain, Str("IncompatibleModules"), NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE);
            if (ret == IDABORT)
            {
                //FIXME: really need a better way to abort startup than this...
                network.reset();
                DisableMenusWhileStreaming(false);
                OSLeaveMutex (hStartupShutdownMutex);
                bStartingUp = false;
                return;
            }
            else if (ret == IDRETRY)
            {
                goto retryHookTestV2;
            }
        }
    }
    //------------------------------------------------------------------

    String strEncoder = AppConfig->GetString(TEXT("Audio Encoding"), TEXT("Codec"), TEXT("AAC"));
    BOOL isAAC = strEncoder.CompareI(TEXT("AAC"));

    UINT format = AppConfig->GetInt(L"Audio Encoding", L"Format", 1);

    if (!isAAC)
        format = 0;

    switch (format) {
    case 0: sampleRateHz = 44100; break;
    default:
    case 1: sampleRateHz = 48000; break;
    }

    Log(L"------------------------------------------");
    Log(L"Audio Format: %u Hz", sampleRateHz);

    //------------------------------------------------------------------

    BOOL isStereo = AppConfig->GetInt(L"Audio Encoding", L"isStereo", 1);

    switch (isStereo) {
    case 0: audioChannels = 1; break;
    default:
    case 1: audioChannels = 2; break;
    }

    Log(L"------------------------------------------");
    Log(L"Audio Channels: %u Ch", audioChannels);

    //------------------------------------------------------------------

    AudioDeviceList playbackDevices;
    bool useInputDevices = AppConfig->GetInt(L"Audio", L"InputDevicesForDesktopSound", false) != 0;
    GetAudioDevices(playbackDevices, useInputDevices ? ADT_RECORDING : ADT_PLAYBACK);

    String strPlaybackDevice = AppConfig->GetString(TEXT("Audio"), TEXT("PlaybackDevice"), TEXT("Default"));
    if(strPlaybackDevice.IsEmpty() || !playbackDevices.HasID(strPlaybackDevice))
    {
        //AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), TEXT("Default"));
        strPlaybackDevice = TEXT("Default");
    }

    Log(TEXT("Playback device %s"), strPlaybackDevice.Array());
    playbackDevices.FreeData();

    desktopAudio = CreateAudioSource(false, strPlaybackDevice);

    if(!desktopAudio) {
        CrashError(TEXT("Cannot initialize desktop audio sound, more info in the log file."));
    }

    if (useInputDevices)
        Log(L"Use Input Devices enabled, not recording standard desktop audio");

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING, false, true);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        //AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    String strDefaultMic;
    bool bHasDefault = GetDefaultMicID(strDefaultMic);

    if(strDevice.CompareI(TEXT("Disable")))
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    else
    {
        bool bUseDefault = strDevice.CompareI(TEXT("Default")) != 0;
        if(!bUseDefault || bHasDefault)
        {
            if(bUseDefault)
                strDevice = strDefaultMic;

            micAudio = CreateAudioSource(true, strDevice);

            if(!micAudio)
                OBSMessageBox(hwndMain, Str("MicrophoneFailure"), NULL, 0);
            else
            {
                int offset = AppConfig->GetInt(TEXT("Audio"), TEXT("MicTimeOffset"), 0);
                Log(L"Mic time offset: %d", offset);
                micAudio->SetTimeOffset(offset);
            }

            EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), micAudio != NULL);
        }
        else
            EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    }

    UINT bitRate = (UINT)AppConfig->GetInt(TEXT("Audio Encoding"), TEXT("Bitrate"), 96);

#ifdef USE_AAC
	audioEncoder = CreateAACEncoder(bitRate);
#endif

    //-------------------------------------------------------------

    desktopVol = AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);
    micVol     = AppConfig->GetFloat(TEXT("Audio"), TEXT("MicVolume"),     1.0f);

    //-------------------------------------------------------------
    bRunning = true;

    //-------------------------------------------------------------

    int maxBitRate = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int bufferSize = maxBitRate;
    if (AppConfig->GetInt(L"Video Encoding", L"UseBufferSize", 0) != 0)
        bufferSize = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
    int quality    = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Quality"),    8);
    String preset  = AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"),     TEXT("veryfast"));
    bUsing444      = false;//AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Use444"),     0) != 0;
    bUseCFR        = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCFR"), 1) != 0;

    //-------------------------------------------------------------

    bufferingTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 700);
    Log(TEXT("Scene buffering time set to %u"), bufferingTime);

    //-------------------------------------------------------------

    bForceMicMono = AppConfig->GetInt(TEXT("Audio"), TEXT("ForceMicMono")) != 0;
    bRecievedFirstAudioFrame = false;

    //hRequestAudioEvent = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
    hSoundDataMutex = OSCreateMutex();
    hSoundThread = OSCreateThread((XTHREAD)OBS::MainAudioThread, NULL);
    //-------------------------------------------------------------

    colorDesc.fullRange = AppConfig->GetInt(L"Video", L"FullRange") != 0;
    colorDesc.primaries = ColorPrimaries_BT709;
    colorDesc.transfer  = ColorTransfer_IEC6196621;
    colorDesc.matrix    = outputCX >= 1280 || outputCY > 576 ? ColorMatrix_BT709 : ColorMatrix_SMPTE170M;

    videoEncoder = nullptr;
    String videoEncoderErrors;
	String vencoder = L";NVENC";// AppConfig->GetString(L"Video Encoding", L"Encoder");


    if(vencoder == L"NVENC")
		videoEncoder = CreateNVENCEncoder(fps, outputCX, outputCY, quality, preset, bUsing444, colorDesc, maxBitRate, bufferSize, bUseCFR, videoEncoderErrors);
    else
        videoEncoder = CreateX264Encoder(fps, outputCX, outputCY, quality, preset, bUsing444, colorDesc, maxBitRate, bufferSize, bUseCFR);

    if (!videoEncoder)
    {
        Log(L"Couldn't initialize encoder");
        Stop(true);

        if (videoEncoderErrors.IsEmpty())
            videoEncoderErrors = Str("Encoder.InitFailed");
        else
            videoEncoderErrors = String(Str("Encoder.InitFailedWithReason")) + videoEncoderErrors;

        OBSMessageBox(hwndMain, videoEncoderErrors.Array(), nullptr, MB_OK | MB_ICONERROR); //might want to defer localization until here to automatically
                                                                                         //output english localization to logfile
        return;
    }
    //-------------------------------------------------------------

    if ((!replayBufferOnly && !StartRecording(recordingOnly)) && !bStreaming)
    {
        Stop(true);
        return;
    }

    //-------------------------------------------------------------

    curFramePic = NULL;
    bShutdownVideoThread = false;
    bShutdownEncodeThread = false;
    //ResetEvent(hVideoThread);
    hEncodeThread = OSCreateThread((XTHREAD)OBS::EncodeThread, NULL);
    hVideoThread = OSCreateThread((XTHREAD)OBS::MainCaptureThread, NULL);

    //EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), TRUE);
    
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, 0);
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED);

    //UpdateRenderViewMessage();

    OSLeaveMutex (hStartupShutdownMutex);

    bStartingUp = false;
}

void OBS::Stop(bool overrideKeepRecording, bool stopReplayBuffer)
{
    if((!bStreaming && !bRecording && !bRunning && !bRecordingReplayBuffer) ) return;

    //ugly hack to prevent hotkeys from being processed while we're stopping otherwise we end up
    //with callbacks from the ProcessEvents call in DelayedPublisher which causes havoc.
    OSEnterMutex(hHotkeyMutex);

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);

    if((!overrideKeepRecording && ((bRecording && bKeepRecording) || (!stopReplayBuffer && bRecordingReplayBuffer))) && networkMode == 0) {
        videoEncoder->RequestKeyframe();

        if (!networkStop.func && network)
        {
            networkStop.func = [this]()
            {
                videoEncoder->RequestKeyframe();

                auto stream = move(network);

                Log(TEXT("=====Stream End (recording continues): %s========================="), CurrentDateTimeString().Array());

                bStreamFlushed = false;

                AddPendingStream(stream.release(), [this]()
                {
                    bStreaming = false;
                    bSentHeaders = false;

                    bStreamFlushed = true;

                    if (!bStreaming && !bRecordingReplayBuffer && bRunning && !bRecording) PostStopMessage(true);
                });
            };
            networkStop.time = (DWORD)(GetVideoTime() - firstFrameTimestamp);
        }

        OSLeaveMutex(hHotkeyMutex);

        if (bRecordingReplayBuffer && (bRecording && (overrideKeepRecording || !bKeepRecording)))
            StopRecording();

        return;
    }

    OSEnterMutex(hStartupShutdownMutex);

    //we only want the capture thread to stop first, so we can ensure all packets are flushed
    bShutdownEncodeThread = true;
    ShowWindow(hwndProjector, SW_HIDE);

    if(hEncodeThread){
        OSTerminateThread(hEncodeThread, 30001);
        hEncodeThread = NULL;
    }

    bShutdownVideoThread = true;
    SetEvent(hVideoEvent);

    if(hVideoThread){
        OSTerminateThread(hVideoThread, 30002);
        hVideoThread = NULL;
    }

    bRunning = false;

    for(UINT i=0; i<globalSources.Num(); i++)
        globalSources[i].source->EndScene();

    //-------------------------------------------------------------

    if(hSoundThread)
    {
        //ReleaseSemaphore(hRequestAudioEvent, 1, NULL);
        OSTerminateThread(hSoundThread, 20000);
    }

    //if(hRequestAudioEvent)
    //    CloseHandle(hRequestAudioEvent);
    if(hSoundDataMutex)
        OSCloseMutex(hSoundDataMutex);

    hSoundThread = NULL;
    //hRequestAudioEvent = NULL;
    hSoundDataMutex = NULL;

    //-------------------------------------------------------------

    StopBlankSoundPlayback();

    //-------------------------------------------------------------

    if (bStreaming)
    {
        bStreamFlushed = false;
        AddPendingStream(network.release(), [this]()
        {
            bStreaming = false;
            bStreamFlushed = true;

        });
    }
    else
    {
        network.reset();
        bStreaming = false;

    }
    
    if (bRecording) StopRecording(true);

    delete micAudio;
    micAudio = NULL;

    delete desktopAudio;
    desktopAudio = NULL;

    delete audioEncoder;
    audioEncoder = NULL;

    delete videoEncoder;
    videoEncoder = NULL;

    //-------------------------------------------------------------

    for(UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();
    pendingAudioFrames.Clear();

    for(UINT i=0; i<globalSources.Num(); i++)
        globalSources[i].FreeData();
    globalSources.Clear();

    //-------------------------------------------------------------

    for(UINT i=0; i<auxAudioSources.Num(); i++)
        delete auxAudioSources[i];
    auxAudioSources.Clear();

    //-------------------------------------------------------------

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING, false, true);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();
    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), !strDevice.CompareI(TEXT("Disable")));

    //-------------------------------------------------------------

    //ClearStreamInfo();

    audioWarningId = 0;

    Log(TEXT("=====Stream End: %s================================================="), CurrentDateTimeString().Array());

    //update notification icon to reflect current status
//    UpdateNotificationAreaIcon();

    bEditMode = false;
    SendMessage(GetDlgItem(hwndMain, ID_SCENEEDITOR), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), FALSE);
   // ClearStatusBar();

    InvalidateRect(hwndRenderFrame, NULL, TRUE);

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);
    SetThreadExecutionState(ES_CONTINUOUS);

    String processPriority = AppConfig->GetString(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
    if (scmp(processPriority, TEXT("Normal")))
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

    //UpdateRenderViewMessage();

    DisableMenusWhileStreaming(false);

    OSLeaveMutex(hStartupShutdownMutex);

    OSLeaveMutex(hHotkeyMutex);
}

DWORD STDCALL OBS::MainAudioThread(LPVOID lpUnused)
{
    CoInitialize(0);
    App->MainAudioLoop();
    CoUninitialize();
    return 0;
}

#define INVALID_LL 0xFFFFFFFFFFFFFFFFLL

inline void CalculateVolumeLevels(float *buffer, int totalFloats, float mulVal, float &RMS, float &MAX)
{
    float sum = 0.0f;
    int totalFloatsStore = totalFloats;

    float Max = 0.0f;

    if((UPARAM(buffer) & 0xF) == 0)
    {
        UINT alignedFloats = totalFloats & 0xFFFFFFFC;
        __m128 sseMulVal = _mm_set_ps1(mulVal);

        for(UINT i=0; i<alignedFloats; i += 4)
        {
            __m128 sseScaledVals = _mm_mul_ps(_mm_load_ps(buffer+i), sseMulVal);

            /*compute squares and add them to the sum*/
            __m128 sseSquares = _mm_mul_ps(sseScaledVals, sseScaledVals);
            sum += sseSquares.m128_f32[0] + sseSquares.m128_f32[1] + sseSquares.m128_f32[2] + sseSquares.m128_f32[3];

            /* 
                sse maximum of squared floats 
                concept from: http://stackoverflow.com/questions/9795529/how-to-find-the-horizontal-maximum-in-a-256-bit-avx-vector
            */
            __m128 sseSquaresP = _mm_shuffle_ps(sseSquares, sseSquares, _MM_SHUFFLE(1, 0, 3, 2));
            __m128 halfmax = _mm_max_ps(sseSquares, sseSquaresP);
            __m128 halfmaxP = _mm_shuffle_ps(halfmax, halfmax, _MM_SHUFFLE(0,1,2,3));
            __m128 maxs = _mm_max_ps(halfmax, halfmaxP);

            Max = max(Max, maxs.m128_f32[0]);
        }

        buffer      += alignedFloats;
        totalFloats -= alignedFloats;
    }

    for(int i=0; i<totalFloats; i++)
    {
        float val = buffer[i] * mulVal;
        float pow2Val = val * val;
        sum += pow2Val;
        Max = max(Max, pow2Val);
    }

    RMS = sqrt(sum / totalFloatsStore);
    MAX = sqrt(Max);
}

inline float toDB(float RMS)
{
    float db = 20.0f * log10(RMS);
    if(!_finite(db))
        return VOL_MIN;
    return db;
}

bool OBS::QueryAudioBuffers(bool bQueriedDesktopDebugParam)
{
    bool bGotSomeAudio = false;

    if (!latestAudioTime) {
        desktopAudio->GetEarliestTimestamp(latestAudioTime); //will always return true
    } else {
        QWORD latestDesktopTimestamp;
        if (desktopAudio->GetLatestTimestamp(latestDesktopTimestamp)) {
            if ((latestAudioTime+10) > latestDesktopTimestamp)
                return false;
        }
        latestAudioTime += 10;
    }

    bufferedAudioTimes << latestAudioTime;

    OSEnterMutex(hAuxAudioMutex);
    for(UINT i=0; i<auxAudioSources.Num(); i++)
    {
        if (auxAudioSources[i]->QueryAudio2(auxAudioSources[i]->GetVolume(), true) != NoAudioAvailable)
            bGotSomeAudio = true;
    }

    OSLeaveMutex(hAuxAudioMutex);

    if(micAudio != NULL)
    {
        if (micAudio->QueryAudio2(curMicVol, true) != NoAudioAvailable)
            bGotSomeAudio = true;
    }

    return bGotSomeAudio;
}

bool OBS::QueryNewAudio()
{
    bool bAudioBufferFilled = false;

    while (!bAudioBufferFilled) {
        bool bGotAudio = false;

        //don't let the audio get backed up too far, as this breaks things
        if (desktopAudio->GetBufferedTime() > App->bufferingTime * 1.5)
        {
            if (!audioWarningId)
                audioWarningId = App->AddStreamInfo(TEXT("Audio is processing too slow. Free up CPU, reduce the number of audio sources or avoid resampling."), StreamInfoPriority_Critical);

            bAudioBufferFilled = true;
        }
        else
        {
            if (audioWarningId && desktopAudio->GetBufferedTime() <= App->bufferingTime)
            {
                App->RemoveStreamInfo(audioWarningId);
                audioWarningId = 0;
            }

            if ((desktopAudio->QueryAudio2(curDesktopVol)) != NoAudioAvailable) {
                QueryAudioBuffers(true);
                bGotAudio = true;
            }

            bAudioBufferFilled = desktopAudio->GetBufferedTime() >= App->bufferingTime;
        }

        if (!bGotAudio && bAudioBufferFilled)
            QueryAudioBuffers(false);

        if (bAudioBufferFilled || !bGotAudio)
            break;
    }

    /* wait until buffers are completely filled before accounting for burst */
    if (!bAudioBufferFilled)
    {
        QWORD timestamp;
        int burst = 0;

        // No more desktop data, drain auxilary/mic buffers until they're dry to prevent burst data
        OSEnterMutex(hAuxAudioMutex);
        for(UINT i=0; i<auxAudioSources.Num(); i++)
        {
            while (auxAudioSources[i]->QueryAudio2(auxAudioSources[i]->GetVolume(), true) != NoAudioAvailable)
                burst++;

            if (auxAudioSources[i]->GetLatestTimestamp(timestamp))
                auxAudioSources[i]->SortAudio(timestamp);

            /*if (burst > 10)
                Log(L"Burst happened for %s", auxAudioSources[i]->GetDeviceName2());*/
        }

        OSLeaveMutex(hAuxAudioMutex);

        burst = 0;

        if (micAudio)
        {
            while (micAudio->QueryAudio2(curMicVol, true) != NoAudioAvailable)
                burst++;

            /*if (burst > 10)
                Log(L"Burst happened for %s", micAudio->GetDeviceName2());*/

            if (micAudio->GetLatestTimestamp(timestamp))
                micAudio->SortAudio(timestamp);
        }
    }

    return bAudioBufferFilled;
}

void OBS::EncodeAudioSegment(float *buffer, UINT numFrames, QWORD timestamp)
{
    DataPacket packet;
    if(audioEncoder->Encode(buffer, numFrames, packet, timestamp))
    {
        OSEnterMutex(hSoundDataMutex);

        FrameAudio *frameAudio = pendingAudioFrames.CreateNew();
        frameAudio->audioData.CopyArray(packet.lpPacket, packet.size);
        frameAudio->timestamp = timestamp;

        OSLeaveMutex(hSoundDataMutex);
    }
}

void OBS::MainAudioLoop()
{
    const unsigned int audioSamplesPerSec = App->GetSampleRateHz();
    const unsigned int audioSampleSize = audioSamplesPerSec/100;

    DWORD taskID = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskID);

    bufferedAudioTimes.Clear();

    micMax = desktopMax = VOL_MIN;
    micPeak = desktopPeak = VOL_MIN;

    UINT audioFramesSinceMeterUpdate = 0;
    UINT audioFramesSinceMicMaxUpdate = 0;
    UINT audioFramesSinceDesktopMaxUpdate = 0;

    List<float> mixBuffer, levelsBuffer;
    mixBuffer.SetSize(audioSampleSize*2);
    levelsBuffer.SetSize(audioSampleSize*2);

    latestAudioTime = 0;

    //---------------------------------------------
    // the audio loop of doom

    while (true) {
        
        if (!bRunning)
            break;

        //-----------------------------------------------

        float *desktopBuffer = nullptr, *micBuffer = nullptr;

        curDesktopVol = desktopVol * desktopBoost;
		
		curMicVol = micVol;
        curMicVol *= micBoost;

        bool bDesktopMuted = (curDesktopVol < VOLN_MUTELEVEL);
        bool bMicEnabled   = (micAudio != NULL);

        if (QueryNewAudio()) {
            QWORD timestamp = bufferedAudioTimes[0];
            bufferedAudioTimes.Remove(0);

            zero(mixBuffer.Array(),    audioSampleSize*2*sizeof(float));
            zero(levelsBuffer.Array(), audioSampleSize*2*sizeof(float));

            //----------------------------------------------------------------------------
            // get latest sample for calculating the volume levels

            float *latestDesktopBuffer = NULL, *latestMicBuffer = NULL;

            desktopAudio->GetBuffer(&desktopBuffer, timestamp);
            desktopAudio->GetNewestFrame(&latestDesktopBuffer);

            if (micAudio != NULL) {
                micAudio->GetBuffer(&micBuffer, timestamp);
                micAudio->GetNewestFrame(&latestMicBuffer);
            }

            //----------------------------------------------------------------------------
            // mix desktop samples

            if (desktopBuffer)
                MixAudio(mixBuffer.Array(), desktopBuffer, audioSampleSize*2, false);

            if (latestDesktopBuffer)
                MixAudio(levelsBuffer.Array(), latestDesktopBuffer, audioSampleSize*2, false);

            //----------------------------------------------------------------------------
            // get latest aux volume level samples and mix

            OSEnterMutex(hAuxAudioMutex);

            for (UINT i=0; i<auxAudioSources.Num(); i++) {
                float *latestAuxBuffer;

                if(auxAudioSources[i]->GetNewestFrame(&latestAuxBuffer))
                    MixAudio(levelsBuffer.Array(), latestAuxBuffer, audioSampleSize*2, false);
            }

            //----------------------------------------------------------------------------
            // mix output aux sound samples with the desktop

            for (UINT i=0; i<auxAudioSources.Num(); i++) {
                float *auxBuffer;

                if(auxAudioSources[i]->GetBuffer(&auxBuffer, timestamp))
                    MixAudio(mixBuffer.Array(), auxBuffer, audioSampleSize*2, false);
            }

            OSLeaveMutex(hAuxAudioMutex);

            //----------------------------------------------------------------------------
            // multiply samples by volume and compute RMS and max of samples
            // Use 1.0f instead of curDesktopVol, since aux audio sources already have their volume set, and shouldn't be boosted anyway.

            float desktopRMS = 0, micRMS = 0, desktopMx = 0, micMx = 0;
            if (latestDesktopBuffer)
                CalculateVolumeLevels(levelsBuffer.Array(), audioSampleSize*2, 1.0f, desktopRMS, desktopMx);
            if (bMicEnabled && latestMicBuffer)
                CalculateVolumeLevels(latestMicBuffer, audioSampleSize*2, curMicVol, micRMS, micMx);

            //----------------------------------------------------------------------------
            // convert RMS and Max of samples to dB 

            desktopRMS = toDB(desktopRMS);
            micRMS = toDB(micRMS);
            desktopMx = toDB(desktopMx);
            micMx = toDB(micMx);

            //----------------------------------------------------------------------------
            // update max if sample max is greater or after 1 second

            float maxAlpha = 0.15f;
            UINT peakMeterDelayFrames = audioSamplesPerSec * 3;

            if (micMx > micMax)
                micMax = micMx;
            else
                micMax = maxAlpha * micMx + (1.0f - maxAlpha) * micMax;

            if(desktopMx > desktopMax)
                desktopMax = desktopMx;
            else
                desktopMax = maxAlpha * desktopMx + (1.0f - maxAlpha) * desktopMax;

            //----------------------------------------------------------------------------
            // update delayed peak meter

            if (micMax > micPeak || audioFramesSinceMicMaxUpdate > peakMeterDelayFrames) {
                micPeak = micMax;
                audioFramesSinceMicMaxUpdate = 0;
            } else {
                audioFramesSinceMicMaxUpdate += audioSampleSize;
            }

            if (desktopMax > desktopPeak || audioFramesSinceDesktopMaxUpdate > peakMeterDelayFrames) {
                desktopPeak = desktopMax;
                audioFramesSinceDesktopMaxUpdate = 0;
            } else {
                audioFramesSinceDesktopMaxUpdate += audioSampleSize;
            }

            //----------------------------------------------------------------------------
            // low pass the level sampling

            float rmsAlpha = 0.15f;
            desktopMag = rmsAlpha * desktopRMS + desktopMag * (1.0f - rmsAlpha);
            micMag = rmsAlpha * micRMS + micMag * (1.0f - rmsAlpha);

            //----------------------------------------------------------------------------
            // update the meter about every 50ms

            audioFramesSinceMeterUpdate += audioSampleSize;
            if (audioFramesSinceMeterUpdate >= (audioSampleSize*5)) {
                //PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_MICVOLUMEMETER, VOLN_METERED), 0);
                audioFramesSinceMeterUpdate = 0;
            }

            //----------------------------------------------------------------------------
            // mix mic and desktop sound
            // also, it's perfectly fine to just mix into the returned buffer

            if (bMicEnabled && micBuffer)
                MixAudio(mixBuffer.Array(), micBuffer, audioSampleSize*2, bForceMicMono);

            EncodeAudioSegment(mixBuffer.Array(), audioSampleSize, timestamp);
        }
        else
        {
            OSSleep(5); //screw it, just run it every 5ms
        }

        //-----------------------------------------------

        if (!bRecievedFirstAudioFrame && pendingAudioFrames.Num())
            bRecievedFirstAudioFrame = true;
    }

    desktopMag = desktopMax = desktopPeak = VOL_MIN;
    micMag = micMax = micPeak = VOL_MIN;

    //PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_MICVOLUMEMETER, VOLN_METERED), 0);

    for (UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();

    AvRevertMmThreadCharacteristics(hTask);
}

void OBS::RequestKeyframe(int waitTime)
{
    if(bRequestKeyframe && waitTime > keyframeWait)
        return;

    bRequestKeyframe = true;
    keyframeWait = waitTime;
}

void OBS::AddPendingStream(ClosableStream *stream, std::function<void()> finishedCallback)
{
    using namespace std;
    struct args_t
    {
        using stream_t = remove_pointer_t<decltype(stream)>;
        unique_ptr<stream_t> stream;
        decltype(finishedCallback) finishedCallback;
        args_t(stream_t *stream, decltype(finishedCallback) finishedCallback) : stream(stream), finishedCallback(move(finishedCallback)) {}
    };

    auto args = make_unique<args_t>(stream, move(finishedCallback));

    ScopedLock l(pendingStreams.mutex);
    pendingStreams.streams.emplace_back(OSCreateThread([](void *arg) -> DWORD
    {
        unique_ptr<args_t> args(static_cast<args_t*>(arg));
        args->stream.reset();
        if (args->finishedCallback)
            args->finishedCallback();
        return 0;
    }, args.release()));
}

void OBS::AddPendingStreamThread(HANDLE thread)
{
    ScopedLock l(pendingStreams.mutex);
    pendingStreams.streams.emplace_back(thread);
}

void OBS::ClosePendingStreams()
{
    ScopedLock l(pendingStreams.mutex);
    if (pendingStreams.streams.empty())
        return;

    using namespace std;
    vector<HANDLE> handles;
    for (auto &pendingStream : pendingStreams.streams)
        handles.push_back(pendingStream.get());

    if (WaitForMultipleObjects(handles.size(), handles.data(), true, 5) != WAIT_OBJECT_0)
    {
        using ::locale;
        int res = IDNO;
        do
        {
            auto res = OBSMessageBox(hwndMain, Str("StreamClosePending"), nullptr, MB_YESNO | MB_ICONEXCLAMATION);

            if (res != IDYES)
                return;

            if (WaitForMultipleObjects(handles.size(), handles.data(), true, 15 * 1000) == WAIT_OBJECT_0)
                return;

        } while (res == IDYES);
    }
}
