/****************************************************************************************/
/*  MaterialEditorDlg.cpp                                                               */
/*                                                                                      */
/*  Material Editor (roadmap Phase 2). One slot per channel (browse or drop an image),  */
/*  "Auto-find" by suffix, grayscale AO / roughness (or gloss) / metal maps packed into */
/*  ORM, and the PBR scalars and flags. "Import && Apply" runs G3DTexImport next to the */
/*  editor, which writes BC-compressed DDS textures into a pak under the material's     */
/*  directory and a new .jmat, then the material is reloaded and swapped into the open  */
/*  level, so the 3D view shows the result.                                             */
/****************************************************************************************/
#include "stdafx.h"
#include "GWE.H"
#include "MainFrm.h"
#include "Doc.h"
#include "MaterialEditorDlg.h"
#include "Materials.h"
#include "MaterialList.h"
#include "Level.h"

namespace
{
	const TCHAR* const ImageFilter =
		_T("Images (*.png;*.tga;*.dds;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)|*.png;*.tga;*.dds;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff|All files (*.*)|*.*||");

	// Suffixes after the material's stem, per slot (as G3DTexImport searches them)
	const TCHAR* const SlotSuffixes[CMaterialEditorDlg::SLOT_COUNT][5] = {
		{ NULL },
		{ _T("_n"), _T("_normal"), _T("_nrm"), _T("_nor"), NULL },
		{ _T("_orm"), _T("_arm"), NULL },
		{ _T("_e"), _T("_emissive"), _T("_emit"), _T("_emission"), NULL },
		{ _T("_h"), _T("_height"), _T("_disp"), _T("_displacement"), NULL },
		{ _T("_ao"), _T("_occlusion"), NULL },
		{ _T("_rough"), _T("_roughness"), _T("_gloss"), _T("_glossiness"), NULL },
		{ _T("_metal"), _T("_metallic"), _T("_metalness"), NULL },
	};
	const TCHAR* const BaseSuffixes[] = { _T("_albedo"), _T("_basecolor"), _T("_base"), _T("_diffuse"), _T("_color"), _T("_col"), _T("_d"), NULL };
	const TCHAR* const Extensions[] = { _T(".png"), _T(".tga"), _T(".dds"), _T(".jpg"), _T(".jpeg"), _T(".bmp"), _T(".tif"), _T(".tiff"), NULL };

	bool EndsWithNoCase(const CString& Text, const TCHAR* Suffix)
	{
		const int Length = lstrlen(Suffix);
		return Text.GetLength() > Length && Text.Right(Length).CompareNoCase(Suffix) == 0;
	}

	CString Quote(const CString& Text)
	{
		return _T("\"") + Text + _T("\"");
	}

	// Directory, and file name without extension or base suffix
	void SplitStem(const CString& Path, CString& Dir, CString& Stem)
	{
		const int Slash = Path.ReverseFind(_T('\\')) > Path.ReverseFind(_T('/')) ? Path.ReverseFind(_T('\\')) : Path.ReverseFind(_T('/'));
		Dir = (Slash >= 0) ? Path.Left(Slash + 1) : CString();
		Stem = Path.Mid(Slash + 1);
		const int Dot = Stem.ReverseFind(_T('.'));
		if (Dot > 0)
			Stem = Stem.Left(Dot);
		for (int i = 0; BaseSuffixes[i]; i++)
			if (EndsWithNoCase(Stem, BaseSuffixes[i]))
			{
				Stem = Stem.Left(Stem.GetLength() - lstrlen(BaseSuffixes[i]));
				break;
			}
	}
}

BEGIN_MESSAGE_MAP(CMaterialEditorDlg, CDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MATED_BASE_BROWSE, IDC_MATED_METAL_BROWSE, OnBrowse)
	ON_BN_CLICKED(IDC_MATED_AUTOFIND, OnAutoFind)
	ON_BN_CLICKED(IDC_MATED_APPLY, OnApply)
	ON_CBN_SELCHANGE(IDC_MATED_ALPHAMODE, OnAlphaModeChanged)
	ON_WM_DROPFILES()
END_MESSAGE_MAP()

