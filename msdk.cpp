#include <stdint.h>
#include <stdio.h>
#include "common_utils.h"

extern "C"
{
#include "msdk.h"
}
typedef struct
{
    mfxHDL vaHandle;
    MFXVideoSession *session;
    std::unique_ptr<MFXVideoENCODE> m_pmfx_enc_;
    mfxVideoParam mfxEncParams;
    std::vector<mfxExtBuffer *> m_enc_ext_params_;
    mfxBitstream *mfxBS;
    mfxU8 *surfaceBuffers;
    mfxU8 *bsBuffers;
    std::vector<mfxFrameSurface1> pEncSurfaces;
    mfxEncodeCtrl ctrl;
    mfxSyncPoint syncp;
#ifdef _BIN
    FILE *out;
#endif
} EncodeContext;
uint32_t DestoryEncoder(EncHandle context)
{
    EncodeContext *ctx = (EncodeContext *)context;
 #if 0
    mfxStatus sts = MFX_ERR_NONE;
    while (MFX_ERR_NONE <= sts) {
        for (;;) {
            // Encode a frame asychronously (returns immediately)
            sts = ctx->m_pmfx_enc_->EncodeFrameAsync(NULL, NULL, ctx->mfxBS, &(ctx->syncp));
            if (MFX_ERR_NONE < sts && !ctx->syncp) {  // Repeat the call if warning and no output
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
            } else if (MFX_ERR_NONE < sts && ctx->syncp) {
                sts = MFX_ERR_NONE;  // Ignore warnings if output is available
                break;
            } else
                break;
        }
        if (MFX_ERR_NONE == sts) {
            sts = ctx->session->SyncOperation(ctx->syncp, 60000);  // Synchronize. Wait until encoded frame is ready
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            //sts = WriteBitStreamFrame(&mfxBS, fSink.get());
            //MSDK_BREAK_ON_ERROR(sts);
        }
    }
#endif
    ctx->m_pmfx_enc_->Close();
    ctx->m_pmfx_enc_.reset();
    delete ctx->session;
    Release(ctx->vaHandle);
    free(ctx->surfaceBuffers);
    free(ctx->bsBuffers);
    delete ctx->mfxBS;
    delete ctx;
    return 0;
}

uint32_t QueryHardware()
{
    mfxIMPL impl = MFX_IMPL_AUTO;
    mfxVersion version = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
    mfxSession session;
    MFXInit(MFX_IMPL_AUTO, &version, &session);
    MFXQueryIMPL(session, &impl);
    MFXClose(session);
    return (impl & MFX_IMPL_HARDWARE) == MFX_IMPL_HARDWARE;
}

