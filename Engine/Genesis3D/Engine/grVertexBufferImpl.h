/*!
	@file grVertexBufferImpl.h
	@author Anthony Rufrano (paradoxnj)
	@brief The vertex buffer implementation
*/
#ifndef GR_VERTEXBUFFER_IMPL_H
#define GR_VERTEXBUFFER_IMPL_H

#include <vector>
#include "grVertexBuffer.h"

class grVertexBufferImpl : public grVertexBuffer
{
public:
	grVertexBufferImpl();
	virtual ~grVertexBufferImpl();

private:
	std::vector<grTLVertex>					m_Buffer;
	grMaterial								*m_pMaterial;
	uint32									m_RefCount;

public:
	uint32									AddRef();
	uint32									Release();

	int16									AddVertices(grTLVertex *Pnts, int32 NumPoints);
	grBoolean								SetMaterial(grMaterial *Material);
	void									ClearBuffer();
};

#endif
