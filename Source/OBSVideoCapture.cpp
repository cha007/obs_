#include "Main.h"

#include <inttypes.h>
#include "mfxstructures.h"
extern "C"
{
#include "../x264/x264.h"
}

#include <memory>

void Convert444toI420(LPBYTE input, int width, int pitch, int height, int startY, int endY, LPBYTE *output);
void Convert444toNV12(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output);

DWORD STDCALL OBS::EncodeThread(LPVOID lpUnused)
{
    App->EncodeLoop();
    return 0;
}

DWORD STDCALL OBS::MainCaptureThread(LPVOID lpUnused)
{
    App->MainCaptureLoop();
    return 0;
}


struct Convert444Data
{
    LPBYTE input;
    LPBYTE output[3];
    bool bNV12;
    bool bKillThread;
    HANDLE hSignalConvert, hSignalComplete;
    int width, height, inPitch, outPitch, startY, endY;
    DWORD numThreads;
};

DWORD STDCALL Convert444Thread(Convert444Data *data)
{
    do
    {
        WaitForSingleObject(data->hSignalConvert, INFINITE);
        if(data->bKillThread) break;
        if(data->bNV12)
            Convert444toNV12(data->input, data->width, data->inPitch, data->outPitch, data->height, data->startY, data->endY, data->output);
        else
            Convert444toNV12(data->input, data->width, data->inPitch, data->width, data->height, data->startY, data->endY, data->output);

        SetEvent(data->hSignalComplete);
    }while(!data->bKillThread);

    return 0;
}

bool OBS::BufferVideoData(const List<DataPacket> &inputPackets, const List<PacketType> &inputTypes, DWORD timestamp, DWORD out_pts, QWORD firstFrameTime, VideoSegment &segmentOut){
    VideoSegment &segmentIn = *bufferedVideo.CreateNew();
    segmentIn.timestamp = timestamp;
    segmentIn.pts = out_pts;

    segmentIn.packets.SetSize(inputPackets.Num());
    for(UINT i=0; i<inputPackets.Num(); i++)
    {
        segmentIn.packets[i].data.CopyArray(inputPackets[i].lpPacket, inputPackets[i].size);
        segmentIn.packets[i].type =  inputTypes[i];
    }

    bool dataReady = false;

    OSEnterMutex(hSoundDataMutex);
    for (UINT i = 0; i < pendingAudioFrames.Num(); i++)
    {
        if (firstFrameTime < pendingAudioFrames[i].timestamp && pendingAudioFrames[i].timestamp - firstFrameTime >= bufferedVideo[0].timestamp)
        {
            dataReady = true;
            break;
        }
    }
    OSLeaveMutex(hSoundDataMutex);

    if (dataReady)
    {
        segmentOut.packets.TransferFrom(bufferedVideo[0].packets);
        segmentOut.timestamp = bufferedVideo[0].timestamp;
        segmentOut.pts = bufferedVideo[0].pts;
        bufferedVideo.Remove(0);

        return true;
    }

    return false;
}

#define NUM_OUT_BUFFERS 3

struct EncoderPicture
{
    x264_picture_t *picOut;
    mfxFrameSurface1 *mfxOut;
    EncoderPicture() : picOut(nullptr), mfxOut(nullptr) {}
};

bool operator==(const EncoderPicture& lhs, const EncoderPicture& rhs)
{
    if(lhs.picOut && rhs.picOut)
        return lhs.picOut == rhs.picOut;
    if(lhs.mfxOut && rhs.mfxOut)
        return lhs.mfxOut == rhs.mfxOut;
    return false;
}

struct FrameProcessInfo
{
    EncoderPicture *pic;

    DWORD frameTimestamp;
    QWORD firstFrameTime;
};