uint32_t InitEncoder(
    uint16_t Width,
    uint16_t Height,
    uint16_t QP,
    uint16_t FrameRate,
    EncHandle *context)
{
    // Initialize Intel Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
    // - Version 1.0 is selected for greatest backwards compatibility.
    // OS specific notes
    // - On Windows both SW and HW libraries may present
    // - On Linux only HW library only is available
    //   If more recent API features are needed, change the version accordingly
    mfxU32 pnFrameRateExtN;
    mfxU32 pnFrameRateExtD;
    ConvertFrameRate(FrameRate,&pnFrameRateExtN,&pnFrameRateExtD);

    mfxIMPL impl = MFX_IMPL_AUTO_ANY; // MFX_IMPL_HARDWARE
    mfxVersion ver = {{0, 1}};
    MFXVideoSession *session = new MFXVideoSession();
    mfxStatus sts = MFX_ERR_NONE;
    EncodeContext *ctx = new EncodeContext();
    memset(ctx, 0, sizeof(EncodeContext));
    sts = Initialize(impl, ver, session, &(ctx->vaHandle));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // //Create Media SDK encoder
    // MFXVideoENCODE *mfxENC = new MFXVideoENCODE(*session);
    ctx->m_pmfx_enc_.reset(new MFXVideoENCODE(*session));
    // mfxVideoParam *mfxEncParams = new mfxVideoParam();
    memset(&ctx->mfxEncParams, 0, sizeof(mfxVideoParam));

    ctx->mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;
    // ctx->mfxEncParams.mfx.CodecProfile = MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
    ctx->mfxEncParams.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;    
    ctx->mfxEncParams.mfx.CodecLevel = MFX_LEVEL_AVC_31;
    //ctx->mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
    ctx->mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
    ctx->mfxEncParams.mfx.GopPicSize = FrameRate * 5;
    // //Encoder参数设置：
    // //禁止B帧
    ctx->mfxEncParams.mfx.GopRefDist = 1;
    // 同步模式
    ctx->mfxEncParams.AsyncDepth = 2;
    //帧的引用计数
    ctx->mfxEncParams.mfx.NumRefFrame = 0;
    //每个视频帧中的切片数； 每个切片包含一个或多个小块行。 
    ctx->mfxEncParams.mfx.NumSlice = 0;
    // //IdrInterval指定了IDR帧的间隔，单位为I帧；若IdrInterval=0，则每个I帧均为IDR帧。若IdrInterval=1，则每隔一个I帧为IDR帧
    ctx->mfxEncParams.mfx.IdrInterval = 0;
    //ctx->mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    //ctx->mfxEncParams.mfx.TargetKbps = Bitrate;

    ctx->mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    //m_mfxEncParams.mfx.QPI, QPP, QPB 恒定QP（CQP）模式的I，P和B帧的量化参数（QP）
    ctx->mfxEncParams.mfx.QPI = QP % 52;
    ctx->mfxEncParams.mfx.QPP = QP % 52;
    ctx->mfxEncParams.mfx.FrameInfo.FrameRateExtN = pnFrameRateExtN;
    ctx->mfxEncParams.mfx.FrameInfo.FrameRateExtD = pnFrameRateExtD;
    ctx->mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    ctx->mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    ctx->mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    ctx->mfxEncParams.mfx.FrameInfo.CropX = 0;
    ctx->mfxEncParams.mfx.FrameInfo.CropY = 0;
    ctx->mfxEncParams.mfx.FrameInfo.CropW = Width;
    ctx->mfxEncParams.mfx.FrameInfo.CropH = Height;
    // Width must be a multiple of 16
    // Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    ctx->mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(Width);
    ctx->mfxEncParams.mfx.FrameInfo.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == ctx->mfxEncParams.mfx.FrameInfo.PicStruct) ? MSDK_ALIGN16(Height) : MSDK_ALIGN32(Height);

    ctx->mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    mfxExtCodingOption extendedCodingOptions;
    memset(&extendedCodingOptions, 0, sizeof(mfxExtCodingOption));
    extendedCodingOptions.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    extendedCodingOptions.Header.BufferSz = sizeof(extendedCodingOptions);
    extendedCodingOptions.AUDelimiter = MFX_CODINGOPTION_OFF;
    extendedCodingOptions.PicTimingSEI = MFX_CODINGOPTION_OFF;
    extendedCodingOptions.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
    mfxExtCodingOption2 extendedCodingOptions2;
    memset(&extendedCodingOptions2, 0, sizeof(mfxExtCodingOption));
    extendedCodingOptions2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    extendedCodingOptions2.Header.BufferSz = sizeof(extendedCodingOptions2);
    extendedCodingOptions2.RepeatPPS = MFX_CODINGOPTION_OFF;
    ctx->m_enc_ext_params_.push_back((mfxExtBuffer *)&extendedCodingOptions);
    ctx->m_enc_ext_params_.push_back((mfxExtBuffer *)&extendedCodingOptions2);

    if (!ctx->m_enc_ext_params_.empty())
    {
        ctx->mfxEncParams.ExtParam = &ctx->m_enc_ext_params_[0]; // vector is stored linearly in memory
        ctx->mfxEncParams.NumExtParam = (mfxU16)ctx->m_enc_ext_params_.size();
    }

    // Validate video encode parameters (optional)
    // - In this example the validation result is written to same structure
    // - MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is returned if some of the video parameters are not supported,
    //   instead the encoder will select suitable parameters closest matching the requested configuration
    sts = ctx->m_pmfx_enc_->Query(&ctx->mfxEncParams, &ctx->mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for encoder
    mfxFrameAllocRequest EncRequest;
    memset(&EncRequest, 0, sizeof(EncRequest));
    sts = ctx->m_pmfx_enc_->QueryIOSurf(&ctx->mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU16 nEncSurfNum = EncRequest.NumFrameSuggested;
    // Allocate surfaces for encoder
    // - Width and height of buffer must be aligned, a multiple of 32
    // - Frame surface array keeps pointers all surface planes and general frame info
    mfxU16 width = (mfxU16)MSDK_ALIGN32(EncRequest.Info.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(EncRequest.Info.Height);
    mfxU8 bitsPerPixel = 12; // NV12 format is a 12 bits per pixel format
    mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
    // std::vector<mfxU8> surfaceBuffersData(surfaceSize * nEncSurfNum);
    // mfxU8* surfaceBuffers = surfaceBuffersData.data();
    mfxU8 *surfaceBuffers = (mfxU8 *)malloc(surfaceSize * nEncSurfNum);
    ctx->surfaceBuffers = surfaceBuffers;
    // Allocate surface headers (mfxFrameSurface1) for encoder
    std::vector<mfxFrameSurface1> pEncSurfaces(nEncSurfNum);
    for (int i = 0; i < nEncSurfNum; i++)
    {
        memset(&pEncSurfaces[i], 0, sizeof(mfxFrameSurface1));
        pEncSurfaces[i].Info = ctx->mfxEncParams.mfx.FrameInfo;
        pEncSurfaces[i].Data.Y = &surfaceBuffers[surfaceSize * i];
        pEncSurfaces[i].Data.U = pEncSurfaces[i].Data.Y + width * height;
        pEncSurfaces[i].Data.V = pEncSurfaces[i].Data.U + 1;
        pEncSurfaces[i].Data.Pitch = width;
        ClearYUVSurfaceSysMem(&pEncSurfaces[i], width, height);
    }
    // Initialize the Media SDK encoder
    sts = ctx->m_pmfx_enc_->Init(&ctx->mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Retrieve video parameters selected by encoder.
    // - BufferSizeInKB parameter is required to set bit stream buffer size
    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    sts = ctx->m_pmfx_enc_->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Prepare Media SDK bit stream buffer

    mfxBitstream *mfxBS = new mfxBitstream();
    memset(mfxBS, 0, sizeof(mfxBS));
    mfxBS->MaxLength = par.mfx.BufferSizeInKB * 1000;
    // std::vector<mfxU8> bstData(mfxBS->MaxLength);
    // mfxBS.Data = bstData.data();
    ctx->bsBuffers = (mfxU8 *)malloc(mfxBS->MaxLength);
    mfxBS->Data = ctx->bsBuffers;
    ctx->pEncSurfaces = pEncSurfaces;
    ctx->session = session;
    ctx->mfxBS = mfxBS;
#ifdef _BIN
    ctx->out = OpenFile("out.h264", "wb");
    // ctx->input = OpenFile("input.yuv", "rb");
#endif
    *context = (EncHandle *)ctx;
    return 0;
}

uint8_t *EncodeFrame(EncHandle context, uint8_t *y, uint8_t *u, uint8_t *v, int32_t *encodedSize, int32_t *frameType, uint64_t *timeStamp, uint8_t forceIDR)
{
    mfxStatus sts = MFX_ERR_NONE;
    EncodeContext *ctx = (EncodeContext *)context;
    if (forceIDR)
    {
        ctx->ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
    }
    else
    {
        ctx->ctrl.FrameType = MFX_FRAMETYPE_UNKNOWN;
    }
    int nEncSurfIdx = GetFreeSurfaceIndex(ctx->pEncSurfaces); // Find free frame surface
    mfxFrameSurface1 *pSurface = &(ctx->pEncSurfaces[nEncSurfIdx]);
    //复制I420->NV12
    if (y)
    {
        mfxU16 w, h, i, pitch;
        mfxU8 *ptr;
        mfxFrameInfo *pInfo = &pSurface->Info;
        mfxFrameData *pData = &pSurface->Data;

        if (pInfo->CropH > 0 && pInfo->CropW > 0)
        {
            w = pInfo->CropW;
            h = pInfo->CropH;
        }
        else
        {
            w = pInfo->Width;
            h = pInfo->Height;
        }

        pitch = pData->Pitch;
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;
        for (i = 0; i < h; i++)
        {
            memcpy(ptr + i * pitch, y, w);
            y += w;
        }
        w /= 2;
        h /= 2;
        int skip = 0;
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;
        for (mfxU16 i = 0; i < h; i++)
        {
            for (mfxU16 j = 0; j < w; j++)
            {
                ptr[i * pitch + j * 2 + 0] = u[skip];
                ptr[i * pitch + j * 2 + 1] = v[skip];
                skip++;
            }
        }
    }
    for (;;)
    {
        // Encode a frame asychronously (returns immediately)
        sts = ctx->m_pmfx_enc_->EncodeFrameAsync(&(ctx->ctrl), pSurface, ctx->mfxBS, &(ctx->syncp));

        if (MFX_ERR_NONE < sts && !ctx->syncp)
        { // Repeat the call if warning and no output
            if (MFX_WRN_DEVICE_BUSY == sts)
                MSDK_SLEEP(1); // Wait if device is busy, then repeat the same call
        }
        else if (MFX_ERR_NONE < sts && ctx->syncp)
        {
            sts = MFX_ERR_NONE; // Ignore warnings if output is available
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            // Allocate more bitstream buffer memory here if needed...
            break;
        }
        else
            break;
        *encodedSize = sts;
    }
    if (MFX_ERR_NONE == sts)
    {
        sts = ctx->session->SyncOperation(ctx->syncp, 60000); // Synchronize. Wait until encoded frame is ready
        *encodedSize = ctx->mfxBS->DataLength;
        *frameType = ctx->mfxBS->FrameType;
#ifdef _BIN
        //printf("\n BS Length. %d\n", ctx->mfxBS->DataLength);
        sts = WriteBitStreamFrame(ctx->mfxBS, ctx->out);
#endif
        ctx->mfxBS->DataLength = 0;
        return ctx->mfxBS->Data + ctx->mfxBS->DataOffset;
    }
    return ctx->mfxBS->Data;
}
#ifdef _BIN
int main(int argc, char** argv) {
    void* ctx1 = 0;
    void* ctx2 = 0;
    void* ctx3 = 0;
    void* ctx4 = 0;
    void* ctx5 = 0;
    uint8_t* yuv = (uint8_t*)malloc(3840 * 1200 * 3 >> 1);
    uint8_t* y = yuv;
    uint8_t* u = yuv + 3840 * 1200;
    uint8_t* v = yuv + 3840 * 1200 + (3840 * 1200 >> 2);
    mfxTime tStart, tEnd;
    mfxGetTime(&tStart);
    InitEncoder(3840, 1200, 10000, 120, &ctx1);
    // InitEncoder(3840, 1200, 10000, 30, &ctx2);
    // InitEncoder(3840, 1200, 10000, 30, &ctx3);
    // InitEncoder(3840, 1200, 10000, 30, &ctx4);
    // InitEncoder(3840, 1200, 10000, 30, &ctx5);
    for (int i = 0; i < 1000; i++) {
        int32_t encodedsize = 0;
        int32_t frameType = 0;
        uint64_t timeStamp = 0;
        uint8_t* buf = 0;
        memset(y, 100, 3840 * 1200);
        memset(u, 10 + i % 50, 3840 * 100 >> 2);
        memset(v, 50 + i % 50, 3840 * 100 >> 2);
        buf = EncodeFrame(ctx1, y, u, v, &encodedsize,&frameType,&timeStamp, 0);
        //printf("\nLen: %d \t Type: %x \t TimeStamp: %d", encodedsize,frameType,timeStamp);
        //printf("\nBuf: %x %x %x %x %x %x %x %x %x ", buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[15]);
        // buf = EncodeFrame(ctx2, y, u, v, &encodedsize, 0);
        // buf = EncodeFrame(ctx3, y, u, v, &encodedsize, 0);
        // buf = EncodeFrame(ctx4, y, u, v, &encodedsize, 0);
        // buf = EncodeFrame(ctx5, y, u, v, &encodedsize, 0);
        // printf("BUF %x SIZE %d\n",buf,encodedsize);
        //  EncodeFrame(ctx2, 0, 0, 0);
        //  EncodeFrame(ctx3, 0, 0, 0);
        //  EncodeFrame(ctx4, 0, 0, 0);
        //  EncodeFrame(ctx5, 0, 0, 0);
    }
    DestoryEncoder(ctx1);
    // DestoryEncoder(ctx2);
    // DestoryEncoder(ctx3);
    // DestoryEncoder(ctx4);
    // DestoryEncoder(ctx5);
    mfxGetTime(&tEnd);
    double elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
    double fps = ((double)1000 / elapsed);
    printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);
    fflush(stdout);
}
#endif
