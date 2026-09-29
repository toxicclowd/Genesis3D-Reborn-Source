/****************************************************************************************/
/*  grMemAllocInfo.c                                                                    */
/*                                                                                      */
/*  Author: David Eisele                                                                */
/*  Description: Extended memoryleak debugging module                                   */
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
/*  This file was not part of the original Jet3D, released December 12, 1999.           */
/*                                                                                      */
/****************************************************************************************/

// Public dependents
#include "grMemAllocInfo.h"

#ifndef GR_DEACTIVATE_JMAI

//#define GR_MEMALLOC_USE_NO_JE_RAM		// first thought malloc/free is better/faster, but
										// there wouldn't be any check on overwriting, etc.
										// use this option with caution

// Private dependents
#include <string.h>
#include <crtdbg.h>
#include <malloc.h>
#include <assert.h>
#include "grMemAllocInfo._h"	
#include "cpu.h"
#include "Ram.h"
#include "Util.h"

extern void grRam_jMAI_Lock();
extern void grRam_jMAI_UnLock();

////////////////////////////////////////////////////////////////////////////////////////////////
//
// private vars....
//
////////////////////////////////////////////////////////////////////////////////////////////////

static char						PreferencesFileName[255]={"Preferences.jMAI"};
static char						MainInfoFileName[255]={"Current_dump.txt"};
static char						MainFreedFileName[255]={"Freed_dump.txt"};
static grMemAllocInfoChain		*MainInfoChain=NULL;
static grMemAllocInfoBreakPoint *MainBreakPoints=NULL;
static grMemAllocInfoFileChain	*MainFileChain=NULL;
static grMemAllocInfoPtr		*MainPtrArray=NULL;
static grMemAllocInfoPtr		*MainLastPtr=NULL;
static grMemAllocInfoChain		*LastUsedInfo=NULL;
static uint32					jMAI_Flags=0;
static uint32					MainChainNum=0;
static grMemAllocInfoPtr*		(*jMAI_pushPtrChain)(uint32 MoveNum)=NULL;


////////////////////////////////////////////////////////////////////////////////////////////////
//
// public functions....
//
////////////////////////////////////////////////////////////////////////////////////////////////

GRAPI void GRCC grMemAllocInfo_Create(const char *FName)
{
	if (jMAI_Flags&jMAI_CREATED)
		grMemAllocInfo_Destroy();

	strcpy(PreferencesFileName, FName);

	#ifdef GR_MEMALLOC_USE_NO_JE_RAM
		MainPtrArray=(grMemAllocInfoPtr*)malloc((1<<6)*sizeof(grMemAllocInfoPtr));
	#else
		grRam_jMAI_Lock();
		MainPtrArray=GR_RAM_ALLOCATE_ARRAY(grMemAllocInfoPtr, 1<<6);
		grRam_jMAI_UnLock();
	#endif
	MainPtrArray->NamePtr=(char*)(0);
	MainPtrArray->Data=NULL;

	MainLastPtr=MainPtrArray+1;
	MainLastPtr->Data=NULL;
	MainLastPtr->NamePtr=(char*)(-1);

	MainLastPtr++;
	MainChainNum=2;

	LastUsedInfo=NULL;
	MainBreakPoints=NULL;
	MainFileChain=NULL;
	MainInfoChain=NULL;
	jMAI_Flags=jMAI_CREATED;
	#if 0
		grCPU_GetInfo();
		if ( grCPU_Features & GR_CPU_HAS_MMX )
			jMAI_pushPtrChain=jMAI_pushPtrChain_mmx;
		else
			jMAI_pushPtrChain=jMAI_pushPtrChain_x86;
	#endif
}

GRAPI void GRCC grMemAllocInfo_Activate()
{
	assert(jMAI_Flags&jMAI_CREATED);
	grMemAllocInfo_LoadFileNames();
	grMemAllocInfo_LoadFlags();
	grMemAllocInfo_LoadBreakpoints();
	grMemAllocInfo_UpdateAllBreakPoints();	// If internal structure is already created, we can apply the BP
	grMemAllocInfo_LoadActiveFileFlags();
	grMemAllocInfo_UpdateAllActiveFlags();
	jMAI_Flags|=jMAI_ACTIVE;
}

GRAPI void GRCC grMemAllocInfo_DeActivate(grBoolean DojMAIReport) 
{
	grMemAllocInfoChain	*InfoChain;
	assert(jMAI_Flags&jMAI_CREATED);
	jMAI_Flags&=~jMAI_ACTIVE;
	if (DojMAIReport==GR_TRUE)
	{
		grMemAllocInfo_Report(GR_FALSE);
		if(jMAI_Flags&jMAI_SAVE_ON_FREE) 
			grMemAllocInfo_Report(GR_TRUE);
	}
	InfoChain=MainInfoChain;
	while (InfoChain!=NULL)
	{
		grMemAllocInfo_KillDataChain(&(InfoChain->DataChain));
		grMemAllocInfo_KillDataChain(&(InfoChain->FreedChain));
		InfoChain=InfoChain->Next;
	}
	grMemAllocInfo_KillBreakPoints(&MainBreakPoints);
	grMemAllocInfo_KillFileChain(&MainFileChain);
}