CMaterialEditorDlg::CMaterialEditorDlg(const CString& MaterialName, CWnd* pParent)
	: CDialog(IDD, pParent), m_strName(MaterialName), m_bImported(false)
{
}

BOOL CMaterialEditorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_MATED_NAME, m_strName);
	if (m_strName.IsEmpty())
		SetWindowText(_T("New Material (PBR)"));
	else
	{
		SetWindowText(_T("Edit Material (PBR) - ") + m_strName);
		((CEdit*)GetDlgItem(IDC_MATED_NAME))->SetReadOnly(TRUE);
	}
	SetDlgItemText(IDC_MATED_PAK, _T("Imported"));

	CComboBox* Alpha = (CComboBox*)GetDlgItem(IDC_MATED_ALPHAMODE);
	Alpha->AddString(_T("Opaque"));
	Alpha->AddString(_T("Cutout"));
	Alpha->AddString(_T("Blend"));

	// Start from the material's current parameters (defaults for a legacy material)
	grMaterialSpec_PBR PBR;
	grMaterialSpec_DefaultPBR(&PBR);
	PBR.Metal = 0.0f;
	MaterialList_Struct* List = ((CGweApp*)AfxGetApp())->GetMaterialList();
	MaterialIterator MI;
	Material_Struct* Material = List ? MaterialList_SearchByName(List, &MI, m_strName.GetBuffer()) : NULL;
	m_strName.ReleaseBuffer();
	if (Material)
	{
		grMaterialSpec_GetPBR(Materials_GetMaterialSpec(Material), &PBR);

		// The shipped bitmap is the obvious base
		CString Dir, Stem;
		SplitStem(CString(Materials_GetPath(Material)), Dir, Stem);
		for (int i = 0; Extensions[i]; i++)
			if (GetFileAttributes(Dir + m_strName + Extensions[i]) != INVALID_FILE_ATTRIBUTES)
			{
				SetDlgItemText(SlotEdit(SLOT_BASE), Dir + m_strName + Extensions[i]);
				break;
			}
	}

	SetFloat(IDC_MATED_TINT_R, PBR.BaseColor[0]);
	SetFloat(IDC_MATED_TINT_G, PBR.BaseColor[1]);
	SetFloat(IDC_MATED_TINT_B, PBR.BaseColor[2]);
	SetFloat(IDC_MATED_ROUGHNESS, PBR.Roughness);
	SetFloat(IDC_MATED_METALNESS, PBR.Metal);
	SetFloat(IDC_MATED_EMIT_R, PBR.Emissive[0]);
	SetFloat(IDC_MATED_EMIT_G, PBR.Emissive[1]);
	SetFloat(IDC_MATED_EMIT_B, PBR.Emissive[2]);
	SetFloat(IDC_MATED_INTENSITY, PBR.EmissiveIntensity);
	SetFloat(IDC_MATED_HEIGHTSCALE, PBR.HeightScale);
	SetFloat(IDC_MATED_CUTOFF, PBR.AlphaCutoff);
	Alpha->SetCurSel(PBR.AlphaMode <= 2 ? PBR.AlphaMode : 0);
	CheckDlgButton(IDC_MATED_TWOSIDED, (PBR.Flags & GR_MATERIAL_PBR_TWO_SIDED) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_MATED_RETRO, (PBR.Flags & GR_MATERIAL_PBR_RETRO) ? BST_CHECKED : BST_UNCHECKED);
	OnAlphaModeChanged();

	DragAcceptFiles(TRUE);
	return TRUE;
}

void CMaterialEditorDlg::OnOK()
{
	// Enter must not close the dialog; Close does
}

CString CMaterialEditorDlg::GetText(UINT nID)
{
	CString Text;
	GetDlgItemText(nID, Text);
	Text.Trim();
	return Text;
}

void CMaterialEditorDlg::SetFloat(UINT nID, float Value)
{
	CString Text;
	Text.Format(_T("%g"), Value);
	SetDlgItemText(nID, Text);
}

