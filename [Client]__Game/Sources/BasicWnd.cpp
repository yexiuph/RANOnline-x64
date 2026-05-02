#include "pch.h"

#include "DxGlobalStage.h"
#include "SUBPATH.h"
#include "RANPARAM.h"
#include "dxparamset.h"
#include "../[Lib]__EngineUI/Sources/Cursor.h"
#include "DxCursor.h"

#include "Basic.h"
#include "BasicWnd.h"
#include "BlockProg.h"
#include "GLogicData.h"
#include "GLogic.h"
#include "s_NetClient.h"
#include "rol_clipboard.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//CBLOCK_PROG	g_BLOCK_PROG;
HWND g_hWnd(NULL);

/////////////////////////////////////////////////////////////////////////////
// CBasicWnd

CBasicWnd::CBasicWnd() :
	CD3DApplication(),
	m_bCreated(FALSE),
	m_wndSizeX(800),
	m_wndSizeY(600),

	m_hCursorDefault(FALSE),
	m_nGGTimer(0),
	m_nGGATimer(0),
	m_nGGA12Timer(0),

	m_hMutex( NULL )
{
	m_bUseDepthBuffer = TRUE;
	m_pApp = (CBasicApp*)AfxGetApp();

	GLOGIC::bGLOGIC_PACKFILE = FALSE;
	GLOGIC::bGLOGIC_ZIPFILE = TRUE;
	GLOGIC::bENGLIB_ZIPFILE = TRUE;
}

CBasicWnd::~CBasicWnd()
{
}

BEGIN_MESSAGE_MAP(CBasicWnd, CWnd)
//{{AFX_MSG_MAP(CBasicWnd)
	ON_WM_ACTIVATEAPP()
	ON_WM_SETCURSOR()
	ON_WM_KEYDOWN()
	ON_WM_SIZE()
	ON_MESSAGE(NET_NOTIFY,  OnNetNotify)
	//}}AFX_MSG_MAP
	ON_WM_SYSCOMMAND()
	ON_WM_MOUSEMOVE()
	ON_WM_GETMINMAXINFO()
	ON_WM_NCACTIVATE()
	ON_WM_TIMER()
	ON_WM_ACTIVATE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBasicWnd message handlers

