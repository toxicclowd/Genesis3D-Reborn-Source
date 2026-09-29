/****************************************************************************************/
/*  JENAMEMGR.C                                                                         */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Jet3D Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.jet3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Jet3D, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
#include <memory.h>
#include <assert.h>
#include <stdio.h>

#include "grChain.h"
#include "Ram.h"
#include "VFile.h"

#ifdef NEWSAVE

typedef struct grNameMgr
{
	int32					RefCount;
	grVFile					*System;
	grVFile                 *Dir;
    int32                   Flags;

    grChain                 *List;
} grNameMgr;

#define GR_NAME_MGR_NAME_SIZE 8 // without null terminator

typedef struct ReadWriteData
{
    char                                PointerText[GR_NAME_MGR_NAME_SIZE]; // no null terminator
    void                                *DataPtr;
    grNameMgr_WriteToFileCallback       WriteToFile;
	 grBoolean							Flushed;
} ReadWriteData;

///////////////////////////////////////
// Local fucntions
///////////////////////////////////////

grBoolean SaveCallbackData(grNameMgr *NM,
                           char *PointerText,
                           void *DataPtr,
                           grNameMgr_WriteToFileCallback CB_Write)
    {
    ReadWriteData *RWData;

	assert(NM);
	assert(PointerText);
	assert(DataPtr);

    RWData = (ReadWriteData *)grRam_Allocate(sizeof(ReadWriteData));

	if (!RWData)
		return GR_FALSE;

    memcpy(RWData->PointerText, PointerText, GR_NAME_MGR_NAME_SIZE);
    RWData->WriteToFile = CB_Write;
    RWData->DataPtr = DataPtr;
	RWData->Flushed = GR_FALSE;

    if (!grChain_AddLinkData(NM->List, RWData))
		{
		grRam_Free(RWData);
		return GR_FALSE;
		}

	return GR_TRUE;
    }

ReadWriteData *FindCallbackData(grNameMgr *NM, char *PointerText)
{
    grChain_Link *Link;

	assert(NM);
	assert(PointerText);

    // get rid of data from the list
	for (Link = grChain_GetFirstLink(NM->List); Link; Link = grChain_LinkGetNext(Link))
	{
		// locals
		ReadWriteData	*RWData;

		// get data pointer
		RWData = (ReadWriteData *)grChain_LinkGetLinkData( Link );

        if (memcmp(RWData->PointerText, PointerText, GR_NAME_MGR_NAME_SIZE) == 0)
            {
            return RWData;
            }
	}

    return NULL;
}

ReadWriteData *FindCallbackDataByPtr(grNameMgr *NM, void *DataPtr)
{
    grChain_Link *Link;

	assert(NM);
	assert(DataPtr);

    // get rid of data from the list
	for (Link = grChain_GetFirstLink(NM->List); Link; Link = grChain_LinkGetNext(Link))
	{
		// locals
		ReadWriteData	*RWData;

		// get data pointer
		RWData = (ReadWriteData *)grChain_LinkGetLinkData( Link );

        if (RWData->DataPtr == DataPtr)
            {
            return RWData;
            }
	}

    return NULL;
}

grBoolean PtrToText(void *Ptr, char *TextBuff)
    {
	int32 count;

	assert(Ptr);
	assert(TextBuff);

    count = sprintf(TextBuff,"%08x",Ptr);

    if (count > GR_NAME_MGR_NAME_SIZE || count < GR_NAME_MGR_NAME_SIZE)
		return GR_FALSE;

    return GR_TRUE;
    }

///////////////////////////////////////
// Public functions
///////////////////////////////////////

GRAPI grNameMgr * GRCC grNameMgr_Create(grVFile *System, int32 CreateFlags)
{
	grNameMgr		*NM;

	assert (System);
    assert (CreateFlags == GR_NAME_MGR_CREATE_FOR_WRITE || CreateFlags == GR_NAME_MGR_CREATE_FOR_READ);

	NM = GR_RAM_ALLOCATE_STRUCT(grNameMgr);

	if (!NM)
		return NULL;

	memset(NM, 0, sizeof(*NM));

	NM->RefCount = 1;
    NM->Flags = CreateFlags;

    NM->List = grChain_Create();

	if (!NM->List)
		{
		grRam_Free(NM);
		return NULL;
		}

	NM->System = System;

	return NM;
}

GRAPI grBoolean GRCC grNameMgr_CreateRef(grNameMgr *NameMgr)
{
	assert(NameMgr);

	NameMgr->RefCount++;

	return GR_TRUE;
}

