/****************************************************************************************/
/*  SYMBOL.C                                                                            */
/*                                                                                      */
/*  Author: Eli Boling                                                                  */
/*  Description: Generic symbol table/property list implementation                      */
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
#include	<stdlib.h>
#include	<limits.h>
#include	<assert.h>
#include	<string.h>

#include	"vec3d.h"
#include	"getypes.h"
#include	"ram.h"

#include	"RefPool.h"
#include	"GCHeap.h"
#include	"Symbol.h"

//grSymbol_Table *	TempST;

// Note:  You must not have more than 64k of hash buckets, as our hash value is 16bits
#define	NUMHASHBUCKETS	223
//#define	NUMHASHBUCKETS	5

typedef	struct	grSymbol_Rec		grSymbol_Rec;
typedef	struct	grSymbol_ListRec	grSymbol_ListRec;

typedef	struct	PropList
{
	grSymbol_Rec *	Symbol;
	union
	{
		int					Integer;
		char *				String;
		float				Float;
		grVec3d				Vec3d;
		GR_RGBA				Color;
		grBoolean			Boolean;
		void *				Void;
		grSymbol_ListRec *	List;
		grSymbol_Rec *		Sym;		// Sym, not Symbol to reduce typo risks with Symbol, above
/*
		grModel *		Model;
		grPortal *		Portal;
*/
	}	Value;
	struct PropList *	Next;
}	PropList;

typedef	struct	grSymbol_Table
{
	grSymbol_List **	Symbols;
	grSymbol *			QualifierListProperty;
	int					RefCount;
	RefPool *			SymbolReferences;
	RefPool *			ListReferences;
	GCHeap *			SymbolHeap;
	GCHeap *			ListHeap;
	int					GCAbleOperationCount;
}	grSymbol_Table;

#define	SYMREF_INCREMENT	100
#define	LISTREF_INCREMENT	20

#define	GCABLE_OP_COUNT_THRESHHOLD	20

#define	SYMBOL_REMOVED			0x01
#define	SYMBOL_WRITTEN			0x02
#define	SYMBOL_WRITING			0x04
#define	SYMBOL_BEINGDESTROYED	0x08
#define	SYMBOL_MARKED			0x10

typedef	struct			grSymbol_Rec
{
	char *				Name;
	int					NameLength;
	unsigned short		HashValue;
	grSymbol_Type		Type;
	int					EnumValue;
	PropList *			Properties;
	grSymbol_Rec *		Qualifier;
//	int					RefCount;			// RefCount and SymbolId could be unioned
	unsigned int		SymbolId;			// to save space
	grSymbol_Table *	SymbolTable;
	unsigned char		Flags;
}	grSymbol_Rec;

typedef	struct	grSymbol
{
	grSymbol_Rec *		Symbol;
}	grSymbol;

typedef	struct	SymListElt
{
	grSymbol_Rec *		Symbol;
	struct SymListElt *	Next;
}	SymListElt;

typedef struct	grSymbol_ListRec
{
	SymListElt *		Elts;
	int					CurrentIndex;
	SymListElt *		Current;
	grSymbol_Table *	SymbolTable;
//	int				RefCount;
}	grSymbol_ListRec;

typedef	struct	grSymbol_List
{
	grSymbol_ListRec *		List;
}	grSymbol_List;

typedef	struct	Symbol_Array
{
	int				ElementCount;
	int				CurrentIndex;
	grSymbol_Rec **	Symbols;
}	Symbol_Array;

#define	QUALLIST_PROPERTY	"*QualifierListProperty*"

static	void FinalizeSymbol(void *P);
static	void FinalizeList(void *P);

static	void grSymbol_ListDestroyNoRef(grSymbol_List **pSymList);

static	unsigned short	HashValues[256];
static	int				HashInitialized = 0;

#pragma message ("Name hashing doesn't support wide char")
static	void			HashInit(void)
{
	int	i;

	if	(HashInitialized)
		return;

	HashInitialized = 1;

	for	(i = 0; i < 256; i++)
		HashValues[i] = (unsigned short)rand();
}

static	unsigned short	Hash(const char *s, int Count)
{
	unsigned short	Value;

	HashInit();

	Value = 0;
	while	(Count--)
	{
		Value += HashValues[*s];
		s++;
	}

	return Value;
}

static	grSymbol *	ReferenceSymbol(grSymbol_Table *ST, grSymbol_Rec *Symbol)
{
	grSymbol *	HSymbol;
	
	assert(ST);
	assert(Symbol);

	HSymbol = (grSymbol *)RefPool_RefCreate(ST->SymbolReferences);
	if	(HSymbol)
		HSymbol->Symbol = Symbol;

	return HSymbol;
}

static	grSymbol_List *	ReferenceList(grSymbol_Table *ST, grSymbol_ListRec *List)
{
	grSymbol_List *	HList;
	
	assert(ST);
	assert(List);

	HList = (grSymbol_List *)RefPool_RefCreate(ST->ListReferences);
	if	(HList)
		HList->List = List;

	return HList;
}

static	Symbol_Array *Symbol_ArrayCreate(int ElementCount)
{
	Symbol_Array *	SymArray;

	SymArray = grRam_Allocate(sizeof(*SymArray));
	if	(!SymArray)
		return SymArray;

	SymArray->Symbols = grRam_Allocate(sizeof(*SymArray->Symbols) * ElementCount);
	if	(!SymArray->Symbols)
	{
		grRam_Free(SymArray);
		return NULL;
	}
	memset(SymArray->Symbols, 0, sizeof(*SymArray->Symbols) * ElementCount);
	SymArray->ElementCount = ElementCount;
	SymArray->CurrentIndex = 0;

	return SymArray;
}

static	void	Symbol_ArrayDestroy(Symbol_Array **pSymArray)
{
	Symbol_Array *	SymArray;

	SymArray = *pSymArray;

	assert(pSymArray);
	assert(SymArray);

	grRam_Free(SymArray->Symbols);
	grRam_Free(SymArray);
	*pSymArray = NULL;
}

static	void	Symbol_ArrayAddSymbol(Symbol_Array *SymArray, grSymbol_Rec *Symbol)
{
	assert(SymArray);
	assert(Symbol);
	assert(SymArray->ElementCount > SymArray->CurrentIndex);

	SymArray->Symbols[SymArray->CurrentIndex++] = Symbol;
}

static	grSymbol_Rec *	Symbol_ArrayGetSymbol(Symbol_Array *SymArray, int Index)
{
	grSymbol_Rec *	Symbol;

	assert(SymArray);
	assert(Index < SymArray->CurrentIndex);

	Symbol = SymArray->Symbols[Index];
	assert(Symbol);
	return Symbol;
}

static	grBoolean	grSymbol_IsValid(const grSymbol_Rec *Sym)
{
	if	(!Sym)
		return GR_FALSE;

//	if	(Sym->RefCount == 0)
//		return GR_FALSE;

	if	(!Sym->Name)
		return GR_FALSE;

	return GR_TRUE;
}

grBoolean	grSymbol_TableIsValid(const grSymbol_Table *ST)
{
	int	i;

	if	(!ST)
		return GR_FALSE;

	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		SymListElt *	Elts;

		Elts = ST->Symbols[i]->List->Elts;
		while	(Elts)
		{
			if	(!grSymbol_IsValid(Elts->Symbol))
				return GR_FALSE;
			Elts = Elts->Next;
		}
	}

	return GR_TRUE;
}

GRAPI	grSymbol_Table * GRCC grSymbol_TableCreate(void)
{
	grSymbol_Table *	ST;
	int					i;

	ST = grRam_Allocate(sizeof(*ST));
	if	(!ST)
		return ST;

	ST->Symbols = grRam_Allocate(sizeof(*ST->Symbols) * NUMHASHBUCKETS);
	if	(!ST->Symbols)
	{
		grRam_Free(ST);
		return NULL;
	}

	ST->RefCount = 1;
	ST->GCAbleOperationCount = 0;

	ST->SymbolReferences = RefPool_Create(SYMREF_INCREMENT);
	ST->ListReferences = RefPool_Create(LISTREF_INCREMENT);
	if	(!ST->SymbolReferences || !ST->ListReferences)
	{
		grSymbol_TableDestroy(&ST);
		return NULL;
	}

	ST->SymbolHeap = GCHeap_Create(sizeof(grSymbol_Rec), 100, FinalizeSymbol);
	ST->ListHeap = GCHeap_Create(sizeof(grSymbol_ListRec), 100, FinalizeList);
	if	(!ST->SymbolHeap || !ST->ListHeap)
	{
		grSymbol_TableDestroy(&ST);
		return NULL;
	}

	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		ST->Symbols[i] = grSymbol_ListCreate(ST);
		if	(!ST->Symbols[i])
		{
			grSymbol_TableDestroy(&ST);
			return NULL;
		}
	}

	ST->QualifierListProperty = grSymbol_Create(ST, NULL, QUALLIST_PROPERTY, GR_SYMBOL_TYPE_LIST);
	if	(!ST->QualifierListProperty)
	{
		grSymbol_TableDestroy(&ST);
		return NULL;
	}

	return ST;
}

