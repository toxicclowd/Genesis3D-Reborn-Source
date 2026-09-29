/*!
	@file grVertexBuffer.h
	@author Anthony Rufrano (paradoxnj)
	@brief A vertex buffer
*/
#ifndef GR_VERTEXBUFFER_H
#define GR_VERTEXBUFFER_H

#include "BaseType.h"
#include "grMaterial.h"

#ifdef __cplusplus

class grVertexBuffer : virtual public grUnknown
{
protected:
	virtual ~grVertexBuffer()						{}

public:
	/*!
		@fn int16 grVertexBuffer::AddVertices(grTLVertex *Pnts, int32 NumPoints)
		@brief Adds vertices to the vertex buffer
		@param[in] Pnts Vertices to add
		@param[in] NumPoints The number of vertices to add
		@return The start index in the array
	*/
	virtual int16					AddVertices(grTLVertex *Pnts, int32 NumPoints) = 0;

	/*!
		@fn grBoolean grVertexBuffer::SetMaterial(grMaterial *Material)
		@brief Sets the material
		@param[in] Material The material to use
		@return GR_TRUE on success, GR_FALSE on failure
	*/
	virtual grBoolean				SetMaterial(grMaterial *Material) = 0;

	/*!
		@fn void grVertexBuffer::ClearBuffer()
		@brief Empties the vertex buffer
	*/
	virtual void					ClearBuffer() = 0;
};

typedef grVertexBuffer grVertexBuffer;

#else

typedef struct grVertexBuffer grVertexBuffer;

#endif // __cplusplus

#endif // GR_VERTEXBUFFER_H