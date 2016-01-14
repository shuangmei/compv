/* Copyright (C) 2016 Doubango Telecom <https://www.doubango.org>
*
* This file is part of Open Source ComputerVision (a.k.a CompV) project.
* Source code hosted at https://github.com/DoubangoTelecom/compv
* Website hosted at http://compv.org
*
* CompV is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CompV is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CompV.
*/
#include "compv/image/compv_imageconv_common.h"
#include "compv/parallel/compv_asynctask.h"

COMPV_NAMESPACE_BEGIN()

COMPV_ERROR_CODE ImageConvKernelxx_AsynExec(const struct compv_asynctoken_param_xs* pc_params)
{
    const int funcId = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[0].pcParamPtr, int);
    switch (funcId) {
    case COMPV_IMAGECONV_FUNCID_RGBAToI420_Y: {
        rgbaToI420Kernel_CompY CompY = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[1].pcParamPtr, rgbaToI420Kernel_CompY);
        const uint8_t* rgbaPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[2].pcParamPtr, const uint8_t*);
        uint8_t* outYPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[3].pcParamPtr, uint8_t*);
        vcomp_scalar_t height = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[4].pcParamPtr, int);
        vcomp_scalar_t width = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[5].pcParamPtr, int);
        vcomp_scalar_t stride = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[6].pcParamPtr, int);
        CompY(rgbaPtr, outYPtr, height, width, stride);
        break;
    }
    case COMPV_IMAGECONV_FUNCID_RGBAToI420_UV: {
        rgbaToI420Kernel_CompUV CompUV = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[1].pcParamPtr, rgbaToI420Kernel_CompUV);
        const uint8_t* rgbaPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[2].pcParamPtr, const uint8_t*);
        uint8_t* outUPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[3].pcParamPtr, uint8_t*);
        uint8_t* outVPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[4].pcParamPtr, uint8_t*);
        vcomp_scalar_t height = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[5].pcParamPtr, int);
        vcomp_scalar_t width = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[6].pcParamPtr, int);
        vcomp_scalar_t stride = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[7].pcParamPtr, int);
        CompUV(rgbaPtr, outUPtr, outVPtr, height, width, stride);
        break;
    }
    case COMPV_IMAGECONV_FUNCID_I420ToRGBA: {
        i420ToRGBAKernel toRGBA = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[1].pcParamPtr, i420ToRGBAKernel);
        const uint8_t* yPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[2].pcParamPtr, const uint8_t*);
        const uint8_t* uPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[3].pcParamPtr, const uint8_t*);
        const uint8_t* vPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[4].pcParamPtr, const uint8_t*);
        uint8_t* outRgbaPtr = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[5].pcParamPtr, uint8_t*);
        vcomp_scalar_t height = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[6].pcParamPtr, int);
        vcomp_scalar_t width = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[7].pcParamPtr, int);
        vcomp_scalar_t stride = COMPV_ASYNCTASK_GET_PARAM_ASIS(pc_params[8].pcParamPtr, int);
        toRGBA(yPtr, uPtr, vPtr, outRgbaPtr, height, width, stride);
        break;
    }
    default:
        COMPV_DEBUG_ERROR("%d is an invalid funcId", funcId);
        return COMPV_ERROR_CODE_E_INVALID_CALL;
    }
    return COMPV_ERROR_CODE_S_OK;
}

COMPV_NAMESPACE_END()