void OBS::SendFrame(VideoSegment &curSegment, QWORD firstFrameTime)
{
    if(!bSentHeaders)
    {
        if(network && curSegment.packets[0].data[0] == 0x17) {
            network->BeginPublishing();
            bSentHeaders = true;
        }
    }

    OSEnterMutex(hSoundDataMutex);

    if(pendingAudioFrames.Num())
    {
        while(pendingAudioFrames.Num())
        {
            if(firstFrameTime < pendingAudioFrames[0].timestamp)
            {
                UINT audioTimestamp = UINT(pendingAudioFrames[0].timestamp-firstFrameTime);

                //stop sending audio packets when we reach an audio timestamp greater than the video timestamp
                if(audioTimestamp > curSegment.timestamp)
                    break;

                if(audioTimestamp == 0 || audioTimestamp > lastAudioTimestamp)
                {
                    List<BYTE> &audioData = pendingAudioFrames[0].audioData;
                    if(audioData.Num())
                    {
                        //Log(TEXT("a:%u, %llu"), audioTimestamp, frameInfo.firstFrameTime+audioTimestamp);

                        if(network)
                            network->SendPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);

                        if (fileStream)
                        {
                            auto shared_data = std::make_shared<const std::vector<BYTE>>(audioData.Array(), audioData.Array() + audioData.Num());
                            if (fileStream)
                                fileStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);                          
                        }

                        audioData.Clear();

                        lastAudioTimestamp = audioTimestamp;
                    }
                }
            }
            else
                nop();

            pendingAudioFrames[0].audioData.Clear();
            pendingAudioFrames.Remove(0);
        }
    }

    OSLeaveMutex(hSoundDataMutex);

    for(UINT i=0; i<curSegment.packets.Num(); i++)
    {
        VideoPacketData &packet = curSegment.packets[i];

        if(packet.type == PacketType_VideoHighest)
            bRequestKeyframe = false;

        //Log(TEXT("v:%u, %llu"), curSegment.timestamp, frameInfo.firstFrameTime+curSegment.timestamp);

        if (network)
        {
            if (!HandleStreamStopInfo(networkStop, packet.type, curSegment))
                network->SendPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
        }

        if (fileStream )
        {
            auto shared_data = std::make_shared<const std::vector<BYTE>>(packet.data.Array(), packet.data.Array() + packet.data.Num());
            if (fileStream)
            {
                if (!HandleStreamStopInfo(fileStreamStop, packet.type, curSegment))
                    fileStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
            }
        }
    }
}

bool OBS::HandleStreamStopInfo(OBS::StopInfo &info, PacketType type, const VideoSegment& segment)
{
    if (type == PacketType_Audio || !info.func)
        return false;

    if (segment.pts < info.time)
        return false;

    if (!info.timeSeen)
    {
        info.timeSeen = true;
        return false;
    }

    info.func();
    info = {};
    return true;
}

bool OBS::ProcessFrame(FrameProcessInfo &frameInfo)
{
    List<DataPacket> videoPackets;
    List<PacketType> videoPacketTypes;

    //------------------------------------
    // encode

    bufferedTimes << frameInfo.frameTimestamp;

    VideoSegment curSegment;
    bool bProcessedFrame, bSendFrame = false;
    VOID *picIn;

    //profileIn("call to encoder");

    if (bShutdownEncodeThread)
        picIn = NULL;
    else
        picIn = frameInfo.pic->picOut ? (LPVOID)frameInfo.pic->picOut : (LPVOID)frameInfo.pic->mfxOut;

    DWORD out_pts = 0;
    videoEncoder->Encode(picIn, videoPackets, videoPacketTypes, bufferedTimes[0], out_pts);

    bProcessedFrame = (videoPackets.Num() != 0);

    //buffer video data before sending out
    if(bProcessedFrame)
    {
        bSendFrame = BufferVideoData(videoPackets, videoPacketTypes, bufferedTimes[0], out_pts, frameInfo.firstFrameTime, curSegment);
        bufferedTimes.Remove(0);
    }
    else
        nop();

    //profileOut;

    //------------------------------------
    // upload
    //send headers before the first frame if not yet sent
    if(bSendFrame)
        SendFrame(curSegment, frameInfo.firstFrameTime);

    return bProcessedFrame;
}


