#include "nvmain.h"
#include "NVENCEncoder.h"
#include <Objbase.h>

bool doLog = true;
unsigned int encoderRefs = 0;

extern "C" bool __cdecl CheckNVENCHardwareSupport(bool log)
{
    doLog = log;

    if (!checkNvEnc())
        return false;
    
    return true;
}

extern "C" VideoEncoder* __cdecl CreateNVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
{
    if (!initNvEnc())
        return 0;

    NVENCEncoder *res = new NVENCEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);

    if (!res->isAlive())
    {
        delete res;
        return 0;
    }

    encoderRefs += 1;

    return res;
}

void encoderRefDec()
{
    assert(encoderRefs);

    encoderRefs -= 1;

    if (encoderRefs == 0)
        deinitNvEnc();
}

String guidToString(const GUID &guid)
{
    LPOLESTR resStr;
    String res;

    if (StringFromCLSID(guid, &resStr) == S_OK)
    {
        res = resStr;
        CoTaskMemFree(resStr);
    }

    return res;
}

bool stringToGuid(const String &string, GUID *guid)
{
    return CLSIDFromString(string.Array(), guid) == NOERROR;
}