//GRAPI	const grSymbol_List *GRCC grSymbol_TableGetQualifiedSymbolList(
GRAPI	 grSymbol_List *GRCC grSymbol_TableGetQualifiedSymbolList(
	const grSymbol_Table *	ST,
	const grSymbol *		HQualifier)
{
	grSymbol_List *	List;
	int				i;
	SymListElt *	Elts;

	assert(ST);
	List = grSymbol_ListCreate((grSymbol_Table *)ST);
	if	(!List)
		return List;
	
	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		Elts = ST->Symbols[i]->List->Elts;
		while	(Elts)
		{
			if	((!HQualifier && !Elts->Symbol->Qualifier) || (Elts->Symbol->Qualifier == HQualifier->Symbol))
			{
				grSymbol *	HSymbol;
				HSymbol = ReferenceSymbol((grSymbol_Table *)ST, Elts->Symbol);
				if	(!HSymbol)
				{
					grSymbol_ListDestroy(&List);
					return NULL;
				}
				
				if	(grSymbol_ListAddSymbol(List, HSymbol) == GR_FALSE)
				{
					grSymbol_Destroy(&HSymbol);
					grSymbol_ListDestroy(&List);
					return NULL;
				}
				grSymbol_Destroy(&HSymbol);
			}
			Elts = Elts->Next;
		}
	}
	return List;
}

#if 0
GRAPI	void GRCC grSymbol_TableCreateRef(grSymbol_Table *ST)
{
	assert(ST);
	assert(ST->RefCount > 0);
	ST->RefCount++;
}
#endif

#define	ST_SIGNATURE		0x30305453	/* ST00 */
#define	ST_SIGNATURE_END	0x31305453	/* ST01 */

static	grBoolean WriteUInt(grVFile *File, unsigned int Value)
{
	unsigned char	Buff[5];
	char *			pBuff;

	assert(File);

	pBuff = &Buff[0];
	while	(Value > 0x7f)
	{
		*pBuff++ = 0x80 | (Value & 0x7f);
		Value = Value >> 7;
	}
	assert(pBuff - Buff < 5);
	*pBuff++ = Value;
	return grVFile_Write(File, Buff, pBuff - Buff);
}

static	grBoolean ReadUInt(grVFile *File, unsigned int *Value)
{
	unsigned char	C;
	int				Shift;

	assert(File);

	*Value = 0;
	Shift = 0;
	do
	{
		if	(grVFile_Read(File, &C, 1) == GR_FALSE)
			return GR_FALSE;
		*Value = *Value | (((unsigned int)(C & ~0x80)) << Shift);
		Shift += 7;
	}	while	(C & 0x80);
	return GR_TRUE;
}

static	grBoolean WriteString(grVFile *File, const char *String)
{
	int	Length;

	assert(File);
	assert(String);

	Length = strlen(String);
	assert(Length < 0x8000000);
	if	(WriteUInt(File, Length) == GR_FALSE)
		return GR_FALSE;
	return grVFile_Write(File, String, Length);
}

static	grBoolean WriteFloat(grVFile *File, grFloat Float)
{
	return grVFile_Write(File, &Float, sizeof(Float));
}

static	grBoolean ReadFloat(grVFile *File, grFloat *Float)
{
	return grVFile_Read(File, Float, sizeof(*Float));
}

static	char *	ReadString(grVFile *File)
{
	int		Length;
	char *	String;

	assert(File);

	if	(ReadUInt(File, &Length) == GR_FALSE)
		return NULL;
	String = grRam_Allocate(Length + 1);
	if	(!String)
		return NULL;
	if	(grVFile_Read(File, String, Length) == GR_FALSE)
	{
		grRam_Free(String);
		return NULL;
	}
	String[Length] = '\0';
	return String;
}

static	grBoolean WriteSymbol(grSymbol_Rec *Symbol, grVFile *File, unsigned int *SymbolId)
{
	unsigned char	ReferenceWritten;

	assert(Symbol);
	assert(File);

	assert(!(Symbol->Flags & SYMBOL_WRITING));

	if	(Symbol->Flags & SYMBOL_WRITTEN)
	{
	
		ReferenceWritten = 0xff;
		if	(grVFile_Write(File, &ReferenceWritten, sizeof(ReferenceWritten)) == GR_FALSE)
			return GR_FALSE;
		if	(WriteUInt(File, Symbol->SymbolId) == GR_FALSE)
			return GR_FALSE;
	
		return GR_TRUE;
	}
	else
	{
		unsigned char	QualifierPresent;

		assert(SymbolId);

		Symbol->Flags |= SYMBOL_WRITING;

		ReferenceWritten = 0;
		if	(grVFile_Write(File, &ReferenceWritten, sizeof(ReferenceWritten)) == GR_FALSE)
			return GR_FALSE;

		if	(Symbol->Qualifier)
		{
			QualifierPresent = 0xff;
			if	(grVFile_Write(File, &QualifierPresent, sizeof(QualifierPresent)) == GR_FALSE)
				return GR_FALSE;
			if	(WriteSymbol(Symbol->Qualifier, File, SymbolId) == GR_FALSE)
				return GR_FALSE;
		}
		else
		{
			QualifierPresent = 0;
			if	(grVFile_Write(File, &QualifierPresent, sizeof(QualifierPresent)) == GR_FALSE)
				return GR_FALSE;
		}

		if	(WriteString(File, Symbol->Name) == GR_FALSE)
			return GR_FALSE;

		if	(WriteUInt(File, Symbol->Type) == GR_FALSE)
			return GR_FALSE;

		if	(WriteUInt(File, Symbol->EnumValue) == GR_FALSE)
			return GR_FALSE;

		Symbol->SymbolId = *SymbolId;
		*SymbolId = *SymbolId + 1;
		Symbol->Flags |= SYMBOL_WRITTEN;
		Symbol->Flags &= ~SYMBOL_WRITING;
	}

	return GR_TRUE;
}

static	grSymbol_Rec *ReadSymbol(grSymbol_Table *ST, grVFile *File, Symbol_Array *SymArray)
{
	unsigned char	ReferenceWritten;
	grSymbol_Rec *	Qualifier;
	grSymbol_Type	Type;
	int				EnumValue;
	char *			Name;

	assert(ST);
	assert(File);
	assert(SymArray);

	if	(grVFile_Read(File, &ReferenceWritten, sizeof(ReferenceWritten)) == GR_FALSE)
		return NULL;

	if	(ReferenceWritten == 0xff)
	{
		unsigned int	Index;
		grSymbol_Rec *		Symbol;

		if	(ReadUInt(File, &Index) == GR_FALSE)
			return NULL;
		Symbol = Symbol_ArrayGetSymbol(SymArray, Index);
		return Symbol;
	}
	else
	{
		unsigned char	QualifierPresent;
		grSymbol *		HSymbol;
//		grSymbol *		HQualifier;
		grSymbol		LocalQualifier;
		grSymbol_Rec *	Symbol;

		assert(ReferenceWritten == 0);

		if	(grVFile_Read(File, &QualifierPresent, sizeof(QualifierPresent)) == GR_FALSE)
			return NULL;
		if	(QualifierPresent == 0xff)
		{
			Qualifier = ReadSymbol(ST, File, SymArray);
			if	(!Qualifier)
				return NULL;
		}
		else
		{
			assert(QualifierPresent == 0);
			Qualifier = NULL;
		}
		Name = ReadString(File);
		if	(!Name)
			return NULL;

		if	(ReadUInt(File, &Type) == GR_FALSE)
		{
			grRam_Free(Name);
			return NULL;
		}
		if	(ReadUInt(File, &EnumValue) == GR_FALSE)
		{
			grRam_Free(Name);
			return NULL;
		}

		LocalQualifier.Symbol = Qualifier;
		Symbol = NULL;
		HSymbol = grSymbol_Create(ST, Qualifier ? &LocalQualifier : NULL, Name, Type);
		if	(HSymbol)
		{
			Symbol = HSymbol->Symbol;
			Symbol_ArrayAddSymbol(SymArray, Symbol);
			grSymbol_Destroy(&HSymbol);
		}

		grRam_Free(Name);

		return Symbol;
	}
	assert(!"Shouldn't get here");
}

static	grBoolean WriteSymbolList(grVFile *File, const grSymbol_ListRec *List)
{
	SymListElt *	Elts;
	int				Count;

	Elts = List->Elts;
	Count = 0;
	while	(Elts)
	{
		Count++;
		assert(Elts->Symbol->Flags & SYMBOL_WRITTEN);
		Elts = Elts->Next;
	}
	if	(WriteUInt(File, Count) == GR_FALSE)
		return GR_FALSE;

	Elts = List->Elts;
	while	(Elts)
	{
		if	(WriteUInt(File, Elts->Symbol->SymbolId) == GR_FALSE)
			return GR_FALSE;
		Elts = Elts->Next;
	}

	return GR_TRUE;
}