void CMaterialEditorDlg::AssignFile(const CString& Path, int Slot)
{
	SetDlgItemText(SlotEdit(Slot), Path);
	if (Slot == SLOT_ROUGH)
	{
		CString Dir, Stem;
		SplitStem(Path, Dir, Stem);
		CheckDlgButton(IDC_MATED_GLOSS, (EndsWithNoCase(Stem, _T("_gloss")) || EndsWithNoCase(Stem, _T("_glossiness"))) ? BST_CHECKED : BST_UNCHECKED);
	}
}

int CMaterialEditorDlg::SlotForFile(const CString& Path)
{
	CString Dir, Stem;
	SplitStem(Path, Dir, Stem);
	for (int Slot = SLOT_NORMAL; Slot < SLOT_COUNT; Slot++)
		for (int i = 0; SlotSuffixes[Slot][i]; i++)
			if (EndsWithNoCase(Stem, SlotSuffixes[Slot][i]))
				return Slot;
	return SLOT_BASE;
}

void CMaterialEditorDlg::OnBrowse(UINT nID)
{
	const int Slot = (nID - IDC_MATED_BASE_BROWSE) / 2;
	CFileDialog Dialog(TRUE, NULL, GetText(SlotEdit(Slot)), OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, ImageFilter, this);
	if (Dialog.DoModal() == IDOK)
		AssignFile(Dialog.GetPathName(), Slot);
}

void CMaterialEditorDlg::OnDropFiles(HDROP hDropInfo)
{
	// Dropped on a slot: that slot. Elsewhere: each file goes where its suffix says.
	POINT Point;
	DragQueryPoint(hDropInfo, &Point);
	ClientToScreen(&Point);
	int Target = -1;
	for (int Slot = 0; Slot < SLOT_COUNT; Slot++)
	{
		CRect Rect;
		GetDlgItem(SlotEdit(Slot))->GetWindowRect(&Rect);
		if (Rect.PtInRect(Point))
			Target = Slot;
	}

	const UINT Count = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	for (UINT i = 0; i < Count; i++)
	{
		TCHAR Path[MAX_PATH];
		DragQueryFile(hDropInfo, i, Path, MAX_PATH);
		AssignFile(Path, (Target >= 0 && Count == 1) ? Target : SlotForFile(Path));
	}
	DragFinish(hDropInfo);
}

void CMaterialEditorDlg::OnAutoFind()
{
	const CString Base = GetText(SlotEdit(SLOT_BASE));
	if (Base.IsEmpty())
	{
		AfxMessageBox(_T("Choose the base color image first."));
		return;
	}
	CString Dir, Stem;
	SplitStem(Base, Dir, Stem);
	int Found = 0;
	for (int Slot = SLOT_NORMAL; Slot < SLOT_COUNT; Slot++)
	{
		for (int i = 0; SlotSuffixes[Slot][i]; i++)
			for (int e = 0; Extensions[e]; e++)
			{
				const CString Candidate = Dir + Stem + SlotSuffixes[Slot][i] + Extensions[e];
				if (GetText(SlotEdit(Slot)).IsEmpty() && GetFileAttributes(Candidate) != INVALID_FILE_ATTRIBUTES)
				{
					AssignFile(Candidate, Slot);
					Found++;
				}
			}
	}
	CString Message;
	Message.Format(_T("Auto-find: %d map(s) found next to %s.\r\n"), Found, (LPCTSTR)Base);
	SetDlgItemText(IDC_MATED_LOG, Message);
}

void CMaterialEditorDlg::OnAlphaModeChanged()
{
	GetDlgItem(IDC_MATED_CUTOFF)->EnableWindow(((CComboBox*)GetDlgItem(IDC_MATED_ALPHAMODE))->GetCurSel() == 1);
}

