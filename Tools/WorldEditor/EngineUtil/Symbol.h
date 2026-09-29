/****************************************************************************************/
/*  SYMBOL.H                                                                            */
/*                                                                                      */
/*  Author: Eli Boling                                                                  */
/*  Description: Generic symbol table/property list interface                           */
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
#ifndef	SYMBOL_H
#define	SYMBOL_H

#include	"basetype.h"
#include	"vfile.h"

#ifdef	__cplusplus
extern "C" {
#endif

//--------------------------------
// Types
//--------------------------------

typedef	enum
{
	GR_SYMBOL_TYPE_INT,				// Basic integer type
	GR_SYMBOL_TYPE_STRING,			// Basic char *
	GR_SYMBOL_TYPE_FLOAT,			// Basic float type
	GR_SYMBOL_TYPE_VEC3D,			// grVec3d by value
	GR_SYMBOL_TYPE_COLOR,			// GR_RGBA (RGB only) by value
	GR_SYMBOL_TYPE_BOOLEAN,			// grBoolean
	GR_SYMBOL_TYPE_ENUM,			// An enumeration
	GR_SYMBOL_TYPE_PVOID,			// pointer to void (application side only)
	GR_SYMBOL_TYPE_LIST,			// List of symbols
	GR_SYMBOL_TYPE_SYMBOL,			// Another symbol
//	GR_SYMBOL_TYPE_OBJECT,			// Special tag to distinguish object instances (hack?)

	GR_SYMBOL_TYPE_MODEL,			// Pointer to a grModel
	GR_SYMBOL_TYPE_PORTAL,			// Pointer to a grPortal

	GR_SYMBOL_TYPE_VOID				// No type at all

}	grSymbol_Type;

typedef	struct	grSymbol		grSymbol;

typedef	struct	grSymbol_Table	grSymbol_Table;

typedef struct	grSymbol_List	grSymbol_List;

//--------------------------------
// APIs
//--------------------------------

GRAPI	grSymbol_Table * GRCC grSymbol_TableCreate(void);
	// Creates a symbol table

//GRAPI	const grSymbol_List *GRCC grSymbol_TableGetQualifiedSymbolList(const grSymbol_Table *ST, const grSymbol *Qualifier);
GRAPI	grSymbol_List *GRCC grSymbol_TableGetQualifiedSymbolList(const grSymbol_Table *ST, const grSymbol *Qualifier);

GRAPI	void GRCC grSymbol_TableCreateRef(grSymbol_Table *ST);
	// Refs a symbol table.

GRAPI	grSymbol_Table *GRCC grSymbol_TableCreateFromFile(grVFile *File);
	// Reloads a symbol table from a file.

GRAPI	grBoolean GRCC grSymbol_TableWriteToFile(const grSymbol_Table *ST, grVFile *File);
	// Writes a symbol table to a file

GRAPI	void GRCC grSymbol_TableDestroy(grSymbol_Table **ST);
	// Destroys the symbol table

GRAPI	grSymbol *GRCC grSymbol_TableFindSymbol(grSymbol_Table *ST, grSymbol *Qualifier, const char *Name);
	// Finds the given symbol in the symbol table.  The resulting symbol, if found, is NOT
	// referenced.

GRAPI	grSymbol *GRCC grSymbol_Create(grSymbol_Table *ST, grSymbol *Qualifier, const char *Name, grSymbol_Type Type);
	// Creates a symbol with the given qualifier in the given symbol table.  The symbol type
	// is primarily used to distinguish symbols that are going to be used as properties, so
	// that we can reliable retrieve the data from the property.

GRAPI	void GRCC grSymbol_CreateRef(grSymbol *S, grSymbol **Result);
	// References a symbol.

GRAPI	void GRCC grSymbol_Destroy(grSymbol **S);
	// Destroys a symbol.

GRAPI	void GRCC grSymbol_TableCollectGarbage(grSymbol_Table *ST);
	// Sweeps for orphaned symbols, and reclaims the lost memory (garbage collects).

GRAPI	grBoolean GRCC grSymbol_Compare(const grSymbol *S1, const grSymbol *S2);
	// Compares two symbols to see if they are the same.  You must use this function,
	// since you are essentially guaranteed that references to the same symbol will always
	// be different pointers, even though the same symbols occupy the same storage.  This
	// is due to the reference model that we are keeping for the symbol table.

GRAPI	grBoolean GRCC grSymbol_Rename(grSymbol *Symbol, const char *NewName);
	// Renames a symbol,  Returns GR_TRUE on success, GR_FALSE on failure.

GRAPI	grSymbol *GRCC grSymbol_TableGetSymbol(grSymbol_Table *ST, grSymbol *Qualifier, const char *Name);
	// Finds a symbol by name with a given qualifier from the given symbol table.

GRAPI	grBoolean GRCC grSymbol_TableRemoveSymbol(grSymbol_Table *ST, grSymbol *Symbol);
	// Removes a symbol from the symbol table

GRAPI	grSymbol_Type GRCC grSymbol_GetType(const grSymbol *Sym);
	// Gets the type of a symbol.

GRAPI	grSymbol *GRCC grSymbol_GetQualifier(const grSymbol *Sym);
	// Gets the qualifier of a symbol.

GRAPI	grBoolean GRCC grSymbol_SetProperty(grSymbol *S, grSymbol *Property, const void *Data, int DataLength, grSymbol_Type Type);
	// Sets a property on a symbol.  The type of the Property symbol must match the passed type,
	// or this API will fail.

GRAPI	grBoolean GRCC grSymbol_GetProperty(
	grSymbol *	Sym,
	grSymbol *	Property,
	void *				Data,
	int 				DataLength,
	grSymbol_Type 		Type);
	// Sets a property from a symbol.  The type of the Property symbol must match the passed type,
	// or this API will fail.

GRAPI	grBoolean GRCC grSymbol_CopyProperty(
	grSymbol *Dest,
	grSymbol *DestProp,
	grSymbol *Src,
	grSymbol *SrcProp);
	// Copies the property data from one property field to another.  The values are
	// copied directly, so if you are copying a list, you get a reference count bump
	// on the list, not a true copy of the list, so be careful here.

GRAPI	grBoolean GRCC grSymbol_GetEnumValue(const grSymbol *S, int *Value);
	// Fast mechanism for getting the numeric enumeration value for a symbol.
GRAPI	grBoolean GRCC grSymbol_SetEnumValue(grSymbol *S, int Value);
	// Fast mechanism for setting the numeric enumeration value for a symbol.
	
GRAPI	const char *GRCC grSymbol_GetName(const grSymbol *S);
	// Gets the name for the symbol.  You can hold this pointer for as long as
	// the symbol exists, but you must not alter it under any circumstances.

GRAPI	grBoolean	GRCC grSymbol_GetFullName(const grSymbol *S, char *Buff, int MaxLen);
	// Gets the qualified name of a symbol.  This is mostly as a debugging/informational aid.

GRAPI	grSymbol_List *	GRCC grSymbol_ListCreate(grSymbol_Table *ST);
	// Creates a symbol list.  Symbol lists that are set as property values on a symbol
	// will be written to file and streamed back in automatically by a symbol table.

GRAPI	void GRCC grSymbol_ListCreateRef(grSymbol_List *L, grSymbol_List **Result);
	// References a symbol list.

GRAPI	void GRCC grSymbol_ListDestroy(grSymbol_List **L);
	// Destroys a symbol list.

//grSymbol_List *grSymbol_ListNext(const grSymbol_List *L);
	// Iterates a symbol list.

GRAPI	grSymbol *GRCC grSymbol_ListGetSymbol(const grSymbol_List *L, int Index);
	// Gets the current symbol from this list.

GRAPI	grBoolean GRCC grSymbol_ListAddSymbol(grSymbol_List *L, grSymbol *S);
	// Adds a symbol to the symbol list.

GRAPI	void GRCC grSymbol_ListRemoveSymbol(grSymbol_List *L, grSymbol *S);
	// Removes a symbol from the given symbol list.

#if 0
// Pseudo code for what an editor might do to build classes:

typedef struct	FieldDefs
{
	char *			Name;
	grSymbol_Type	Type;
	char *			DefaultValue;
}	FieldDefs;

static	FieldDefs	CoronaFields[] =
{
	{ "MinRadius", 	GR_SYMBOL_TYPE_INT, 	"20" },
	{ "MaxRadius", 	GR_SYMBOL_TYPE_INT, 	"40" },
	{ "FadeTime", 	GR_SYMBOL_TYPE_FLOAT, 	"0.5" },
	{ "Color", 		GR_SYMBOL_TYPE_COLOR, 	"255 255 255" },
	{ "Origin", 	GR_SYMBOL_TYPE_VEC3D, 	"0 0 0" },
};

grSymbol *	CreateType(grSymbol_Table *ST, const char *Name, const FieldDefs *Fields, int FieldCount)
{
	grSymbol *			TypeSym;
	grSymbol *			Qualifier;
	grSymbol *			FieldSym;
	grSymbol_List *		FieldList;
	int					i;

	// Create a qualifier for the type, and intern the type symbol in that package
	Qualifier = grSymbol_Create(ST, NULL, Name, GR_SYMBOL_TYPE_VOID);
	TypeSym = grSymbol_Create(ST, Qualifier, Name, GR_SYMBOL_TYPE_VOID);
	grSymbol_Destroy(&Qualifier);

	// Create a symbol list to hold the field definition symbols.
	FieldList = grSymbol_ListCreate();

	/*
		Rip through the fields, creating symbols in a list for each field name, and
		setting the default value property on each symbol.  The default value property
		will be used to initialize new entities that are added to the world.
	*/
	for	(i = 0; i < FieldCount; i++)
	{
		// Create and symbol for the current field.  Use the type symbol for the package for
		// namespace control.
		FieldSym = grSymbol_Create(ST, TypeSym, Fields[i].Name, Fields[i].Type);
		switch	(Fields[i].Type)
		{
			int		Integer;
			gefloat	Float;
			grVec3d	Vector;
			GR_RGBA	Color;

		case	GR_SYMBOL_TYPE_INT:
			Integer = atoi(Fields[i].DefaultValue);
			grSymbol_SetProperty(FieldSym, 
									grEclipseNames(ST, GR_ECLIPSENAMES_FIELDDEFAULTVALUE),
									&Integer, sizeof(Integer), GR_SYMBOL_TYPE_INT);
			break;

		case	GR_SYMBOL_TYPE_FLOAT:
			Integer = atof(Fields[i].DefaultValue);
			grSymbol_SetProperty(FieldSym, 
									grEclipseNames(ST, GR_ECLIPSENAMES_FIELDDEFAULTVALUE),
									&Float, sizeof(Float), GR_SYMBOL_TYPE_FLOAT);

		case	GR_SYMBOL_TYPE_COLOR:
			sscanf(Fields[i].DefaultValue, "%f %f %f", &Color.r, &Color.g, &Color.b);
			grSymbol_SetProperty(FieldSym, 
									grEclipseNames(ST, GR_ECLIPSENAMES_FIELDDEFAULTVALUE),
									&COlor, sizeof(Color), GR_SYMBOL_TYPE_COLOR);
			break;

		case	GR_SYMBOL_TYPE_VEC3D:
			sscanf(Fields[i].DefaultValue, "%f %f %f", &Vector.X, &Vector.Y, &Vector.Z);
			grSymbol_SetProperty(FieldSym, 
									grEclipseNames(ST, GR_ECLIPSENAMES_FIELDDEFAULTVALUE),
									&Vector, sizeof(Vector), GR_SYMBOL_TYPE_VEC3D);
			break;

		etc...
		}
		FieldList = grSymbol_ListAddSymbol(FieldList, FieldSym);
	}
	grSymbol_SetProperty(TypeSym,
							grEclipseNames(ST, GR_ECLIPSENAMES_STRUCTUREFIELDS),
							&FieldList, sizeof(FieldList), GR_SYMBOL_TYPE_LIST);

	return TypeSym;
}

// Sample code for adding an entity as we know it to the world
AddEntity(grSymbol_Table *ST, grSymbol *Type, const char *Name)
{
	grSymbol *		Entity;
	grSymbol_List *	List;

	Entity = grSymbol_Create(ST, grSymbol_GetQualifier(Type), Name, GR_SYMBOL_TYPE_OBJECT);
	grSymbol_GetProperty(Type,
							grEclipseNames(ST, GR_ECLIPSENAMES_STRUCTUREFIELDS),
							&List, sizeof(List), GR_SYMBOL_TYPE_LIST);
	
	/*
		We've got the list of fields from the type symbol.  Run through each field,
		setting the default value for each field into the new entity symbol.  Here,
		we are using not the default value symbol name for the target, but the actual
		field name, since we are dealing with an instance, not a type now.
	*/
	while	(List)
	{
		grSymbol *	FieldSym;

		FieldSym = grSymbol_ListGetSymbol(List);
		grSymbol_CopyProperty(Entity,
								 FieldSym,
								 FieldSym,
								 grEclipseNames(ST, GR_ECLIPSENAMES_FIELDDEFAULTVALUE));
		
		List = grSymbol_ListNext(List);
	}
}

//  Then, on the application side:

InitCoronas(grSymbol *ST)
{
	grSymbol *		TypeName;
	grSymbol_List *	List;
	int				Count;
	int				i;

	TypeName = grSymbol_TableGetSymbol(ST, NULL, "Corona");
	List = grSymbol_TableGetQualifiedSymbolList(ST, TypeName);
	while	(List);
	{
		int				MinRadius;
		grSymbol *		Entity;

		Entity = grSymbol_ListGetSymbol(List);
		grSymbol_GetProperty(Entity,
								grSymbol_TableGetSymbol(ST, TypeName, "MinRadius"),
								&MinRadius, sizeof(MinRadius), GR_SYMBOL_TYPE_INT);
		// do something with the field values, ...
		List = grSymbol_ListNext(List);
	}
}

#endif

#ifdef	__cplusplus
}
#endif

#endif