GRAPI void GRCC grNameMgr_Destroy(grNameMgr **NameMgr)
{
    grChain_Link *Link;

	assert(NameMgr);

	(*NameMgr)->RefCount --;

	if ((*NameMgr)->RefCount == 0)
	{
        // close open name manager directory
        if ((*NameMgr)->Dir)
            {
		    grVFile_Close((*NameMgr)->Dir);
            }

        // get rid of data from the list
		for (Link = grChain_GetFirstLink((*NameMgr)->List); Link; Link = grChain_LinkGetNext(Link))
		{
			// locals
			ReadWriteData	*RWData;

			// get data pointer
			RWData = (ReadWriteData *)grChain_LinkGetLinkData( Link );
            grRam_Free(RWData);
		}

        // destroy the list
        grChain_Destroy(&(*NameMgr)->List);

		grRam_Free(*NameMgr);
	}

	*NameMgr = NULL;
}

GRAPI grBoolean GRCC grNameMgr_Write(grNameMgr *NM, grVFile *VFile, void *PtrToData, grNameMgr_WriteToFileCallback CB_Write)
	{
	void *Data;
    char NameString[GR_NAME_MGR_NAME_SIZE+1];

	assert(NM);
	assert(VFile);
	assert(PtrToData);
	assert(CB_Write);
    assert(NM->Flags == GR_NAME_MGR_CREATE_FOR_WRITE);

    if (!PtrToText(PtrToData, NameString))
		return GR_FALSE;

	grVFile_Write(VFile, NameString, GR_NAME_MGR_NAME_SIZE);

    Data = FindCallbackDataByPtr(NM, PtrToData);
    if (!Data)
        {
	    SaveCallbackData(NM, NameString, PtrToData, CB_Write);
        }

	return GR_TRUE;
	}

GRAPI grBoolean GRCC grNameMgr_Read(grNameMgr *NM, grVFile *VFile, grNameMgr_CreateFromFileCallback CB_Read, void **ReturnPointer)
	{
	ReadWriteData *Data;
	char NameString[GR_NAME_MGR_NAME_SIZE+1];
	grVFile *File = NULL;

	assert(NM);
	assert(VFile);
	assert(ReturnPointer);
	assert(CB_Read);

    assert(NM->Flags == GR_NAME_MGR_CREATE_FOR_READ);

	grVFile_Read(VFile, NameString, GR_NAME_MGR_NAME_SIZE);
    NameString[GR_NAME_MGR_NAME_SIZE] = '\0';

    Data = FindCallbackData(NM, NameString);
	if (Data && Data->DataPtr)
		{
        *ReturnPointer = Data->DataPtr;
		return GR_TRUE;
		}
	else
		{
		void *DataPtr;

        if (!NM->Dir)
            {
	        NM->Dir = grVFile_Open(NM->System, "NameMgr", GR_VFILE_OPEN_READONLY|GR_VFILE_OPEN_DIRECTORY);
            }

		File = grVFile_Open(NM->Dir, NameString, GR_VFILE_OPEN_READONLY);

		if (!File)
			goto Error;

		DataPtr = CB_Read(File, NM);

		if (!DataPtr)
			goto Error;

		grVFile_Close(File);
		File = NULL;

		*ReturnPointer = DataPtr;

	    if (!SaveCallbackData(NM, NameString, DataPtr, NULL))
			goto Error;

		return GR_TRUE;
		}

	return GR_TRUE;

	Error:

	if (File)
		grVFile_Close(File);

	return GR_FALSE;
	}