CString CMaterialEditorDlg::MaterialsDirectory()
{
	MaterialList_Struct* List = ((CGweApp*)AfxGetApp())->GetMaterialList();
	MaterialIterator MI;
	Material_Struct* Material = List ? MaterialList_SearchByName(List, &MI, m_strName.GetBuffer()) : NULL;
	m_strName.ReleaseBuffer();
	if (Material)
	{
		CString Dir, Stem;
		SplitStem(CString(Materials_GetPath(Material)), Dir, Stem);
		return Dir;
	}
	// A new material goes with the others
	Material = List ? MaterialList_GetFirstMaterial(List, &MI) : NULL;
	if (Material)
	{
		CString Dir, Stem;
		SplitStem(CString(Materials_GetPath(Material)), Dir, Stem);
		return Dir;
	}
	TCHAR Path[MAX_PATH];
	GetModuleFileName(NULL, Path, MAX_PATH);
	CString Dir(Path);
	return Dir.Left(Dir.ReverseFind(_T('\\')) + 1) + _T("GlobalMaterials\\");
}

CString CMaterialEditorDlg::ToolPath()
{
	TCHAR Path[MAX_PATH];
	GetModuleFileName(NULL, Path, MAX_PATH);
	CString Dir(Path);
	Dir = Dir.Left(Dir.ReverseFind(_T('\\')) + 1);
#ifdef _DEBUG
	const TCHAR* Names[] = { _T("G3DTexImportd.exe"), _T("G3DTexImport.exe") };
#else
	const TCHAR* Names[] = { _T("G3DTexImport.exe"), _T("G3DTexImportd.exe") };
#endif
	for (int i = 0; i < 2; i++)
		if (GetFileAttributes(Dir + Names[i]) != INVALID_FILE_ATTRIBUTES)
			return Dir + Names[i];
	return CString();
}