GRAPI void GRCC grMemAllocInfo_Destroy()
{
	assert(!(jMAI_Flags & jMAI_ACTIVE));
	if (jMAI_Flags&jMAI_CREATED)
	{
		grMemAllocInfo_KillInfoChain(&MainInfoChain);
		grMemAllocInfo_KillPtrArray(&MainPtrArray);
		LastUsedInfo=NULL;
		MainLastPtr=NULL;
		MainChainNum=jMAI_Flags=0;
		jMAI_Flags&=~jMAI_CREATED;
	}
}

GRAPI uint32 GRCC grMemAllocInfo_GetFlags()
{
	return jMAI_Flags;
}

GRAPI void GRCC grMemAllocInfo_FileReport(const char *FName, const char *DumpFile, grBoolean FreedMemoryReport)
{
	grMemAllocInfoChain		*InfoChain=NULL;
	uint32					TotalPointers,CurPtr;
	uint32					TotalMem,CurMem;
	FILE					*fp;

	assert(jMAI_Flags&jMAI_CREATED);
	fp=fopen(DumpFile,"a+");
	if(fp == NULL)
		fp=stdout;

	if (FreedMemoryReport)
		jMAI_RPT1(fp, "\n jMAI :  Current freed Pointers (%s)\n\n", FName);
	else
		jMAI_RPT1(fp, "\n jMAI :  Current registered Pointers (%s)\n\n", FName);

	InfoChain=MainInfoChain;
	if(InfoChain!=NULL && InfoChain->IsActive==GR_FALSE)
	{
		jMAI_RPT0(fp, "WARNING: This file was EXCLUDED!!!!\n\n");
		fclose(fp);
		return;
	}
	TotalPointers=0;TotalMem=0;
	while (InfoChain!=NULL)
	{
		if (!stricmp(FName, InfoChain->AllocFileName))
		{
			grMemAllocInfo_InfoReport_(fp, InfoChain, FreedMemoryReport, &CurPtr, &CurMem);
			TotalPointers+=CurPtr;
			TotalMem+=TotalMem;
		}
		InfoChain=InfoChain->Next;
	}
	if(FreedMemoryReport)
	{
		jMAI_RPT1(fp, "Freed pointers : %9u  ", TotalPointers);
		jMAI_RPT1(fp, "Freed memory   : %9u\n", TotalMem);
	}
	else
	{
		jMAI_RPT1(fp, "Active pointers : %9u  ", TotalPointers);
		jMAI_RPT1(fp, "Occupied memory : %9u\n", TotalMem);
	}

	fclose(fp);
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// dll_public functions....
//
////////////////////////////////////////////////////////////////////////////////////////////////

void GRCC grMemAllocInfo_Alloc(uint32 Size, void *Pointer, const char *FName, int LNr)
{
	grMemAllocInfoChain		*InfoChain;
	grMemAllocInfoDataChain *NewMemChain=NULL;

	if(!(jMAI_Flags&jMAI_ACTIVE)) return;

	if (Pointer==NULL)
		return; 

	InfoChain=grMemAllocInfo_GetInfoChain(FName, LNr);

	if(InfoChain==NULL) return;

	#ifdef GR_MEMALLOC_USE_NO_JE_RAM
		NewMemChain=(grMemAllocInfoDataChain*)malloc(sizeof(grMemAllocInfoDataChain));
	#else
		grRam_jMAI_Lock();
		NewMemChain=GR_RAM_ALLOCATE_STRUCT(grMemAllocInfoDataChain);
		grRam_jMAI_UnLock();
	#endif
	assert(NewMemChain != NULL);
	// Connect Chains
	NewMemChain->Prev=NULL;
	NewMemChain->Next=InfoChain->DataChain;
	if (InfoChain->DataChain!=NULL) InfoChain->DataChain->Prev=NewMemChain;

	// Set Infos
	NewMemChain->CurrentPointer=Pointer;

	NewMemChain->CallNr=++(InfoChain->CurrCallNr);
	NewMemChain->AllocSize=Size;
	
	NewMemChain->FreeFileName=NULL;
	NewMemChain->FreeLineNr=-1;

	NewMemChain->ReAllocFileName=NULL;
	NewMemChain->ReAllocLineNr=-1;
	NewMemChain->ReAllocSize=-1;

	//paste new chain into InfoChain
	InfoChain->DataChain=NewMemChain;

	*(grMemAllocInfoDataChain**)Pointer=NewMemChain;		// SaveChain
	NewMemChain->Root=InfoChain;				// We want to go back ;=)

}

void GRCC grMemAllocInfo_Realloc(uint32 Size, void *Pointer, const char *FName, int LNr)
{
	grMemAllocInfoDataChain *ReAllocInfo;
	
	if(!(jMAI_Flags&jMAI_ACTIVE)) return;

	ReAllocInfo=*(grMemAllocInfoDataChain**)Pointer;		// LoadChain

	if (ReAllocInfo==NULL || Pointer==NULL)
		return;

	ReAllocInfo->CurrentPointer=Pointer;
	#pragma warning( push )
		#pragma warning(disable : 4090)	// I know we save an const ptr, but it will only be comp.ed ;=)
		ReAllocInfo->ReAllocFileName=FName;
	#pragma warning( pop )
	ReAllocInfo->ReAllocLineNr=LNr;
	ReAllocInfo->ReAllocSize=Size;
}



void GRCC grMemAllocInfo_Free(void *Pointer, const char *FName, int LNr)
{
	grMemAllocInfoChain		*InfoChain, *FreedChain=NULL;
	grMemAllocInfoDataChain *FreeInfo;
	
	if(!(jMAI_Flags&jMAI_ACTIVE)) return;

	FreeInfo=(grMemAllocInfoDataChain *)Pointer;

	if (FreeInfo==NULL)
		return;

	InfoChain=FreeInfo->Root;
	
	if(jMAI_Flags&jMAI_SAVE_ON_FREE)
	{
		FreeInfo->CurrentPointer=NULL;
		#pragma warning( push )
			#pragma warning(disable : 4090)	// I know we save an const ptr, but it will only be comp.ed ;=)
			FreeInfo->FreeFileName=FName;
		#pragma warning( pop )
		FreeInfo->FreeLineNr=LNr;

		if(FreeInfo->Next!=NULL)
			FreeInfo->Next->Prev=FreeInfo->Prev;
		if(FreeInfo->Prev!=NULL)
			FreeInfo->Prev->Next=FreeInfo->Next;
		else
			InfoChain->DataChain=FreeInfo->Next;

		FreeInfo->Prev=NULL;
		if(InfoChain->FreedChain!=NULL)
			InfoChain->FreedChain->Prev=FreeInfo;
		FreeInfo->Next=InfoChain->FreedChain;
		InfoChain->FreedChain=FreeInfo;
	} 
	else
	{
		if(FreeInfo->Prev==NULL)
		{
			InfoChain->DataChain=FreeInfo->Next;
			if (InfoChain->DataChain!=NULL)
				InfoChain->DataChain->Prev=NULL;
		} else
		{
			FreeInfo->Prev->Next=FreeInfo->Next;
			if (FreeInfo->Next!=NULL) FreeInfo->Next->Prev=FreeInfo->Prev;
		}
		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			free(FreeInfo);
		#else
			grRam_Free(FreeInfo);
		#endif
	}
}

grBoolean GRCC grMemAllocInfo_BREAK(const void *Data)
{
	if ((jMAI_Flags&jMAI_ACTIVE) && (Data!=NULL))
	{
		grMemAllocInfoDataChain		*InfoChain=(grMemAllocInfoDataChain*)Data;
		grMemAllocInfoBPCallChain	*CallChain;
		if (InfoChain->Root->GlobalBreak==GR_TRUE)
			return GR_TRUE;
		for (CallChain = InfoChain->Root->BreakPoints; CallChain != NULL; CallChain=CallChain->Next)
			if (CallChain->CallNr==InfoChain->CallNr) return GR_TRUE;
	}
	return GR_FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//
// private functions....
//
////////////////////////////////////////////////////////////////////////////////////////////////

static grMemAllocInfoChain* GRCC grMemAllocInfo_GetInfoChain(const char *FName, int LNr)
{
	grMemAllocInfoChain	*InfoChain;
	if( (LastUsedInfo!=NULL) && (FName==LastUsedInfo->AllocFileName) && (LNr==LastUsedInfo->AllocLineNr) )
		return LastUsedInfo;
	else
	{
		if ((InfoChain=grMemAllocInfo_GetChain(FName, LNr, GR_FALSE))!=NULL)
			return (LastUsedInfo=InfoChain);
		else
			return NULL;
	}
}

static grMemAllocInfoChain* grMemAllocInfo_GetChain(const char *FName, int LNr, grBoolean FreedChain)
{
	grMemAllocInfoChain *InfoChain;
	grMemAllocInfoPtr	 *ChainPtr;
	uint32				MoveNum;
	// Do some qsort/qsearch stuff
	_asm
	{
		mov   esi,MainPtrArray
		mov   eax,FName
		mov   ebx,0
		mov   ecx,MainChainNum
		dec   ecx
		mov   edx,ecx
		shr   edx,1
SearchLoop:
		cmp   ebx,edx
		je    NotFoundPtr
		mov   edi,edx
		shl   edi,4
		add   edi,esi
		cmp   eax,dword ptr [edi]
		je    FoundNamePtr
		jb    MinSide
		jmp   short MaxSide
FoundNamePtr:	// in dlls the pointer isn't unique :(
		mov   eax,LNr
		cmp   eax,dword ptr [edi+4]
		je    FoundPtr
		mov   eax,FName	// Restore eax
		jb    MinSide
		//jmp short MaxSide
MaxSide:
		mov   ebx,edx
		add   edx,ecx
		shr   edx,1
		jmp   short SearchLoop
MinSide:
		mov   ecx,edx
		add   edx,ebx
		shr   edx,1
		jmp   short SearchLoop
NotFoundPtr:
		mov   ecx,MainChainNum
		dec   ecx
		sub   ecx,ebx
		mov   MoveNum,ecx
		mov   edi,0
		mov   InfoChain,edi
		jmp short SearchLoopEnd
FoundPtr:
		mov   eax,dword ptr [edi+8]
		mov   InfoChain,eax
SearchLoopEnd:
	}
	if (InfoChain==NULL && FreedChain==GR_FALSE)
	{

		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			InfoChain=(grMemAllocInfoChain*)malloc(sizeof(grMemAllocInfoChain));
		#else
			grRam_jMAI_Lock();
			InfoChain=GR_RAM_ALLOCATE_STRUCT(grMemAllocInfoChain);
			grRam_jMAI_UnLock();
		#endif

		assert(InfoChain != NULL);
		InfoChain->AllocLineNr=LNr;
		InfoChain->DataChain=NULL;
		InfoChain->FreedChain=NULL;
		InfoChain->BreakPoints=NULL;
		InfoChain->Next=MainInfoChain;
		InfoChain->CurrCallNr=0;
		if (jMAI_Flags&jMAI_EXCLUDE_ALL_FILES)
			InfoChain->IsActive=GR_FALSE;
		else
			InfoChain->IsActive=GR_TRUE;
		InfoChain->GlobalBreak=GR_FALSE;
		MainInfoChain=InfoChain;

		if (((++MainChainNum)&((1<<6)-1))==0)
		{
			#ifdef GR_MEMALLOC_USE_NO_JE_RAM
				MainPtrArray=(grMemAllocInfoPtr*)realloc(MainPtrArray, (MainChainNum+(1<<6))*sizeof(grMemAllocInfoPtr));
			#else
				MainPtrArray=GR_RAM_REALLOC_ARRAY(MainPtrArray, grMemAllocInfoPtr, MainChainNum+(1<<6));
			#endif
			MainLastPtr=MainPtrArray+(MainChainNum-1);
		}
		// Put new entry in right position
		// Hmm, in Bitmap/Compression mmx is disabled => Do it the same
		// Also the mmx-code should be corrected and tested
		#if 0
			ChainPtr=jMAI_pushPtrChain(MoveNum);
		#else
			ChainPtr=jMAI_pushPtrChain_x86(MoveNum);
		#endif

		ChainPtr->Data=InfoChain;
		ChainPtr->LNr=LNr;
		#pragma warning( push )
			#pragma warning(disable : 4090)	// I know we save an const ptr, but it will only be comp.ed ;=)
			ChainPtr->NamePtr=FName;
			InfoChain->AllocFileName=FName;
		#pragma warning( pop )

		// Set up Breakpoints...
		if (MainBreakPoints!=NULL)
			grMemAllocInfo_UpdateBreakPoints(InfoChain);
		// Set up ActiveFlag
		if (MainFileChain!=NULL)
			grMemAllocInfo_UpdateFileFlags(InfoChain);
	}
	if ((InfoChain!=NULL) && (InfoChain->IsActive==GR_FALSE))
		return NULL;
	else
		return InfoChain;
}

static void GRCC grMemAllocInfo_KillDataChain(grMemAllocInfoDataChain **DataChain)
{
	grMemAllocInfoDataChain *NextChain;
	while((*DataChain)!=NULL)
	{
		NextChain=(*DataChain)->Next;
		if((*DataChain)->CurrentPointer!=NULL)
		{
			assert(*(grMemAllocInfoDataChain**)((*DataChain)->CurrentPointer)==(*DataChain));
			*(uint32*)((*DataChain)->CurrentPointer)=0; // Kill ref., or bad thing could happen
		}

		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			free(*DataChain);
		#else
			grRam_Free(*DataChain);
		#endif

		*DataChain=NextChain;
	}
}

static void GRCC grMemAllocInfo_KillInfoChain(grMemAllocInfoChain **InfoChain)
{
	grMemAllocInfoChain *NextChain;
	while((*InfoChain)!=NULL)
	{
		NextChain=(*InfoChain)->Next;
		grMemAllocInfo_KillDataChain(&(*InfoChain)->DataChain);
		grMemAllocInfo_KillDataChain(&(*InfoChain)->FreedChain);
		grMemAllocInfo_KillBPCallChain(&(*InfoChain)->BreakPoints);

		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			free(*InfoChain);
		#else
			grRam_Free(*InfoChain);
		#endif

		*InfoChain=NextChain;
	}
}

static void GRCC grMemAllocInfo_KillPtrArray(grMemAllocInfoPtr **PtrChain)
{
	#ifdef GR_MEMALLOC_USE_NO_JE_RAM
		free(*PtrChain);(*PtrChain)=NULL;
	#else
		grRam_Free(*PtrChain);
	#endif
}

static void GRCC grMemAllocInfo_KillBPCallChain(grMemAllocInfoBPCallChain **CallChain)
{
	grMemAllocInfoBPCallChain *NextCall;
	while (*CallChain!=NULL)
	{
		NextCall=(*CallChain)->Next;
		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			free(*CallChain);
		#else
			grRam_Free(*CallChain);
		#endif
		*CallChain=NextCall;
	}
}

static void GRCC grMemAllocInfo_KillFileChain(grMemAllocInfoFileChain **FChain)
{
	grMemAllocInfoFileChain *NextChain;
	while((*FChain)!=NULL)
	{
		NextChain=(*FChain)->Next;
		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			free((*FChain)->FileName);
			free(*FChain);
		#else
			grRam_Free((*FChain)->FileName);
			grRam_Free(*FChain);
		#endif
		*FChain=NextChain;
	}
}

static void GRCC grMemAllocInfo_KillBreakPoints(grMemAllocInfoBreakPoint **BChain)
{
	grMemAllocInfoBreakPoint *NextChain;
	while((*BChain)!=NULL)
	{
		NextChain=(*BChain)->Next;
		#ifdef GR_MEMALLOC_USE_NO_JE_RAM
			free((*BChain)->AllocFileName);
			free(*BChain);
		#else
			grRam_Free((*BChain)->AllocFileName);
			grRam_Free(*BChain);
		#endif
		*BChain=NextChain;
	}
}

static void GRCC grMemAllocInfo_AddBreakpoint(const char *FileName, int LineNr, uint32 CallNr)
{
	grMemAllocInfoBreakPoint *BreakPoint=NULL;
	char					 *FName;

	assert(jMAI_Flags&jMAI_CREATED);

	#ifdef GR_MEMALLOC_USE_NO_JE_RAM
		FName=(char*)malloc(strlen(FileName)+1);
		memcpy(FName, FileName, strlen(FileName)+1);
		BreakPoint=(grMemAllocInfoBreakPoint*)malloc(sizeof(grMemAllocInfoBreakPoint));
	#else
		FName=Util_StrDup(FileName);
		grRam_jMAI_Lock();
		BreakPoint=GR_RAM_ALLOCATE_STRUCT(grMemAllocInfoBreakPoint);
		grRam_jMAI_UnLock();
	#endif
	assert(BreakPoint != NULL);
	BreakPoint->AllocFileName=FName;
	BreakPoint->AllocLineNr=LineNr;
	BreakPoint->CallNr=CallNr;
	BreakPoint->Next=MainBreakPoints;
	if (MainBreakPoints!=NULL)
		MainBreakPoints->Prev=BreakPoint;
	BreakPoint->Prev=NULL;
	MainBreakPoints=BreakPoint;
}

static void GRCC grMemAllocInfo_LoadBreakpoints()
{
	FILE*	fp;
	char	FileName[255];	// This should be enough
	char    Dummy[255];		// Same...
	int		LineNr=-1;
	uint32  CallNr;
	char*   DummyPtr=NULL;

	Util_GetAppPath(FileName,255);
	strcat(FileName, PreferencesFileName);
	fp = fopen(FileName, "r");
	if(fp == NULL)
		return;
	while (!feof(fp) && (DummyPtr==NULL))
	{
		if (fgets(Dummy,255,fp)==NULL) continue;

		DummyPtr=strstr(Dummy, "<BREAKPOINTS>");
	}
	while (!feof(fp))
	{
		if (fgets(Dummy,255,fp)==NULL) continue;

		DummyPtr=strstr(Dummy, "</BREAKPOINTS>");
		if (DummyPtr!=NULL) break;

		DummyPtr=strchr(Dummy, '(');
		if (DummyPtr==NULL)
			DummyPtr=strchr(Dummy, '\n');

		strncpy(FileName, Dummy, DummyPtr-Dummy);
		*(FileName+(uint32)(DummyPtr-Dummy))='\0';

		if (sscanf(DummyPtr,"( %u )",&LineNr)!=1)
			LineNr=0;

		DummyPtr=strchr(Dummy,'[');
		if( (DummyPtr == NULL) || (sscanf(DummyPtr,"[ %u ]",&CallNr)!=1) )
			CallNr=0;

		grMemAllocInfo_AddBreakpoint(FileName, LineNr, CallNr);
	}
	fclose(fp);
}

static void GRCC grMemAllocInfo_UpdateAllBreakPoints()
{
	grMemAllocInfoChain	*InfoChain=MainInfoChain;
	while (InfoChain!=NULL)
	{
		grMemAllocInfo_UpdateBreakPoints(InfoChain);
		InfoChain=InfoChain->Next;
	}
}

static void GRCC grMemAllocInfo_UpdateBreakPoints(grMemAllocInfoChain *InfoChain)
{
	grMemAllocInfoBreakPoint	*BreakPoint=MainBreakPoints;
	grMemAllocInfoBreakPoint	*NextBreakPoint;

	// Destroy old ones
	InfoChain->GlobalBreak=GR_FALSE;
	grMemAllocInfo_KillBPCallChain(&InfoChain->BreakPoints);

	while (BreakPoint!=NULL)
	{
		NextBreakPoint=BreakPoint->Next;
		if (!stricmp(InfoChain->AllocFileName, BreakPoint->AllocFileName) &&
			(BreakPoint->AllocLineNr==0 || BreakPoint->AllocLineNr==InfoChain->AllocLineNr))
		{
			if (BreakPoint->CallNr==0)
				InfoChain->GlobalBreak=GR_TRUE;
			else
			{
				grMemAllocInfoBPCallChain	*BPChain;

				#ifdef GR_MEMALLOC_USE_NO_JE_RAM
					BPChain=(grMemAllocInfoBPCallChain*)malloc(sizeof(grMemAllocInfoBPCallChain));
				#else
					grRam_jMAI_Lock();
					BPChain=grRam_Allocate(sizeof(grMemAllocInfoBPCallChain));
					grRam_jMAI_UnLock();
				#endif
				BPChain->CallNr=BreakPoint->CallNr;
				BPChain->Next=InfoChain->BreakPoints;
				InfoChain->BreakPoints=BPChain;
			}
			// Now kill the BP in MainBreakPoints, if it isn't global...
			if (BreakPoint->AllocLineNr!=0)
			{
				if (BreakPoint->Prev==NULL)
				{
					MainBreakPoints=BreakPoint->Next;
					if (BreakPoint->Next!=NULL)
						BreakPoint->Next->Prev=NULL;
				} else
				{
					BreakPoint->Prev->Next=BreakPoint->Next;
					if (BreakPoint->Next!=NULL)
						BreakPoint->Next->Prev=BreakPoint->Prev;
				}
	
				#ifdef GR_MEMALLOC_USE_NO_JE_RAM
					free(BreakPoint->AllocFileName);
					free(BreakPoint);
				#else
					grRam_Free(BreakPoint->AllocFileName);
					grRam_Free(BreakPoint);
				#endif

			}
		}
		BreakPoint=NextBreakPoint;
	}
}

static void GRCC grMemAllocInfo_AddFile(const char *FileName, uint32 Flags)
{
	grMemAllocInfoFileChain	*FileChain=NULL;
	char					*FName;

	assert(jMAI_Flags&jMAI_CREATED);

	#ifdef GR_MEMALLOC_USE_NO_JE_RAM
		FName=(char*)malloc(strlen(FileName)+1);
		memcpy(FName, FileName, strlen(FileName)+1);
		FileChain=(grMemAllocInfoFileChain*)malloc(sizeof(grMemAllocInfoFileChain));
	#else
		FName=Util_StrDup(FileName);
		grRam_jMAI_Lock();
		FileChain=GR_RAM_ALLOCATE_STRUCT(grMemAllocInfoFileChain);
		grRam_jMAI_UnLock();
	#endif
	assert(FileChain != NULL);
	FileChain->FileName=FName;
	FileChain->Flags=Flags;
	FileChain->Next=MainFileChain;
	if (MainFileChain!=NULL)
		MainFileChain->Prev=FileChain;
	FileChain->Prev=NULL;
	MainFileChain=FileChain;
}

static void GRCC grMemAllocInfo_LoadActiveFileFlags()
{
	FILE*	fp;
	char	FileName[255];	// This should be enough
	char*	Dummy=NULL;

	Util_GetAppPath(FileName,255);
	strcat(FileName, PreferencesFileName);
	fp = fopen(FileName, "r");
	if(fp == NULL)
		return;
	while (!feof(fp) && (Dummy==NULL))
	{
		if (fgets(FileName,255,fp)==NULL) continue;

		Dummy=strstr(FileName, "<INCLUDE>");
	}
	while (!feof(fp))
	{
		if (fgets(FileName,255,fp)==NULL) continue;

		Dummy=strstr(FileName, "</INCLUDE>");
		if (Dummy!=NULL)
			break;

		Dummy=strchr(FileName, '\n');
		if (Dummy!=NULL)
			*Dummy='\0';
		else
			continue;

		grMemAllocInfo_AddFile(FileName, jMAI_INCLUDE);
	}
	fseek(fp,0,SEEK_SET);Dummy=NULL;
	while (!feof(fp) && (Dummy==NULL))
	{
		if (fgets(FileName,255,fp)==NULL) continue;

		Dummy=strstr(FileName, "<EXCLUDE>");
	}
	while (!feof(fp))
	{
		if (fgets(FileName,255,fp)==NULL) continue;

		Dummy=strstr(FileName, "</EXCLUDE>");
		if (Dummy!=NULL)
			break;

		Dummy=strchr(FileName, '\n');
		if (Dummy!=NULL)
			*Dummy='\0';
		else
			continue;

		grMemAllocInfo_AddFile(FileName, jMAI_EXCLUDE);
	}

	fclose(fp);
}

static void GRCC grMemAllocInfo_UpdateAllActiveFlags()
{
	grMemAllocInfoChain	*InfoChain=MainInfoChain;
	while (InfoChain!=NULL)
	{
		grMemAllocInfo_UpdateFileFlags(InfoChain);
		InfoChain=InfoChain->Next;
	}
}

static void GRCC grMemAllocInfo_UpdateFileFlags(grMemAllocInfoChain *InfoChain)
{
	grMemAllocInfoFileChain	*FileChain=MainFileChain;
	grMemAllocInfoFileChain	*NextFile;

	while (FileChain!=NULL)
	{
		NextFile=FileChain->Next;
		if (!stricmp(InfoChain->AllocFileName, FileChain->FileName))
		{
			if (FileChain->Flags&jMAI_INCLUDE)
				InfoChain->IsActive=GR_TRUE;
			if (FileChain->Flags&jMAI_EXCLUDE)
				InfoChain->IsActive=GR_FALSE;
		}
		FileChain=NextFile;
	}
}

static void GRCC grMemAllocInfo_Report(grBoolean FreedMemoryReport)
{
	grMemAllocInfoChain		*InfoChain;
	grMemAllocInfoPtr		*InfoPtr;
	int32					TotalMem, TotalPointers, CurrPointers, CurrMem, FMem, FPointers;
	FILE					*fp;
	char					AppPath[255],*LastFName;
	grBoolean				WasActive;

	assert(jMAI_Flags&jMAI_CREATED);
	Util_GetAppPath(AppPath,255);
	if(FreedMemoryReport)
	{
		strcat(AppPath, MainFreedFileName);
		fp=fopen(AppPath, "a+");
		if(fp == NULL)
			fp=stdout;
		jMAI_RPT0(fp, "\n jMAI :  Current freed Pointers \n\n");
	}
	else
	{
		strcat(AppPath, MainInfoFileName);
		fp=fopen(AppPath, "a+");
		if(fp == NULL)
			fp=stdout;
		jMAI_RPT0(fp, "\n jMAI :  Current registered Pointers \n\n");
	}

	InfoPtr=MainPtrArray+1;
	TotalMem=0;TotalPointers=0;FMem=0;FPointers=0;
	LastFName=NULL;WasActive=GR_TRUE;
	while(InfoPtr->Data!=NULL)
	{
		InfoChain=InfoPtr->Data;
		if(LastFName!=NULL && stricmp(LastFName, InfoChain->AllocFileName))
		{
			if (WasActive==GR_FALSE)
				jMAI_RPT1(fp, "%s was excluded!\n\n", LastFName);
			else if (FMem||FPointers)
			{
				jMAI_RPT0(fp, "--------------------------------------------------------------------------------------------------------\n");
				if(FreedMemoryReport)
				{
					jMAI_RPT1(fp, "Freed pointers : %.9u  ", FPointers);
					jMAI_RPT1(fp, "Freed memory   : %.9u\n\n", FMem);
				}
				else
				{
					jMAI_RPT1(fp, "Active pointers : %.9u  ", FPointers);
					jMAI_RPT1(fp, "Occupied memory : %.9u\n\n", FMem);
				}
				FMem=0;FPointers=0;
			}
		}
		if (InfoChain->IsActive==GR_TRUE)
			{
			grMemAllocInfo_InfoReport_(fp,InfoChain,FreedMemoryReport,&CurrPointers,&CurrMem);
			TotalPointers+=CurrPointers;FPointers+=CurrPointers;
			TotalMem+=CurrMem;FMem+=CurrMem;
		}
		WasActive=InfoChain->IsActive;
		LastFName=InfoChain->AllocFileName;
		
		InfoPtr++;
	}

	if(FMem||FPointers)
	{
		if (WasActive==GR_FALSE)
			jMAI_RPT1(fp, "%s was excluded!\n\n", LastFName);
		else 
		{
			jMAI_RPT0(fp, "--------------------------------------------------------------------------------------------------------\n");
			if(FreedMemoryReport)
			{
				jMAI_RPT1(fp, "Freed pointers : %.9u  ", FPointers);
				jMAI_RPT1(fp, "Freed memory   : %.9u\n\n", FMem);
			}
			else
			{
				jMAI_RPT1(fp, "Active pointers : %.9u  ", FPointers);
				jMAI_RPT1(fp, "Occupied memory : %.9u\n\n", FMem);
			}
		}
	}

	jMAI_RPT0(fp, "========================================================================================================\n");
	if(FreedMemoryReport)
	{
		jMAI_RPT1(fp,     "Total freed pointers : %.9u  ", TotalPointers);
		jMAI_RPT1(fp,     "Total freed memory   : %.9u\n", TotalMem);
	}
	else
	{
		jMAI_RPT1(fp, "Total active pointers : %.9u  ", TotalPointers);
		jMAI_RPT1(fp, "Total occupied memory : %.9u\n", TotalMem);
	}
	fclose(fp);
}

static void GRCC grMemAllocInfo_InfoReport_(FILE *fp, grMemAllocInfoChain *InfoChain, grBoolean ShowFreed,
											 uint32 *PtrNum, uint32 *PtrMem)
{
	grMemAllocInfoDataChain *MemInfo;
	uint32					TotalPointers;
	uint32					TotalMem;

	TotalPointers=0;TotalMem=0;

	assert(jMAI_Flags&jMAI_CREATED);
	assert(InfoChain!=NULL);
	if (ShowFreed==GR_FALSE)
		MemInfo=InfoChain->DataChain;
	else
		MemInfo=InfoChain->FreedChain;
	while(MemInfo!=NULL)
	{
		TotalPointers++;
		if(MemInfo->AllocSize>0)
		{
			jMAI_RPT4(fp, "%s(%5u):   %9u byte(s)   allocated [%9u]\n",InfoChain->AllocFileName, InfoChain->AllocLineNr, MemInfo->AllocSize, MemInfo->CallNr);
			if(MemInfo->ReAllocSize>0)
			{
				jMAI_RPT3(fp, "%s(%5u):   %9u byte(s) reallocated\n", MemInfo->ReAllocFileName, MemInfo->ReAllocLineNr, MemInfo->ReAllocSize);
				TotalMem+=MemInfo->ReAllocSize;
				
			} else TotalMem+=MemInfo->AllocSize;
			if(MemInfo->FreeLineNr>0)
				jMAI_RPT2(fp, "%s(%5u):   Memory freed\n", MemInfo->FreeFileName, MemInfo->FreeLineNr);
		}
		else if(MemInfo->ReAllocSize>0)
		{
			jMAI_RPT0(fp, "!Reallocation without allocation!\n");
			jMAI_RPT3(fp, "%s(%5u):   %9u byte(s) reallocated\n", MemInfo->ReAllocFileName, MemInfo->ReAllocLineNr, MemInfo->ReAllocSize);
			TotalMem+=MemInfo->ReAllocSize;
			if(MemInfo->FreeLineNr>0)
				jMAI_RPT2(fp, "%s(%5u):   Memory freed\n", MemInfo->FreeFileName, MemInfo->FreeLineNr);
		}
		else if(MemInfo->FreeLineNr>0)
		{
			jMAI_RPT0(fp, "!Freeing without allocation!\n");
			jMAI_RPT2(fp,         "%s(%5u):   Memory freed\n", MemInfo->FreeFileName, MemInfo->FreeLineNr);
		}
		MemInfo=MemInfo->Next;
	}

	*PtrMem=TotalMem;
	*PtrNum=TotalPointers;
}

static void GRCC grMemAllocInfo_LoadFlags()
{
	FILE*	fp;
	char    Dummy[255];
	char*   DummyPtr=NULL;

	Util_GetAppPath(Dummy,255);
	strcat(Dummy, PreferencesFileName);
	fp = fopen(Dummy, "r");
	if(fp == NULL)
		return;
	while (!feof(fp) && (DummyPtr==NULL))
	{
		if (fgets(Dummy,255,fp)==NULL) continue;

		DummyPtr=strstr(Dummy, "<FLAGS>");
	}
	while (!feof(fp))
	{
		if (fgets(Dummy,255,fp)==NULL) continue;

		DummyPtr=strstr(Dummy, "</FLAGS>");
		if (DummyPtr!=NULL) break;

		DummyPtr=strstr(Dummy, "ExcludeAllFiles=");
		if (DummyPtr!=NULL)
		{
			DummyPtr=strchr(Dummy, '1');
			if (DummyPtr!=NULL)
				jMAI_Flags|=jMAI_EXCLUDE_ALL_FILES;
		}

		DummyPtr=strstr(Dummy, "SaveOnFree=");
		if (DummyPtr!=NULL)
		{
			DummyPtr=strchr(Dummy, '1');
			if (DummyPtr!=NULL)
				jMAI_Flags|=jMAI_SAVE_ON_FREE;
		}
	}
	fclose(fp);
}

static void GRCC grMemAllocInfo_LoadFileNames()
{
	FILE*	fp;
	char    Dummy[255];
	char	*DummyPtr=NULL,*FStart=NULL,*FEnd=NULL;

	Util_GetAppPath(Dummy,255);
	strcat(Dummy, PreferencesFileName);
	fp = fopen(Dummy, "r");
	if(fp == NULL)
		return;
	while (!feof(fp) && (DummyPtr==NULL))
	{
		if (fgets(Dummy,255,fp)==NULL) continue;

		DummyPtr=strstr(Dummy, "<NAMES>");
	}
	while (!feof(fp))
	{
		if (fgets(Dummy,255,fp)==NULL) continue;

		DummyPtr=strstr(Dummy, "</NAMES>");
		if (DummyPtr!=NULL) break;

		DummyPtr=strstr(Dummy, "dumpfile=");
		if (DummyPtr!=NULL)
		{
			FStart=strchr(DummyPtr,'\"');
			if (FStart!=NULL)
			{
				FEnd=strchr(FStart+1,'\"');
				if (FEnd!=NULL)
				{
					*FEnd='\0';
					strncpy(MainInfoFileName,FStart+1,FEnd-FStart);
				}
			}
		}

		DummyPtr=strstr(Dummy, "freedfile=");
		if (DummyPtr!=NULL)
		{
			FStart=strchr(DummyPtr,'\"');
			if (FStart!=NULL)
			{
				FEnd=strchr(FStart+1,'\"');
				if (FEnd!=NULL)
				{
					*FEnd='\0';
					strncpy(MainFreedFileName,FStart+1,FEnd-FStart);
				}
			}
		}
	}
	fclose(fp);
}

static grMemAllocInfoPtr *jMAI_pushPtrChain_x86(uint32 MoveNum)
{
	_asm
	{
		pushf
		std
		mov   edi,MainLastPtr
		add   edi,12
		mov   esi,edi
		sub   esi,16
		add   edi,4
		mov   MainLastPtr,edi	// Push this one
		sub   edi,4
		mov   ecx,MoveNum
		shl   ecx,2
		rep   movsd
		mov   eax,esi
		add   eax,4
		popf
	}
}

static grMemAllocInfoPtr* jMAI_pushPtrChain_mmx(uint32 MoveNum)
{

	// TODO : CORRECT ME!!!!!
	uint32    SaveErg;
	grCPU_EnterMMX();
	_asm
	{
		mov   eax,MainLastPtr
		add   eax,8
		mov   MainLastPtr,eax
		sub   eax,8
		mov   ecx,MoveNum
// What is better? : incremental (cache) or decremental (instructions) ?!?
		sub   eax,8
		shr   ecx,1
		jnc   MMX_Test
		// Copy last 8 bytes
		movq  mm0,[eax+8]
		movq  [eax+16],mm0
		sub   eax,8
		// Copy next 16 bytes...
MMX_Test:
		shr   ecx,1
		jnc   MMX_Test2
		movq  mm0,[eax+16]
		movq  mm1,[eax+24]
		movq  [eax+32],mm0
		movq  [eax+40],mm1
		sub   eax,16
MMX_Test2:
		or    ecx,ecx
		jz    MMX_End
MMX_Loop:
		movq  mm0,[eax+24]
		movq  mm1,[eax+16]
		movq  [eax+40],mm0
		movq  mm2,[eax+8]
		movq  [eax+32],mm1
		movq  mm3,[eax]
		movq  [eax+24],mm2
		movq  [eax+16],mm3
		sub   eax,32
		Loop  MMX_Loop
		add   eax,32
MMX_End:
		mov   SaveErg,eax
	}
	grCPU_LeaveMMX();
	return (grMemAllocInfoPtr*)SaveErg;
}

#endif
