/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/drawing/opengl/compv_blitter_gl.h"
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "compv/drawing/compv_drawing.h"
#include "compv/gl/compv_gl_utils.h"
#include "compv/gl/compv_gl_info.h"
#include "compv/gl/compv_gl_func.h"

COMPV_NAMESPACE_BEGIN()

CompVBlitterGL::CompVBlitterGL()
	: m_bInit(false)
	, m_nWidth(0)
	, m_nHeight(0)
	, m_nStride(0)
	, m_bToScreen(false)
	, m_uNameVertexBuffer(0)
	, m_uNameIndiceBuffer(0)
	, m_uNamePrgAttPosition(0)
	, m_uNamePrgAttTexCoord(0)
	, m_uNamePrgUnifMVP(0)
	, m_uNameVAO(0)
{

}

CompVBlitterGL::~CompVBlitterGL()
{
	COMPV_CHECK_CODE_ASSERT(deInit());
}

// Bind to VAO and activate the program
COMPV_ERROR_CODE CompVBlitterGL::bind()
{
	COMPV_CHECK_EXP_RETURN(!CompVGLUtils::isGLContextSet(), COMPV_ERROR_CODE_E_GL_NO_CONTEXT);
	COMPV_CHECK_EXP_RETURN(!m_bInit, COMPV_ERROR_CODE_E_INVALID_STATE);
	COMPV_CHECK_CODE_RETURN(m_ptrProgram->useBegin());

	// Because MVP could be dirty we have to send the data again
	// FIXME(dmi): find a way to detect that MVP is dirty
	if (m_bMVP && m_ptrMVP) {
		COMPV_glUniformMatrix4fv(m_uNamePrgUnifMVP, 1, GL_FALSE, m_ptrMVP->matrix()->ptr());
	}

	if (CompVGLInfo::extensions::vertex_array_object()) {
		COMPV_glBindVertexArray(m_uNameVAO);
	}
	else {
		COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
		COMPV_glBindBuffer(GL_ARRAY_BUFFER, m_uNameVertexBuffer);
		COMPV_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uNameIndiceBuffer);
		COMPV_glEnableVertexAttribArray(m_uNamePrgAttPosition);
		COMPV_glEnableVertexAttribArray(m_uNamePrgAttTexCoord);
		COMPV_glVertexAttribPointer(m_uNamePrgAttPosition, 3, GL_FLOAT, GL_FALSE, sizeof(CompVGLVertex), 0);
		COMPV_glVertexAttribPointer(m_uNamePrgAttTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(CompVGLVertex), (GLvoid*)(sizeof(GLfloat) * 3));
	}

	return COMPV_ERROR_CODE_S_OK;
}

