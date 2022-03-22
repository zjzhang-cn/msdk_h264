//go:build dragonfly || freebsd || linux || netbsd || openbsd || solaris
// +build dragonfly freebsd linux netbsd openbsd solaris

package msdk_h264

// #cgo pkg-config: libmfx libva-drm
// #cgo CFLAGS: -I ./
// #include <stdio.h>
// #include <stdint.h>
// #include "msdk.h"
import "C"
import (
	"errors"
	"image"
	"io"
	"sync"
	"unsafe"

	"github.com/pion/mediadevices/pkg/codec"
	"github.com/pion/mediadevices/pkg/io/video"
	"github.com/pion/mediadevices/pkg/prop"
)

type encoder struct {
	context  C.EncHandle
	r        video.Reader
	mu       sync.Mutex
	closed   bool
	forceIDR int
}

func newEncoder(r video.Reader, p prop.Media, params Params) (codec.ReadCloser, error) {
	var context C.EncHandle
	C.InitEncoder(C.uint16_t(p.Width), C.uint16_t(p.Height), C.uint16_t(params.BitRate/1000), 120, &context)
	if context == C.EncHandle(nil) {
		return nil, errors.New("failed to create msdk context")
	}

	e := &encoder{
		context:  context,
		r:        video.ToI420(r),
		closed:   false,
		forceIDR: 0,
	}
	return e, nil
}

func (e *encoder) Read() ([]byte, func(), error) {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.closed {
		return nil, func() {}, io.EOF
	}

	img, _, err := e.r.Read()
	if err != nil {
		return nil, func() {}, err
	}
	yuvImg := img.(*image.YCbCr)
	var frameSize C.int
	var frameType C.int
	var timeStamp C.uint64_t

	s := C.EncodeFrame(e.context,
		(*C.uchar)(&yuvImg.Y[0]),
		(*C.uchar)(&yuvImg.Cb[0]),
		(*C.uchar)(&yuvImg.Cr[0]),
		&frameSize, &frameType, &timeStamp, C.uint8_t(e.forceIDR))
	e.forceIDR = 0
	encoded := C.GoBytes(unsafe.Pointer(s), frameSize)
	return encoded, func() {}, err
}

func (e *encoder) SetBitRate(b int) error {
	panic("SetBitRate is not implemented")
}

func (e *encoder) ForceKeyFrame() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.forceIDR = 1
	return nil
}

func (e *encoder) Close() error {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.closed {
		return nil
	}

	C.DestoryEncoder(e.context)
	e.closed = true
	return nil
}