static	grBoolean ReadSymbolList(grSymbol_Table *ST, grVFile *File, grSymbol_ListRec **List, Symbol_Array *SymArray)
{
	int				Count;
	grSymbol_List *	HList;

	HList = grSymbol_ListCreate(ST);
	if	(!HList)
		return GR_FALSE;

	*List = HList->List;

	if	(ReadUInt(File, &Count) == GR_FALSE)
	{
		grSymbol_ListDestroy(&HList);
		return GR_FALSE;
	}

	while	(Count--)
	{
		int				Index;
		grSymbol_Rec *	Symbol;
		grSymbol *		HSymbol;

		if	(ReadUInt(File, &Index) == GR_FALSE)
		{
			grSymbol_ListDestroy(&HList);
			return GR_FALSE;
		}

		Symbol = Symbol_ArrayGetSymbol(SymArray, Index);
		HSymbol = ReferenceSymbol(ST, Symbol);
		if	(!HSymbol)
		{
			grSymbol_ListDestroy(&HList);
			return GR_FALSE;
		}
		if	(grSymbol_ListAddSymbol(HList, HSymbol) == GR_FALSE)
		{
			grSymbol_Destroy(&HSymbol);
			grSymbol_ListDestroy(&HList);
			return GR_FALSE;
		}
		grSymbol_Destroy(&HSymbol);
	}
	return GR_TRUE;
}

