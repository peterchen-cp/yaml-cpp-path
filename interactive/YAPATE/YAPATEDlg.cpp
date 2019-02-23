#include "stdafx.h"
#include "YAPATE.h"
#include "YAPATEDlg.h"
#include "afxdialogex.h"
#include <yaml-path/yaml-path.h>
#include <utility>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

char const * initialText = 
R"(
-  name : Joe
   color: red
   friends : ~
-  name : Sina
   color: blue
-  name : Estragon
   color : red
   friends :
      Wladimir : good
      Godot : unreliable
)";

CYAPATEDlg::CYAPATEDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_YAPATE_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CYAPATEDlg::DoDataExchange(CDataExchange* pDX)
{
   CDialog::DoDataExchange(pDX);
   DDX_Control(pDX, IDC_ED_FILE, m_edFile);
   DDX_Control(pDX, IDC_DD_PATH, m_ddPath);
   DDX_Control(pDX, IDC_DD_METHOD, m_ddMethod);
}

BEGIN_MESSAGE_MAP(CYAPATEDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
   ON_CBN_EDITCHANGE(IDC_DD_PATH, &CYAPATEDlg::OnEnChangeEdPath)
   ON_CBN_SELCHANGE(IDC_DD_PATH, &CYAPATEDlg::OnEnChangeEdPath)
   ON_EN_CHANGE(IDC_ED_FILE, &CYAPATEDlg::OnEnChangeEdFile)
   ON_CBN_SELCHANGE(IDC_DD_METHOD, &CYAPATEDlg::OnCbnSelchangeDdMethod)
END_MESSAGE_MAP()

enum EMethod
{
   emSelect,
   emRequire,
   emPathResolve,
   emPathValidate,
};

BOOL CYAPATEDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Großes Symbol verwenden
	SetIcon(m_hIcon, FALSE);		// Kleines Symbol verwenden

   m_edFile.SetEventMask(m_edFile.GetEventMask() | ENM_CHANGE);
   m_edFile.SetWindowText(CString(initialText));

   m_ddMethod.SetItemData(m_ddMethod.AddString(_T("YAML::Select")), emSelect);
   m_ddMethod.SetItemData(m_ddMethod.AddString(_T("YAML::Require")), emRequire);
   m_ddMethod.SetItemData(m_ddMethod.AddString(_T("YAML::PathResolve")), emPathResolve);
   m_ddMethod.SetItemData(m_ddMethod.AddString(_T("YAML::PathValidate")), emPathValidate);
   m_ddMethod.SetCurSel(0);


   m_ddPath.AddString(_T("name"));
   m_ddPath.AddString(_T("[1].name"));
   m_ddPath.AddString(_T("name[2]"));
   m_ddPath.AddString(_T("color"));
   m_ddPath.AddString(_T("[color=red]"));
   m_ddPath.AddString(_T("[color=blue]"));
   m_ddPath.AddString(_T("[friends=]"));
   m_ddPath.AddString(_T("friends.Wladimir"));
   m_ddPath.AddString(_T("friends.Godot[0]"));
   m_ddPath.AddString(_T("[1]"));
   m_ddPath.AddString(_T("[1].name"));
   m_ddPath.AddString(_T("[1].wealth"));

   UpdateOutput();
	return TRUE;
}

void CYAPATEDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // Gerätekontext zum Zeichnen

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Symbol in Clientrechteck zentrieren
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Symbol zeichnen
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

HCURSOR CYAPATEDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

CStringA Utf8(const CStringW& uni)
{
   if (uni.IsEmpty()) return ""; // nothing to do
   CStringA utf8;
   int cc = 0;
   if ((cc = WideCharToMultiByte(CP_UTF8, 0, uni, -1, NULL, 0, 0, 0) - 1) > 0)
   {
      char *buf = utf8.GetBuffer(cc);
      if (buf) WideCharToMultiByte(CP_UTF8, 0, uni, -1, buf, cc, 0, 0);
      utf8.ReleaseBuffer();
   }
   return utf8;
}

CStringW FromUtf8(const CStringA& utf8)
{
   if (utf8.IsEmpty()) return L""; // nothing to do
   CStringW uni;
   int cc = 0;
   if ((cc = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0) - 1) > 0)
   {
      wchar_t *buf = uni.GetBuffer(cc);
      if (buf) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, cc);
      uni.ReleaseBuffer();
   }
   return uni;
}


void CYAPATEDlg::OnEnChangeEdPath()
{
   UpdateOutput();
}