// Unbind the VAO and deactivate the program
COMPV_ERROR_CODE CompVBlitterGL::unbind()
{
	COMPV_CHECK_EXP_RETURN(!CompVGLUtils::isGLContextSet(), COMPV_ERROR_CODE_E_GL_NO_CONTEXT);

	if (CompVGLInfo::extensions::vertex_array_object()) {
		COMPV_glBindVertexArray(0);
	}
	else {
		COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED();
		COMPV_glBindBuffer(GL_ARRAY_BUFFER, 0);
		COMPV_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	if (m_ptrProgram) {
		COMPV_CHECK_CODE_RETURN(m_ptrProgram->useEnd());
	}

	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVBlitterGL::setMVP(CompVMVPPtr mvp)
{
	COMPV_CHECK_EXP_RETURN(!mvp, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	m_ptrMVP = mvp;
	if (m_bInit && m_bMVP) {
		COMPV_CHECK_EXP_RETURN(!CompVGLUtils::isGLContextSet(), COMPV_ERROR_CODE_E_GL_NO_CONTEXT);
		COMPV_ERROR_CODE err;
		COMPV_CHECK_CODE_BAIL(err = m_ptrProgram->useBegin());
		COMPV_glUniformMatrix4fv(m_uNamePrgUnifMVP, 1, GL_FALSE, mvp->matrix()->ptr());
	bail:
		COMPV_CHECK_CODE_ASSERT(m_ptrProgram->useEnd());
		return err;
	}
	
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVBlitterGL::setSize(size_t width, size_t height, size_t stride)
{
	CompVGLVertex newVertices[4];
	COMPV_CHECK_CODE_RETURN(CompVBlitterGL::updateVertices(width, height, stride, m_bToScreen, &newVertices));

	if (m_uNameVertexBuffer) {
		COMPV_glBindBuffer(GL_ARRAY_BUFFER, m_uNameVertexBuffer);
		COMPV_glBufferData(GL_ARRAY_BUFFER, sizeof(newVertices), newVertices, GL_STATIC_DRAW);
	}

	m_nWidth = width;
	m_nHeight = height;
	m_nStride = stride;
	*m_Vertices = *newVertices;

	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVBlitterGL::init(size_t width, size_t height, size_t stride, const std::string& prgVertexData, const std::string& prgFragData, bool bMVP /*= false*/, bool bToScreen /*= false*/)
{
	if (m_bInit) {
		return COMPV_ERROR_CODE_S_OK;
	}
	COMPV_CHECK_EXP_RETURN(!CompVGLUtils::isGLContextSet(), COMPV_ERROR_CODE_E_GL_NO_CONTEXT);
	COMPV_CHECK_EXP_RETURN(!width || !height || stride < width || prgVertexData.empty() || prgFragData.empty(), COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	COMPV_ERROR_CODE err = COMPV_ERROR_CODE_S_OK;
	m_bInit = true; // Make sure deInit() will be executed if this function fails
	CompVGLVertex newVertices[4];

	// Model-View-Projection
	if (!bMVP) {
		m_ptrMVP = NULL;
	}
	else if (!m_ptrMVP) {
		COMPV_CHECK_CODE_RETURN(CompVMVP::newObjProjection2D(&m_ptrMVP));
	}

	if (CompVGLInfo::extensions::vertex_array_object()) {
		if (!m_uNameVAO) {
			COMPV_CHECK_CODE_BAIL(err = CompVGLUtils::vertexArraysGen(&m_uNameVAO));
		}
		COMPV_glBindVertexArray(m_uNameVAO);
	}

	// Vertex buffer
	if (!m_uNameVertexBuffer) {
		COMPV_CHECK_CODE_BAIL(err = CompVGLUtils::bufferGen(&m_uNameVertexBuffer));
	}
	COMPV_glBindBuffer(GL_ARRAY_BUFFER, m_uNameVertexBuffer);
	COMPV_CHECK_CODE_RETURN(CompVBlitterGL::updateVertices(width, height, stride, bToScreen, &newVertices));
	COMPV_glBufferData(GL_ARRAY_BUFFER, sizeof(newVertices), newVertices, GL_STATIC_DRAW);

	// Indice buffer
	if (!m_uNameIndiceBuffer) {
		COMPV_CHECK_CODE_BAIL(err = CompVGLUtils::bufferGen(&m_uNameIndiceBuffer));
		COMPV_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uNameIndiceBuffer);
		COMPV_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCompVGLTexture2DIndices), kCompVGLTexture2DIndices, GL_STATIC_DRAW);
	}

	COMPV_CHECK_EXP_BAIL(prgVertexData.empty(), err = COMPV_ERROR_CODE_E_GL);
	COMPV_CHECK_EXP_BAIL(prgFragData.empty(), err = COMPV_ERROR_CODE_E_GL);
	COMPV_CHECK_CODE_BAIL(CompVGLProgram::newObj(&m_ptrProgram, prgVertexData.c_str(), prgVertexData.length(), prgFragData.c_str(), prgFragData.length()));
	COMPV_CHECK_CODE_ASSERT(err = m_ptrProgram->useBegin());

	COMPV_glGetAttribLocation(&m_uNamePrgAttPosition, m_ptrProgram->name(), "position");
	COMPV_glGetAttribLocation(&m_uNamePrgAttTexCoord, m_ptrProgram->name(), "texCoord");

	if (bMVP) {
		COMPV_glGetUniformLocation(&m_uNamePrgUnifMVP, m_ptrProgram->name(), "MVP");
	}
	COMPV_glBindBuffer(GL_ARRAY_BUFFER, m_uNameVertexBuffer);
	COMPV_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uNameIndiceBuffer);
	COMPV_glEnableVertexAttribArray(m_uNamePrgAttPosition);
	COMPV_glEnableVertexAttribArray(m_uNamePrgAttTexCoord);
	COMPV_glVertexAttribPointer(m_uNamePrgAttPosition, 3, GL_FLOAT, GL_FALSE, sizeof(CompVGLVertex), 0);
	COMPV_glVertexAttribPointer(m_uNamePrgAttTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(CompVGLVertex), (GLvoid*)(sizeof(GLfloat) * 3));
	if (bMVP) {
		COMPV_DEBUG_INFO_CODE_FOR_TESTING();
		// Set aspect ratio
		//float arX = static_cast<float>(width) / static_cast<float>(height);
		//float arY = static_cast<float>(height) / static_cast<float>(width);
		//COMPV_CHECK_CODE_BAIL(err = m_ptrMVP->projection()->setAspectRatio(arX));
		//COMPV_CHECK_CODE_BAIL(err = m_ptrMVP->model()->matrix()->scale(CompVDrawingVec3f(1.f/arX, 1.f/arY, 1.f)));
		COMPV_glUniformMatrix4fv(m_uNamePrgUnifMVP, 1, GL_FALSE, m_ptrMVP->matrix()->ptr());
	}

	m_nWidth = width;
	m_nHeight = height;
	m_nStride = stride;
	m_bToScreen = bToScreen;
	*m_Vertices = *newVertices;
	m_bMVP = bMVP;

bail:
	if (m_ptrProgram) {
		COMPV_CHECK_CODE_ASSERT(m_ptrProgram->useEnd());
	}
	if (CompVGLInfo::extensions::vertex_array_object()) {
		COMPV_glBindVertexArray(0);
	}
	COMPV_glActiveTexture(GL_TEXTURE0);
	COMPV_glBindTexture(GL_TEXTURE_2D, 0);
	COMPV_glBindBuffer(GL_ARRAY_BUFFER, 0);
	COMPV_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	if (COMPV_ERROR_CODE_IS_NOK(err)) {
		COMPV_CHECK_CODE_ASSERT(deInit());
		m_bInit = false;
	}

	return err;
}

COMPV_ERROR_CODE CompVBlitterGL::deInit()
{
	if (!m_bInit) {
		return COMPV_ERROR_CODE_S_OK;
	}
	COMPV_CHECK_EXP_RETURN(!CompVGLUtils::isGLContextSet(), COMPV_ERROR_CODE_E_GL_NO_CONTEXT);

	CompVGLUtils::bufferDelete(&m_uNameVertexBuffer);
	CompVGLUtils::bufferDelete(&m_uNameIndiceBuffer);
	CompVGLUtils::vertexArraysDelete(&m_uNameVAO);
	m_ptrProgram = NULL;

	m_bInit = false;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_ERROR_CODE CompVBlitterGL::updateVertices(size_t width, size_t height, size_t stride, bool bToScreen, CompVGLVertex(*Vertices)[4])
{
	COMPV_CHECK_EXP_RETURN(!CompVGLUtils::isGLContextSet(), COMPV_ERROR_CODE_E_GL_NO_CONTEXT);

	GLfloat uMax = static_cast<GLfloat>(width) / static_cast<GLfloat>(stride);
	GLfloat vMax = 1.f;
	if (bToScreen) {
		COMPV_CHECK_EXP_RETURN(sizeof(m_Vertices) != sizeof(kCompVGLScreenVertices), COMPV_ERROR_CODE_E_SYSTEM);
		memcpy(&(*Vertices)[0], kCompVGLScreenVertices, sizeof(kCompVGLScreenVertices));
		(*Vertices)[0].TexCoord[0] = uMax, (*Vertices)[0].TexCoord[1] = 0.f;
		(*Vertices)[1].TexCoord[0] = uMax, (*Vertices)[0].TexCoord[1] = vMax;
		(*Vertices)[2].TexCoord[0] = 0.f, (*Vertices)[0].TexCoord[1] = vMax;
		(*Vertices)[3].TexCoord[0] = 0.f, (*Vertices)[0].TexCoord[1] = 0.f;
	}
	else {
		COMPV_CHECK_EXP_RETURN(sizeof(m_Vertices) != sizeof(kCompVGLTexture2DVertices), COMPV_ERROR_CODE_E_SYSTEM);
		memcpy(&(*Vertices)[0], kCompVGLTexture2DVertices, sizeof(kCompVGLTexture2DVertices));
		(*Vertices)[0].TexCoord[0] = uMax, (*Vertices)[0].TexCoord[1] = vMax;
		(*Vertices)[1].TexCoord[0] = uMax, (*Vertices)[0].TexCoord[1] = 0.f;
		(*Vertices)[2].TexCoord[0] = 0.f, (*Vertices)[0].TexCoord[1] = 0.f;
		(*Vertices)[3].TexCoord[0] = 0.f, (*Vertices)[0].TexCoord[1] = vMax;
	}
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_NAMESPACE_END()

#endif /* defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) */