static	grBoolean WriteSymbolProperties(grSymbol_Rec *Symbol, grVFile *File)
{
	PropList *	PList;
	int			PropertyCount;

	assert(Symbol->Flags & SYMBOL_WRITTEN);

	if	(WriteUInt(File, Symbol->SymbolId) == GR_FALSE)
		return GR_FALSE;

	PList = Symbol->Properties;
	PropertyCount = 0;
	while	(PList)
	{
		PropertyCount++;
		PList = PList->Next;
	}

	if	(WriteUInt(File, PropertyCount) == GR_FALSE)
		return GR_FALSE;

	PList = Symbol->Properties;
	while	(PList)
	{
		unsigned char	UC;

		assert(PList->Symbol->Flags & SYMBOL_WRITTEN);
		if	(WriteUInt(File, PList->Symbol->SymbolId) == GR_FALSE)
			return GR_FALSE;
//		if	(WriteUInt(File, PList->Symbol->Type) == GR_FALSE)
//			return GR_FALSE;
		switch	(PList->Symbol->Type)
		{
		case	GR_SYMBOL_TYPE_INT:
			if	(WriteUInt(File, PList->Value.Integer) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_STRING:
			if	(WriteString(File, PList->Value.String) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_FLOAT:
			if	(WriteFloat(File, PList->Value.Float) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_VEC3D:
			if	(WriteFloat(File, PList->Value.Vec3d.X) == GR_FALSE)
				return GR_FALSE;
			if	(WriteFloat(File, PList->Value.Vec3d.Y) == GR_FALSE)
				return GR_FALSE;
			if	(WriteFloat(File, PList->Value.Vec3d.Z) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_COLOR:
			if	(WriteFloat(File, PList->Value.Color.r) == GR_FALSE)
				return GR_FALSE;
			if	(WriteFloat(File, PList->Value.Color.g) == GR_FALSE)
				return GR_FALSE;
			if	(WriteFloat(File, PList->Value.Color.b) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_BOOLEAN:
			if	(PList->Value.Boolean == GR_TRUE)
				UC = 1;
			else
				UC = 0;
			if	(grVFile_Write(File, &UC, 1) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_ENUM:
			if	(WriteUInt(File, PList->Value.Sym->SymbolId) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_PVOID:
#pragma message ("Need to implement property writers for PVOID?")
			assert(!"Not implemented");
			break;

		case	GR_SYMBOL_TYPE_LIST:
			if	(WriteSymbolList(File, PList->Value.List) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_SYMBOL:
			if	(WriteUInt(File, PList->Value.Sym->SymbolId) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_MODEL:
		case	GR_SYMBOL_TYPE_PORTAL:
			assert(!"Not implemented");

		case	GR_SYMBOL_TYPE_VOID:
			assert(!"Not a legal property type");
			break;
		}
		PList = PList->Next;
	}

	return GR_TRUE;
}

static	int	TypeSizes[] =
{
	sizeof(int),		//GR_SYMBOL_TYPE_INT
	sizeof(char *),		//GR_SYMBOL_TYPE_STRING
	sizeof(grFloat),	//GR_SYMBOL_TYPE_FLOAT
	sizeof(grVec3d),	//GR_SYMBOL_TYPE_VEC3D
	sizeof(grRGBA),		//GR_SYMBOL_TYPE_COLOR
	sizeof(grBoolean),	//GR_SYMBOL_TYPE_BOOLEAN
	sizeof(grSymbol *),	//GR_SYMBOL_TYPE_ENUM
	sizeof(void *),		//GR_SYMBOL_TYPE_PVOID
	sizeof(grSymbol_List *), //GR_SYMBOL_TYPE_LIST
	sizeof(grSymbol *),	//GR_SYMBOL_TYPE_SYMBOL

	0, 					//	GR_SYMBOL_TYPE_MODEL,
	0,					//	GR_SYMBOL_TYPE_PORTAL,
};

static	grBoolean SetProp(grSymbol_Rec *Symbol, grSymbol_Rec *Property, void *Data)
{
	grSymbol *	HSymbol;
	grSymbol *	HProperty;
	grBoolean	Result;

#pragma message ("Really horrible way of setting a property internally.  Fix this.")
	HSymbol = ReferenceSymbol(Symbol->SymbolTable, Symbol);
	HProperty = ReferenceSymbol(Property->SymbolTable, Property);
	Result = grSymbol_SetProperty(HSymbol, HProperty, Data, TypeSizes[Property->Type], Property->Type);
	grSymbol_Destroy(&HSymbol);
	grSymbol_Destroy(&HProperty);

	return Result;
}

static	grBoolean ReadSymbolProperties(grVFile *File, grSymbol_Table *ST, Symbol_Array *SymArray)
{
	int				Index;
	grSymbol_Rec *	Symbol;
	grSymbol_Rec *	Property;
	int				Count;

	if	(ReadUInt(File, &Index) == GR_FALSE)
		return GR_FALSE;

	Symbol = Symbol_ArrayGetSymbol(SymArray, Index);
	if	(ReadUInt(File, &Count) == GR_FALSE)
		return GR_FALSE;

	while	(Count--)
	{
//		grSymbol_Type	Type;
		PropList		PEntry;
		grSymbol		LocalSymbol;
		grSymbol_List	LocalList;

		if	(ReadUInt(File, &Index) == GR_FALSE)
			return GR_FALSE;
		Property = Symbol_ArrayGetSymbol(SymArray, Index);
		switch	(Property->Type)
		{
			unsigned char	UC;
			int				Index;

		case	GR_SYMBOL_TYPE_INT:
			if	(ReadUInt(File, &PEntry.Value.Integer) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_FLOAT:
			if	(ReadFloat(File, &PEntry.Value.Float) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_STRING:
			PEntry.Value.String = ReadString(File);
			if	(!PEntry.Value.String)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_VEC3D:
			if	(ReadFloat(File, &PEntry.Value.Vec3d.X) == GR_FALSE)
				return GR_FALSE;
			if	(ReadFloat(File, &PEntry.Value.Vec3d.Y) == GR_FALSE)
				return GR_FALSE;
			if	(ReadFloat(File, &PEntry.Value.Vec3d.Z) == GR_FALSE)
				return GR_FALSE;
			break;

		case	GR_SYMBOL_TYPE_COLOR:
			if	(ReadFloat(File, &PEntry.Value.Color.r) == GR_FALSE)
				return GR_FALSE;
			if	(ReadFloat(File, &PEntry.Value.Color.g) == GR_FALSE)
				return GR_FALSE;
			if	(ReadFloat(File, &PEntry.Value.Color.b) == GR_FALSE)
				return GR_FALSE;
			PEntry.Value.Color.a = 0.0f;
			break;

		case	GR_SYMBOL_TYPE_BOOLEAN:
			if	(grVFile_Read(File, &UC, 1) == GR_FALSE)
				return GR_FALSE;
			if	(UC == 1)
				PEntry.Value.Boolean = GR_TRUE;
			else
			{
				assert(UC == 0);
				PEntry.Value.Boolean = GR_FALSE;
			}
			break;

		case	GR_SYMBOL_TYPE_ENUM:
		case	GR_SYMBOL_TYPE_SYMBOL:
			if	(ReadUInt(File, &Index) == GR_FALSE)
				return GR_FALSE;
			// Hacking a little bit here.
			LocalSymbol.Symbol = Symbol_ArrayGetSymbol(SymArray, Index);
			PEntry.Value.Sym = (grSymbol_Rec *)&LocalSymbol;
			break;

		case	GR_SYMBOL_TYPE_PVOID:
			assert(!"Can't read this from a file");
			break;

		case	GR_SYMBOL_TYPE_LIST:
			if	(ReadSymbolList(ST, File, &LocalList.List, SymArray) == GR_FALSE)
				return GR_FALSE;
			PEntry.Value.List = (grSymbol_ListRec *)&LocalList;
			break;

		case	GR_SYMBOL_TYPE_MODEL:
		case	GR_SYMBOL_TYPE_PORTAL:
			assert(!"Need to think about models and portals");
			break;

		case	GR_SYMBOL_TYPE_VOID:
			assert(!"Illegal property in a file");
			break;

		default:
			assert(!"Unknown property type");
		}

		if	(SetProp(Symbol, Property, &PEntry.Value) == GR_FALSE)
		{
			switch	(Property->Type)
			{
			case	GR_SYMBOL_TYPE_STRING:
				grRam_Free(PEntry.Value.String);
				break;
			}

			return GR_FALSE;
		}
	}
	return GR_TRUE;
}

GRAPI	grBoolean GRCC grSymbol_TableWriteToFile(const grSymbol_Table *ST, grVFile *File)
{
	int				i;
	unsigned int	SymbolCount;
	SymListElt *	Elts;
	unsigned int	Signature;
	unsigned int	SymbolId;

	assert(ST);
	assert(File);
	
	/*
		If we unioned RefCount and SymbolId in the symbol structure, we could save
		runtime space in the symbol table by saving off all the refcounts in an array
		here, and assigning persistent ids in the RefCount field.
	*/
	SymbolCount = 0;
	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		Elts = ST->Symbols[i]->List->Elts;
		while	(Elts)
		{
			SymbolCount++;
			Elts->Symbol->Flags &= ~SYMBOL_WRITTEN;
			Elts->Symbol->SymbolId = -1;
			Elts = Elts->Next;
		}
	}

	Signature = ST_SIGNATURE;
	if	(grVFile_Write(File, &Signature, sizeof(Signature)) == GR_FALSE)
		return GR_FALSE;
	if	(grVFile_Write(File, &SymbolCount, sizeof(SymbolCount)) == GR_FALSE)
		return GR_FALSE;

	SymbolId = 0;

	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		Elts = ST->Symbols[i]->List->Elts;
		while	(Elts)
		{
			if	(WriteSymbol(Elts->Symbol, File, &SymbolId) == GR_FALSE)
				return GR_FALSE;
			Elts = Elts->Next;
		}
	}

	assert(SymbolId == SymbolCount);

	if	(grVFile_Write(File, "STPROP", 6) == GR_FALSE)
		return GR_FALSE;

	// Now do all the properties.
	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		Elts = ST->Symbols[i]->List->Elts;
		while	(Elts)
		{
			if	(WriteSymbolProperties(Elts->Symbol, File) == GR_FALSE)
				return GR_FALSE;
			Elts = Elts->Next;
		}
	}

	Signature = ST_SIGNATURE_END;
	if	(grVFile_Write(File, &Signature, sizeof(Signature)) == GR_FALSE)
		return GR_FALSE;

	return GR_TRUE;
}

GRAPI	grSymbol_Table *GRCC grSymbol_TableCreateFromFile(grVFile *File)
{
	grSymbol_Table *	ST;
	int					SymbolCount;
	unsigned int		Signature;
//	grSymbol **			SymArray;
	Symbol_Array *		SymArray;
//	int					SymbolId;
//	grSymbol_Rec *		Symbol;
	char				PropSignature[6];
	int					i;

	assert(File);

	if	(grVFile_Read(File, &Signature, sizeof(Signature)) == GR_FALSE)
		return NULL;
	if	(grVFile_Read(File, &SymbolCount, sizeof(SymbolCount)) == GR_FALSE)
		return NULL;

	if	(Signature != ST_SIGNATURE)
		return NULL;

	ST = grSymbol_TableCreate();
	if	(!ST)
		return ST;

	if	(SymbolCount > 0)
	{
//		SymbolId = 0;
//			SymArray = grRam_Allocate(sizeof(*SymArray) * SymbolCount);
		SymArray = Symbol_ArrayCreate(SymbolCount);
		if	(!SymArray)
		{
			grSymbol_TableDestroy(&ST);
			return NULL;
		}

		for	(i = 0; i < SymbolCount; i++)
		{
			if	(ReadSymbol(ST, File, SymArray) == NULL)
			{
				grRam_Free(SymArray);
				grSymbol_TableDestroy(&ST);
				return NULL;
			}
		}
//		Symbol_ArrayDestroy(&SymArray);
	}

	if	(grVFile_Read(File, PropSignature, 6) == GR_FALSE)
	{
		if	(SymArray)
			Symbol_ArrayDestroy(&SymArray);
		grSymbol_TableDestroy(&ST);
		return NULL;
	}
	if	(strncmp(PropSignature, "STPROP", 6))
	{
		if	(SymArray)
			Symbol_ArrayDestroy(&SymArray);
		grSymbol_TableDestroy(&ST);
		return NULL;
	}

	for	(i = 0; i < SymbolCount; i++)
	{
		if	(ReadSymbolProperties(File, ST, SymArray) == GR_FALSE)
		{
			if	(SymArray)
				Symbol_ArrayDestroy(&SymArray);
			grSymbol_TableDestroy(&ST);
			return NULL;
		}
	}

	if	(grVFile_Read(File, &Signature, sizeof(Signature)) == GR_FALSE)
	{
		if	(SymArray)
			Symbol_ArrayDestroy(&SymArray);
		grSymbol_TableDestroy(&ST);
		return NULL;
	}

	if	(Signature != ST_SIGNATURE_END)
	{
		if	(SymArray)
			Symbol_ArrayDestroy(&SymArray);
		grSymbol_TableDestroy(&ST);
		return NULL;
	}

	if	(SymArray)
		Symbol_ArrayDestroy(&SymArray);

	return ST;
}

GRAPI	void GRCC grSymbol_TableDestroy(grSymbol_Table **pST)
{
	grSymbol_Table *	ST;
	int					i;

	assert(pST);
	assert(*pST);

	ST = *pST;
//TempST = ST;
	assert(ST->RefCount > 0);

	if	(--ST->RefCount > 0)
		return;

	assert(ST->Symbols);

	if	(ST->QualifierListProperty)
		grSymbol_Destroy(&ST->QualifierListProperty);

	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		assert(ST->Symbols[i]);
		grSymbol_ListDestroy(&ST->Symbols[i]);
	}

	if	(ST->SymbolReferences)
		RefPool_Destroy(&ST->SymbolReferences);

	if	(ST->ListReferences)
		RefPool_Destroy(&ST->ListReferences);

	if	(ST->SymbolHeap)
	{
		GCHeap_Sweep(ST->SymbolHeap);
		GCHeap_Destroy(&ST->SymbolHeap);
	}

	if	(ST->ListHeap)
	{
		GCHeap_Sweep(ST->ListHeap);
		GCHeap_Destroy(&ST->ListHeap);
	}

	grRam_Free(ST->Symbols);
	grRam_Free(ST);
//TempST = NULL;
	*pST = NULL;
}

GRAPI	grSymbol *GRCC grSymbol_TableFindSymbol(
	grSymbol_Table *	ST,
	grSymbol *			HQualifier,
	const char *		Name)
{
	unsigned short	HashValue;
	int				Length;
	SymListElt *	Elts;
	grSymbol_Rec *	Sym;
	grSymbol_Rec *	Qualifier;

	assert(ST);
	assert(Name);

	assert(grSymbol_TableIsValid(ST) == GR_TRUE);

	Length = strlen(Name);

	HashValue = Hash(Name, Length);
	if	(HQualifier)
	{
		assert(HQualifier->Symbol);
		Qualifier = HQualifier->Symbol;
		HashValue += (unsigned short)Qualifier;
	}
	else
	{
		Qualifier = NULL;
	}

	Elts = ST->Symbols[HashValue % NUMHASHBUCKETS]->List->Elts;
	Sym = NULL;
	while	(Elts)
	{
		Sym = Elts->Symbol;
		if	(Length == Sym->NameLength)
		{
			// Is this the one?
			if	(Sym->Qualifier == Qualifier && !stricmp(Sym->Name, Name))
				break;
		}
		Elts = Elts->Next;
	}

	// If we found one, just return a ref to it.
	if	(Elts)
	{
		assert(Sym);
		return ReferenceSymbol(Sym->SymbolTable, Sym);
	}

	return NULL;
}

#if 1
typedef	grBoolean	(*SymbolWalker)(grSymbol_Rec *Symbol);
typedef	grBoolean	(*ListWalker)(grSymbol_ListRec *List, SymbolWalker Walker);

static	grBoolean	WalkListWithoutReference(grSymbol_ListRec *List, SymbolWalker Walker)
{
	SymListElt *	Elts;

	Elts = List->Elts;
	while	(Elts)
	{
		if	((Walker)(Elts->Symbol) == GR_FALSE)
			return GR_FALSE;
		Elts = Elts->Next;
	}
	return GR_TRUE;
}

static	grBoolean	WalkListAndMark(grSymbol_ListRec *List, SymbolWalker Walker)
{
	SymListElt *	Elts;

	assert(List->SymbolTable);
	assert(List->SymbolTable->ListHeap);

	GCHeap_MarkObject(List);

	Elts = List->Elts;
	while	(Elts)
	{
		if	((Walker)(Elts->Symbol) == GR_FALSE)
			return GR_FALSE;
		Elts = Elts->Next;
	}
	return GR_TRUE;
}

static	void	grSymbol_Walk(grSymbol_Rec *Symbol, SymbolWalker Walker, ListWalker LWalker)
{
	PropList *		PList;

	if	(Symbol->Qualifier)
	{
		if	((Walker)(Symbol->Qualifier) == GR_FALSE)
			return;
	}

	PList = Symbol->Properties;
	while	(PList)
	{
		if	((Walker)(PList->Symbol) == GR_FALSE)
			return;
		switch	(PList->Symbol->Type)
		{
		case	GR_SYMBOL_TYPE_ENUM:
		case	GR_SYMBOL_TYPE_SYMBOL:
			if	((Walker)(PList->Value.Sym) == GR_FALSE)
				return;
			break;

		case	GR_SYMBOL_TYPE_LIST:
			if	((LWalker)(PList->Value.List, Walker) == GR_FALSE)
				return;
			break;
		}
		PList = PList->Next;
	}
}
#endif

GRAPI	grSymbol *GRCC grSymbol_Create(
	grSymbol_Table *	ST,
	grSymbol *			HQualifier,
	const char *		Name,
	grSymbol_Type		Type)
{
	unsigned short		HashValue;
	int					Length;
	SymListElt *		Elts;
	grSymbol_ListRec *	SymList;
	grSymbol_Rec *		Sym;
	grSymbol_Rec *		Qualifier;
	grSymbol *			HSymbol;
	grSymbol_List		List;

	assert(ST);
	assert(Name);

	assert(grSymbol_TableIsValid(ST) == GR_TRUE);

	if	(HQualifier)
		Qualifier = HQualifier->Symbol;
	else
		Qualifier = NULL;

	Length = strlen(Name);

	HashValue = Hash(Name, Length) + (unsigned short)Qualifier;
//	if	(Qualifier)
//		HashValue += Qualifier->HashValue;

	Elts = ST->Symbols[HashValue % NUMHASHBUCKETS]->List->Elts;
	Sym = NULL;
	while	(Elts)
	{
		Sym = Elts->Symbol;
		if	(Length == Sym->NameLength)
		{
			// Is this the one?
			if	(Sym->Qualifier == Qualifier && !stricmp(Sym->Name, Name))
				break;
		}
		Elts = Elts->Next;
	}

	// If we found one, just return a ref to it.
	if	(Elts)
	{
		assert(Sym);
//		grSymbol_CreateRef(Sym);
		return ReferenceSymbol(Sym->SymbolTable, Sym);
//		return Sym;
	}

	// Didn't find it.  Create one.
//	Sym = grRam_Allocate(sizeof(*Sym));
	Sym = GCHeap_AllocateFixed(ST->SymbolHeap);
	if	(!Sym)
		return NULL;
	Sym->Name = grRam_Allocate(Length + 1);
	if	(!Sym->Name)
		return NULL;

	memcpy(Sym->Name, Name, Length + 1);
	Sym->NameLength = Length;
	Sym->Type = Type;
	Sym->HashValue = HashValue;
	Sym->EnumValue = 0;
	Sym->Properties = NULL;
	Sym->Qualifier = Qualifier;
	Sym->SymbolTable = ST;
//	Sym->RefCount = 1;
	Sym->Flags = 0;
//	if	(Qualifier)
//		grSymbol_CreateRef(Qualifier);

	SymList = ST->Symbols[HashValue % NUMHASHBUCKETS]->List;

	HSymbol = (grSymbol *)RefPool_RefCreate(ST->SymbolReferences);
	HSymbol->Symbol = Sym;

	List.List = SymList;
	if	(grSymbol_ListAddSymbol(&List, HSymbol) == GR_FALSE)
	{
//		if	(Qualifier)
//			grSymbol_Destroy(&Qualifier);
		grRam_Free(Sym->Name);
//		grRam_Free(Sym);
		return NULL;
	}

	// Should be referenced twice, once for the user, and once for the symbol list
//	assert(Sym->RefCount == 2);

	return HSymbol;
}

GRAPI	grBoolean GRCC grSymbol_Compare(const grSymbol *S1, const grSymbol *S2)
{
	if	(S1->Symbol == S2->Symbol)
		return GR_TRUE;
	else
		return GR_FALSE;
}

static	char *	DuplicateString(const char *S)
{
	int		Length;
	char *	Result;

	assert(S);

	Length = strlen(S) + 1;
	Result = grRam_Allocate(Length);
	if	(Result)
		memcpy(Result, S, Length);
	return Result;
}

GRAPI	grBoolean GRCC grSymbol_Rename(grSymbol *HSymbol, const char *NewName)
{
	grSymbol_Rec *	Symbol;
	unsigned short	HashValue;
	int				Length;
	char *			NewNameCopy;

	assert(HSymbol);
	assert(HSymbol->Symbol);

	Symbol = HSymbol->Symbol;
	Length = strlen(NewName);
	HashValue = Hash(NewName, Length) + (unsigned short)Symbol->Qualifier;
//	if	(Symbol->Qualifier)
//		HashValue += Symbol->Qualifier->HashValue;
	NewNameCopy = DuplicateString(NewName);
	if	(!NewNameCopy)
		return GR_FALSE;

	if	(grSymbol_ListAddSymbol(Symbol->SymbolTable->Symbols[HashValue % NUMHASHBUCKETS], HSymbol) == GR_FALSE)
	{
		grRam_Free(NewNameCopy);
		return GR_FALSE;
	}
	grRam_Free(Symbol->Name);
	Symbol->Name = NewNameCopy;
	Symbol->HashValue = HashValue;
	grSymbol_ListRemoveSymbol(Symbol->SymbolTable->Symbols[Symbol->HashValue % NUMHASHBUCKETS], HSymbol);
	return GR_TRUE;
}

static	grBoolean grSymbol_MarkSymbol(grSymbol_Rec *Sym)
{
	assert(Sym);

	if	(Sym->Flags & SYMBOL_MARKED)
		return GR_TRUE;

	Sym->Flags |= SYMBOL_MARKED;
	GCHeap_MarkObject(Sym);
	grSymbol_Walk(Sym, grSymbol_MarkSymbol, WalkListAndMark);
	return GR_TRUE;
}

static	grBoolean grSymbol_ClearMarks(grSymbol_Rec *Sym)
{
	assert(Sym);

	if	(!(Sym->Flags & SYMBOL_MARKED))
		return GR_TRUE;

	Sym->Flags &= ~SYMBOL_MARKED;
	grSymbol_Walk(Sym, grSymbol_ClearMarks, WalkListWithoutReference);
	return GR_TRUE;
}

GRAPI	void GRCC grSymbol_TableCollectGarbage(grSymbol_Table *ST)
{
	void **	Ref;

	assert(ST);
	assert(ST->SymbolReferences);
	assert(ST->ListReferences);

//printf("\nGC\n");

	ST->GCAbleOperationCount = 0;

	Ref = RefPool_GetNextRef(ST->SymbolReferences, NULL);
	while	(Ref)
	{
		grSymbol_MarkSymbol(*Ref);
		Ref = RefPool_GetNextRef(ST->SymbolReferences, Ref);
	}

	Ref = RefPool_GetNextRef(ST->ListReferences, NULL);
	while	(Ref)
	{
		WalkListAndMark(*Ref, grSymbol_MarkSymbol);
		Ref = RefPool_GetNextRef(ST->ListReferences, Ref);
	}

	GCHeap_Sweep(ST->SymbolHeap);
	GCHeap_Sweep(ST->ListHeap);

	Ref = RefPool_GetNextRef(ST->SymbolReferences, NULL);
	while	(Ref)
	{
		grSymbol_ClearMarks(*Ref);
		Ref = RefPool_GetNextRef(ST->SymbolReferences, Ref);
	}

	Ref = RefPool_GetNextRef(ST->ListReferences, NULL);
	while	(Ref)
	{
		WalkListWithoutReference(*Ref, grSymbol_ClearMarks);
		Ref = RefPool_GetNextRef(ST->ListReferences, Ref);
	}
}

GRAPI	void GRCC grSymbol_CreateRef(grSymbol *HS, grSymbol **Result)
{
	assert(HS);
	assert(HS->Symbol);
	*Result = ReferenceSymbol(HS->Symbol->SymbolTable, HS->Symbol);
}

static	void FinalizeSymbol(void *P)
{
	PropList *		PList;
	grSymbol_Rec *	Symbol;

	assert(P);

	Symbol = P;

//	printf("Finalizing symbol %s\n", Symbol->Name);

	PList = Symbol->Properties;
	while	(PList)
	{
		PropList *	Temp;

		assert(PList->Symbol);

		if	(PList->Symbol->Type == GR_SYMBOL_TYPE_STRING)
		{
			assert(PList->Value.String);
			grRam_Free(PList->Value.String);
		}
		Temp = PList;
		PList = PList->Next;
		grRam_Free(Temp);
	}

	grRam_Free(Symbol->Name);
}

GRAPI	void GRCC grSymbol_Destroy(grSymbol **pHS)
{
	assert(pHS);
	assert(*pHS);

	RefPool_RefDestroy((*pHS)->Symbol->SymbolTable->SymbolReferences, (void ***)pHS);
}

GRAPI	grSymbol_Type GRCC grSymbol_GetType(const grSymbol *HSym)
{
	assert(HSym);
	assert(HSym->Symbol);
	return HSym->Symbol->Type;
}

GRAPI	grSymbol *GRCC grSymbol_GetQualifier(const grSymbol *HSym)
{
	grSymbol_Rec *	Sym;

	assert(HSym);
	assert(HSym->Symbol);

	Sym = HSym->Symbol;

	return ReferenceSymbol(Sym->SymbolTable, Sym->Qualifier);
}

static	PropList *	GRCC FindProperty(PropList *List, const grSymbol_Rec *Sym)
{
//	assert(List);
	assert(Sym);

	while	(List && Sym != List->Symbol)
		List = List->Next;

	return List;
}

GRAPI	grBoolean GRCC grSymbol_SetProperty(
	grSymbol *		HSym,
	grSymbol *		HProperty,
	const void *	Data,
	int 			DataLength,
	grSymbol_Type	Type)
{
	grSymbol_Rec *	Sym;
	grSymbol_Rec *	Property;
	PropList *	Prop;
	grBoolean	IsNewProperty;

	assert(HSym);
	assert(HSym->Symbol);
	assert(HProperty);
	assert(HProperty->Symbol);
	assert(Data);
	assert(DataLength > 0);

	Sym = HSym->Symbol;
	Property = HProperty->Symbol;

	if	(Type != Property->Type)
		return GR_FALSE;

	Prop = FindProperty(Sym->Properties, Property);
	if	(!Prop)
	{
		Prop = grRam_Allocate(sizeof(*Prop));
		if	(!Prop)
			return GR_FALSE;
		Prop->Symbol = Property;
		IsNewProperty = GR_TRUE;
	}
	else
	{
		IsNewProperty = GR_FALSE;
	}

	switch	(Type)
	{
	case	GR_SYMBOL_TYPE_INT:
		if	(DataLength != sizeof(Prop->Value.Integer))
			goto fail;
		assert(sizeof(Prop->Value.Integer) == sizeof(int));
		Prop->Value.Integer = *(int *)Data;
		break;

	case	GR_SYMBOL_TYPE_FLOAT:
		if	(DataLength != sizeof(Prop->Value.Float))
			goto fail;
		assert(sizeof(Prop->Value.Float) == sizeof(grFloat));
		Prop->Value.Float = *(grFloat *)Data;
		break;

	case	GR_SYMBOL_TYPE_VEC3D:
		if	(DataLength != sizeof(Prop->Value.Vec3d))
			goto fail;
		assert(sizeof(Prop->Value.Vec3d) == sizeof(grVec3d));
		Prop->Value.Vec3d = *(grVec3d *)Data;
		break;

	case	GR_SYMBOL_TYPE_COLOR:
		if	(DataLength != sizeof(Prop->Value.Color))
			goto fail;
		assert(sizeof(Prop->Value.Color) == sizeof(GR_RGBA));
		Prop->Value.Color = *(GR_RGBA *)Data;
		break;

	case	GR_SYMBOL_TYPE_BOOLEAN:
		if	(DataLength != sizeof(Prop->Value.Boolean))
			goto fail;
		assert(sizeof(Prop->Value.Boolean) == sizeof(grBoolean));
		Prop->Value.Boolean = *(grBoolean *)Data;
		break;

	case	GR_SYMBOL_TYPE_STRING:
		Prop->Value.String = DuplicateString(Data);
		if	(!Prop->Value.String)
			goto fail;
		break;

	case	GR_SYMBOL_TYPE_ENUM:
	case	GR_SYMBOL_TYPE_SYMBOL:
		if	(DataLength != sizeof(Prop->Value.Sym))
			goto fail;
		assert(sizeof(Prop->Value.Sym) == sizeof(grSymbol *));
		Prop->Value.Sym = (*(grSymbol **)Data)->Symbol;
		break;

	case	GR_SYMBOL_TYPE_LIST:
		if	(DataLength != sizeof(Prop->Value.List))
			goto fail;
		assert(sizeof(Prop->Value.List) == sizeof(grSymbol_List *));
		Prop->Value.List = (*(grSymbol_List **)Data)->List;

		if	(++Sym->SymbolTable->GCAbleOperationCount > GCABLE_OP_COUNT_THRESHHOLD)
			grSymbol_TableCollectGarbage(Sym->SymbolTable);

		break;

	default:
#pragma message ("Need a few more properties implemented here")
		assert(!"Not implemented");
	}

	if	(IsNewProperty == GR_TRUE)
	{
		Prop->Next = Sym->Properties;
		Sym->Properties = Prop;
	}

	return GR_TRUE;

fail:
	if	(IsNewProperty == GR_TRUE)
		grRam_Free(Prop);
	return GR_FALSE;
}

GRAPI	grBoolean GRCC grSymbol_GetProperty(
	grSymbol *	HSym,
	grSymbol *	HProperty,
	void *				Data,
	int 				DataLength,
	grSymbol_Type 		Type)
{
	grSymbol_Rec *	Sym;
	grSymbol_Rec *	Property;
	PropList *		Prop;
//	grBoolean		Result;

	assert(HSym);
	assert(HSym->Symbol);
	assert(HProperty);
	assert(HProperty->Symbol);
	assert(Data);
	assert(DataLength);

	Sym = HSym->Symbol;
	Property = HProperty->Symbol;

	Prop = FindProperty(Sym->Properties, Property);
	if	(!Prop)
		return GR_FALSE;

	if	(Property->Type != Type)
		return GR_FALSE;

	switch	(Type)
	{
	case	GR_SYMBOL_TYPE_INT:
		if	(DataLength != sizeof(Prop->Value.Integer))
			return GR_FALSE;
		assert(sizeof(Prop->Value.Integer) == sizeof(int));
		*(int *)Data = Prop->Value.Integer;
		break;

	case	GR_SYMBOL_TYPE_FLOAT:
		if	(DataLength != sizeof(Prop->Value.Float))
			return GR_FALSE;
		assert(sizeof(Prop->Value.Float) == sizeof(grFloat));
		*(grFloat *)Data = Prop->Value.Float;
		break;

	case	GR_SYMBOL_TYPE_VEC3D:
		if	(DataLength != sizeof(Prop->Value.Vec3d))
			return GR_FALSE;
		assert(sizeof(Prop->Value.Vec3d) == sizeof(grVec3d));
		*(grVec3d *)Data = Prop->Value.Vec3d;
		break;

	case	GR_SYMBOL_TYPE_COLOR:
		if	(DataLength != sizeof(Prop->Value.Color))
			return GR_FALSE;
		assert(sizeof(Prop->Value.Color) == sizeof(GR_RGBA));
		*(GR_RGBA *)Data = Prop->Value.Color;
		break;

	case	GR_SYMBOL_TYPE_BOOLEAN:
		if	(DataLength != sizeof(Prop->Value.Boolean))
			return GR_FALSE;
		assert(sizeof(Prop->Value.Boolean) == sizeof(grBoolean));
		*(grBoolean *)Data = Prop->Value.Boolean;
		break;

	case	GR_SYMBOL_TYPE_STRING:
		assert(sizeof(Prop->Value.String) == sizeof(char *));
		*(char **)Data = Prop->Value.String;
		break;

	case	GR_SYMBOL_TYPE_ENUM:
	case	GR_SYMBOL_TYPE_SYMBOL:
		if	(DataLength != sizeof(Prop->Value.Sym))
			return GR_FALSE;
		assert(sizeof(Prop->Value.Sym) == sizeof(grSymbol *));
		*(grSymbol **)Data = ReferenceSymbol(Prop->Value.Sym->SymbolTable, Prop->Value.Sym);
		if	(!*(grSymbol **)Data)
			return GR_FALSE;
		break;

	case	GR_SYMBOL_TYPE_LIST:
		if	(DataLength != sizeof(Prop->Value.List))
			return GR_FALSE;
		assert(sizeof(Prop->Value.List) == sizeof(grSymbol_List *));
		*(grSymbol_List **)Data = ReferenceList(Prop->Value.List->SymbolTable, Prop->Value.List);
		if	(!*(grSymbol_List **)Data)
			return GR_FALSE;
		break;

	default:
#pragma message ("Need a few more properties implemented here")
		assert(!"Not implemented");
	}

	return GR_TRUE;
}

GRAPI	grBoolean GRCC grSymbol_CopyProperty(
	grSymbol *HDest,
	grSymbol *HDestProp,
	grSymbol *HSrc,
	grSymbol *HSrcProp)
{
	grSymbol_Rec *	Dest;
	grSymbol_Rec *	DestProp;
	grSymbol_Rec *	Src;
	grSymbol_Rec *	SrcProp;
//	grBoolean		Result;

	assert(HDest);
	assert(HDestProp);
	assert(HSrc);
	assert(HSrcProp);
	assert(HDest->Symbol);
	assert(HDestProp->Symbol);
	assert(HSrc->Symbol);
	assert(HSrcProp->Symbol);

	Dest = HDest->Symbol;
	DestProp = HDestProp->Symbol;
	Src = HSrc->Symbol;
	SrcProp = HSrcProp->Symbol;

	if	(DestProp->Type != SrcProp->Type)
		return GR_FALSE;

	switch	(DestProp->Type)
	{
		int				Integer;
		grFloat			Float;
		grVec3d			Vector;
		GR_RGBA			Color;
		grSymbol *		Symbol;
		grSymbol_List *	SymList;
		void *			Data;
		char *			String;
		grBoolean		Result;

	case	GR_SYMBOL_TYPE_INT:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Integer, sizeof(Integer), GR_SYMBOL_TYPE_INT) == GR_FALSE)
			return GR_FALSE;
		return grSymbol_SetProperty(HDest, HDestProp, &Integer, sizeof(Integer), GR_SYMBOL_TYPE_INT);

	case	GR_SYMBOL_TYPE_STRING:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &String, sizeof(String), GR_SYMBOL_TYPE_STRING) == GR_FALSE)
			return GR_FALSE;
		String = DuplicateString(String);
		if	(!String)
			return GR_FALSE;
		Result = grSymbol_SetProperty(HDest, HDestProp, String, sizeof(String), GR_SYMBOL_TYPE_STRING);
		if	(Result == GR_FALSE)
			grRam_Free(String);
		return Result;

	case	GR_SYMBOL_TYPE_FLOAT:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Float, sizeof(Float), GR_SYMBOL_TYPE_FLOAT) == GR_FALSE)
			return GR_FALSE;
		return grSymbol_SetProperty(HDest, HDestProp, &Float, sizeof(Float), GR_SYMBOL_TYPE_FLOAT);

	case	GR_SYMBOL_TYPE_COLOR:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Color, sizeof(Color), GR_SYMBOL_TYPE_COLOR) == GR_FALSE)
			return GR_FALSE;
		return grSymbol_SetProperty(HDest, HDestProp, &Color, sizeof(Color), GR_SYMBOL_TYPE_COLOR);

	case	GR_SYMBOL_TYPE_VEC3D:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Vector, sizeof(Vector), GR_SYMBOL_TYPE_VEC3D) == GR_FALSE)
			return GR_FALSE;
		return grSymbol_SetProperty(HDest, HDestProp, &Vector, sizeof(Vector), GR_SYMBOL_TYPE_VEC3D);

	case	GR_SYMBOL_TYPE_BOOLEAN:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Result, sizeof(Result), GR_SYMBOL_TYPE_BOOLEAN) == GR_FALSE)
			return GR_FALSE;
		return grSymbol_SetProperty(HDest, HDestProp, &Result, sizeof(Result), GR_SYMBOL_TYPE_BOOLEAN);

	case	GR_SYMBOL_TYPE_ENUM:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Symbol, sizeof(Symbol), GR_SYMBOL_TYPE_ENUM) == GR_FALSE)
			return GR_FALSE;
		Result = grSymbol_SetProperty(HDest, HDestProp, &Symbol, sizeof(Symbol), GR_SYMBOL_TYPE_ENUM);
		grSymbol_Destroy(&Symbol);
		return Result;

	case	GR_SYMBOL_TYPE_PVOID:
#pragma message ("We don't copy void data, we just copy the pointer")
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Data, sizeof(Data), GR_SYMBOL_TYPE_PVOID) == GR_FALSE)
			return GR_FALSE;
		return grSymbol_SetProperty(HDest, HDestProp, &Data, sizeof(Data), GR_SYMBOL_TYPE_PVOID);

	case	GR_SYMBOL_TYPE_LIST:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &SymList, sizeof(SymList), GR_SYMBOL_TYPE_LIST) == GR_FALSE)
			return GR_FALSE;
		Result = grSymbol_SetProperty(HDest, HDestProp, &SymList, sizeof(SymList), GR_SYMBOL_TYPE_LIST);
		grSymbol_ListDestroy(&SymList);
		return Result;

	case	GR_SYMBOL_TYPE_SYMBOL:
		if	(grSymbol_GetProperty(HSrc, HSrcProp, &Symbol, sizeof(Symbol), GR_SYMBOL_TYPE_SYMBOL) == GR_FALSE)
			return GR_FALSE;
		Result = grSymbol_SetProperty(HDest, HDestProp, &Symbol, sizeof(Symbol), GR_SYMBOL_TYPE_SYMBOL);
		grSymbol_Destroy(&Symbol);
		return Result;

	case	GR_SYMBOL_TYPE_MODEL:
	case	GR_SYMBOL_TYPE_PORTAL:
#pragma message ("CopyProperty: Model and portal not implemented")
		assert(!"Not implemented");
		return GR_FALSE;

	case	GR_SYMBOL_TYPE_VOID:
		return GR_FALSE;

	default:
		assert(!"Bad symbol type");
		return GR_FALSE;
	}
	assert(!"Shouldn't get here");
	return GR_FALSE;
}

GRAPI	grBoolean GRCC grSymbol_GetEnumValue(const grSymbol *HSym, int *Value)
{
	assert(HSym);
	assert(HSym->Symbol);
	assert(Value);

	if	(HSym->Symbol->Type != GR_SYMBOL_TYPE_ENUM)
		return GR_FALSE;
	*Value = HSym->Symbol->EnumValue;
	return GR_TRUE;
}

GRAPI	grBoolean GRCC grSymbol_SetEnumValue(grSymbol *HSym, int Value)
{
	assert(HSym);
	assert(HSym->Symbol);

	if	(HSym->Symbol->Type != GR_SYMBOL_TYPE_ENUM)
		return GR_FALSE;
	HSym->Symbol->EnumValue = Value;
	return GR_TRUE;
}

GRAPI	const char *GRCC grSymbol_GetName(const grSymbol *HSym)
{
	assert(HSym);
	assert(HSym->Symbol);
	assert(HSym->Symbol->Name);

	return HSym->Symbol->Name;
}

