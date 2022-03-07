#include "mfx/mfxvideo.h"
#if (MFX_VERSION_MAJOR == 1) && (MFX_VERSION_MINOR < 8)
#include "mfxlinux.h"
#endif

#include "common_utils.h"
#include "common_vaapi.h"

/* =====================================================
 * Linux implementation of OS-specific utility functions
 */

mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, MFXVideoSession* pSession, mfxHDL* vaHandle)
{
    mfxStatus sts = MFX_ERR_NONE;

    // Initialize Intel Media SDK Session
    sts = pSession->Init(impl, &ver);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Create VA display
    mfxHDL displayHandle = { 0 };
    sts = CreateVAEnvDRM(&displayHandle,vaHandle);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Provide VA display handle to Media SDK
    sts = pSession->SetHandle(static_cast < mfxHandleType >(MFX_HANDLE_VA_DISPLAY), displayHandle);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    return sts;
}

void Release(mfxHDL vaHandle)
{
    CleanupVAEnvDRM(vaHandle);
}

void mfxGetTime(mfxTime* timestamp)
{
    clock_gettime(CLOCK_REALTIME, timestamp);
}


double TimeDiffMsec(mfxTime tfinish, mfxTime tstart)
{
    double result;
    long long elapsed_nsec = tfinish.tv_nsec - tstart.tv_nsec;
    long long elapsed_sec = tfinish.tv_sec - tstart.tv_sec;

    //if (tstart.tv_sec==0) return -1;

    //timespec uses two fields -- check if borrowing necessary
    if (elapsed_nsec < 0) {
        elapsed_sec -= 1;
        elapsed_nsec += 1000000000;
    }
    //return total converted to milliseconds
    result = (double)elapsed_sec *1000.0;
    result += (double)elapsed_nsec / 1000000;

    return result;
}
