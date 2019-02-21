
// YAPATEDlg.h: Headerdatei
//

#pragma once

#include <yaml-cpp/yaml.h>


// CYAPATEDlg-Dialogfeld
class CYAPATEDlg : public CDialog
{
// Konstruktion
public:
	CYAPATEDlg(CWnd* pParent = nullptr);	// Standardkonstruktor

// Dialogfelddaten
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_YAPATE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV-Unterstützung


// Implementierung
protected:
	HICON m_hIcon;

	// Generierte Funktionen für die Meldungstabellen
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
   afx_msg void OnEnChangeEdPath();
   afx_msg void OnEnChangeEdFile();
private:
   void UpdateOutput();
   YAML::Node m_yfile;
   CString m_fileError;
public:
   CRichEditCtrl m_edFile;
   CComboBox m_ddMethod;
   CComboBox m_ddPath;
   afx_msg void OnCbnSelchangeDdMethod();
};
