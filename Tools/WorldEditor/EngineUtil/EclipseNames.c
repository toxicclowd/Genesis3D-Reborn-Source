/****************************************************************************************/
/*  ECLIPSENAMES.C                                                                      */
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
#include	"symbol.h"
#include	"eclipsenames.h"

static	grSymbol *GRCC AcquireSymbol(grSymbol_Table *ST, grSymbol *Qualifier, const char *Name, grSymbol_Type Type)
{
	grSymbol *	Symbol;

	Symbol = grSymbol_TableFindSymbol(ST, Qualifier, Name);
	if	(Symbol)
		return Symbol;

	Symbol = grSymbol_Create(ST, Qualifier, Name, Type);
	if	(Symbol)
	{
		grSymbol_Destroy(&Symbol);
		Symbol = grSymbol_TableFindSymbol(ST, Qualifier, Name);
	}
	return Symbol;
}

static	const char *Names[] =
{
	"StructureFields",
	"DefaultValue",
	"Types",
	"TypeDefinitions"
};

static	grSymbol_Type Types[] =
{
	GR_SYMBOL_TYPE_LIST,
	GR_SYMBOL_TYPE_VOID,
	GR_SYMBOL_TYPE_VOID,
	GR_SYMBOL_TYPE_LIST,
};

grSymbol *grEclipseNames(grSymbol_Table *ST, grEclipseNames_Id Id)
{
	grSymbol *	Qualifier;

	Qualifier = AcquireSymbol(ST, NULL, "Eclipse", GR_SYMBOL_TYPE_VOID);
	if	(!Qualifier)
		return NULL;

	return AcquireSymbol(ST, Qualifier, Names[Id], Types[Id]);
}