GRAPI grBoolean GRCC grNameMgr_WriteFlush(grNameMgr *NM)
	{
	grVFile *File = NULL;
	grChain_Link *Link;//, *Next;
	char NameString[GR_NAME_MGR_NAME_SIZE+1];
	ReadWriteData *RWData;
	int Count, i, ret;

	assert(NM);
    assert(NM->Flags == GR_NAME_MGR_CREATE_FOR_WRITE);
	assert(NM->System);

	assert(NM->Dir == NULL);

	NM->Dir = grVFile_Open(NM->System, "NameMgr", GR_VFILE_OPEN_CREATE | GR_VFILE_OPEN_DIRECTORY);

	if (!NM->Dir)
		goto Error;

	assert(NM->List);

	Count = grChain_GetLinkCount(NM->List);
	for (i = 0; i < Count; i++)
		{
		Link = grChain_GetLinkByIndex(NM->List, i);
   		// get data pointer
		RWData = (ReadWriteData *)grChain_LinkGetLinkData( Link );

		if (RWData->Flushed)
			continue;

        memcpy(NameString, RWData->PointerText, GR_NAME_MGR_NAME_SIZE);
        NameString[GR_NAME_MGR_NAME_SIZE] = '\0';

		File = grVFile_Open(NM->Dir, NameString, GR_VFILE_OPEN_CREATE);

		if (!File)
			goto Error;

		ret = RWData->WriteToFile(RWData->DataPtr, File, NM);
		Count = grChain_GetLinkCount(NM->List); // get a new count

		if (!ret)
			goto Error;

		grVFile_Close(File);
		File = NULL;

		RWData->Flushed = GR_TRUE;
		}

	grVFile_Close(NM->Dir);
	NM->Dir = NULL;

	return GR_TRUE;

	Error:

	if (File)
		grVFile_Close(File);

	if (NM->Dir)
		grVFile_Close(NM->Dir);

	return GR_FALSE;
}

#endif

#if 0
//	Example read code
grActor *grActor_CreateFromFile(grVFile *VFile)
{
	// Create a new actor
	Actor = GR_RAM_ALLOCATE_STRUCT(grActor);

	if (!Actor)
		return NULL;
	
	if (!grVFile_Read(VFile, &Actor->Number, sizeof(Actor->Number))
		goto ExitWithError;

	return Actor;

	ExitWithError:
	{
		if (Actor)
			grRam_Free(Actor);

		return NULL;
	}
}

//	Example write code
grBoolean grActor_WriteToFile(const grActor *Actor, grVFile *VFile)
{
	uint32		Count;

	if (!grVFile_Write(VFile, &Actor->Number, sizeof(Actor->Number))
		return GR_FALSE:

	return GR_TRUE;
}


grBoolean grWorld_Load(World,NameMgr,VFile)
{
	read ..  ..

	vfile_read(&NumActors);
	for (i=0;NumActors; i++)
		if (!grNameMgr_Read(NameMgr, VFile, grActor_ReadFromFile, &(Actor[i])))
			{
				log a failed load;
			}

	read .. . . .

}

grBoolean grWorld_Save(World,NameMgr,VFile)
{
	grNameMgr_Create();

	write .. .. .

	vfile_write(NumActors);
	for (i=0;NumActors; i++)
		if (!grNameMgr_Write(NameMgr, VFile, grActor_WriteFromFile, &(Actor[i])))
			{
				ErrorLog;
			}

	write  .. . ..

	grNameMgr_Flush(NameMgr);
	grNameMgr_Destroy(NameMgr);
}


grNameMgr_Write(NameMgr, VFile, Name, Obj, CallbackWrite);
grNameMgr_Read(NameMgr, VFile, CallbackRead, ReturnPointer);



// search for CreateFromFile		
actor.h(106):GRAPI grActor_Def *GRCC grActor_DefCreateFromFile(grVFile *pFile);
array.h(51):extern grArray *		grArray_CreateFromFile(grVFile * File,grArray_IOFunc ElementReader,void *ReaderContext);
bitmap.h(44):GRAPI grBitmap *	GRCC	grBitmap_CreateFromFile( grVFile *F );
bitmap.h(45):GRAPI grBitmap *	GRCC	grBitmap_CreateFromFileName(const grVFile *BaseFS,const char *Name);
bitmap.h(243):GRAPI grBitmap_Palette *	GRCC	grBitmap_Palette_CreateFromFile(grVFile *F);
body.h(133):GRAPI grBody  *GRCC  grBody_CreateFromFile(grVFile *pFile);
grBrush.h(64):GRAPI grBrush		*grBrush_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMGr);
grChain.h(39):grChain		*grChain_CreateFromFile(grVFile *VFile, grChain_IOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr);
grFaceInfo.h(73):GRAPI grFaceInfo_Array *grFaceInfo_ArrayCreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grGArray.h(47):grGArray	*grGArray_CreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grIndexPoly.h(41):grIndexPoly *grIndexPoly_CreateFromFile(grVFile *VFile);
grLight.h(31):GRAPI grLight		*grLight_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
grMaterial.h(47):GRAPI grMaterial_Array		*grMaterial_ArrayCreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
grPtrMgr.h(47):grActor *grActor_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
grVertArray.h(39):GRAPI grVertArray		*grVertArray_CreateFromFile(grVFile *VFile);
grModel.h(35):GRAPI grModel		*grModel_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
grWorld.h(71):GRAPI grWorld	*	grWorld_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr, grResourceMgr * pResourceMgr );
motion.h(150):GRAPI grMotion *GRCC grMotion_CreateFromFile(grVFile *f);
object.h(99):GRAPI grObject *	GRCC grObject_CreateFromFile(grVFile * File, grObjectIO *ObjIO);
object.h(186):	void *		( GRCC * CreateFromFile)(grVFile * File, grObjectIO *ObjIO);
path.h(128):GRAPI grPath* GRCC grPath_CreateFromFile(grVFile *F);
strblock.h(28):GRAPI grStrBlock* GRCC grStrBlock_CreateFromFile(grVFile* pFile);
terrain.h(51):GRAPI grTerrain *	GRCC grTerrain_CreateFromFile(grVFile * File, grPtrMgr *PtrMgr);

