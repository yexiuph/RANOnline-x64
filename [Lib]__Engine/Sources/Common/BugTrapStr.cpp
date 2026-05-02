#include "pch.h"
#include "./BugTrapStr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace BUG_TRAP
{
	TCHAR szEMail[MAX_PATH]			= _T("randev2@mincoms.com");
	TCHAR szSupportURL[MAX_PATH]	= _T("http://forum.ragezone.com");
	TCHAR szCountry[MAX_PATH]		= _T("_RZ");

	TCHAR szServerAddr[MAX_PATH]	= _T("ranbugreport.mincoms.co.kr");
	SHORT nPort						= 8888;

};
