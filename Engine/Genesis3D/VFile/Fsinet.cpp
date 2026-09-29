/****************************************************************************************/
/*  FSINET.CPP                                                                          */
/*                                                                                      */
/*  Author: Eli Boling                                                                  */
/*  Description: Internet file system implementation                                    */
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
#include	<windows.h>
#include	<objbase.h>
#include	<urlmon.h>

#include	<stdio.h>
#include	<assert.h>

#include	"basetype.h"
#include	"ram.h"
#include	"vfile.h"
#include	"vfile._h"
#include	"ThreadQueue.h"

#include	"fsinet.h"

#include	"log.h"
#include	"ThreadLog.h"

typedef	enum
{
	STATE_READING,
	STATE_ERROR,
	STATE_DATACOMPLETE,
	STATE_COMPLETE,
}	INetFileState;
	// These should be in temporal order (except for Error) : 
	//	Reading,DataComplete,Complete

struct	Statistics
{
	int		MaxBlockSize;
	int		MinBlockSize;
	int		TotalBlocksReceived;
};

struct	INetFile : public IBindStatusCallback
{
	INetFile();
	~INetFile();
	grBoolean				Initialize(grBoolean InitAsDirectory);

	STDMETHODIMP			QueryInterface(REFIID riid,  void **ppvObj);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

	STDMETHODIMP			OnStartBinding( 
		/* [in] */ DWORD dwReserved,
		/* [in] */ IBinding __RPC_FAR *pib);
	
	STDMETHODIMP			GetPriority( 
		/* [out] */ LONG __RPC_FAR *pnPriority);
	
	STDMETHODIMP	OnLowResource( 
		/* [in] */ DWORD reserved);
	
	STDMETHODIMP	OnProgress( 
		/* [in] */ ULONG ulProgress,
		/* [in] */ ULONG ulProgressMax,
		/* [in] */ ULONG ulStatusCode,
		/* [in] */ LPCWSTR szStatusText);
	
	STDMETHODIMP	OnStopBinding( 
		/* [in] */ HRESULT hresult,
		/* [unique][in] */ LPCWSTR szError);
	
	STDMETHODIMP	GetBindInfo( 
		/* [out] */ DWORD __RPC_FAR *grfBINDF,
		/* [unique][out][in] */ BINDINFO __RPC_FAR *pbindinfo);
	
	STDMETHODIMP	OnDataAvailable( 
		/* [in] */ DWORD grfBSCF,
		/* [in] */ DWORD dwSize,
		/* [in] */ FORMATETC __RPC_FAR *pformatetc,
		/* [in] */ STGMEDIUM __RPC_FAR *pstgmed);
	
	STDMETHODIMP	OnObjectAvailable( 
		/* [in] */ REFIID riid,
		/* [iid_is][in] */ IUnknown __RPC_FAR *punk);

	long MemoryFilePos(void)
	{
		return ClientPos + TrueFileBase;
	}

	uint32 BytesAvailable(void)
	{
		return (CheckedForHints == GR_TRUE) ? (long)m_dwTotalRead - MemoryFilePos() : 0;
	}

	grBoolean	GetFullPath(char *Buff, int MaxLen) const;

	unsigned int	Signature;
	INetFileState	State;
	char *			FullPath;
	IBinding *		Binding;
	DWORD			RefCount;
	IStream *		m_spStream;
	DWORD			m_dwTotalRead;
	grVFile *		MemoryFile;
	CRITICAL_SECTION	Lock;

	grThreadQueue_Job *	Job;

	grBoolean		IgnoreHints;		// Should we ignore hints completely?
	grBoolean		HasHints;			// Does this file have hints
	grBoolean		CheckedForHints;	// We have decided whether or not we have hints
	long			TrueFileBase;		// Position of first user bits (past hint)
	long			ClientPos;			// Client's file position
	grVFile_Hints	Hints;
	grVFile *		HintsFile;
	long			HintsPosition;		// Relative position into the hints

	grBoolean		IsDirectory;		// Only set if this is really a directory
	INetFile *		Parent;				// Set to outer 

	grVFile_RemoteFileStatistics		Stats;
};

#ifdef	__BORLANDC__
#define	grRam_Allocate	malloc
#define	grRam_Free	free
#endif

//	"IF01"
#define	INETFILE_SIGNATURE	0x31304649

#define	CHECK_HANDLE(H)	assert(H);assert(H->Signature == INETFILE_SIGNATURE);

INetFile::INetFile()
{
	// Don't use memset here.  It smashes the vtable entry (!?).
	FullPath = NULL;
	Binding = NULL;
	RefCount = 0;
	m_spStream = NULL;
	m_dwTotalRead = 0;
	MemoryFile = NULL;
	Signature = 0;
	TrueFileBase = 0;
	ClientPos = 0;
	Job = NULL;
	IgnoreHints = GR_FALSE;
	HasHints = GR_FALSE;
	CheckedForHints = GR_FALSE;
	Hints.HintData = NULL;
	Hints.HintDataLength = 0;
	HintsPosition = 0;
	State = STATE_READING;
	IsDirectory = GR_FALSE;
	Parent = NULL;
	memset(&Stats, 0, sizeof(Stats));
	InitializeCriticalSection(&Lock);
}