// search for WriteToFile
array.h(52):extern grBoolean		grArray_WriteToFile(const grArray * Array,grVFile * File,grArray_IOFunc ElementWriter,void *WriterContext);
bitmap.h(46):GRAPI grBoolean 	GRCC	grBitmap_WriteToFile( const grBitmap *Bmp, grVFile *F );
bitmap.h(47):GRAPI grBoolean	GRCC	grBitmap_WriteToFileName(const grBitmap * Bmp,const grVFile *BaseFS,const char *Name);
bitmap.h(258):GRAPI grBoolean		GRCC	grBitmap_Palette_WriteToFile(const grBitmap_Palette *Palette,grVFile *F);
body.h(132):GRAPI grBoolean GRCC grBody_WriteToFile(const grBody *B, grVFile *pFile);
grBrush.h(66):GRAPI grBoolean	grBrush_WriteToFile(const grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMGr);
grChain.h(40):grBoolean	grChain_WriteToFile(const grChain *Chain, grVFile *VFile, grChain_IOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr);
grGArray.h(48):grBoolean	grGArray_WriteToFile(const grGArray *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grIndexPoly.h(42):grBoolean	grIndexPoly_WriteToFile(const grIndexPoly *Poly, grVFile *VFile);
grLight.h(33):GRAPI grBoolean	grLight_WriteToFile(const grLight *Light, grVFile *VFile, grPtrMgr *PtrMgr);
grPtrMgr.h(91):grBoolean grActor_WriteToFile(const grActor *Actor, grVFile *VFile, grPtrMgr *PtrMgr)
grVertArray.h(40):GRAPI grBoolean		grVertArray_WriteToFile(const grVertArray *Array, grVFile *VFile);
grModel.h(36):GRAPI grBoolean	grModel_WriteToFile(const grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr);
grWorld.h(72):GRAPI grBoolean	grWorld_WriteToFile(const grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr);
motion.h(151):GRAPI grBoolean GRCC grMotion_WriteToFile(const grMotion *M,grVFile *pFile);
object.h(100):GRAPI grBoolean	GRCC grObject_WriteToFile(const grObject * Object,grVFile * File, grObjectIO *ObjIO);
path.h(131):GRAPI grBoolean GRCC grPath_WriteToFile(const grPath *P, grVFile *F);
strblock.h(29):GRAPI grBoolean GRCC grStrBlock_WriteToFile(const grStrBlock *SB,grVFile *pFile);
terrain.h(55):GRAPI grBoolean 	GRCC grTerrain_WriteToFile(const grTerrain *pTerrain,grVFile * File, grPtrMgr *PtrMgr);


// search for grPtrMgr
grBrush.h(64):GRAPI grBrush		*grBrush_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMGr);
grBrush.h(66):GRAPI grBoolean	grBrush_WriteToFile(const grBrush *Brush, grVFile *VFile, grPtrMgr *PtrMGr);
grChain.h(39):grChain		*grChain_CreateFromFile(grVFile *VFile, grChain_IOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr);
grChain.h(40):grBoolean	grChain_WriteToFile(const grChain *Chain, grVFile *VFile, grChain_IOFunc *IOFunc, void *Context, grPtrMgr *PtrMgr);
grFaceInfo.h(73):GRAPI grFaceInfo_Array *grFaceInfo_ArrayCreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grFaceInfo.h(74):GRAPI grBoolean	grFaceInfo_ArrayWriteToFile(const grFaceInfo_Array *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grGArray.h(47):grGArray	*grGArray_CreateFromFile(grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grGArray.h(48):grBoolean	grGArray_WriteToFile(const grGArray *Array, grVFile *VFile, grGArray_IOFunc *IOFunc, void *IOFuncContext, grPtrMgr *PtrMgr);
grLight.h(31):GRAPI grLight		*grLight_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
grLight.h(33):GRAPI grBoolean	grLight_WriteToFile(const grLight *Light, grVFile *VFile, grPtrMgr *PtrMgr);
grMaterial.h(47):GRAPI grMaterial_Array		*grMaterial_ArrayCreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
grMaterial.h(48):GRAPI grBoolean			grMaterial_ArrayWriteToFile(grMaterial_Array *MatArray, grVFile *VFile, grPtrMgr *PtrMgr);
grModel.h(35):GRAPI grModel		*grModel_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr);
grModel.h(36):GRAPI grBoolean	grModel_WriteToFile(const grModel *Model, grVFile *VFile, grPtrMgr *PtrMgr);
grWorld.h(71):GRAPI grWorld	*	grWorld_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr, grResourceMgr * pResourceMgr );
grWorld.h(72):GRAPI grBoolean	grWorld_WriteToFile(const grWorld *World, grVFile *VFile, grPtrMgr *PtrMgr);
grWorld.h(154):GRAPI grObjectIO *grWorld_CreateObjectIO(grWorld *pWorld, grPtrMgr *PtrMgr);
grWorld.h(157):GRAPI grPtrMgr *grWorld_GetObjectIOPtrMgr(grObjectIO *ObjIO);
terrain.h(51):GRAPI grTerrain *	GRCC grTerrain_CreateFromFile(grVFile * File, grPtrMgr *PtrMgr);
terrain.h(55):GRAPI grBoolean 	GRCC grTerrain_WriteToFile(const grTerrain *pTerrain,grVFile * File, grPtrMgr *PtrMgr);