GRAPI	grBoolean	GRCC grSymbol_GetFullName(const grSymbol *HSym, char *Buff, int MaxLen)
{
	// Really slow implementation for deeply nested names
	grSymbol_Rec *	Sym;

	assert(HSym);
	assert(HSym->Symbol);
	assert(Buff);

	Sym = HSym->Symbol;

	*Buff = '\0';
	if	(Sym->Qualifier)
	{
		grSymbol *		HQualifier;
		grBoolean		Result;

		HQualifier = ReferenceSymbol(Sym->SymbolTable, Sym->Qualifier);
		if	(!HQualifier)
			return GR_FALSE;
		Result = grSymbol_GetFullName(HQualifier, Buff, MaxLen);
		grSymbol_Destroy(&HQualifier);
		if	(Result == GR_FALSE)
			return GR_FALSE;
	}

	MaxLen -= strlen(Buff) + 2;
	if	(MaxLen < Sym->NameLength + 1)
		return GR_FALSE;

	strcat(Buff, "::");
	strcat(Buff, Sym->Name);

	return GR_TRUE;
}

GRAPI	grSymbol_List *	GRCC grSymbol_ListCreate(grSymbol_Table *ST)
{
	grSymbol_ListRec *	List;
	
	assert(ST);
	assert(ST->ListHeap);

	List = GCHeap_AllocateFixed(ST->ListHeap);
	if	(List)
	{
		grSymbol_List *	HList;

		List->Elts = NULL;
		List->CurrentIndex = -1;
		List->Current = NULL;
		List->SymbolTable = ST;

		HList = (grSymbol_List *)RefPool_RefCreate(ST->ListReferences);
		if	(HList)
			HList->List = List;

		return HList;
	}

	return NULL;
}

GRAPI	void GRCC grSymbol_ListCreateRef(grSymbol_List *HL, grSymbol_List **Result)
{
	grSymbol_List *	HList;

	assert(HL);
	assert(HL->List);

	HList = (grSymbol_List *)RefPool_RefCreate(HL->List->SymbolTable->ListReferences);
	if	(HList)
		HList->List = HL->List;

	*Result = HList;
}

static	void FinalizeList(void *P)
{
	grSymbol_ListRec *	List;
	SymListElt *		Elts;

	assert(P);
	List = P;

//	printf("Finalizing list %p\n", List);

	Elts = List->Elts;
	while	(Elts)
	{
		SymListElt *	Temp;

		Temp = Elts;
		Elts = Elts->Next;
		grRam_Free(Temp);
	}
}

GRAPI	void GRCC grSymbol_ListDestroy(grSymbol_List **pSymList)
{
	assert(pSymList);
	assert(*pSymList);

	RefPool_RefDestroy((*pSymList)->List->SymbolTable->ListReferences, (void ***)pSymList);
}

