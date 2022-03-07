#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <sys/ioctl.h>
#include <map>
#include <string>

#include "common_vaapi.h"

mfxStatus va_to_mfx_status(VAStatus va_res) {
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res) {
        case VA_STATUS_SUCCESS:
            mfxRes = MFX_ERR_NONE;
            break;
        case VA_STATUS_ERROR_ALLOCATION_FAILED:
            mfxRes = MFX_ERR_MEMORY_ALLOC;
            break;
        case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
        case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
        case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
        case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
        case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
        case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
        case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
            mfxRes = MFX_ERR_UNSUPPORTED;
            break;
        case VA_STATUS_ERROR_INVALID_DISPLAY:
        case VA_STATUS_ERROR_INVALID_CONFIG:
        case VA_STATUS_ERROR_INVALID_CONTEXT:
        case VA_STATUS_ERROR_INVALID_SURFACE:
        case VA_STATUS_ERROR_INVALID_BUFFER:
        case VA_STATUS_ERROR_INVALID_IMAGE:
        case VA_STATUS_ERROR_INVALID_SUBPICTURE:
            mfxRes = MFX_ERR_NOT_INITIALIZED;
            break;
        case VA_STATUS_ERROR_INVALID_PARAMETER:
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        default:
            mfxRes = MFX_ERR_UNKNOWN;
            break;
    }
    return mfxRes;
}

// global variables shared by the below functions

// // VAAPI display handle
// VADisplay m_va_dpy = NULL;
// // gfx card file descriptor
// int m_fd = -1;

constexpr uint32_t DRI_MAX_NODES_NUM = 16;
constexpr uint32_t DRI_RENDER_START_INDEX = 128;
constexpr uint32_t DRM_DRIVER_NAME_LEN = 4;
const char* DRM_INTEL_DRIVER_NAME = "i915";
const char* DRI_PATH = "/dev/dri/";
const char* DRI_NODE_RENDER = "renderD";

typedef struct _VAHandle {
    VADisplay m_va_dpy;
    int m_fd;
} VAHandle;

int get_drm_driver_name(int fd, char* name, int name_size) {
    drm_version_t version = {};
    version.name_len = name_size;
    version.name = name;
    return ioctl(fd, DRM_IOWR(0, drm_version), &version);
}

int open_intel_adapter() {
    std::string adapterPath = DRI_PATH;
    adapterPath += DRI_NODE_RENDER;

    char driverName[DRM_DRIVER_NAME_LEN + 1] = {};
    mfxU32 nodeIndex = DRI_RENDER_START_INDEX;

    for (mfxU32 i = 0; i < DRI_MAX_NODES_NUM; ++i) {
        std::string curAdapterPath = adapterPath + std::to_string(nodeIndex + i);

        int fd = open(curAdapterPath.c_str(), O_RDWR);
        if (fd < 0)
            continue;

        if (!get_drm_driver_name(fd, driverName, DRM_DRIVER_NAME_LEN) &&
            !strcmp(driverName, DRM_INTEL_DRIVER_NAME)) {
            return fd;
        }
        close(fd);
    }

    return -1;
}

mfxStatus CreateVAEnvDRM(mfxHDL* displayHandle, mfxHDL* vaHandle) {
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;
    int major_version = 0, minor_version = 0;
    VADisplay m_va_dpy=NULL;
    int m_fd = open_intel_adapter();

    if (m_fd < 0)
        sts = MFX_ERR_NOT_INITIALIZED;

    if (MFX_ERR_NONE == sts) {
        m_va_dpy = vaGetDisplayDRM(m_fd);

        *displayHandle = m_va_dpy;

        if (!m_va_dpy) {
            close(m_fd);
            sts = MFX_ERR_NULL_PTR;
        }
    }
    if (MFX_ERR_NONE == sts) {
        va_res = vaInitialize(m_va_dpy, &major_version, &minor_version);
        sts = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != sts) {
            close(m_fd);
            m_fd = -1;
        }
    }
    if (MFX_ERR_NONE != sts)
        throw std::bad_alloc();

    VAHandle* handle = new VAHandle();
    handle->m_fd=m_fd;
    handle->m_va_dpy=m_va_dpy;
    *vaHandle=(mfxHDL)handle;
    return MFX_ERR_NONE;
}

void CleanupVAEnvDRM(mfxHDL vaHandle) {
    VAHandle* handle = (VAHandle*)vaHandle;
    if (handle->m_va_dpy) {
        vaTerminate(handle->m_va_dpy);
    }
    if (handle->m_fd >= 0) {
        close(handle->m_fd);
    }
    delete handle;
}

// utiility function to convert MFX fourcc to VA format
unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc) {
    switch (fourcc) {
        case MFX_FOURCC_NV12:
            return VA_FOURCC_NV12;
        case MFX_FOURCC_YUY2:
            return VA_FOURCC_YUY2;
        case MFX_FOURCC_YV12:
            return VA_FOURCC_YV12;
        case MFX_FOURCC_RGB4:
            return VA_FOURCC_ARGB;
        case MFX_FOURCC_P8:
            return VA_FOURCC_P208;

        default:
            assert(!"unsupported fourcc");
            return 0;
    }
}