bool CMaterialEditorDlg::RunTool(const CString& CommandLine, CString& Output)
{
	SECURITY_ATTRIBUTES Security = { sizeof(Security), NULL, TRUE };
	HANDLE Read = NULL, Write = NULL;
	if (!CreatePipe(&Read, &Write, &Security, 0))
		return false;
	SetHandleInformation(Read, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO Startup = { sizeof(Startup) };
	Startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	Startup.wShowWindow = SW_HIDE;
	Startup.hStdOutput = Write;
	Startup.hStdError = Write;
	PROCESS_INFORMATION Process = {};
	CString Line = CommandLine;
	const BOOL Started = CreateProcess(NULL, Line.GetBuffer(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &Startup, &Process);
	Line.ReleaseBuffer();
	CloseHandle(Write);
	if (!Started)
	{
		CloseHandle(Read);
		Output = _T("Could not start G3DTexImport.");
		return false;
	}

	CWaitCursor Wait;
	char Buffer[512];
	DWORD Bytes = 0;
	CStringA Text;
	while (ReadFile(Read, Buffer, sizeof(Buffer) - 1, &Bytes, NULL) && Bytes > 0)
	{
		Buffer[Bytes] = 0;
		Text += Buffer;
	}
	CloseHandle(Read);
	WaitForSingleObject(Process.hProcess, INFINITE);
	DWORD ExitCode = 1;
	GetExitCodeProcess(Process.hProcess, &ExitCode);
	CloseHandle(Process.hProcess);
	CloseHandle(Process.hThread);

	Text.Replace("\r\n", "\n");
	Text.Replace("\n", "\r\n");
	Output = CString(Text);
	return ExitCode == 0;
}

void CMaterialEditorDlg::ReloadMaterial()
{
	CGweDoc* Doc = ((CMainFrame*)AfxGetMainWnd())->GetCurrentDocument();
	MaterialList_Struct* List = ((CGweApp*)AfxGetApp())->GetMaterialList();
	MaterialIterator MI;
	Material_Struct* Material = List ? MaterialList_SearchByName(List, &MI, m_strName.GetBuffer()) : NULL;
	m_strName.ReleaseBuffer();
	if (!Material || !Doc)
		return;
	if (!Materials_Reload(Material, Doc->GetG3DEngine(), Doc->GetResourceMgr()))
	{
		AfxMessageBox(_T("The new material was written but could not be loaded."));
		return;
	}
	if (Doc->GetLevel() &&
		Level_ReplaceMaterialSpec(Doc->GetLevel(), m_strName, (grMaterialSpec*)Materials_GetMaterialSpec(Material)))
		Doc->UpdateAllViews(NULL);
}

void CMaterialEditorDlg::OnApply()
{
	if (m_strName.IsEmpty())
	{
		// A new material: its name becomes a file and resource name
		const CString Name = GetText(IDC_MATED_NAME);
		if (Name.IsEmpty() || Name.SpanIncluding(_T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")) != Name)
		{
			AfxMessageBox(_T("Give the material a name made of letters, digits, _ and -."));
			GetDlgItem(IDC_MATED_NAME)->SetFocus();
			return;
		}
		m_strName = Name;
	}
	const CString Base = GetText(SlotEdit(SLOT_BASE));
	if (Base.IsEmpty())
	{
		AfxMessageBox(_T("Choose the base color image first."));
		return;
	}
	const CString Tool = ToolPath();
	if (Tool.IsEmpty())
	{
		AfxMessageBox(_T("G3DTexImport was not found next to the editor. Build Tools\\TexImport."));
		return;
	}
	CString Pak = GetText(IDC_MATED_PAK);
	CString Dir = MaterialsDirectory();
	const CString Target = Dir + m_strName + _T(".jmat");
	if (GetFileAttributes(Target) != INVALID_FILE_ATTRIBUTES &&
		AfxMessageBox(_T("Replace ") + Target + _T(" with the imported material?\n\nKeep a copy if you may want the original back."),
			MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return;
	Dir.TrimRight(_T('\\'));

	CString Command = Quote(Tool) + _T(" -material ") + Quote(m_strName) + _T(" ") + Quote(Base) +
		_T(" -outdir ") + Quote(Dir);
	if (!Pak.IsEmpty())
		Command += _T(" -pak ") + Quote(Pak);
	const TCHAR* const MapOptions[SLOT_COUNT] = { NULL, _T("-normal"), _T("-orm"), _T("-emissivemap"), _T("-heightmap"),
		_T("-ao"), IsDlgButtonChecked(IDC_MATED_GLOSS) ? _T("-gloss") : _T("-rough"), _T("-metalmap") };
	for (int Slot = SLOT_NORMAL; Slot < SLOT_COUNT; Slot++)
	{
		const CString Map = GetText(SlotEdit(Slot));
		if (!Map.IsEmpty())
			Command += CString(_T(" ")) + MapOptions[Slot] + _T(" ") + Quote(Map);
	}
	Command += _T(" -tint ") + GetText(IDC_MATED_TINT_R) + _T(" ") + GetText(IDC_MATED_TINT_G) + _T(" ") + GetText(IDC_MATED_TINT_B);
	Command += _T(" -roughness ") + GetText(IDC_MATED_ROUGHNESS);
	Command += _T(" -metal ") + GetText(IDC_MATED_METALNESS);
	Command += _T(" -emissive ") + GetText(IDC_MATED_EMIT_R) + _T(" ") + GetText(IDC_MATED_EMIT_G) + _T(" ") + GetText(IDC_MATED_EMIT_B);
	Command += _T(" -intensity ") + GetText(IDC_MATED_INTENSITY);
	Command += _T(" -height ") + GetText(IDC_MATED_HEIGHTSCALE);
	switch (((CComboBox*)GetDlgItem(IDC_MATED_ALPHAMODE))->GetCurSel())
	{
	case 1: Command += _T(" -cutout ") + GetText(IDC_MATED_CUTOFF); break;
	case 2: Command += _T(" -blend"); break;
	default: break;
	}
	if (IsDlgButtonChecked(IDC_MATED_TWOSIDED))
		Command += _T(" -twosided");
	if (IsDlgButtonChecked(IDC_MATED_RETRO))
		Command += _T(" -retro");
	if (IsDlgButtonChecked(IDC_MATED_FLIPGREEN))
		Command += _T(" -flipgreen");

	CString Output;
	const bool Ok = RunTool(Command, Output);
	SetDlgItemText(IDC_MATED_LOG, Output);
	if (Ok)
	{
		m_bImported = true;
		// From now on this dialog edits that material
		((CEdit*)GetDlgItem(IDC_MATED_NAME))->SetReadOnly(TRUE);
		SetWindowText(_T("Edit Material (PBR) - ") + m_strName);
		ReloadMaterial();
	}
	else
		AfxMessageBox(_T("G3DTexImport failed; see the log in the dialog."));
}