bool STDCALL SleepToNS(QWORD qwNSTime)
{
    QWORD t = GetQPCTimeNS();

    if (t >= qwNSTime)
        return false;

    unsigned int milliseconds = (unsigned int)((qwNSTime - t)/1000000);
    if (milliseconds > 1) //also accounts for windows 8 sleep problem
    {
        //trap suspicious sleeps that should never happen
        if (milliseconds > 10000)
        {
            Log(TEXT("Tried to sleep for %u seconds, that can't be right! Triggering breakpoint."), milliseconds);
            DebugBreak();
        }
        OSSleep(milliseconds);
    }

    for (;;)
    {
        t = GetQPCTimeNS();
        if (t >= qwNSTime)
            return true;
        Sleep(1);
    }
}

bool STDCALL SleepTo100NS(QWORD qw100NSTime)
{
    QWORD t = GetQPCTime100NS();

    if (t >= qw100NSTime)
        return false;

    unsigned int milliseconds = (unsigned int)((qw100NSTime - t)/10000);
    if (milliseconds > 1) //also accounts for windows 8 sleep problem
        OSSleep(milliseconds);

    for (;;)
    {
        t = GetQPCTime100NS();
        if (t >= qw100NSTime)
            return true;
        Sleep(1);
    }
}

UINT OBS::FlushBufferedVideo()
{
    UINT framesFlushed = 0;

    if (bufferedVideo.Num())
    {
        QWORD startTime = GetQPCTimeMS();
        DWORD baseTimestamp = bufferedVideo[0].timestamp;
        DWORD lastTimestamp = bufferedVideo.Last().timestamp;

        Log(TEXT("FlushBufferedVideo: Flushing %d packets over %d ms"), bufferedVideo.Num(), (lastTimestamp - baseTimestamp));

        for (UINT i = 0; i<bufferedVideo.Num(); i++)
        {
            //we measure our own time rather than sleep between frames due to potential sleep drift
            QWORD curTime;

            curTime = GetQPCTimeMS();
            while (curTime - startTime < bufferedVideo[i].timestamp - baseTimestamp)
            {
                OSSleep(1);
                curTime = GetQPCTimeMS();
            }

            SendFrame(bufferedVideo[i], firstFrameTimestamp);
            bufferedVideo[i].Clear();

            framesFlushed++;
        }

        bufferedVideo.Clear();
    }

    return framesFlushed;
}

