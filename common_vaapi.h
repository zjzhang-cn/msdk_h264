#pragma once

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <new>
#include <stdlib.h>
#include <assert.h>

#include "mfx/mfxvideo++.h"

#include "va/va.h"
#include "va/va_drm.h"

// =================================================================
// VAAPI functionality required to manage VA surfaces
mfxStatus CreateVAEnvDRM(mfxHDL* displayHandle,mfxHDL* vaHandle);
void CleanupVAEnvDRM(mfxHDL vaHandle);

// utility
mfxStatus va_to_mfx_status(VAStatus va_res);