INetFile::~INetFile()
{

	// CB : this wait must be outside the critical section !
	if	(Job)
		grThreadQueue_WaitOnJob(Job, GR_THREADQUEUE_STATUS_COMPLETED);

	EnterCriticalSection(&Lock);

	if	(Job)
		grThreadQueue_JobDestroy(&Job);

	Signature = 0;
	if	(FullPath)
		grRam_Free(FullPath);

	if	(HasHints == GR_TRUE)
	{
		assert(Hints.HintData != NULL);
		assert(Hints.HintDataLength > 0);
		grRam_Free(Hints.HintData);
	}

	if	(MemoryFile)
		grVFile_Close(MemoryFile);

	LeaveCriticalSection(&Lock);
	DeleteCriticalSection(&Lock);
}

// This function is a little pointless.  Should clean this up
grBoolean INetFile::Initialize(grBoolean InitAsDirectory)
{
	grVFile_MemoryContext	Context;

	// This function is only used for non-directory files.

	if	(InitAsDirectory == GR_FALSE)
	{
		memset(&Context, 0, sizeof(Context));
		MemoryFile = grVFile_OpenNewSystem(NULL, GR_VFILE_TYPE_MEMORY, NULL, &Context, GR_VFILE_OPEN_CREATE);
		if	(!MemoryFile)
			return GR_FALSE;
	}

	return GR_TRUE;
}