void OBS::EncodeLoop()
{
    QWORD streamTimeStart = GetQPCTimeNS();
    QWORD frameTimeNS = 1000000000/fps;
    bool bufferedFrames = true; //to avoid constantly polling number of frames
    int numTotalDuplicatedFrames = 0, numTotalFrames = 0, numFramesSkipped = 0;

    bufferedTimes.Clear();

    bool bUsingQSV = videoEncoder->isQSV();//GlobalConfig->GetInt(TEXT("Video Encoding"), TEXT("UseQSV")) != 0;

    QWORD sleepTargetTime = streamTimeStart+frameTimeNS;
    latestVideoTime = firstSceneTimestamp = streamTimeStart/1000000;
    latestVideoTimeNS = streamTimeStart;

    firstFrameTimestamp = 0;

    UINT encoderInfo = 0;
    QWORD messageTime = 0;

    EncoderPicture *lastPic = NULL;

    UINT skipThreshold = encoderSkipThreshold*2;
    UINT no_sleep_counter = 0;

    CircularList<QWORD> bufferedTimes;

    while (!bShutdownEncodeThread || (bufferedFrames && (1 || bUsingQSV))) {
        if (!SleepToNS(sleepTargetTime += (frameTimeNS/2)))
            no_sleep_counter++;
        else
            no_sleep_counter = 0;

        latestVideoTime = sleepTargetTime/1000000;
        latestVideoTimeNS = sleepTargetTime;

        if (no_sleep_counter < skipThreshold) {
            SetEvent(hVideoEvent);
            if (encoderInfo) {
                if (messageTime == 0) {
                    messageTime = latestVideoTime+3000;
                } else if (latestVideoTime >= messageTime) {
                    RemoveStreamInfo(encoderInfo);
                    encoderInfo = 0;
                    messageTime = 0;
                }
            }
        } else {
            numFramesSkipped++;
            if (!encoderInfo)
                encoderInfo = AddStreamInfo(Str("EncoderLag"), StreamInfoPriority_Critical);
            messageTime = 0;
        }

        if (!SleepToNS(sleepTargetTime += (frameTimeNS/2)))
            no_sleep_counter++;
        else
            no_sleep_counter = 0;
        bufferedTimes << latestVideoTime;

        if (curFramePic && firstFrameTimestamp) {
            while (bufferedTimes[0] < firstFrameTimestamp)
                bufferedTimes.Remove(0);

            DWORD curFrameTimestamp = DWORD(bufferedTimes[0] - firstFrameTimestamp);
            bufferedTimes.Remove(0);

            FrameProcessInfo frameInfo;
            frameInfo.firstFrameTime = firstFrameTimestamp;
            frameInfo.frameTimestamp = curFrameTimestamp;
            frameInfo.pic = curFramePic;

            if (lastPic == frameInfo.pic)
                numTotalDuplicatedFrames++;

            if(bUsingQSV)
                curFramePic->mfxOut->Data.TimeStamp = curFrameTimestamp;
            else
                curFramePic->picOut->i_pts = curFrameTimestamp;

            ProcessFrame(frameInfo);

            lastPic = frameInfo.pic;

            numTotalFrames++;
        }

        if (bShutdownEncodeThread)
            bufferedFrames = videoEncoder->HasBufferedFrames();
    }

    //flush all video frames in the "scene buffering time" buffer
    if (firstFrameTimestamp)
        numTotalFrames += FlushBufferedVideo();

    Log(TEXT("Total frames encoded: %d, total frames duplicated: %d (%0.2f%%)"), numTotalFrames, numTotalDuplicatedFrames, (numTotalFrames > 0) ? (double(numTotalDuplicatedFrames)/double(numTotalFrames))*100.0 : 0.0f);
    if (numFramesSkipped)
        Log(TEXT("Number of frames skipped due to encoder lag: %d (%0.2f%%)"), numFramesSkipped, (numTotalFrames > 0) ? (double(numFramesSkipped)/double(numTotalFrames))*100.0 : 0.0f);

    SetEvent(hVideoEvent);
    bShutdownVideoThread = true;
}

