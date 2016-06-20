/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#if !defined(_COMPV_MATH_STATS_INTRIN_AVX2_H_)
#define _COMPV_MATH_STATS_INTRIN_AVX2_H_

#include "compv/compv_config.h"
#include "compv/compv_common.h"

#if defined(_COMPV_API_H_)
#error("This is a private file and must not be part of the API")
#endif

#if COMPV_ARCH_X86 && COMPV_INTRINSIC

COMPV_NAMESPACE_BEGIN()

void MathStatsNormalize2DHartley_float64_Intrin_AVX(const COMPV_ALIGNED(AVX) compv_float64_t* x, const COMPV_ALIGNED(AVX) compv_float64_t* y, compv_uscalar_t numPoints, compv_float64_t* tx1, compv_float64_t* ty1, compv_float64_t* s1);
void MathStatsNormalize2DHartley_4_float64_Intrin_AVX(const COMPV_ALIGNED(AVX) compv_float64_t* x, const COMPV_ALIGNED(AVX) compv_float64_t* y, compv_uscalar_t numPoints, compv_float64_t* tx1, compv_float64_t* ty1, compv_float64_t* s1);

COMPV_NAMESPACE_END()

#endif /* COMPV_ARCH_X86 && COMPV_INTRINSIC */

#endif /* _COMPV_MATH_STATS_INTRIN_AVX2_H_ */