BOOL CBasicWnd::Create() 
{
	CBasicApp *pBasicApp = (CBasicApp *) AfxGetApp();

	CRect rectWnd;
	LONG nWindowStyle = WS_POPUP | WS_VISIBLE;
	
	if ( RANPARAM::bScrWindowed )
	{
		m_nScreenWinX = GetSystemMetrics(SM_CXSCREEN);
		m_nScreenWinY = GetSystemMetrics(SM_CYSCREEN);

		m_wndSizeX = RANPARAM::dwScrWidth;
		m_wndSizeY = RANPARAM::dwScrHeight;

		if ( m_wndSizeX>m_nScreenWinX || m_wndSizeY>m_nScreenWinY )
		{
			m_wndSizeX = m_nScreenWinX;
			m_wndSizeY = m_nScreenWinY;
		}

		if ( RANPARAM::bScrWndFullSize )
		{
			m_wndSizeX = m_nScreenWinX;
			m_wndSizeY = m_nScreenWinY;

			//	Note : 와이드 스크린 크기를 넘어설 경우
			//			2 스크린으로 인식.
			float fScreen = m_nScreenWinX / float(m_nScreenWinY);
			if ( fScreen > (16.0f/9.0f) )
			{
				m_wndSizeX = m_nScreenWinX/2;
			}

			rectWnd = CRect( 0, 0, m_wndSizeX, m_wndSizeY );
		}
		else
		{
			if ( m_wndSizeX>=(m_nScreenWinX) || m_wndSizeY>=(m_nScreenWinY) )
			{
				m_wndSizeX = 800;
				m_wndSizeY = 600;

				if ( m_wndSizeX>=m_nScreenWinX || m_wndSizeY>=m_nScreenWinY )
				{
					m_wndSizeX = m_nScreenWinX-40;
					m_wndSizeY = m_nScreenWinY-60;
				}
			}

			nWindowStyle = WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | 
						   WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_CLIPCHILDREN;

			RECT rt = {0, 0, m_wndSizeX, m_wndSizeY};
			AdjustWindowRectEx(&rt, nWindowStyle, FALSE, WS_EX_APPWINDOW);

			m_wndSizeX = rt.right - rt.left;
			m_wndSizeY = rt.bottom - rt.top;
			INT nX = (m_nScreenWinX - m_wndSizeX) / 2;
			INT nY = (m_nScreenWinY - m_wndSizeY) / 2;

			rectWnd = CRect( nX, nY, m_wndSizeX, m_wndSizeY );
		}
	}
	else
	{
		m_nScreenWinX = GetSystemMetrics(SM_CXSCREEN);
		m_nScreenWinY = GetSystemMetrics(SM_CYSCREEN);

		m_wndSizeX = m_nScreenWinX;
		m_wndSizeY = m_nScreenWinY;

		rectWnd = CRect( 0, 0, m_wndSizeX, m_wndSizeY );
	}

	//	Note : 중복 실행 방지.
	//
#if !defined(_RELEASED)
	m_hMutex = CreateMutex( NULL, TRUE, "RAN-ONLINE" );
#endif

	if ( GetLastError() == ERROR_ALREADY_EXISTS ) 
	{ 
		return FALSE;
	}

	LPCSTR strClass = AfxRegisterWndClass
						(
							CS_DBLCLKS,
							::LoadCursor(NULL, IDC_ARROW),
							(HBRUSH)GetStockObject(BLACK_BRUSH),
							AfxGetApp()->LoadIcon(IDR_MAINFRAME)
						);
	if( !strClass )
	{
		AfxMessageBox( _T( "Class Registration Failed\n" ) );
		return FALSE;
	}

	if ( CreateEx ( WS_EX_APPWINDOW, strClass, AfxGetAppName(), nWindowStyle,
		rectWnd.left, rectWnd.top, rectWnd.right, rectWnd.bottom, NULL, NULL, 0 ) == FALSE )
	{
		return FALSE;
	}

	// Modified to use the title in param.ini - YeXiuPH
	// Removed the quotes to fix what YeXiuPH wants ^ - Lars 04/03/2020
	SetWindowText (RANPARAM::WindowTitle);

	UpdateWindow();

	//	Note : 블럭 프로그램 디텍터.
	//
	//bool bBLOCK_LOAD = g_BLOCK_PROG.LOAD ( pBasicApp->m_szAppPath, "form.set" );
	//if ( !bBLOCK_LOAD )		return E_FAIL;

	//	Note : 디텍터 시작.
	//g_BLOCK_PROG.DoSTARTTHREAD ();

	CD3DApplication::SetScreen( RANPARAM::dwScrWidth, 
								RANPARAM::dwScrHeight, 
								RANPARAM::emScrFormat, 
								RANPARAM::uScrRefreshHz, 
								RANPARAM::bScrWindowed );

	if ( FAILED( CD3DApplication::Create ( m_hWnd, m_hWnd, AfxGetInstanceHandle() ) ) )
		return FALSE;
	
	m_bCreated = TRUE;
	g_hWnd = m_hWnd;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

void CBasicWnd::PostNcDestroy() 
{
	::UnregisterClass( AfxGetAppName(), AfxGetInstanceHandle() ); 

	//	Note : 디텍터 종료.
	//g_BLOCK_PROG.DoENDTHREAD ();

	//	Note : 모든 게임관련 겍�?정리.
	//
	Cleanup3DEnvironment();

	CloseHandle( m_hMutex );
	m_hMutex = NULL;

	CWnd::PostNcDestroy();
	m_pApp->ShutDown();

	if ( RANPARAM::bGameCursor )
	{
		::SetCursor ( NULL );
		if ( m_hCursorDefault )		::DestroyCursor ( m_hCursorDefault );
	}

	delete this;

	HWND hTrayWnd = ::FindWindow("Shell_TrayWnd", NULL);
	::SendMessage( hTrayWnd, WM_COMMAND, 416, 0 );
}

/////////////////////////////////////////////////////////////////////////////

#if _MSC_VER >= 1200
	void CBasicWnd::OnActivateApp(BOOL bActive, DWORD hTask)
#else
	void CBasicWnd::OnActivateApp(BOOL bActive, HTASK hTask)
#endif
{
	CWnd::OnActivateApp(bActive, hTask);
	//m_pApp->SetActive(bActive);
}

/////////////////////////////////////////////////////////////////////////////

BOOL CBasicWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if ( RANPARAM::bGameCursor )
	{
		HCURSOR hCursl = CCursor::GetInstance().GetCurCursor();
		if ( hCursl )
		{
			CCursor::GetInstance().SetCursorNow ();
			return TRUE;
		}
	}

	return CWnd::OnSetCursor ( pWnd, nHitTest, message );
}

void CBasicWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( RANPARAM::bGameCursor )
	{
		HCURSOR hCursl = CCursor::GetInstance().GetCurCursor();
		if ( hCursl )
		{
			CCursor::GetInstance().SetCursorNow ();
			return;
		}
	}

	__super::OnMouseMove(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////

void CBasicWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch(nChar)
	{
	case VK_ESCAPE:
		//PostMessage(WM_CLOSE, 0, 0);
		break;
	};

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

/////////////////////////////////////////////////////////////////////////////

// 통신알림 메시지
LRESULT CBasicWnd::OnNetNotify(WPARAM wParam, LPARAM lParam)
{
	DxGlobalStage::GetInstance().OnNetNotify ( wParam, lParam );
	return NULL;
}