STDMETHODIMP INetFile::QueryInterface(REFIID riid,  void **ppvObj)
{
	*ppvObj = NULL;

	if	(riid == IID_IUnknown || riid == IID_IBindStatusCallback)
	{
		*ppvObj = this;
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE INetFile::AddRef(void)
{
	return ++RefCount;
}

ULONG STDMETHODCALLTYPE INetFile::Release(void)
{
	return --RefCount;
}

STDMETHODIMP INetFile::OnStartBinding(
	DWORD /*Reserved*/,
	IBinding __RPC_FAR *pib)
{
	Binding = pib;
	if	(Binding)
		Binding->AddRef();
	return S_OK;
}

STDMETHODIMP INetFile::GetPriority( 
            /* [out] */ LONG __RPC_FAR * /*pnPriority*/)
{
//	*pnPriority = THREAD_PRIORITY_NORMAL;
	return S_OK;
}
        
STDMETHODIMP INetFile::OnLowResource( 
            /* [in] */ DWORD /*reserved*/)
{
#pragma message ("FSInet : We're not nice about low resources")
	return S_OK;
}
        
STDMETHODIMP INetFile::OnProgress( 
            /* [in] */ ULONG /*ulProgress*/,
            /* [in] */ ULONG /*ulProgressMax*/,
            /* [in] */ ULONG /*ulStatusCode*/,
            /* [in] */ LPCWSTR /*szStatusText*/)
{
	return S_OK;
}
        
STDMETHODIMP INetFile::OnStopBinding( 
            /* [in] */ HRESULT /*hresult*/,
            /* [unique][in] */ LPCWSTR /*szError*/)
{
	if	(Binding)
	{
		Binding->Release();
		Binding = NULL;
	}
	State = STATE_COMPLETE;
	return S_OK;
}
        
STDMETHODIMP INetFile::GetBindInfo( 
            /* [out] */ DWORD __RPC_FAR *grfBINDF,
            /* [unique][out][in] */ BINDINFO __RPC_FAR *pbindinfo)
{
	ULONG	Size;

	*grfBINDF = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_NOWRITECACHE | BINDF_GETNEWESTVERSION;

	Size = pbindinfo->cbSize;
	memset(pbindinfo, 0, Size);
	pbindinfo->cbSize = Size;
	pbindinfo->dwBindVerb = BINDVERB_GET;
	
	return S_OK;
}
        
STDMETHODIMP INetFile::OnDataAvailable( 
            /* [in] */ DWORD grfBSCF,
            /* [in] */ DWORD dwSize,
            /* [in] */ FORMATETC __RPC_FAR * /*pformatetc*/,
            /* [in] */ STGMEDIUM __RPC_FAR *pstgmed)
{
	HRESULT hr = S_OK;

	if	(State == STATE_ERROR)
		return S_OK;

	EnterCriticalSection(&Lock);

	// Get the Stream passed
	if (grfBSCF & BSCF_FIRSTDATANOTIFICATION)
	{
		if (!m_spStream && pstgmed->tymed == TYMED_ISTREAM)
		{
			m_spStream = pstgmed->pstm;
			m_spStream->AddRef();
		}
	}

	DWORD dwRead = dwSize - m_dwTotalRead; // Minimum amount available that hasn't been read
	DWORD dwActuallyRead = 0;            // Placeholder for amount read during this pull

	// If there is some data to be read then go ahead and read them
	if	(m_spStream)
	{
		if	(dwRead > 0)
		{
			BYTE* 					pBytes;
			grVFile_MemoryContext	MemoryContext;

			ThreadLog_Printf("FSInet: got %d bytes on '%s'\n", dwRead, FullPath);
			// Update statistics
			Stats.TotalBlocksReceived++;
			if	(dwRead > (DWORD)(Stats.MaxBlockSize))
				Stats.MaxBlockSize = dwRead;
			if	(dwRead < (DWORD)(Stats.MinBlockSize))
				Stats.MinBlockSize = dwRead;

			if	(grVFile_Seek(MemoryFile, 0, GR_VFILE_SEEKEND) == GR_FALSE)
			{
				LeaveCriticalSection(&Lock);
				State = STATE_ERROR;
				return S_OK;
			}
			if	(grVFile_Seek(MemoryFile, dwRead, GR_VFILE_SEEKCUR) == GR_FALSE)
			{
				LeaveCriticalSection(&Lock);
				State = STATE_ERROR;
				return S_OK;
			}
			grVFile_UpdateContext(MemoryFile, &MemoryContext, sizeof(MemoryContext));
			pBytes = ((BYTE *)MemoryContext.Data) + MemoryContext.DataLength - dwRead;
			if (pBytes == NULL)
			{
				LeaveCriticalSection(&Lock);
				State = STATE_ERROR;
				return S_OK;
			}
			hr = m_spStream->Read(pBytes, dwRead, &dwActuallyRead);
			if	(!SUCCEEDED(hr))
			{
				LeaveCriticalSection(&Lock);
				State = STATE_ERROR;
				return S_OK;
			}
			m_dwTotalRead += dwActuallyRead;
			Stats.TotalBytesReceived = m_dwTotalRead;
		}
	}

	if (BSCF_LASTDATANOTIFICATION & grfBSCF)
	{
		ThreadLog_Printf("FSInet: last data notification for '%s'\n", FullPath);
		m_spStream->Release();
		State = STATE_DATACOMPLETE;
	}

	if	(CheckedForHints == GR_FALSE && State != STATE_ERROR)
	{
		grVFile_HintsFileHeader *	HintsHeader;

		if	(m_dwTotalRead > sizeof(HintsHeader))
		{
			grVFile_MemoryContext	MemoryContext;

			grVFile_UpdateContext(MemoryFile, &MemoryContext, sizeof(MemoryContext));
			HintsHeader = (grVFile_HintsFileHeader *)MemoryContext.Data;
			if	(HintsHeader->Signature != GR_VFILE_HINTSFILEHEADER_SIGNATURE)
			{
				CheckedForHints = GR_TRUE;
			}
			else
			{
				if	(HintsHeader->HintDataLength + sizeof(*HintsHeader) <= m_dwTotalRead)
				{
					Hints.HintDataLength = HintsHeader->HintDataLength;
					Hints.HintData = grRam_Allocate(HintsHeader->HintDataLength);
					if	(Hints.HintData)
					{
						grVFile_MemoryContext	Context;

#pragma message("FSInet : Need to clean up the hints file implementation")

						Context.Data = Hints.HintData;
						Context.DataLength = Hints.HintDataLength;
						memcpy(Hints.HintData, (char *)MemoryContext.Data + sizeof(*HintsHeader), HintsHeader->HintDataLength);
						TrueFileBase = sizeof(*HintsHeader) + HintsHeader->HintDataLength;
						HintsFile = grVFile_OpenNewSystem(NULL,
														  GR_VFILE_TYPE_MEMORY,
														  NULL,
														  &Context,
														  GR_VFILE_OPEN_READONLY);
						if	(!HintsFile)
							State = STATE_ERROR;
														  
						ThreadLog_Printf("FSInet: got hints for '%s'\n", FullPath);
						CheckedForHints = GR_TRUE;
						HasHints = GR_TRUE;
					}
					else
					{
						State = STATE_ERROR;
					}
				}
			}
		}
		else if	(State >= STATE_DATACOMPLETE)
		{
			// Total size of the file is smaller than the hints header!
			// Now why would anyone stream that?
			CheckedForHints = GR_TRUE;
		}
	}

	LeaveCriticalSection(&Lock);

	return hr;
}
        
STDMETHODIMP INetFile::OnObjectAvailable( 
            /* [in] */ REFIID /*riid*/,
            /* [iid_is][in] */ IUnknown __RPC_FAR * /*punk*/)
{
	assert(!"Should not be getting any objects");
	return S_OK;
}

grBoolean	INetFile::GetFullPath(char *Buff, int MaxLen) const
{
	int	Length;

	assert(FullPath);

	Length = strlen(FullPath) + 1;
	if	(Length > MaxLen)
		return GR_FALSE;

	// <> CB 2/23
	if ( strstr(FullPath,":/") ) // absolute path
	{
		strcpy(Buff, FullPath);
	}
	else // relative path
	{
		if	(Parent)
		{
			if	(Parent->GetFullPath(Buff, MaxLen - Length) == GR_FALSE)
				return GR_FALSE;
		}

		if ( strlen(Buff) > 0 )
		{
			char * EndPtr = Buff + strlen(Buff) - 1;
			if ( *EndPtr != '/' || EndPtr[-1] == ':' )
			{
				EndPtr[1] = '/';
				EndPtr[2] = 0;
			}
		}
		strcat(Buff, FullPath);
	}

	return GR_TRUE;
}

static	void *	GRCC FSINet_FinderCreate(
	grVFile *		/*FS*/,
	void *			/*Handle*/,
	const char *	/*FileSpec*/)
{
	return NULL;
}

static	grBoolean	GRCC FSINet_FinderGetNextFile(void * /*Handle*/)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_FinderGetProperties(void * /*Handle*/, grVFile_Properties * /*Props*/)
{
	return GR_FALSE;
}

static	void GRCC FSINet_FinderDestroy(void * /*Handle*/)
{
	assert(!"Not implemented");
}

WINOLEAPI CoInitializeEx(LPVOID reserved, DWORD flags);

void	MyOpenFile(grThreadQueue_Job *Job, void *Context)
{
	INetFile *	File;
	HRESULT		hr;
	char		AbsolutePath[_MAX_PATH + _MAX_PATH];
//#pragma message("FSInet : Not dealing with paths properly")

	File = (INetFile *)Context;

#if 0
// The code here is for free threaded downloads.  We ought to be able to make
// this work.
{
		HRESULT hr = S_OK;
		IBindCtx *	BindCtx;
		IMoniker *	Moniker;
		IStream *	Stream;
		LPWSTR		WideString;
		int			Length;

		CoInitializeEx(NULL, 0);

		assert(File->FullPath);
		Length = strlen(File->FullPath);

		WideString = (LPWSTR)malloc(Length * 2 + 100);
		::MultiByteToWideChar(CP_ACP, 0, File->FullPath, -1, WideString, Length * 2 + 100);

		hr = CreateURLMoniker(NULL, WideString, &Moniker);
		if (SUCCEEDED(hr))
//			hr = CreateBindCtx(0, &BindCtx);
			hr = CreateAsyncBindCtx(0, File, NULL, &BindCtx);

//		if (SUCCEEDED(hr))
//			hr = RegisterBindStatusCallback(BindCtx, NewFile, 0, 0L);
//		if	(SUCCEEDED(hr))
//		{
//			hr = CreateAsyncBindCtx(0, NewFile, NULL, &BindCtx);
//		}
		else
			Moniker->Release();

//		if (SUCCEEDED(hr))
//			hr = IsValidURL(BindCtx, WideString, 0);

		if (SUCCEEDED(hr))
			hr = Moniker->BindToStorage(BindCtx, 0, IID_IStream, (void**)&Stream);
//		assert (SUCCEEDED(hr));
}
#else
	CoInitializeEx(NULL, 2);
//	printf("[%p] about to open the file\n", File);
	AbsolutePath[0] = 0;
	if	(File->GetFullPath(AbsolutePath, sizeof(AbsolutePath)) == GR_TRUE)
	{
//		Log_Printf("FSInet : about to open : %s\n",AbsolutePath);
		ThreadLog_Printf("FSInet : about to open : %s\n", AbsolutePath);
		hr = URLOpenStream(NULL, AbsolutePath, 0, static_cast<IBindStatusCallback *>(File));
#pragma message ("FSInet : Need to make URLOpenStream err-or handling more robust")
#if 1
		if	(hr != 0)
		{
			File->State = STATE_ERROR;
			ThreadLog_Printf("FSInet : ERROR opening : %s\n", AbsolutePath);
		}
		else
		{
			ThreadLog_Printf("FSInet : opened        : %s\n", AbsolutePath);
		}
#endif
	}
	else
	{
		File->State = STATE_ERROR;
	}
#endif

	while	(File->State == STATE_READING)
	{
		MSG		Msg;

		PeekMessage(&Msg, NULL, 0, 0, PM_NOREMOVE);
	}

	if	(File->State == STATE_ERROR)
	{
		char	Buff[1024];
		sprintf(Buff, "Error opening %s\r\n", AbsolutePath);
		OutputDebugString(Buff);
	}
	else
	{
		char	Buff[1024];
		sprintf(Buff, "Done opening  %s\r\n", AbsolutePath);
		OutputDebugString(Buff);
	}
}

static	void *	GRCC FSINet_Open(
	grVFile *		/*FS*/,
	void *			Handle,
	const char *	Name,
	void *			/*Context*/,
	unsigned int 	OpenModeFlags)
{
	INetFile *	IFS;
	INetFile *	NewFile;
	int			Length;

	/*
		Apartment threading (single threaded) seems to work just fine for UrlOpenStream.
		If you use free threading, however, UrlOpenStream will not work.  If you use
		single threaded Apartment model (the default when using CoInitialize),
		IMoniker::BindToHost will fail with a not bindable error.  If you use free threading,
		then IMoniker::BindToHost will bind OK, and the callback will start getting
		requests, but then we fail further down the pike, and I haven't figured out
		why yet.  One problem with single threaded Apartment model is that we have to
		tickle the message queue a little or we don't get async downloads.  Hence the
		call to PeekMessage in this file.  Free threading is more efficient, and the
		file system has been built to deal with it, and as soon as we can get the
		binding to work, we should switch to it.  For now, we are single threaded
		apartment model on each download.
	*/
#pragma message ("FSInet : Really out to be free threaded")
//	CoInitializeEx(NULL, 2);
//	CoInitializeEx(NULL, 0);

	if	(!(OpenModeFlags & GR_VFILE_OPEN_READONLY))
		return NULL;
	if ( ! Name )	// <> CB 2/10
		return NULL;

	IFS = (INetFile *)Handle;
	CHECK_HANDLE(IFS);

	NewFile = new INetFile;
	if	(!NewFile)
		return NewFile;

	if	(OpenModeFlags & GR_VFILE_OPEN_DIRECTORY)
		NewFile->IsDirectory = GR_TRUE;

	if	(OpenModeFlags & GR_VFILE_OPEN_RAW)
		NewFile->IgnoreHints = GR_TRUE;

	if	(NewFile->Initialize(NewFile->IsDirectory) == GR_FALSE)
	{
		delete NewFile;
		return NULL;
	}

	Length = strlen(Name) + 2;
	NewFile->FullPath = (char *)grRam_Allocate(Length);
	if	(!NewFile->FullPath)
	{
		delete NewFile;
		return NULL;
	}

	memcpy(NewFile->FullPath, Name, Length - 1);
#if 0 // <> CB 2/10
	if	(NewFile->IsDirectory == GR_TRUE)
		strcat(NewFile->FullPath, "/");
#endif

	NewFile->Parent = IFS;
	NewFile->Signature = INETFILE_SIGNATURE;
	NewFile->State = STATE_READING;

	if	(NewFile->IsDirectory == GR_FALSE)
	{
		ThreadLog_Printf("FSInet : starting open thread for : %s\n", NewFile->FullPath);
		NewFile->Job = grThreadQueue_JobCreate(MyOpenFile, NewFile, NULL, 0x1000);
	#if 1 
		// we have to do this right now, because we don't have the emergency wait on job
		grThreadQueue_PollJobs();
	#endif
	}

	return (void *)NewFile;
}

static	void *	GRCC FSINet_OpenNewSystem(
	grVFile *		/*Base*/,
	const char *	Name,
	void *			/*Context*/,
	unsigned int 	OpenModeFlags)
{
	INetFile *	File;
	int			Length;

	if	(!(OpenModeFlags & (GR_VFILE_OPEN_READONLY | GR_VFILE_OPEN_DIRECTORY)))
		return NULL;

	File = new INetFile;
	if	(!File)
		return (void *)File;
	
	File->IsDirectory = GR_TRUE;
	if	(File->Initialize(File->IsDirectory) == GR_FALSE)
	{
		delete File;
		return NULL;
	}

	if ( ! Name ) Name = "http://"; //<> CB 2/10

	Length = strlen(Name) + 2;
	File->FullPath = (char *)grRam_Allocate(Length);
	if	(File->FullPath == NULL)
	{
		delete File;
		return NULL;
	}
	memcpy(File->FullPath, Name, Length - 1);
#if 0  //<> CB 2/10
	strcat(File->FullPath, "/");
#endif

	File->Signature = INETFILE_SIGNATURE;
	File->State = STATE_READING;

	return (void *)File;
}

static	grBoolean	GRCC FSINet_UpdateContext(
	grVFile *		/*FS*/,
	void *			Handle,
	void *			Context,
	int 			ContextSize)
{
	INetFile *						File;
	grVFile_RemoteFileStatistics *	Stats;
	
	File = (INetFile *)Handle;

	if	(File->IsDirectory == GR_TRUE)
		return GR_FALSE;
	
	CHECK_HANDLE(File);

	Stats = (grVFile_RemoteFileStatistics *)Context;
	if	(ContextSize != sizeof(*Stats))
		return GR_FALSE;

	if	(File->State == STATE_ERROR)
		return GR_FALSE;

	*Stats = File->Stats;

	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_Close(void *Handle)
{
	INetFile *	File;
	
	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

//	assert(File->State == STATE_COMPLETE);

//	<> CB moved inside the delete
//	if	(File->Job)
//		grThreadQueue_WaitOnJob(File->Job,GR_THREADQUEUE_STATUS_COMPLETED);
	
	ThreadLog_Printf("FSInet: closing '%s'\n", File->FullPath);
	delete File;

	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_GetS(void *Handle, void *Buff, int MaxLen)
{
	INetFile *	File;
//	DWORD		BytesRead;
//	BOOL		Result;
//	char *		p;
//	char *		End;

	assert(Buff);
	assert(MaxLen != 0);

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->State == STATE_ERROR)
		return GR_FALSE;

#pragma message ("FSINet_GetS: Not implemented")

#if 0
	Result = ReadFile(File->FileHandle, Buff, MaxLen - 1, &BytesRead, NULL);
	if	(BytesRead == 0)
	{
		return GR_FALSE;
	}

	End = (char *)Buff + BytesRead;
	p = Buff;
	while	(p < End)
	{
		/*
		  This code will terminate a line on one of three conditions:
			\r	Character changed to \n, next char set to 0
			\n	Next char set to 0
			\r\n	First \r changed to \n.  \n changed to 0.
		*/
		if	(*p == '\r')
		{
			int Skip = 0;
			
			*p = '\n';		// set end of line
			p++;			// and skip to next char
			// If the next char is a newline, then skip it too  (\r\n case)
			if (*p == '\n')
			{
				Skip = 1;
			}
			*p = '\0';
			// Set the file pointer back a bit since we probably overran
			SetFilePointer(File->FileHandle, -(int)(BytesRead - ((p + Skip) - (char *)Buff)), NULL, FILE_CURRENT); 
			assert(p - (char *)Buff <= MaxLen);
			return GR_TRUE;
		}
		else if	(*p == '\n')
		{
			// Set the file pointer back a bit since we probably overran
			p++;
			SetFilePointer(File->FileHandle, -(int)(BytesRead - (p - (char *)Buff)), NULL, FILE_CURRENT); 
			*p = '\0';
			assert(p - (char *)Buff <= MaxLen);
			return GR_TRUE;
		}
		p++;
	}
#endif
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_BytesAvailable(void *Handle, long *Count)
{
	INetFile *	File;
	MSG			Msg;

	assert(Count);

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->State == STATE_ERROR)
		return GR_FALSE;

	PeekMessage(&Msg, NULL, 0, 0, PM_NOREMOVE);

	if	(File->Job)
	{
		if	(!grThreadQueue_WaitOnJob(File->Job, GR_THREADQUEUE_STATUS_RUNNING))
		{
			grErrorLog_AddString((grErrorLog_ErrorClassType)-1,"FSInet : Wait on job failed!",File->FullPath);
			return GR_FALSE;
		}
	}

	EnterCriticalSection(&File->Lock);
	*Count = File->BytesAvailable();
	LeaveCriticalSection(&File->Lock);

	return GR_TRUE;
}

extern "C" {
	extern grVFile * Hack_VFS_File;
};

static	grBoolean	GRCC FSINet_Read(void *Handle, void *Buff, uint32 Count)
{
	INetFile *	File;
	grBoolean	DataAlreadyAvailable;

	assert(Buff);
	assert(Count != 0);

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	assert(!Hack_VFS_File || grVFile_IsValid(Hack_VFS_File));

	if	(((File->State >= STATE_DATACOMPLETE ) && File->BytesAvailable() < Count) ||
		 (File->State == STATE_ERROR))
	{
		if	(File->State == STATE_ERROR)
			grErrorLog_AddString((grErrorLog_ErrorClassType)-1,"FSInet : Read on file with error",File->FullPath);
		else
		{
			grErrorLog_AddString((grErrorLog_ErrorClassType)-1,"FSInet : Read past EOF", File->FullPath);
			assert(0);
		}
		return GR_FALSE;
	}

	assert(!Hack_VFS_File || grVFile_IsValid(Hack_VFS_File));
	
	if	(!grThreadQueue_WaitOnJob(File->Job,GR_THREADQUEUE_STATUS_RUNNING))
	{
		grErrorLog_AddString((grErrorLog_ErrorClassType)-1,"FSInet : Wait on job failed!", File->FullPath);
		return GR_FALSE;
	}

	assert(!Hack_VFS_File || grVFile_IsValid(Hack_VFS_File));

#pragma message("FSInet : doesn't check for VFHH and handle hints!!!")

	if	(File->BytesAvailable() < Count)
	{
		ThreadLog_Printf("FSInet: stalling in read for %d bytes, %d available on '%s'\n", Count, File->BytesAvailable(), File->FullPath);
		DataAlreadyAvailable = GR_FALSE;
	}
	else
		DataAlreadyAvailable = GR_TRUE;

	// Block until we've got enough bytes
	while	(File->BytesAvailable() < Count)
	{
		grThreadQueue_Sleep(1);
	
		if	(((File->State >= STATE_DATACOMPLETE ) && 
					File->BytesAvailable() < Count) ||
			 (File->State == STATE_ERROR))
		{
			if	(File->State == STATE_ERROR)
			{
				grErrorLog_AddString((grErrorLog_ErrorClassType)-1,"FSInet : Read on file with error",File->FullPath);
			}
			else
			{
				grErrorLog_AddString((grErrorLog_ErrorClassType)-1,"FSInet : Read past EOF",File->FullPath);
				assert(0);
			}
			return GR_FALSE;
		}
	}
	
	if	(DataAlreadyAvailable == GR_FALSE)
		ThreadLog_Printf("FSInet: finished wait for data on '%s'\n", File->FullPath);

	assert(!Hack_VFS_File || grVFile_IsValid(Hack_VFS_File)); // @@ !

#pragma message ("FSInet : Investigate strange behaviour with sleep in FSINet_Read")
		// Behaviour seems better with the sleep out of the loop!??
//		grThreadQueue_Sleep(1);

	EnterCriticalSection(&File->Lock);

	assert(File->BytesAvailable() >= Count);
	grVFile_Seek(File->MemoryFile, File->MemoryFilePos(), GR_VFILE_SEEKSET);
	grVFile_Read(File->MemoryFile, Buff, Count);
	grVFile_Seek(File->MemoryFile, File->m_dwTotalRead, GR_VFILE_SEEKSET);
	File->ClientPos += Count;

	LeaveCriticalSection(&File->Lock);

	assert(!Hack_VFS_File || grVFile_IsValid(Hack_VFS_File));

	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_Write(void * /*Handle*/, const void * /*Buff*/, int /*Count*/)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_Seek(void *Handle, int Where, grVFile_Whence Whence)
{
	INetFile *	File;
	long		FinalPos = 0;
	long		MemoryFileSize;

	File = (INetFile *)Handle;

	if	(File->IsDirectory == GR_TRUE)
		return GR_FALSE;
	
	CHECK_HANDLE(File);

	EnterCriticalSection(&File->Lock);

	grVFile_Size(File->MemoryFile, &MemoryFileSize);

	switch	(Whence)
	{
	case	GR_VFILE_SEEKCUR:
		FinalPos = File->MemoryFilePos() + Where;
		break;

	case	GR_VFILE_SEEKEND:
		if	(File->State < STATE_DATACOMPLETE)
		{
			LeaveCriticalSection(&File->Lock);
			return GR_FALSE;
		}

		FinalPos = MemoryFileSize - Where;
		break;

	case	GR_VFILE_SEEKSET:
		FinalPos = File->TrueFileBase + Where;
		break;

	default:
		assert(!"Unknown seek kind");
	}

	if	(FinalPos > MemoryFileSize)
	{
		LeaveCriticalSection(&File->Lock);
	
		// <> CB ; used to just return false
		// Block until we've got enough bytes

		#pragma message("Fsinet : Seek past EOF ! Grudgingly allowed..")
		grErrorLog_AddString((grErrorLog_ErrorClassType)-1,
			"Fsinet : Seek past EOF ! Grudgingly allowed..",NULL);

		if	(FinalPos > MemoryFileSize)
			ThreadLog_Printf("FSInet: stalling in seek on '%s'\n", File->FullPath);

		while	(FinalPos > MemoryFileSize)
		{
			grThreadQueue_PollJobs();
			grThreadQueue_Sleep(1);

			EnterCriticalSection(&File->Lock);
			grVFile_Size(File->MemoryFile, &MemoryFileSize);
			
			if	((File->State >= STATE_DATACOMPLETE ) ||
				 (File->State == STATE_ERROR))
			{
				LeaveCriticalSection(&File->Lock);
				return GR_FALSE;
			}

			LeaveCriticalSection(&File->Lock);
		}
		
		EnterCriticalSection(&File->Lock);
	}

	File->ClientPos = FinalPos - File->TrueFileBase;

	LeaveCriticalSection(&File->Lock);

	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_EOF(const void *Handle)
{
	INetFile *	File;

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	assert( File->MemoryFilePos() <= (long)File->m_dwTotalRead );

	if	((File->State >= STATE_DATACOMPLETE ) &&
		 (File->MemoryFilePos() == (long)File->m_dwTotalRead))
	{
		return GR_TRUE;
	}

	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_Tell(const void *Handle, long *Position)
{
	const INetFile *	File;

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->State == STATE_ERROR)
		return GR_FALSE;

	*Position = File->ClientPos;

	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_Size(const void *Handle, long * Size)
{
	INetFile *	File;
	MSG Msg;

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->State == STATE_ERROR)
		return GR_FALSE;

	if	(File->State < STATE_DATACOMPLETE) // can only return size when complete!
		return GR_FALSE;

	PeekMessage(&Msg, NULL, 0, 0, PM_NOREMOVE);

	EnterCriticalSection(&File->Lock);
	*Size = File->m_dwTotalRead;
	LeaveCriticalSection(&File->Lock);

	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_GetProperties(const void *Handle, grVFile_Properties *Properties)
{
	const INetFile *	File;
	grVFile_Attributes	Attribs;

	assert(Properties);

	File = (INetFile *)Handle;

	CHECK_HANDLE(File);

	if	(File->State == STATE_ERROR)
		return GR_FALSE;

	Attribs = GR_VFILE_ATTRIB_READONLY | GR_VFILE_ATTRIB_REMOTE;
	
	#pragma message("FSINet : GetProperties says nothing about _DIRECTORY !")
	
	if	(File->IsDirectory) // <> CB 2/11 !
		Attribs |= GR_VFILE_ATTRIB_DIRECTORY;

	Properties->Time.Time1 = 0;
	Properties->Time.Time2 = 0;
	
	Properties->AttributeFlags 		 = Attribs;
	Properties->Size		  		 = 0;

	Properties->Name[0] = 0; // <> CB 2/11 !

	return File->GetFullPath(Properties->Name, sizeof(Properties->Name));
}

static	grBoolean	GRCC FSINet_SetSize(void * /*Handle*/, long /*size*/)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_SetAttributes(void * /*Handle*/, grVFile_Attributes /*Attributes*/)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_SetTime(void * /*Handle*/, const grVFile_Time * /*Time*/)
{
	return GR_FALSE;
}

static	grVFile *	GRCC FSINet_GetHintsFile(void *Handle)
{
	INetFile *	File;

	File = (INetFile *)Handle;

	if	(File->IsDirectory == GR_TRUE)
		return NULL;
	
	CHECK_HANDLE(File);

	if	(File->State == STATE_ERROR)
		return NULL;

	if	(File->IgnoreHints == GR_TRUE)
		return NULL;

	if	(File->CheckedForHints == GR_FALSE)
		ThreadLog_Printf("FSInet: stalling in GetHintsFile on '%s'\n", File->FullPath);

	while	(File->CheckedForHints == GR_FALSE)
	{
		grThreadQueue_JobStatus Status;

		Status = grThreadQueue_JobGetStatus(File->Job);
		if ( Status == GR_THREADQUEUE_STATUS_WAITINGTOBEGIN )
		{
			grThreadQueue_PollJobs();
		}
		if ( Status == GR_THREADQUEUE_STATUS_WAITINGFORTHREAD )
		{
			grThreadQueue_PollJobs();
		}
		grThreadQueue_Sleep(1);
	}

	if	(File->HasHints == GR_TRUE)
	{
		assert(File->HintsFile != NULL);
		return File->HintsFile;
	}

	return NULL;
}

static	grBoolean	GRCC FSINet_FileExists(grVFile * /*FS*/, void *Handle, const char * /*Name*/)
{
	INetFile *	File;

	File = (INetFile *)Handle;

//	if	(File != (void *)INETFILE_SIGNATURE)
//		return GR_TRUE;

//	CHECK_HANDLE(File);

//	assert(!"Not implemented");
//	return GR_FALSE;
#pragma message ("FSInet : FileExists is hacked to make grVFile_Open function")
	return GR_TRUE;
}

static	grBoolean	GRCC FSINet_Disperse(
	grVFile *	/*FS*/,
	void *		/*Handle*/,
	const char * /*Directory*/)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_DeleteFile(grVFile * /*FS*/, void * /*Handle*/, const char * /*Name*/)
{
	return GR_FALSE;
}

static	grBoolean	GRCC FSINet_RenameFile(grVFile * /*FS*/, void * /*Handle*/, const char * /*Name*/, const char * /*NewName*/)
{
	return GR_FALSE;
}

static	grVFile_SystemAPIs	FSINet_APIs =
{
	FSINet_FinderCreate,
	FSINet_FinderGetNextFile,
	FSINet_FinderGetProperties,
	FSINet_FinderDestroy,

	FSINet_OpenNewSystem,
	FSINet_UpdateContext,
	FSINet_Open,
	FSINet_DeleteFile,
	FSINet_RenameFile,
	FSINet_FileExists,
	FSINet_Disperse,
	FSINet_Close,

	FSINet_GetS,
	FSINet_BytesAvailable,
	FSINet_Read,
	FSINet_Write,
	FSINet_Seek,
	FSINet_EOF,
	FSINet_Tell,
	FSINet_Size,

	FSINet_GetProperties,

	FSINet_SetSize,
	FSINet_SetAttributes,
	FSINet_SetTime,

	FSINet_GetHintsFile,

};

const grVFile_SystemAPIs *GRCC FSINet_GetAPIs(void)
{
	return &FSINet_APIs;
}