void CYAPATEDlg::OnEnChangeEdFile()
{

   CString strFile;
   GetDlgItemText(IDC_ED_FILE, strFile);
   m_yfile.reset();
   m_fileError.Empty();
   try
   {
      m_yfile = YAML::Load(Utf8(strFile));
   }
   catch (std::exception const & e)
   {
      m_fileError = FromUtf8(e.what());
   }

   UpdateOutput();
}

void CYAPATEDlg::OnCbnSelchangeDdMethod()
{
   UpdateOutput();
}


CString ToString(YAML::Node n)
{
   if (!n)
      return _T("<<empty>>");

   if (n.IsNull())
      return _T("<<null>>");

   YAML::Emitter e;
   e << n;
   CString result = FromUtf8(e.c_str());
   if (!result.GetLength())
      return _T("<<??>>"); // that would be weird
   return result;
}

void CYAPATEDlg::UpdateOutput()
{
   if (m_fileError.GetLength())
   {
      SetDlgItemText(IDC_ED_RESULT, CString(_T("Source YAML Error:")) + m_fileError);
      SetDlgItemText(IDC_ST_PERF, _T(""));
      return;
   }

   static LARGE_INTEGER qpcTicksPerSec = { 0 };
   if (!qpcTicksPerSec.QuadPart)
      QueryPerformanceFrequency(&qpcTicksPerSec);

   CString strResult;

   CString strPath;
   int sel = m_ddPath.GetCurSel();
   if (sel >= 0)
      m_ddPath.GetLBText(sel, strPath);
   else
      GetDlgItemText(IDC_ED_PATH, strPath);
   auto u8path = Utf8(strPath);
   YAML::PathArg path = u8path;

   LARGE_INTEGER qpcStart = { 0 }, qpcEnd = { 0 };
   try
   {
      const int method = m_ddMethod.GetItemData(m_ddMethod.GetCurSel());
      if (method == emSelect)
      {
         QueryPerformanceCounter(&qpcStart);
         auto node = YAML::Select(m_yfile, path);
         QueryPerformanceCounter(&qpcEnd);
         strResult = _T("SELECT: OK\r\n---\r\n");
         strResult += ToString(node);
      }
      else if (method == emRequire)
      {
         QueryPerformanceCounter(&qpcStart);
         auto node = YAML::Require(m_yfile, path);
         QueryPerformanceCounter(&qpcEnd);
         strResult = _T("Require: OK\r\n---\r\n");
         strResult += ToString(node);
      }
      else if (method == emPathResolve)
      {
         QueryPerformanceCounter(&qpcStart);
         YAML::Node ynode = m_yfile;
         YAML::PathException x;
         YAML::PathResolve(ynode, path, {}, &x);
         QueryPerformanceCounter(&qpcEnd);

         strResult = _T("PathResolve:");
         if (x.Error() != YAML::EPathError::None)
            strResult = strResult + _T("\r\n") + FromUtf8(x.What(true).c_str()) + CString(_T("\r\nthe last matched node was:\r\n---\r\n"));
         else
            strResult += _T("OK (entire path could be resolved)\r\n---\r\n");

         strResult += ToString(ynode);
      }
      else if (method == emPathValidate)
      {
         QueryPerformanceCounter(&qpcStart);
         std::string valid;
         size_t errOffs = 0;
         auto err = YAML::PathValidate(path, &valid, &errOffs);
         QueryPerformanceCounter(&qpcEnd);
         if (err == YAML::EPathError::None)
            strResult = "path is valid";
         else
            strResult.Format(_T("Invalid path: %s\r\n  valid part: %s\r\n  error offset: %d\r\n"), 
               FromUtf8(YAML::PathException::GetErrorMessage(err).c_str()), FromUtf8(std::string(valid).c_str()), errOffs);
      }
   }
   catch (std::exception const & e)
   {
      strResult = CString(_T("Error:")) + FromUtf8(e.what());
   }

   SetDlgItemText(IDC_ED_RESULT, strResult);

   CString duration;
   if (qpcTicksPerSec.QuadPart && qpcEnd.QuadPart)  // queryPerformanceCounter is working... and no exception! yay!
   {
      double seconds = double(qpcEnd.QuadPart - qpcStart.QuadPart) / double(qpcTicksPerSec.QuadPart);
      duration.Format(_T("duration: %.3f ms"), seconds*1000);
   }
   SetDlgItemText(IDC_ST_PERF, duration);
}