//todo: this function is an abomination, this is just disgusting.  fix it.
//...seriously, this is really, really horrible.  I mean this is amazingly bad.
void OBS::MainCaptureLoop()
{
    int curRenderTarget = 0, curYUVTexture = 0, curCopyTexture = 0;
    int copyWait = NUM_RENDER_BUFFERS-1;

    bSentHeaders = false;
    bFirstAudioPacket = true;
    //----------------------------------------
    // x264 input buffers

    int curOutBuffer = 0;

    bool bUsingQSV = videoEncoder->isQSV();
    bUsing444 = false;

    EncoderPicture lastPic;
    EncoderPicture outPics[NUM_OUT_BUFFERS];

    for(int i=0; i<NUM_OUT_BUFFERS; i++)
    {
        if(bUsingQSV)
        {
            outPics[i].mfxOut = new mfxFrameSurface1;
            memset(outPics[i].mfxOut, 0, sizeof(mfxFrameSurface1));
            mfxFrameData& data = outPics[i].mfxOut->Data;
            videoEncoder->RequestBuffers(&data);
        }
        else
        {
            outPics[i].picOut = new x264_picture_t;
            x264_picture_init(outPics[i].picOut);
        }
    }
	    
	if (!bUsingQSV)
		for (int i = 0; i < NUM_OUT_BUFFERS; i++)
			x264_picture_alloc(outPics[i].picOut, X264_CSP_NV12, outputCX, outputCY);    

	int bCongestionControl = 0;// AppConfig->GetInt(TEXT("Video Encoding"), TEXT("CongestionControl"), 0);
	bool bDynamicBitrateSupported = true;// App->GetVideoEncoder()->DynamicBitrateSupported();
	int defaultBitRate = 1000;// AppConfig->GetInt(TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int currentBitRate = defaultBitRate;
    QWORD lastAdjustmentTime = 0;
    UINT adjustmentStreamId = 0;

    //----------------------------------------
    // time/timestamp stuff
    totalStreamTime = 0;
    lastAudioTimestamp = 0;

    //----------------------------------------
    // start audio capture streams

    desktopAudio->StartCapture();
	if (micAudio){
		micAudio->StartCapture();
	}

    //----------------------------------------
    // status bar/statistics stuff

    DWORD fpsCounter = 0;

    int numLongFrames = 0;
    int numTotalFrames = 0;

    bytesPerSec = 0;
    captureFPS = 0;
    curFramesDropped = 0;
    curStrain = 0.0;
    //PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);

    QWORD lastBytesSent[3] = {0, 0, 0};
    DWORD lastFramesDropped = 0;
    double bpsTime = 0.0;

    double lastStrain = 0.0f;
    DWORD numSecondsWaited = 0;

    //----------------------------------------
    // 444->420 thread data

    int numThreads = MAX(OSGetTotalCores()-2, 1);
    HANDLE *h420Threads = (HANDLE*)Allocate(sizeof(HANDLE)*numThreads);
    Convert444Data *convertInfo = (Convert444Data*)Allocate(sizeof(Convert444Data)*numThreads);

    zero(h420Threads, sizeof(HANDLE)*numThreads);
    zero(convertInfo, sizeof(Convert444Data)*numThreads);

    for(int i=0; i<numThreads; i++)
    {
        convertInfo[i].width  = outputCX;
        convertInfo[i].height = outputCY;
        convertInfo[i].hSignalConvert  = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertInfo[i].hSignalComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertInfo[i].bNV12 = bUsingQSV;
        convertInfo[i].numThreads = numThreads;

        if(i == 0)
            convertInfo[i].startY = 0;
        else
            convertInfo[i].startY = convertInfo[i-1].endY;

        if(i == (numThreads-1))
            convertInfo[i].endY = outputCY;
        else
            convertInfo[i].endY = ((outputCY/numThreads)*(i+1)) & 0xFFFFFFFE;
    }

    bool bEncode;
    bool bFirstFrame = true;
    bool bFirstImage = true;
    bool bFirstEncode = true;
	bool bUseThreaded420 = 0;// bUseMultithreadedOptimizations && (OSGetTotalCores() > 1) && !bUsing444;

    List<HANDLE> completeEvents;

    //----------------------------------------

    QWORD streamTimeStart  = GetQPCTimeNS();
    QWORD lastStreamTime   = 0;
    QWORD firstFrameTimeMS = streamTimeStart/1000000;
    QWORD frameLengthNS    = 1000000000/fps;

	UCHAR* myBuffer = (UCHAR*)malloc(outputCX * outputCY * 4);
	UCHAR color = 0;
    while(WaitForSingleObject(hVideoEvent, INFINITE) == WAIT_OBJECT_0)
    {
        if (bShutdownVideoThread)
            break;

        QWORD renderStartTime = GetQPCTimeNS();
        totalStreamTime = DWORD((renderStartTime-streamTimeStart)/1000000);
        QWORD renderStartTimeMS = renderStartTime/1000000;
        QWORD curStreamTime = latestVideoTimeNS;
        if (!lastStreamTime)
            lastStreamTime = curStreamTime-frameLengthNS;
        QWORD frameDelta = curStreamTime-lastStreamTime;
        double fSeconds = double(frameDelta)*0.000000001;

        bool bUpdateBPS = false;

        //------------------------------------

        if(bRequestKeyframe && keyframeWait > 0)
        {
            keyframeWait -= int(frameDelta);

            if(keyframeWait <= 0)
            {
                GetVideoEncoder()->RequestKeyframe();
                bRequestKeyframe = false;
            }
        }

        //------------------------------------

        OSEnterMutex(hSceneMutex);
        //------------------------------------

        QWORD curBytesSent = 0;
        
        if (network) {
            curBytesSent = network->GetCurrentSentBytes();
            curFramesDropped = network->NumDroppedFrames();
        } else if (numSecondsWaited) {
            //reset stats if the network disappears
            bytesPerSec = 0;
            bpsTime = 0;
            numSecondsWaited = 0;
            curBytesSent = 0;
            zero(lastBytesSent, sizeof(lastBytesSent));
        }

        bpsTime += fSeconds;
        if(bpsTime > 1.0f)
        {
            if(numSecondsWaited < 3)
                ++numSecondsWaited;

            //bytesPerSec = DWORD(curBytesSent - lastBytesSent);
            bytesPerSec = DWORD(curBytesSent - lastBytesSent[0]) / numSecondsWaited;

            if(bpsTime > 2.0)
                bpsTime = 0.0f;
            else
                bpsTime -= 1.0;

            if(numSecondsWaited == 3)
            {
                lastBytesSent[0] = lastBytesSent[1];
                lastBytesSent[1] = lastBytesSent[2];
                lastBytesSent[2] = curBytesSent;
            }
            else
                lastBytesSent[numSecondsWaited] = curBytesSent;

            captureFPS = fpsCounter;
            fpsCounter = 0;

            bUpdateBPS = true;
        }

        fpsCounter++;

        if(network) curStrain = network->GetPacketStrain();
        //------------------------------------
        // present/upload

        bEncode = true;

        if(copyWait)
        {
            copyWait--;
            bEncode = false;
        }
        else
        {
            //audio sometimes takes a bit to start -- do not start processing frames until audio has started capturing
            if(!bRecievedFirstAudioFrame)
            {
                static bool bWarnedAboutNoAudio = false;
                if (renderStartTimeMS-firstFrameTimeMS > 10000 && !bWarnedAboutNoAudio)
                {
                    bWarnedAboutNoAudio = true;
                    //AddStreamInfo (TEXT ("WARNING: OBS is not receiving audio frames. Please check your audio devices."), StreamInfoPriority_Critical); 
                }
                bEncode = false;
            }
            else if(bFirstFrame)
            {
                firstFrameTimestamp = lastStreamTime/1000000;
                bFirstFrame = false;
            }

            if(!bEncode)
            {
                if(curYUVTexture == (NUM_RENDER_BUFFERS-1))
                    curYUVTexture = 0;
                else
                    curYUVTexture++;
            }
        }

        lastStreamTime = curStreamTime;

        if(bEncode){
			if (bFirstImage){
				bFirstImage = false;
				memset(myBuffer, 128, outputCX * outputCY * 4);
			}               
            else{                
                int prevOutBuffer = (curOutBuffer == 0) ? NUM_OUT_BUFFERS-1 : curOutBuffer-1;
                int nextOutBuffer = (curOutBuffer == NUM_OUT_BUFFERS-1) ? 0 : curOutBuffer+1;

                EncoderPicture &prevPicOut = outPics[prevOutBuffer];
                EncoderPicture &picOut = outPics[curOutBuffer];
                EncoderPicture &nextPicOut = outPics[nextOutBuffer];

                if(!bUsing444)
                {
                    {
						color += 10;
						if (color > 256){
							color = 0;
						}
						memset(myBuffer, color, outputCX * outputCY * 4);

                        if(bUsingQSV)
                        {
                            mfxFrameData& data = picOut.mfxOut->Data;
                            videoEncoder->RequestBuffers(&data);
                            LPBYTE output[] = {data.Y, data.UV};
							Convert444toNV12((LPBYTE)myBuffer, outputCX, outputCX * 4, data.Pitch, outputCY, 0, outputCY, output);
                        }
						else{
							Convert444toNV12((LPBYTE)myBuffer, outputCX, outputCX * 4, outputCX, outputCY, 0, outputCY, picOut.picOut->img.plane);
						}
                    }
                }

                if(bEncode){
                    InterlockedExchangePointer((volatile PVOID*)&curFramePic, &picOut);
                }

                curOutBuffer = nextOutBuffer;                
            }

            if(curCopyTexture == (NUM_RENDER_BUFFERS-1))
                curCopyTexture = 0;
            else
                curCopyTexture++;

            if(curYUVTexture == (NUM_RENDER_BUFFERS-1))
                curYUVTexture = 0;
            else
                curYUVTexture++;

            /*if (bCongestionControl && bDynamicBitrateSupported && totalStreamTime > 15000)
            {
                if (curStrain > 25)
                {
                    if (renderStartTimeMS - lastAdjustmentTime > 1500)
                    {
                        if (currentBitRate > 100)
                        {
                            currentBitRate = (int)(currentBitRate * (1.0 - (curStrain / 400)));
                            App->GetVideoEncoder()->SetBitRate(currentBitRate, -1);
                            if (!adjustmentStreamId)
                                adjustmentStreamId = App->AddStreamInfo (FormattedString(TEXT("Congestion detected, dropping bitrate to %d kbps"), currentBitRate).Array(), StreamInfoPriority_Low);
                            else
                                App->SetStreamInfo(adjustmentStreamId, FormattedString(TEXT("Congestion detected, dropping bitrate to %d kbps"), currentBitRate).Array());

                            bUpdateBPS = true;
                        }

                        lastAdjustmentTime = renderStartTimeMS;
                    }
                }
                else if (currentBitRate < defaultBitRate && curStrain < 5 && lastStrain < 5)
                {
                    if (renderStartTimeMS - lastAdjustmentTime > 5000)
                    {
                        if (currentBitRate < defaultBitRate)
                        {
                            currentBitRate += (int)(defaultBitRate * 0.05);
                            if (currentBitRate > defaultBitRate)
                                currentBitRate = defaultBitRate;
                        }

                        App->GetVideoEncoder()->SetBitRate(currentBitRate, -1);               
                        App->RemoveStreamInfo(adjustmentStreamId);
                        adjustmentStreamId = 0;

                        bUpdateBPS = true;

                        lastAdjustmentTime = renderStartTimeMS;
                    }
                }
            }*/
        }

        lastRenderTarget = curRenderTarget;

        if(curRenderTarget == (NUM_RENDER_BUFFERS-1))
            curRenderTarget = 0;
        else
            curRenderTarget++;

        if(bUpdateBPS || !CloseDouble(curStrain, lastStrain) || curFramesDropped != lastFramesDropped)
        {
            //PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);
            lastStrain = curStrain;

            lastFramesDropped = curFramesDropped;
        }

        //------------------------------------
        // we're about to sleep so we should flush the d3d command queue
        numTotalFrames++;
    }

    if(!bUsing444)
    {
        if(bUsingQSV)
            for(int i = 0; i < NUM_OUT_BUFFERS; i++)
                delete outPics[i].mfxOut;
        else
            for(int i=0; i<NUM_OUT_BUFFERS; i++)
            {
                x264_picture_clean(outPics[i].picOut);
                delete outPics[i].picOut;
            }
    }

    Free(h420Threads);
    Free(convertInfo);

    Log(TEXT("Total frames rendered: %d, number of late frames: %d (%0.2f%%) (it's okay for some frames to be late)"), numTotalFrames, numLongFrames, (numTotalFrames > 0) ? (double(numLongFrames)/double(numTotalFrames))*100.0 : 0.0f);
}
