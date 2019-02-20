
// YAPATE.h: Hauptheaderdatei für die PROJECT_NAME-Anwendung
//

#pragma once

#ifndef __AFXWIN_H__
	#error "'stdafx.h' vor dieser Datei für PCH einschließen"
#endif

#include "resource.h"		// Hauptsymbole


// CYAPATEApp:
// Siehe YAPATE.cpp für die Implementierung dieser Klasse
//

class CYAPATEApp : public CWinApp
{
public:
	CYAPATEApp();

// Überschreibungen
public:
	virtual BOOL InitInstance();

// Implementierung

	DECLARE_MESSAGE_MAP()
};

extern CYAPATEApp theApp;
