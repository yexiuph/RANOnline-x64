#include "pch.h"
#include "./BaseString.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CBaseString::CBaseString () :
	m_dwIndex(0),
	m_dwLine(0),
	m_bDecode(FALSE),
	m_nVersion(-1),
	m_dwRead(0),
	m_nBufferSize(0),
	m_pBuffer(NULL)
{
	SecureZeroMemory ( m_szFileName, sizeof(m_szFileName) );
}

CBaseString::~CBaseString ()
{
}
