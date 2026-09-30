/****************************************************************************************/
/*  MaterialEditorDlg.h                                                                 */
/*                                                                                      */
/*  Material Editor (roadmap Phase 2): builds a PBR material from source images with    */
/*  G3DTexImport and swaps it into the open level, so the 3D view is the preview.       */
/****************************************************************************************/
#pragma once

#include "resource.h"

class CGweDoc;

class CMaterialEditorDlg : public CDialog
{
public:
	// An empty name makes a new material (the dialog asks for its name).
	CMaterialEditorDlg(const CString& MaterialName, CWnd* pParent = NULL);

	// Whether Import && Apply wrote a material (the texture list then needs a refresh)
	bool		Imported() const	{ return m_bImported; }

	enum { IDD = IDD_MATERIAL_EDITOR };

	// Channel slots, in the order of their edit/browse control pairs
	enum Slot { SLOT_BASE, SLOT_NORMAL, SLOT_ORM, SLOT_EMISSIVE, SLOT_HEIGHT, SLOT_AO, SLOT_ROUGH, SLOT_METAL, SLOT_COUNT };

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	afx_msg void OnBrowse(UINT nID);
	afx_msg void OnAutoFind();
	afx_msg void OnApply();
	afx_msg void OnAlphaModeChanged();
	afx_msg void OnDropFiles(HDROP hDropInfo);
	DECLARE_MESSAGE_MAP()

private:
	CString		m_strName;
	bool		m_bImported;

	static UINT	SlotEdit(int Slot)		{ return IDC_MATED_BASE + Slot * 2; }
	static UINT	SlotBrowse(int Slot)	{ return IDC_MATED_BASE + Slot * 2 + 1; }

	CString		GetText(UINT nID);
	void		SetFloat(UINT nID, float Value);
	void		AssignFile(const CString& Path, int Slot);
	int			SlotForFile(const CString& Path);
	CString		MaterialsDirectory();
	CString		ToolPath();
	bool		RunTool(const CString& CommandLine, CString& Output);
	void		ReloadMaterial();
};