Brush.h(230):Brush *				Brush_CreateFromFile( grVFile * pF, const int32 nVersion, grPtrMgr * pPtrMgr ) ;
BrushList.h(49):BrushList *			BrushList_CreateFromFile( grVFile * pF, grPtrMgr * pPtrMgr ) ;
CamObj.h(64):Camera *		Camera_CreateFromFile( grVFile * pF, grWorld *pWorld, grPtrMgr *PtrMgr );
CamObj.h(65):grBoolean		Camera_WriteToFile( Camera * pCamera, grVFile * pF, grWorld *pWorld, grPtrMgr *PtrMgr );
CameraList.h(43):CameraList *			CameraList_CreateFromFile( grVFile * pF, grWorld *pWorld, grPtrMgr *pPtrMgr  ) ;
CameraList.h(44):grBoolean			CameraList_WriteToFile( CameraList * pList, grVFile * pF, grWorld *pWorld, grPtrMgr *pPtrMgr ) ;
Level.h(170):Level *				Level_CreateFromFile( grVFile * pF, grWorld * pWorld, MaterialList_Struct * pGlobalMaterials, grPtrMgr * pPtrMgr, float Version ) ;
Level.h(171):grBoolean			Level_WriteToFile( Level * pLevel, grVFile * pF, grPtrMgr * pPtrMgr ) ;
Light.h(91):Light * Light_CreateFromFile( grVFile * pF, grWorld * pWorld, grPtrMgr * pPtrMgr );
Light.h(92):grBoolean Light_WriteToFile( Light * pLight, grVFile * pF, grPtrMgr * pPtrMgr );
LightList.h(43):LightList *			LightList_CreateFromFile( grVFile * pF, grWorld  * pWorld, grPtrMgr * pPtrMgr ) ;
LightList.h(44):grBoolean			LightList_WriteToFile( LightList * pList, grVFile * pF, grPtrMgr * pPtrMgr ) ;
UserObj.h(53):UserObj * UserObj_CreateFromFile( grVFile * pF, grWorld *pWorld, grPtrMgr * pPtrMgr );
UserObj.h(54):grBoolean UserObj_WriteToFile( UserObj * pUserObj, grVFile * pF, grWorld *pWorld, grPtrMgr * pPtrMgr );
model.h(72):Model *			Model_CreateFromFile( grVFile * pF, const int32 nVersion, grWorld *pWorld, grPtrMgr * pPtrMgr ) ;
modellist.h(45):ModelList *		ModelList_CreateFromFile( grVFile * pF, grWorld *pWorld, grPtrMgr * pPtrMgr ) ;
modellist.h(46):grBoolean		ModelList_WriteToFile( ModelList * pList, grVFile * pF, grWorld *pWorld, grPtrMgr * pPtrMgr ) ;

#endif