/* RGB to YUV conversion : http ://www.fourcc.org/fccyvrgb.php
Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16
Cr = V = (0.439 * R) - (0.368 * G) - (0.071 * B) + 128
Cb = U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128
and we know that :
Y = (Y * 256) / 256
V = (V * 256) / 256
U = (U * 256) / 256
= >
Y = ((65.792 * R) + (129.024 * G) + (25.088 * B) + 4096) >> 8
V = ((112.384 * R) + (-94.208 * G) + (-18.176 * B) + 32768) >> 8
U = ((-37.888 * R) + (-74.496 * G) + (112.384 * B) + 32768) >> 8
Numerical approx. = >
Y = (((66 * R) + (129 * G) + (25 * B))) >> 8 + 16
= (2 * ((33 * R) + (65 * G) + (13 * B))) >> 8 + 16
= (((33 * R) + (65 * G) + (13 * B))) >> 7 + 16
V = (((112 * R) + (-94 * G) + (-18 * B))) >> 8 + 128
U = (((-38 * R) + (-74 * G) + (112 * B))) >> 8 + 128
*/
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kRGBAToYUV_YCoeffs8[] = {
    33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, // 128bits SSE register
    33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, // 256bits AVX register
};
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kRGBAToYUV_UCoeffs8[] = {
    -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, // 128bits SSE register
    -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, // 256bits AVX register
};
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kRGBAToYUV_VCoeffs8[] = {
    112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, // 128bits SSE register
    112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, // 256bits AVX register
};
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kRGBAToYUV_UVCoeffs8[] = { // U and V interleaved: Each appear #1 time: UVUVUVUV....
    -38, -74, 112, 0, 112, -94, -18, 0, -38, -74, 112, 0, 112, -94, -18, 0,
    -38, -74, 112, 0, 112, -94, -18, 0, -38, -74, 112, 0, 112, -94, -18, 0,
};
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kRGBAToYUV_U2V2Coeffs8[] = { // U and V interleaved: Each appear #2 times: UUVVUUVVUUVV....
    -38, -74, 112, 0, -38, -74, 112, 0, 112, -94, -18, 0, 112, -94, -18, 0,
    -38, -74, 112, 0, -38, -74, 112, 0, 112, -94, -18, 0, 112, -94, -18, 0,
};

COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kRGBAToYUV_U4V4Coeffs8[] = { // AVX-only: U and V interleaved: Each appear #4 times: UUUUVVVVUUUUVVVV.....
    -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0,
    112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0,
};


/* YUV to RGB conversion : http ://www.fourcc.org/fccyvrgb.php
R = 1.164(Y - 16) + 1.596(V - 128)
G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128)
B = 1.164(Y - 16) + 2.018(U - 128)
and we know that :
R = (R * 64) / 64
G = (G * 64) / 64
B = (B * 64) / 64
= >
R * 64 = 74(Y - 16) + 102(V - 128)
G * 64 = 74(Y - 16) - 52(V - 128) - 25(U - 128)
B * 64 = 74(Y - 16) + 129(U - 128)
= >
R * 64 = 74Y' + 0U' + 102V'
G * 64 = 74Y' - 25U' - 52V'
B * 64 = 74Y' + 129U' + 0V'
where Y'=(Y - 16), U' = (U - 128), V'=(V - 128)
= >
R * 32 = 37Y' + 0U' + 51V'
G * 32 = 37Y' - 13U' - 26V'
B * 32 = 37Y' + 65U' + 0V'
= >
R = (37Y' + 0U' + 51V') >> 5
G = (37Y' - 13U' - 26V') >> 5
B = (37Y' + 65U' + 0V') >> 5
*/
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kYUVToRGBA_RCoeffs8[] = { // Extended with a zero to have #4 coeffs: k0, k1, k2, 0
    37, 0, 51, 0, 37, 0, 51, 0, 37, 0, 51, 0, 37, 0, 51, 0, // 128bits SSE register
    37, 0, 51, 0, 37, 0, 51, 0, 37, 0, 51, 0, 37, 0, 51, 0, // 256bits AVX register
};
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kYUVToRGBA_GCoeffs8[] = { // Extended with a zero to have #4 coeffs: k0, k1, k2, 0
    37, -13, -26, 0, 37, -13, -26, 0, 37, -13, -26, 0, 37, -13, -26, 0, // 128bits SSE register
    37, -13, -26, 0, 37, -13, -26, 0, 37, -13, -26, 0, 37, -13, -26, 0, // 256bits AVX register
};
COMPV_GEXTERN COMV_ALIGN_DEFAULT() int8_t kYUVToRGBA_BCoeffs8[] = { // Extended with a zero to have #4 coeffs: k0, k1, k2, 0
    37, 65, 0, 0, 37, 65, 0, 0, 37, 65, 0, 0, 37, 65, 0, 0, // 128bits SSE register
    37, 65, 0, 0, 37, 65, 0, 0, 37, 65, 0, 0, 37, 65, 0, 0, // 256bits AVX register
};