GRAPI	grSymbol *GRCC grSymbol_ListGetSymbol(const grSymbol_List *HL, int Index)
{
	SymListElt *		Elts;
	grSymbol_ListRec *	L;

	assert(HL);
	assert(HL->List);

	L = HL->List;

	if	(L->CurrentIndex >= 0 && Index == L->CurrentIndex + 1)
	{
		if	(!L->Current)
			return NULL;

		L->Current = L->Current->Next;
		L->CurrentIndex++;

		if	(!L->Current)
			return NULL;
	}
	else
	{
		Elts = L->Elts;
	
		L->CurrentIndex = 0;
		while	(Elts && L->CurrentIndex != Index)
		{
			L->CurrentIndex++;
			Elts = Elts->Next;
		}
	
		if	(!Elts)
		{
			L->CurrentIndex = -1;
			return NULL;
		}
	
		L->Current = Elts;
	}

	return ReferenceSymbol(L->SymbolTable, L->Current->Symbol);
}

GRAPI	grBoolean	GRCC grSymbol_TableRemoveSymbol(
	grSymbol_Table *	ST,
	grSymbol *			HSymbol)
{
	grSymbol_Rec *	Symbol;

	assert(ST);
	assert(HSymbol);
	assert(HSymbol->Symbol);

	Symbol = HSymbol->Symbol;

	if	(Symbol->Flags & SYMBOL_REMOVED)
		return GR_TRUE;

	Symbol->Flags |= SYMBOL_REMOVED;

	grSymbol_ListRemoveSymbol(ST->Symbols[Symbol->HashValue % NUMHASHBUCKETS], HSymbol);

	return GR_TRUE;
}

GRAPI	grBoolean GRCC grSymbol_ListAddSymbol(grSymbol_List *HL, grSymbol *HS)
{
	SymListElt *		NewElt;
	grSymbol_ListRec *	List;

	assert(HL);
	assert(HL->List);
	assert(HS);
	assert(HS->Symbol);

	List = HL->List;

	NewElt = grRam_Allocate(sizeof(*NewElt));
	if	(!NewElt)
		return GR_FALSE;

	NewElt->Next = List->Elts;
	NewElt->Symbol = HS->Symbol;

	List->Elts = NewElt;
	List->CurrentIndex = -1;
	List->Current = NULL;

	return GR_TRUE;
}

GRAPI	void GRCC grSymbol_ListRemoveSymbol(grSymbol_List *HL, grSymbol *HS)
{
	grSymbol_ListRec *	L;
	SymListElt			Head;
	SymListElt *		Elts;
	SymListElt *		Temp;
	grSymbol_Rec *		S;

	assert(HL);
	assert(HL->List);
	assert(HS);
	assert(HS->Symbol);

	L = HL->List;

	Head.Symbol = NULL;
	Head.Next = L->Elts;

	S = HS->Symbol;

	Elts = &Head;

	while	(Elts->Next->Symbol != S)
	{
		Elts = Elts->Next;
		assert(Elts->Next);
	}

	Temp = Elts->Next;
		   Elts->Next = Elts->Next->Next;
	if	(Elts == &Head)
		L->Elts = Elts->Next;

	if	(++L->SymbolTable->GCAbleOperationCount > GCABLE_OP_COUNT_THRESHHOLD)
		grSymbol_TableCollectGarbage(L->SymbolTable);

	assert(Temp);
	assert(Temp->Symbol == S);
	grRam_Free(Temp);
}

#if 1
#ifdef	_DEBUG
#if 0
static	void RefCountString(const grSymbol *Sym, char *Buff)
{
	char *	p;

	if	(Sym->Symbol->Qualifier)
		RefCountString(Sym->Symbol->Qualifier, Buff);
	else
		*Buff = 0;

	p = Buff + strlen(Buff);
//	sprintf(p, "%d:", Sym->RefCount);
}
#endif

#if 1
grBoolean grSymbol_Dump(const grSymbol *HSym, grVFile *File)
{
	char 	Buff[256];
	grSymbol_GetFullName(HSym, Buff, sizeof(Buff));
#if 0
{
	char	RFString[256];
	RefCountString(Sym, RFString);
	return grVFile_Printf(File, "  %s (%s)\n", Buff, RFString);
}
#else
	return grVFile_Printf(File, "  %s\n", Buff);
#endif
}
#else
static	int	indent;
grBoolean grSymbol_Dump1(grSymbol_Rec *Sym)
{
	int	i;

	assert(Sym);

	for	(i = 0; i < indent; i++)
		printf(" ");
	printf("%s\n", Sym->Name);

	if	(Sym->Flags & SYMBOL_MARKED)
		return GR_TRUE;

	Sym->Flags |= SYMBOL_MARKED;
	indent += 2;
	grSymbol_Walk(Sym, grSymbol_Dump1, WalkListWithoutReference);
	indent -= 2;
	return GR_TRUE;
}

grBoolean grSymbol_Dump(const grSymbol *HSym, grVFile *File)
{
	indent = 0;
	printf("---\n");
//	printf("%s\n", grSymbol_GetName(Sym));
//	grSymbol_Walk((grSymbol *)Sym, grSymbol_Dump1, WalkListWithoutReference);
	grSymbol_Dump1(HSym->Symbol);
//	grSymbol_ClearMarks((grSymbol **)&Sym);
	grSymbol_ClearMarks(HSym->Symbol);
	return GR_TRUE;
}
#endif

static	int	__cdecl CmpSyms(const void *p1, const void *p2)
{
	grSymbol_Rec *	S1;
	grSymbol_Rec *	S2;
	grSymbol		HS1;
	grSymbol		HS2;
	char 		Buff1[1024];
	char 		Buff2[1024];

	S1 = *(grSymbol_Rec **)p1;
	S2 = *(grSymbol_Rec **)p2;

	HS1.Symbol = S1;
	HS2.Symbol = S2;
	grSymbol_GetFullName(&HS1, Buff1, sizeof(Buff1));
	grSymbol_GetFullName(&HS2, Buff2, sizeof(Buff2));
	return strcmp(Buff1, Buff2);
}

grBoolean grSymbol_TableDump(const grSymbol_Table *ST, grVFile *File, grBoolean SortNames)
{
	int	i;
	int	MaxLength;
	int	MinLength;
	int	NumEmpty;
	int	TotalSyms;

	if	(File)
	{
		grVFile_Printf(File, "Dump of Symbol Table\n");
		grVFile_Printf(File, "--------------------\n");
	}
	else
	{
		printf("Dump of Symbol Table\n");
		printf("--------------------\n");
	}
	MaxLength = -1;
	MinLength = 100000;
	TotalSyms = 0;
	NumEmpty = 0;
	for	(i = 0; i < NUMHASHBUCKETS; i++)
	{
		SymListElt *	Elts;
		int				BucketCount;
		grSymbol		LocalSym;

		BucketCount = 0;
	
		Elts = ST->Symbols[i]->List->Elts;
		if	(File)
			grVFile_Printf(File, "Bucket %d:\n", i);
		else
			printf("Bucket %d:\n", i);
		if	(SortNames == GR_FALSE)
		{
			while	(Elts)
			{
				LocalSym.Symbol = Elts->Symbol;
				grSymbol_Dump(&LocalSym, File);
				BucketCount++;
				Elts = Elts->Next;
			}
		}
		else
		{
			grSymbol_Rec **	SortedSyms;
			int			j;

			while	(Elts)
			{
				BucketCount++;
				Elts = Elts->Next;
			}
			SortedSyms = grRam_Allocate(sizeof(*SortedSyms) * BucketCount);
			if	(!SortedSyms)
				return GR_FALSE;
			Elts = ST->Symbols[i]->List->Elts;
			j = 0;
			while	(Elts)
			{
				SortedSyms[j++] = Elts->Symbol;
				Elts = Elts->Next;
			}
			qsort(SortedSyms, BucketCount, sizeof(*SortedSyms), CmpSyms);
			for	(j = 0; j < BucketCount; j++)
			{
				grSymbol	LocalSym;
				LocalSym.Symbol = SortedSyms[j];
				grSymbol_Dump(&LocalSym, File);
			}
			grRam_Free(SortedSyms);
		}
		TotalSyms += BucketCount;
		if	(MinLength > BucketCount)
			MinLength = BucketCount;
		if	(MaxLength < BucketCount)
			MaxLength = BucketCount;
		if	(BucketCount == 0)
			NumEmpty++;
	}

	if	(File)
	{
		return grVFile_Printf(File, "Max Length: %d\nMin Length: %d\nTotal Syms: %d\nAvg Syms: %4.2f\nNum Empty: %d", MaxLength, MinLength, TotalSyms, (float)TotalSyms / (float)NUMHASHBUCKETS, NumEmpty);
	}
	else
	{
		printf("Max Length: %d\nMin Length: %d\nTotal Syms: %d\nAvg Syms: %4.2f\nNum Empty: %d", MaxLength, MinLength, TotalSyms, (float)TotalSyms / (float)NUMHASHBUCKETS, NumEmpty);
		return GR_TRUE;
	}
}
#endif

#endif
