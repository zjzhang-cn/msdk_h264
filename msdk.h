#pragma once

#ifdef __cplusplus
extern "C" {
#endif
typedef void * EncHandle;
uint32_t QueryHardware();

uint32_t InitEncoder(
    uint16_t Width,
    uint16_t Height,
    uint16_t QP,
    uint16_t FrameRate,
    EncHandle *context);
uint8_t* EncodeFrame(EncHandle context, uint8_t* y, uint8_t* u, uint8_t* v, int32_t* encodedSize, int32_t* frameType, uint64_t* timeStamp, uint8_t ForceIDR);
uint32_t DestoryEncoder(EncHandle);
#ifdef __cplusplus
}
#endif