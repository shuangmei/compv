/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/core/features/fast/intrin/x86/compv_core_feature_fast_dete_intrin_sse2.h"

#if COMPV_ARCH_X86 && COMPV_INTRINSIC
#include "compv/base/intrin/x86/compv_intrin_sse.h"
#include "compv/base/compv_simd_globals.h"
#include "compv/base/compv_debug.h"

#include <algorithm>

COMPV_EXTERNC const COMPV_ALIGN_DEFAULT() uint8_t kCompVFast9Arcs[16][16];
COMPV_EXTERNC const COMPV_ALIGN_DEFAULT() uint8_t kCompVFast12Arcs[16][16];
COMPV_EXTERNC const COMPV_ALIGN_DEFAULT() uint16_t kCompVFast9Flags[16];
COMPV_EXTERNC const COMPV_ALIGN_DEFAULT() uint16_t kCompVFast12Flags[16];

COMPV_NAMESPACE_BEGIN()

// No need to check for 'width'. The caller ('CompVFastProcessRange' function) already checked and prepared it for SSE.
void CompVFastDataRow16_Intrin_SSE2(const uint8_t* IP, COMPV_ALIGNED(SSE) compv_scalar_t width, COMPV_ALIGNED(DEFAULT) const compv_scalar_t *pixels16, compv_scalar_t N, compv_scalar_t threshold, uint8_t* strengths)
{
	COMPV_DEBUG_INFO_CHECK_SSE2();
	compv_scalar_t i, j, k, arcStart;
	int maskB, maskD;
	compv_scalar_t NminusOne = N - 1;
	const compv_scalar_t minsum = (N == 12 ? 3 : 2);
	const __m128i vecThreshold = _mm_set1_epi8(static_cast<int8_t>(threshold));
	const __m128i vecNMinSumMinusOne = _mm_set1_epi8(static_cast<int8_t>(minsum - 1)); // no '_mm_cmpge_epu8'
	const __m128i vecNMinusOne = _mm_set1_epi8(static_cast<int8_t>(NminusOne)); // no '_mm_cmpge_epu8'
	const __m128i vecOne = _mm_load_si128(reinterpret_cast<const __m128i*>(k1_i8));
	const __m128i vecZero = _mm_setzero_si128();
	const __m128i vec0xFF = _mm_cmpeq_epi8(vecZero, vecZero); // 0xFF
	__m128i vec0, vec1, vecSumD1, vecSumB1, vecSD1, vecSB1, vecStrengths, vecBrighter1, vecDarker1, vecMask16[16], vecDarkers16[16], vecBrighters16[16];
	const uint8_t* circle[16] = { // FIXME: use same circle with C++ code
		&IP[pixels16[0]], &IP[pixels16[1]], &IP[pixels16[2]], &IP[pixels16[3]],
		&IP[pixels16[4]], &IP[pixels16[5]], &IP[pixels16[6]], &IP[pixels16[7]],
		&IP[pixels16[8]], &IP[pixels16[9]], &IP[pixels16[10]], &IP[pixels16[11]],
		&IP[pixels16[12]], &IP[pixels16[13]], &IP[pixels16[14]], &IP[pixels16[15]]
	};

#define _mm_cmpgtz_epu8(vec, mask) _mm_andnot_si128(_mm_cmpeq_epi8(vec, vecZero), mask) // no '_mm_cmpgt_epu8', mask should be '0xff'
#define _mm_fast_masks(a, b) \
	vec0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((circle[a] + i))); \
	vec1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((circle[b] + i))); \
	vecDarkers16[a] = _mm_subs_epu8(vecDarker1, vec0); \
	vecDarkers16[b] = _mm_subs_epu8(vecDarker1, vec1); \
	vecBrighters16[a] = _mm_subs_epu8(vec0, vecBrighter1); \
	vecBrighters16[b] = _mm_subs_epu8(vec1, vecBrighter1); \
	vecSD1 = _mm_add_epi8(_mm_cmpgtz_epu8(vecDarkers16[a], vecOne), _mm_cmpgtz_epu8(vecDarkers16[b], vecOne)); \
	vecSB1 = _mm_add_epi8(_mm_cmpgtz_epu8(vecBrighters16[a], vecOne), _mm_cmpgtz_epu8(vecBrighters16[b], vecOne))

	for (i = 0; i < width; i += 16, strengths += 16) {
		vec0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(IP + i));
		vecBrighter1 = _mm_adds_epu8(vec0, vecThreshold);
		vecDarker1 = _mm_subs_epu8(vec0, vecThreshold);

		/* reset strength to zero */
		_mm_storeu_si128(reinterpret_cast<__m128i*>(strengths), vecZero); // neon: must not increment strengths now, use later

		if (i == 1040) { // looking for 1050
			COMPV_DEBUG_INFO_CODE_FOR_TESTING("FIXME");
		}

		

		/***** Cross: 1, 9, 5, 13 *****/
		{
			// compare I1 and I9 aka 0 and 8
			_mm_fast_masks(0, 8);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = vecSD1; // initial -> equal
			vecSumB1 = vecSB1; // initial -> equal

			// compare I5 and I13 aka 4 and 12
			_mm_fast_masks(4, 12);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1);
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumD1, vecNMinSumMinusOne));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumB1, vecNMinSumMinusOne));
			if (!(maskD || maskB)) {
				continue;
			}
		}

		/***** Cross: 2, 10, 6, 14 *****/
		{
			// I2 and I10 aka 1 and 9
			_mm_fast_masks(1, 9);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1); // not initial -> add
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1); // not initial -> add

			// I6 and I14 aka 5 and 13
			_mm_fast_masks(5, 13);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1);
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumD1, vecNMinSumMinusOne));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumB1, vecNMinSumMinusOne));
			if (!(maskD || maskB)) {
				continue;
			}
		}

		/***** Cross: 3, 11, 7, 15 *****/
		{
			// I3 and I11 aka 2 and 10
			_mm_fast_masks(2, 10);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1);
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1);

			// I7 and I15 aka 6 and 14
			_mm_fast_masks(6, 14);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1);
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumD1, vecNMinSumMinusOne));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumB1, vecNMinSumMinusOne));
			if (!(maskD || maskB)) {
				continue;
			}
		}

		/***** Cross: 4, 12, 8, 16 *****/
		{
			// I4 and I12 aka 3 and 11
			_mm_fast_masks(3, 11);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1);
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1);

			// I8 and I16 aka 7 and 15
			_mm_fast_masks(7, 15);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSD1, vecZero));
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSB1, vecZero));
			if (!(maskD || maskB)) {
				continue;
			}
			vecSumD1 = _mm_add_epi8(vecSumD1, vecSD1);
			vecSumB1 = _mm_add_epi8(vecSumB1, vecSB1);
			maskD = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumD1, vecNMinusOne)); // last pair -> not 'vecNMinSumMinusOne' but 'vecNMinusOne'
			maskB = _mm_movemask_epi8(_mm_cmpgt_epi8(vecSumB1, vecNMinusOne)); // last pair -> not 'vecNMinSumMinusOne' but 'vecNMinusOne'
			if (!(maskD || maskB)) {
				continue;
			}
		}

		// Ad this step we are sure we have more that N non-zero darkers or Brighters (but don't know if there're continuous arcs)

		vecStrengths = _mm_setzero_si128();

		if (i == 1040) { // looking for 1050
			COMPV_DEBUG_INFO_CODE_FOR_TESTING("FIXME");
		}

		if (maskD) {
			vecMask16[0] = _mm_cmpgtz_epu8(vecDarkers16[0], vecOne);
			vecMask16[1] = _mm_cmpgtz_epu8(vecDarkers16[1], vecOne);
			vecMask16[2] = _mm_cmpgtz_epu8(vecDarkers16[2], vecOne);
			vecMask16[3] = _mm_cmpgtz_epu8(vecDarkers16[3], vecOne);
			vecMask16[4] = _mm_cmpgtz_epu8(vecDarkers16[4], vecOne);
			vecMask16[5] = _mm_cmpgtz_epu8(vecDarkers16[5], vecOne);
			vecMask16[6] = _mm_cmpgtz_epu8(vecDarkers16[6], vecOne);
			vecMask16[7] = _mm_cmpgtz_epu8(vecDarkers16[7], vecOne);
			vecMask16[8] = _mm_cmpgtz_epu8(vecDarkers16[8], vecOne);
			vecMask16[9] = _mm_cmpgtz_epu8(vecDarkers16[9], vecOne);
			vecMask16[10] = _mm_cmpgtz_epu8(vecDarkers16[10], vecOne);
			vecMask16[11] = _mm_cmpgtz_epu8(vecDarkers16[11], vecOne);
			vecMask16[12] = _mm_cmpgtz_epu8(vecDarkers16[12], vecOne);
			vecMask16[13] = _mm_cmpgtz_epu8(vecDarkers16[13], vecOne);
			vecMask16[14] = _mm_cmpgtz_epu8(vecDarkers16[14], vecOne);
			vecMask16[15] = _mm_cmpgtz_epu8(vecDarkers16[15], vecOne);

			// Sum arc #0 without the tail (#NminusOne lines)
			vec0 = vecMask16[0];
			vec0 = _mm_add_epi8(vec0, vecMask16[1]);
			vec0 = _mm_add_epi8(vec0, vecMask16[2]);
			vec0 = _mm_add_epi8(vec0, vecMask16[3]);
			vec0 = _mm_add_epi8(vec0, vecMask16[4]);
			vec0 = _mm_add_epi8(vec0, vecMask16[5]);
			vec0 = _mm_add_epi8(vec0, vecMask16[6]);
			vec0 = _mm_add_epi8(vec0, vecMask16[7]);
			if (N == 12) {
				vec0 = _mm_add_epi8(vec0, vecMask16[8]);
				vec0 = _mm_add_epi8(vec0, vecMask16[9]);
				vec0 = _mm_add_epi8(vec0, vecMask16[10]);
			}			
			for (j = 0; j < 16; ++j) {
				vec0 = _mm_add_epi8(vec0, vecMask16[(NminusOne + j) & 15]); // add tail
				if (_mm_movemask_epi8(_mm_cmpgt_epi8(vec0, vecNMinusOne))) {
					vec1 = _mm_cmpeq_epi8(vec1, vec1); // 0xff
					for (arcStart = j, k = 0; k < N; ++arcStart, ++k) { // FIXME: unroll
						vec1 = _mm_min_epu8(vecDarkers16[arcStart & 15], vec1);
					}
					vecStrengths = _mm_max_epu8(vecStrengths, vec1);
				}
				vec0 = _mm_sub_epi8(vec0, vecMask16[j & 15]); // sub head
			}
		}

		if (maskB) {
			vecMask16[0] = _mm_cmpgtz_epu8(vecBrighters16[0], vecOne);
			vecMask16[1] = _mm_cmpgtz_epu8(vecBrighters16[1], vecOne);
			vecMask16[2] = _mm_cmpgtz_epu8(vecBrighters16[2], vecOne);
			vecMask16[3] = _mm_cmpgtz_epu8(vecBrighters16[3], vecOne);
			vecMask16[4] = _mm_cmpgtz_epu8(vecBrighters16[4], vecOne);
			vecMask16[5] = _mm_cmpgtz_epu8(vecBrighters16[5], vecOne);
			vecMask16[6] = _mm_cmpgtz_epu8(vecBrighters16[6], vecOne);
			vecMask16[7] = _mm_cmpgtz_epu8(vecBrighters16[7], vecOne);
			vecMask16[8] = _mm_cmpgtz_epu8(vecBrighters16[8], vecOne);
			vecMask16[9] = _mm_cmpgtz_epu8(vecBrighters16[9], vecOne);
			vecMask16[10] = _mm_cmpgtz_epu8(vecBrighters16[10], vecOne);
			vecMask16[11] = _mm_cmpgtz_epu8(vecBrighters16[11], vecOne);
			vecMask16[12] = _mm_cmpgtz_epu8(vecBrighters16[12], vecOne);
			vecMask16[13] = _mm_cmpgtz_epu8(vecBrighters16[13], vecOne);
			vecMask16[14] = _mm_cmpgtz_epu8(vecBrighters16[14], vecOne);
			vecMask16[15] = _mm_cmpgtz_epu8(vecBrighters16[15], vecOne);

			// Sum arc #0 without the tail (#NminusOne lines)
			vec0 = vecMask16[0];
			vec0 = _mm_add_epi8(vec0, vecMask16[1]);
			vec0 = _mm_add_epi8(vec0, vecMask16[2]);
			vec0 = _mm_add_epi8(vec0, vecMask16[3]);
			vec0 = _mm_add_epi8(vec0, vecMask16[4]);
			vec0 = _mm_add_epi8(vec0, vecMask16[5]);
			vec0 = _mm_add_epi8(vec0, vecMask16[6]);
			vec0 = _mm_add_epi8(vec0, vecMask16[7]);
			if (N == 12) {
				vec0 = _mm_add_epi8(vec0, vecMask16[8]);
				vec0 = _mm_add_epi8(vec0, vecMask16[9]);
				vec0 = _mm_add_epi8(vec0, vecMask16[10]);
			}
			for (j = 0; j < 16; ++j) {
				vec0 = _mm_add_epi8(vec0, vecMask16[(NminusOne + j) & 15]); // add tail
				if (_mm_movemask_epi8(_mm_cmpgt_epi8(vec0, vecNMinusOne))) {
					vec1 = _mm_cmpeq_epi8(vec1, vec1); // 0xff
					for (arcStart = j, k = 0; k < N; ++arcStart, ++k) { // FIXME: unroll
						vec1 = _mm_min_epu8(vecBrighters16[arcStart & 15], vec1);
					}
					vecStrengths = _mm_max_epu8(vecStrengths, vec1);
				}
				vec0 = _mm_sub_epi8(vec0, vecMask16[j & 15]); // sub head
			}
		}
		
		_mm_storeu_si128(reinterpret_cast<__m128i*>(strengths), vecStrengths);
	}
}

COMPV_NAMESPACE_END()

#endif /* COMPV_ARCH_X86 && COMPV_INTRINSIC */
