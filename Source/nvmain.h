#ifndef H_OBSNVENC_MAIN__H
#define H_OBSNVENC_MAIN__H

#pragma warning(disable: 4005)

#include "Main.h"
#include "cudaDynload.h"
#include "nvEncodeAPI.h"

extern bool doLog;
#define NvLog(...) if(doLog) Log(__VA_ARGS__)

typedef NVENCSTATUS(NVENCAPI* PNVENCODEAPICREATEINSTANCE)(NV_ENCODE_API_FUNCTION_LIST *functionList);
extern NV_ENCODE_API_FUNCTION_LIST *pNvEnc;

extern int iNvencDeviceCount;
extern CUdevice pNvencDevices[16];
extern unsigned int iNvencUseDeviceID;

bool _checkCudaErrors(CUresult err, const char *func);
#define checkCudaErrors(f) 
//if(!_checkCudaErrors(f, #f)) goto error

bool checkNvEnc();
bool initNvEnc();
void deinitNvEnc();

extern unsigned int encoderRefs;
void encoderRefDec();

struct OSMutexLocker
{
    HANDLE& h;
    bool enabled;

    OSMutexLocker(HANDLE &h) : h(h), enabled(true) { OSEnterMutex(h); }
    ~OSMutexLocker() { if (enabled) OSLeaveMutex(h); }
    OSMutexLocker(OSMutexLocker &&other) : h(other.h), enabled(other.enabled) { other.enabled = false; }
};

template<typename T>
inline bool dataEqual(const T& a, const T& b)
{
    return memcmp(&a, &b, sizeof(T)) == 0;
}

String guidToString(const GUID &guid);
bool stringToGuid(const String &string, GUID *guid);

extern "C" bool CheckNVENCHardwareSupport(bool log);
extern "C" VideoEncoder* CreateNVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);

#endif
