#ifndef H_OBSNVENC_NVENCENCODER__H
#define H_OBSNVENC_NVENCENCODER__H

#include "nvmain.h"
#include <queue>

struct NVENCEncoderSurface
{
    NV_ENC_INPUT_PTR inputSurface;
    int width;
    int height;

    bool locked;
    uint32_t useCount;
};

struct NVENCEncoderOutputSurface
{
    NV_ENC_OUTPUT_PTR outputSurface;
    int size;
    DWORD timestamp;
    uint64_t inputTimestamp;

    NVENCEncoderSurface *inSurf;

    bool busy;
};

class NVENCEncoder : public VideoEncoder
{
public:
    NVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);
    ~NVENCEncoder();

    bool Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts) override;
    void RequestBuffers(LPVOID buffers);

    int  GetBitRate() const;
    bool DynamicBitrateSupported() const;
    bool SetBitRate(DWORD maxBitrate, DWORD bufferSize);

    void GetHeaders(DataPacket &packet);
    void GetSEI(DataPacket &packet);

    void RequestKeyframe();
    String GetInfoString() const;

    bool isQSV();

    int GetBufferedFrames();
    bool HasBufferedFrames();


    bool isAlive() { return alive; }

private:
    bool populateEncodePresetGUIDs();
    bool checkPresetSupport(const GUID &preset);

    void init();
    void ProcessOutput(NVENCEncoderOutputSurface *surf, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD &out_pts);

private:
    bool alive;
    CUcontext cuContext;
    void *encoder;

    int fps;
    int width;
    int height;
    int quality;
    String preset;
    bool bUse444;
    ColorDescription colorDesc;
    int maxBitRate;
    int bufferSize;
    bool bUseCFR;
    bool bUseCBR;
    int keyint;

    int frameShift;

    int maxSurfaceCount;
    NVENCEncoderSurface *inputSurfaces;
    NVENCEncoderOutputSurface *outputSurfaces;

    std::queue<NVENCEncoderOutputSurface*> outputSurfaceQueue;
    std::queue<NVENCEncoderOutputSurface*> outputSurfaceQueueReady;

    NV_ENC_INITIALIZE_PARAMS initEncodeParams;
    NV_ENC_CONFIG encodeConfig;

    List<BYTE> encodeData, headerPacket, seiData;

    List<GUID> encodePresetGUIDs;

    uint32_t outBufferSize;
    uint8_t *pstart;

    HANDLE frameMutex;

    bool dontTouchConfig;
};

#endif
