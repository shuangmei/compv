/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/core/features/hough/compv_core_feature_houghstd.h"
#include "compv/base/math/compv_math_utils.h"
#include "compv/base/parallel/compv_parallel.h"

#include "compv/core/features/hough/intrin/x86/compv_core_feature_houghstd_intrin_sse41.h"
#include "compv/core/features/hough/intrin/x86/compv_core_feature_houghstd_intrin_avx2.h"

#define COMPV_THIS_CLASSNAME	"CompVHoughStd"

#define COMPV_FEATURE_HOUGHSTD_NMS_MIN_SAMPLES_PER_THREAD	(40*40)
#define COMPV_FEATURE_HOUGHSTD_EDGES_MIN_SAMPLES_PER_THREAD	(40*40)
#define COMPV_FEATURE_HOUGHSTD_ACC_MIN_SAMPLES_PER_THREAD	(1*30)

COMPV_NAMESPACE_BEGIN()

#if COMPV_ASM
#	if COMPV_ARCH_X86
	COMPV_EXTERNC void CompVHoughStdAccGatherRow_8mpd_Asm_X86_AVX2(COMPV_ALIGNED(AVX) const int32_t* pCosRho, COMPV_ALIGNED(AVX) const int32_t* pRowTimesSinRho, compv_uscalar_t col, int32_t* pACC, compv_uscalar_t accStride, compv_uscalar_t maxTheta);
#	endif /* COMPV_ARCH_X86 */
#endif /* COMPV_ASM */

static void CompVHoughStdRowTimesSinRho_C(const int32_t* pSinRho, compv_uscalar_t row, int32_t* rowTimesSinRhoPtr, compv_uscalar_t count);
static void CompVHoughStdAccGatherRow_C(const int32_t* pCosRho, const int32_t* pRowTimesSinRho, compv_uscalar_t col, int32_t* pACC, compv_uscalar_t accStride, compv_uscalar_t maxTheta);
static void CompVHoughStdNmsGatherRow_C(const int32_t * pAcc, size_t nAccStride, uint8_t* pNms, size_t nThreshold, size_t colStart, size_t maxCols);
static void CompVHoughStdNmsApplyRow_C(int32_t* pACC, uint8_t* pNMS, size_t threshold, compv_float32_t theta, int32_t barrier, int32_t row, size_t colStart, size_t maxCols, CompVHoughLineVector& lines);

// threshold used for NMS
CompVHoughStd::CompVHoughStd(float rho COMPV_DEFAULT(1.f), float theta COMPV_DEFAULT(kfMathTrigPiOver180), size_t threshold COMPV_DEFAULT(1))
	:CompVHough(COMPV_HOUGH_STANDARD_ID)
	, m_fRho(rho)
	, m_fTheta(theta)
	, m_nThreshold(threshold)
	, m_nWidth(0)
	, m_nHeight(0)
	, m_nMaxLines(INT_MAX)
{

}

CompVHoughStd:: ~CompVHoughStd()
{

}

// override CompVSettable::set
COMPV_ERROR_CODE CompVHoughStd::set(int id, const void* valuePtr, size_t valueSize) /*Overrides(CompVCaps)*/
{
	COMPV_CHECK_EXP_RETURN(!valuePtr || !valueSize, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	switch (id) {
	case COMPV_HOUGH_SET_FLT32_RHO: {
		COMPV_CHECK_EXP_RETURN(valueSize != sizeof(compv_float32_t) || *reinterpret_cast<const compv_float32_t*>(valuePtr) <= 0.f || *reinterpret_cast<const compv_float32_t*>(valuePtr) > 1.f, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
		const compv_float32_t fRho = *reinterpret_cast<const compv_float32_t*>(valuePtr);
		COMPV_CHECK_CODE_RETURN(initCoords(fRho, m_fTheta, m_nThreshold, m_nWidth, m_nHeight));
		return COMPV_ERROR_CODE_S_OK;
	}
	case COMPV_HOUGH_SET_FLT32_THETA: {
		COMPV_CHECK_EXP_RETURN(valueSize != sizeof(compv_float32_t) || *reinterpret_cast<const compv_float32_t*>(valuePtr) <= 0.f, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
		const compv_float32_t fTheta = *reinterpret_cast<const compv_float32_t*>(valuePtr);
		COMPV_CHECK_CODE_RETURN(initCoords(m_fRho, fTheta, m_nThreshold, m_nWidth, m_nHeight));
		return COMPV_ERROR_CODE_S_OK;
	}
	case COMPV_HOUGH_SET_INT_THRESHOLD: {
		COMPV_CHECK_EXP_RETURN(valueSize != sizeof(int) || *reinterpret_cast<const int*>(valuePtr) <= 0, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
		const int nThreshold = *reinterpret_cast<const int*>(valuePtr);
		COMPV_CHECK_CODE_RETURN(initCoords(m_fRho, m_fTheta, static_cast<size_t>(nThreshold), m_nWidth, m_nHeight));
		return COMPV_ERROR_CODE_S_OK;
	}
	case COMPV_HOUGH_SET_INT_MAXLINES: {
		COMPV_CHECK_EXP_RETURN(valueSize != sizeof(int), COMPV_ERROR_CODE_E_INVALID_PARAMETER);
		m_nMaxLines = static_cast<size_t>(reinterpret_cast<const int*>(valuePtr) <= 0  ? INT_MAX : *reinterpret_cast<const int*>(valuePtr));
		return COMPV_ERROR_CODE_S_OK;
	}
	default: {
		COMPV_DEBUG_ERROR_EX(COMPV_THIS_CLASSNAME, "Set with id %d not implemented", id);
		return COMPV_ERROR_CODE_E_NOT_IMPLEMENTED;
	}
	}
}

COMPV_ERROR_CODE CompVHoughStd::process(const CompVMatPtr& edges, CompVHoughLineVector& lines) /*Overrides(CompVHough)*/
{
	COMPV_CHECK_EXP_RETURN(!edges || edges->isEmpty() || edges->subType() != COMPV_SUBTYPE_PIXELS_Y, COMPV_ERROR_CODE_E_INVALID_PARAMETER, "Edges null or not grayscale");

	// Init coords (sine and cosine tables)
	COMPV_CHECK_CODE_RETURN(initCoords(m_fRho, m_fTheta, m_nThreshold, edges->cols(), edges->rows()));

	lines.clear();

	size_t threadsCountNMS = 1, threadsCountACC = 1, threadsCountEdges = 1;
	CompVThreadDispatcherPtr threadDisp = CompVParallel::threadDispatcher();
	const size_t maxThreads = (threadDisp && !threadDisp->isMotherOfTheCurrentThread()) ? static_cast<size_t>(threadDisp->threadsCount()) : 1;
	if (maxThreads > 1) {
		threadsCountEdges = CompVThreadDispatcher::guessNumThreadsDividingAcrossY(edges->cols(), edges->rows(), maxThreads, COMPV_FEATURE_HOUGHSTD_EDGES_MIN_SAMPLES_PER_THREAD);
	}

	// Edges gathering: the edges are always unequally distributed (some regions have almost no edge) which
	// means some threads will almost execute no code. This is why we collect the edges to make sure
	// all threads will have enough work to do.
	std::vector<CompVHoughStdEdge >vecEdges;
	if (threadsCountEdges > 1) {
		const size_t countAny = (edges->rows() / threadsCountEdges);
		const size_t countLast = countAny + (edges->rows() % threadsCountEdges);
		std::vector<std::vector<CompVHoughStdEdge > > vecVecEdges;
		vecVecEdges.resize(threadsCountEdges);
		auto funcPtrEdgesGather = [&](size_t rowStart, size_t rowCount, size_t threadIdx) -> COMPV_ERROR_CODE {
			const int32_t rowStartInt32 = static_cast<int32_t>(rowStart);
			const int32_t rowEndInt32 = static_cast<int32_t>(rowStart + rowCount);
			const int32_t colsInt32 = static_cast<int32_t>(edges->cols());
			const uint8_t* edgesPtr = edges->ptr<const uint8_t>(rowStart);
			std::vector<CompVHoughStdEdge >& mtVecEdges = vecVecEdges[threadIdx];
			for (int32_t j = rowStartInt32; j < rowEndInt32; ++j) {
				for (int32_t i = 0; i < colsInt32; ++i) {
					if (edgesPtr[i]) {
						mtVecEdges.push_back(CompVHoughStdEdge(j, i));
					}
				}
				edgesPtr += edges->strideInBytes();
			}
			return COMPV_ERROR_CODE_S_OK;
		};
		CompVAsyncTaskIds taskIds;
		for (size_t threadIdx = 0, rowStart = 0; threadIdx < threadsCountEdges; ++threadIdx, rowStart += countAny) {
			COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrEdgesGather, rowStart, (threadIdx == (threadsCountEdges - 1)) ? countLast : countAny, threadIdx), taskIds));
		}
		COMPV_CHECK_CODE_RETURN(threadDisp->waitOne(taskIds[0]));
		vecEdges = vecVecEdges[0];
		for (size_t threadIdx = 1; threadIdx < threadsCountEdges; ++threadIdx) {
			COMPV_CHECK_CODE_RETURN(threadDisp->waitOne(taskIds[threadIdx]));
			if (!vecVecEdges[threadIdx].empty()) {
				vecEdges.insert(vecEdges.end(), vecVecEdges[threadIdx].begin(), vecVecEdges[threadIdx].end());
			}
		}
	}
	else {
		const int32_t edges_rows = static_cast<int32_t>(edges->rows());
		const int32_t edges_cols = static_cast<int32_t>(edges->cols());
		const uint8_t* edges_ptr = edges->ptr<const uint8_t>();
		for (int32_t j = 0; j < edges_rows; ++j) {
			for (int32_t i = 0; i < edges_cols; ++i) {
				if (edges_ptr[i]) {
					vecEdges.push_back(CompVHoughStdEdge(j, i));
				}
			}
			edges_ptr += edges->strideInBytes();
		}
	}
	
	// Compute number of threads
	if (maxThreads > 1) {
		threadsCountNMS = CompVThreadDispatcher::guessNumThreadsDividingAcrossY(m_NMS->cols(), m_NMS->rows(), maxThreads, COMPV_FEATURE_HOUGHSTD_NMS_MIN_SAMPLES_PER_THREAD);
		threadsCountACC = CompVThreadDispatcher::guessNumThreadsDividingAcrossY(1, vecEdges.size(), maxThreads, COMPV_FEATURE_HOUGHSTD_ACC_MIN_SAMPLES_PER_THREAD);
	}

	// Accumulator gathering
	CompVHoughAccThreadsCtx accThreadsCtx = { 0 };
	accThreadsCtx.threadsCount = threadsCountACC;
	if (threadsCountACC > 1) {
		COMPV_CHECK_CODE_RETURN(CompVMutex::newObj(&accThreadsCtx.mutex));
		const size_t countAny = (vecEdges.size() / threadsCountACC);
		const size_t countLast = countAny + (vecEdges.size() % threadsCountACC);
		auto funcPtrAccGather = [&](std::vector<CompVHoughStdEdge >::const_iterator &begin, std::vector<CompVHoughStdEdge >::const_iterator &end) -> COMPV_ERROR_CODE {
			COMPV_CHECK_CODE_RETURN(acc_gather(begin, end, &accThreadsCtx));
			return COMPV_ERROR_CODE_S_OK;
		};
		size_t threadIdx;
		std::vector<CompVHoughStdEdge >::const_iterator begin;
		CompVAsyncTaskIds taskIds;
		for (threadIdx = 0, begin = vecEdges.begin(); threadIdx < threadsCountACC; ++threadIdx, begin += countAny) {
			COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrAccGather, begin, (threadIdx == (threadsCountACC - 1)) ? vecEdges.end() : (begin + countAny)), taskIds));
		}
		COMPV_CHECK_CODE_RETURN(threadDisp->wait(taskIds));
	}
	else {
		COMPV_CHECK_CODE_RETURN(acc_gather(vecEdges.begin(), vecEdges.end(), &accThreadsCtx));
	}

	// Non-maxima suppression (NMS)
	if (threadsCountNMS > 1) {
		std::vector<CompVHoughLineVector > mt_lines;
		const size_t countAny = (m_NMS->rows() / threadsCountNMS);
		const size_t countLast = countAny + (m_NMS->rows() % threadsCountNMS);
		auto funcPtrNmsGather = [&](size_t rowStart, size_t rowCount) -> COMPV_ERROR_CODE {
			COMPV_CHECK_CODE_RETURN(nms_gather(rowStart, rowCount, accThreadsCtx.acc));
			return COMPV_ERROR_CODE_S_OK;
		};
		auto funcPtrNmsApply = [&](size_t rowStart, size_t rowCount, size_t threadIdx) -> COMPV_ERROR_CODE {
			COMPV_CHECK_CODE_RETURN(nms_apply(rowStart, rowCount, accThreadsCtx.acc, mt_lines[threadIdx]));
			return COMPV_ERROR_CODE_S_OK;
		};
		size_t rowStart, threadIdx;
		// NMS gathering
		CompVAsyncTaskIds taskIds;
		taskIds.reserve(threadsCountNMS);
		mt_lines.resize(threadsCountNMS);
		for (threadIdx = 0, rowStart = 0; threadIdx < threadsCountNMS - 1; ++threadIdx, rowStart += countAny) {
			COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrNmsGather, rowStart, countAny), taskIds));
		}
		COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrNmsGather, rowStart, countLast), taskIds));
		COMPV_CHECK_CODE_RETURN(threadDisp->wait(taskIds));
		// NMS-apply
		taskIds.clear();
		for (threadIdx = 0, rowStart = 0; threadIdx < threadsCountNMS - 1; ++threadIdx, rowStart += countAny) {
			COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrNmsApply, rowStart, countAny, threadIdx), taskIds));
		}
		COMPV_CHECK_CODE_RETURN(threadDisp->invoke(std::bind(funcPtrNmsApply, rowStart, countLast, threadIdx), taskIds));
		COMPV_CHECK_CODE_RETURN(threadDisp->waitOne(taskIds[0]));
		lines = mt_lines[0];
		for (threadIdx = 1; threadIdx < threadsCountNMS; ++threadIdx) {
			COMPV_CHECK_CODE_RETURN(threadDisp->waitOne(taskIds[threadIdx]));
			if (!mt_lines[threadIdx].empty()) {
				lines.insert(lines.end(), mt_lines[threadIdx].begin(), mt_lines[threadIdx].end());
			}
		}
	}
	else {
		COMPV_CHECK_CODE_RETURN(nms_gather(0, m_NMS->rows(), accThreadsCtx.acc));
		COMPV_CHECK_CODE_RETURN(nms_apply(0, m_NMS->rows(), accThreadsCtx.acc, lines));
	}

	// Sort result and retain best
	if (!lines.empty()) {
		std::sort(lines.begin(), lines.end(), [](const CompVHoughLine & a, const CompVHoughLine & b) -> bool {
			return a.strength > b.strength;
		});
		const size_t maxLines = COMPV_MATH_MIN(lines.size(), m_nMaxLines);
		lines.resize(maxLines);
	}

	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVHoughStd::newObj(CompVHoughPtrPtr hough, float rho COMPV_DEFAULT(1.f), float theta COMPV_DEFAULT(kfMathTrigPiOver180), size_t threshold COMPV_DEFAULT(1))
{
	COMPV_CHECK_EXP_RETURN(!hough || rho <=0 || rho > 1.f, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	CompVHoughPtr hough_ = new CompVHoughStd(rho, theta, threshold);
	COMPV_CHECK_EXP_RETURN(!hough_, COMPV_ERROR_CODE_E_OUT_OF_MEMORY);

	*hough = *hough_;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVHoughStd::initCoords(float fRho, float fTheta, size_t nThreshold, size_t nWidth/*=0*/, size_t nHeight/*=0*/)
{
	nWidth = !nWidth ? m_nWidth : nWidth;
	nHeight = !nHeight ? m_nHeight : nHeight;
	if (m_fRho != fRho || m_fTheta != fTheta || m_nWidth != nWidth || m_nHeight != nHeight) {
		// rho = x*cos(t) + y*sin(t), "cos" and "sin" in [-1, 1] which means we have (x+y)*2 values
		const size_t maxRhoCount = COMPV_MATH_ROUNDFU_2_NEAREST_INT((((nWidth + nHeight) << 1) + 1) / fRho, size_t);
		const size_t maxThetaCount = COMPV_MATH_ROUNDFU_2_NEAREST_INT(kfMathTrigPi / fTheta, size_t);
		COMPV_CHECK_CODE_RETURN(CompVMat::newObjAligned<uint8_t>(&m_NMS, maxRhoCount, maxThetaCount));
		COMPV_CHECK_CODE_RETURN(m_NMS->zero_rows());
		COMPV_CHECK_CODE_RETURN(CompVMat::newObjAligned<int32_t>(&m_SinRho, 1, maxThetaCount));
		COMPV_CHECK_CODE_RETURN(CompVMat::newObjAligned<int32_t>(&m_CosRho, 1, maxThetaCount));

		int32_t* pSinRho = m_SinRho->ptr<int32_t>();
		int32_t* pCosRho = m_CosRho->ptr<int32_t>();
		float tt;
		size_t t;
		for (t = 0, tt = 0.f; t < maxThetaCount; ++t, tt += fTheta) {
			pSinRho[t] = static_cast<int32_t>((COMPV_MATH_SIN(tt) * m_fRho) * 65535.f); 
			pCosRho[t] = static_cast<int32_t>((COMPV_MATH_COS(tt) * m_fRho) * 65535.f);
		}

		m_fRho = fRho;
		m_fTheta = fTheta;
		m_nWidth = nWidth;
		m_nHeight = nHeight;
		m_nBarrier = m_nWidth + m_nHeight;
	}
	m_nThreshold = nThreshold;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVHoughStd::acc_gather(std::vector<CompVHoughStdEdge >::const_iterator& start, std::vector<CompVHoughStdEdge >::const_iterator& end, CompVHoughAccThreadsCtx* threadsCtx)
{
	CompVPtr<CompVMemZero<int32_t> *> acc;
	CompVMatPtr rowTimesSinRho; // row * pSinRho[theta]
	size_t nmsStride, accStride;
	COMPV_CHECK_CODE_RETURN(m_NMS->strideInElts(nmsStride));
	COMPV_CHECK_CODE_RETURN(CompVMemZero<int32_t>::newObj(&acc, m_NMS->rows(), m_NMS->cols()));
	accStride = acc->stride();
	const compv_uscalar_t accStrideScalar = static_cast<compv_uscalar_t>(accStride);
	const int32_t accStrideInt32 = static_cast<int32_t>(accStride);
	const compv_uscalar_t maxThetaCountScalar = static_cast<compv_uscalar_t>(acc->cols());
	COMPV_CHECK_CODE_RETURN(CompVMat::newObjAligned<int32_t>(&rowTimesSinRho, 1, acc->cols())); // maxTheta's int32_t values
	int32_t* rowTimesSinRhoPtr = rowTimesSinRho->ptr<int32_t>();
	const int32_t* pSinRho = m_SinRho->ptr<const int32_t>();
	const int32_t* pCosRho = m_CosRho->ptr<const int32_t>();
	int32_t *pACC = acc->ptr(m_nBarrier);
	int32_t row, col, rho, maxCol = -1, lastRow = -1;
	const bool multiThreaded = (threadsCtx->threadsCount > 1);
	int xmpd = 1;

	void (*CompVHoughStdAccGatherRow_xmpd)(COMPV_ALIGNED(X) const int32_t* pCosRho, COMPV_ALIGNED(X) const int32_t* pRowTimesSinRho, compv_uscalar_t col, int32_t* pACC, compv_uscalar_t accStride, compv_uscalar_t maxTheta)
		= CompVHoughStdAccGatherRow_C;

#if COMPV_ARCH_X86
	if (maxThetaCountScalar >= 16 && COMPV_IS_ALIGNED_SSE(pCosRho) && COMPV_IS_ALIGNED_SSE(rowTimesSinRhoPtr) && COMPV_IS_ALIGNED_SSE(pSinRho)) {
		if (CompVCpu::isEnabled(kCpuFlagSSE41)) {
			COMPV_EXEC_IFDEF_INTRIN_X86((CompVHoughStdAccGatherRow_xmpd = CompVHoughStdAccGatherRow_4mpd_Intrin_SSE41, xmpd = 4));
		}
	}
	if (maxThetaCountScalar >= 32 && COMPV_IS_ALIGNED_AVX2(pCosRho) && COMPV_IS_ALIGNED_AVX2(rowTimesSinRhoPtr) && COMPV_IS_ALIGNED_AVX2(pSinRho)) {
		if (CompVCpu::isEnabled(kCpuFlagAVX2)) {
			COMPV_EXEC_IFDEF_INTRIN_X86((CompVHoughStdAccGatherRow_xmpd = CompVHoughStdAccGatherRow_8mpd_Intrin_AVX2, xmpd = 8));
			COMPV_EXEC_IFDEF_ASM_X86((CompVHoughStdAccGatherRow_xmpd = CompVHoughStdAccGatherRow_8mpd_Asm_X86_AVX2, xmpd = 8));	
		}
	}
#endif /* COMPV_ARCH_X86 */
	
	const compv_uscalar_t xmpd_consumed = (maxThetaCountScalar & -xmpd);
	const compv_uscalar_t xmpd_trailling = (maxThetaCountScalar - xmpd_consumed);
	for (std::vector<CompVHoughStdEdge >::const_iterator it = start; it < end; ++it) {
		row = it->row;
		col = it->col;
		// update maxCol 
		/*if (multiThreaded)*/ { // it's faster to remove the conditional branch (also, we're almost always in MT case)
			maxCol = std::max(maxCol, col);
		}
		// update lastRow
		if (lastRow != row) {
			lastRow = row;
			CompVHoughStdRowTimesSinRho_C(pSinRho, static_cast<compv_uscalar_t>(row), rowTimesSinRhoPtr, maxThetaCountScalar);
		}
		// gather
		CompVHoughStdAccGatherRow_xmpd(pCosRho, rowTimesSinRhoPtr, static_cast<compv_uscalar_t>(col), pACC, accStrideScalar, xmpd_consumed);
		if (xmpd_trailling) {
			for (compv_uscalar_t theta = xmpd_consumed; theta < maxThetaCountScalar; ++theta) {
				rho = (col * pCosRho[theta] + rowTimesSinRhoPtr[theta]) >> 16;
				pACC[theta - (rho * accStrideInt32)]++; //!\\ Not thread-safe
			}
		}
	}
	
	if (multiThreaded) {
		// multi-threaded
		COMPV_CHECK_CODE_RETURN(threadsCtx->mutex->lock());
		if (!threadsCtx->acc) { // First thread ?
			threadsCtx->acc = acc;
		}
		else if (maxCol != -1) { // sum only if there is at least #1 non-null pixel
			const int32_t maxRow = (end - 1)->row;
			const size_t sumStart = m_nBarrier - (maxCol + maxRow); // "cos=1, sin=1"
			const size_t sumEnd = m_nBarrier - (-maxCol); //"cos=-1, sin=0"
			COMPV_CHECK_CODE_ASSERT(CompVMathUtils::sum2<int32_t>(
				threadsCtx->acc->ptr(sumStart), 
				acc->ptr(sumStart), 
				threadsCtx->acc->ptr(sumStart),
				acc->cols(), 
				(sumEnd - sumStart), 
				acc->stride())
			);
		}
		COMPV_CHECK_CODE_RETURN(threadsCtx->mutex->unlock());
	}
	else {
		// single-threaded
		threadsCtx->acc = acc;
	}

	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVHoughStd::nms_gather(size_t rowStart, size_t rowCount, CompVPtr<CompVMemZero<int32_t> *>& acc)
{
	size_t rows = m_NMS->rows();
	size_t maxCols = m_NMS->cols() - 1;
	size_t rowEnd = COMPV_MATH_MIN((rowStart + rowCount), (rows - 1));
	rowStart = COMPV_MATH_MAX(1, rowStart);

	const int32_t *pACC = acc->ptr(rowStart);
	uint8_t* pNMS = m_NMS->ptr<uint8_t>(rowStart);
	size_t nmsStride, accStride, row;
	COMPV_CHECK_CODE_RETURN(m_NMS->strideInElts(nmsStride));
	accStride = acc->stride();

#if COMPV_ARCH_X86 && 0 
	void(*HoughStdNmsGatherRow)(const int32_t * pAcc, compv_uscalar_t nAccStride, uint8_t* pNms, int32_t nThreshold, compv_uscalar_t width) = NULL;
	if (maxCols >= 4 && CompVCpu::isEnabled(kCpuFlagSSE2)) {
		COMPV_EXEC_IFDEF_INTRIN_X86(HoughStdNmsGatherRow = HoughStdNmsGatherRow_Intrin_SSE2);
	}
	if (HoughStdNmsGatherRow) {
		for (row = rowStart; row < rowEnd; ++row) {
			HoughStdNmsGatherRow(pACC, (compv_uscalar_t)accStride, pNMS, m_nThreshold, (compv_uscalar_t)maxCols);
			pACC += accStride, pNMS += nmsStride;
		}
		return COMPV_ERROR_CODE_S_OK;
	}
#endif /* COMPV_ARCH_X86 */
	
	for (row = rowStart; row < rowEnd; ++row) {
		CompVHoughStdNmsGatherRow_C(pACC, accStride, pNMS, m_nThreshold, 1, maxCols);
		pACC += accStride, pNMS += nmsStride;
	}
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVHoughStd::nms_apply(size_t rowStart, size_t rowCount, CompVPtr<CompVMemZero<int32_t> *>& acc, CompVHoughLineVector& lines)
{
	size_t cols = m_NMS->cols();
	int32_t rEnd = static_cast<int32_t>(COMPV_MATH_MIN((rowStart + rowCount), m_NMS->rows()));
	int32_t rStart = static_cast<int32_t>(COMPV_MATH_MAX(0, rowStart));
	uint8_t* pNMS = m_NMS->ptr<uint8_t>(rStart);
	int32_t* pACC = acc->ptr(rStart);
	size_t nmsStride, accStride;
	int32_t nBarrier = static_cast<int32_t>(m_nBarrier);
	COMPV_CHECK_CODE_RETURN(m_NMS->strideInElts(nmsStride));
	accStride = acc->stride();

#if COMPV_ARCH_X86 && 0
	void(*HoughStdNmsApplyRow)(COMPV_ALIGNED(X) int32_t* pACC, COMPV_ALIGNED(X) uint8_t* pNMS, int32_t threshold, compv_float32_t theta, int32_t barrier, int32_t row, size_t maxCols, CompVPtrBox(CompVCoordPolar2f)& coords) = NULL;
	if (cols >= 8 && CompVCpu::isEnabled(kCpuFlagSSE2) && COMPV_IS_ALIGNED_SSE(pACC) && COMPV_IS_ALIGNED_SSE(pNMS)) {
		COMPV_EXEC_IFDEF_INTRIN_X86(HoughStdNmsApplyRow = HoughStdNmsApplyRow_Intrin_SSE2);
	}
	if (HoughStdNmsApplyRow) {
		for (int32_t row = rStart; row < rEnd; ++row) {
			HoughStdNmsApplyRow(pACC, pNMS, m_nThreshold, m_fTheta, nBarrier, row, cols, coords);
			pACC += accStride, pNMS += nmsStride;
		}
		return COMPV_ERROR_CODE_S_OK;
	}
#endif /* COMPV_ARCH_X86 */
	
	for (int32_t row = rStart; row < rEnd; ++row) {
		CompVHoughStdNmsApplyRow_C(pACC, pNMS, m_nThreshold, m_fTheta, nBarrier, row, 0, cols, lines);
		pACC += accStride, pNMS += nmsStride;
	}
	return COMPV_ERROR_CODE_S_OK;
}

static void CompVHoughStdRowTimesSinRho_C(const int32_t* pSinRho, compv_uscalar_t row, int32_t* rowTimesSinRhoPtr, compv_uscalar_t count)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No SIMD for GPU implementation found");
	const int32_t rowInt32 = static_cast<int32_t>(row);
	for (compv_uscalar_t i = 0; i < count; ++i) {
		rowTimesSinRhoPtr[i] = pSinRho[i] * rowInt32;
	}
}

// Not thread-safe
static void CompVHoughStdAccGatherRow_C(const int32_t* pCosRho, const int32_t* pRowTimesSinRho, compv_uscalar_t col, int32_t* pACC, compv_uscalar_t accStride, compv_uscalar_t maxTheta)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No SIMD for GPU implementation found");
	int32_t rhoInt32;
	const int32_t colInt32 = static_cast<int32_t>(col);
	const int32_t accStrideInt32 = static_cast<int32_t>(accStride);
	for (compv_uscalar_t theta = 0; theta < maxTheta; ++theta) {
		rhoInt32 = (colInt32 * pCosRho[theta] + pRowTimesSinRho[theta]) >> 16;
		pACC[theta - (rhoInt32 * accStrideInt32)]++; //!\\ Not thread-safe
	}
}

static void CompVHoughStdNmsGatherRow_C(const int32_t * pAcc, size_t nAccStride, uint8_t* pNms, size_t nThreshold, size_t colStart, size_t maxCols)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No SIMD for GPU implementation found");
	int32_t t;
	const int32_t *curr, *top, *bottom;
	int stride = static_cast<int>(nAccStride);
	const int32_t thresholdInt32 = static_cast<int32_t>(nThreshold);
	for (size_t col = colStart; col < maxCols; ++col) {
		if (pAcc[col] > thresholdInt32) {
			t = pAcc[col];
			curr = &pAcc[col];
			top = &pAcc[col - stride];
			bottom = &pAcc[col + stride];
			if (curr[-1] > t || curr[+1] > t || top[-1] > t || top[0] > t || top[+1] > t || bottom[-1] > t || bottom[0] > t || bottom[+1] > t) {
				pNms[col] = 0xf;
			}
		}
	}
}

static void CompVHoughStdNmsApplyRow_C(int32_t* pACC, uint8_t* pNMS, size_t threshold, compv_float32_t theta, int32_t barrier, int32_t row, size_t colStart, size_t maxCols, CompVHoughLineVector& lines)
{
	COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED("No SIMD for GPU implementation found");
	const int32_t thresholdInt32 = static_cast<int32_t>(threshold);
	for (size_t col = colStart; col < maxCols; ++col) {
		if (pNMS[col]) {
			pNMS[col] = 0; // reset for  next time
			// do not push the line
		}
		else if (pACC[col] > thresholdInt32) {
			lines.push_back(CompVHoughLine(
				static_cast<compv_float32_t>(barrier - row), 
				col * theta, 
				static_cast<size_t>(pACC[col])
			));
		}
	}
}

COMPV_NAMESPACE_END()
