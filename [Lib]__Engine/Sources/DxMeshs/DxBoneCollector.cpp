#include "pch.h"

#include "./stlfunctions.h"
#include "./StringUtils.h"
#include "./StringFile.h"
#include <algorithm>

#include "./DxMethods.h"
#include "./editmeshs.h"
#include "./DxViewPort.h"
#include "./DxInputDevice.h"
#include "DxSkinDefine.h"

#include "DxBoneCollector.h"
#include "YXModelLoader.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define __ROOT	("__ROOT")
DxBoneCollector& DxBoneCollector::GetInstance()
{
	static DxBoneCollector Instance;
	return Instance;
}

extern DWORD UPDATEBONECOUNT;

D3DXMATRIX	 g_matTemp;
float	DxSkeleton::g_fWeight = 1.f;


void DxBoneTrans::ResetBone ()
{
#ifdef USE_ANI_QUATERNION
	m_dwFlag = 0L;
	m_fScale = 1.f;
	D3DXQuaternionIdentity( &m_vQuatMIX );
	m_pQuatPosCUR = m_pQuatPosORIG;
	m_pQuatPosPREV = m_pQuatPosORIG;

#else
	matRot = matRotOrig;

#endif

	if ( pBoneTransFirstChild != NULL )		pBoneTransFirstChild->ResetBone ();
	if ( pBoneTransSibling != NULL )		pBoneTransSibling->ResetBone ();
}

void DxSkeleton::UpdateBones ( DxBoneTrans *pBoneCur, const D3DXMATRIX &matCur )
{
	++UPDATEBONECOUNT;

#ifndef USE_ANI_QUATERNION

	//	Note : pBoneCur의 메트릭스를 계산. matCombined = matRot * matParent * matTrans
	D3DXMatrixMultiply( &pBoneCur->matCombined, &pBoneCur->matRot, &matCur );
	
	//	Note : 자식 프레임의 메트릭스 모두 계산.
	//
	DxBoneTrans *pBoneChild = pBoneCur->pBoneTransFirstChild;
	while( pBoneChild )
	{
		UpdateBones( pBoneChild, pBoneCur->matCombined );

		pBoneChild = pBoneChild->pBoneTransSibling;
	}

#else

	// Note : Base
	SQuatPos* pQuatPos = &pBoneCur->m_pQuatPosCUR;
	DWORD dwFlag = pBoneCur->m_dwFlag;

	// Note : Animation Blending
	if( dwFlag & USE_ANI_BLENDING )
	{
		SQuatPos* pQuatPosPREV = &pBoneCur->m_pQuatPosPREV;

		// Note : 보간 작업.
		D3DXVec3Lerp( &pQuatPos->m_vPos, &pQuatPosPREV->m_vPos, &pQuatPos->m_vPos, g_fWeight );
		D3DXVec3Lerp( &pQuatPos->m_vScale, &pQuatPosPREV->m_vScale, &pQuatPos->m_vScale, g_fWeight );
		D3DXQuaternionSlerp( &pQuatPos->m_vQuat, &pQuatPosPREV->m_vQuat, &pQuatPos->m_vQuat, g_fWeight );
	}

	// Note : Ani 를 더했다면..~!!
	if( dwFlag & USE_ANI_ADD )
	{
		D3DXQuaternionNormalize( &pQuatPos->m_vQuat, &pQuatPos->m_vQuat );
		D3DXQuaternionNormalize( &pBoneCur->m_vQuatMIX , &pBoneCur->m_vQuatMIX  );
		//D3DXQuaternionMultiply( &pQuatPos->m_vQuat, &pQuatPos->m_vQuat, &pBoneCur->m_vQuatMIX );
		D3DXQuaternionMultiply( &pQuatPos->m_vQuat, &pBoneCur->m_vQuatMIX, &pQuatPos->m_vQuat );
	}

	D3DXMatrixRotationQuaternion( &g_matTemp, &pQuatPos->m_vQuat );

	g_matTemp._11 *= pQuatPos->m_vScale.x * pBoneCur->m_fScale;
	g_matTemp._12 *= pQuatPos->m_vScale.x * pBoneCur->m_fScale;
	g_matTemp._13 *= pQuatPos->m_vScale.x * pBoneCur->m_fScale;
	g_matTemp._21 *= pQuatPos->m_vScale.y * pBoneCur->m_fScale;
	g_matTemp._22 *= pQuatPos->m_vScale.y * pBoneCur->m_fScale;
	g_matTemp._23 *= pQuatPos->m_vScale.y * pBoneCur->m_fScale;
	g_matTemp._31 *= pQuatPos->m_vScale.z * pBoneCur->m_fScale;
	g_matTemp._32 *= pQuatPos->m_vScale.z * pBoneCur->m_fScale;
	g_matTemp._33 *= pQuatPos->m_vScale.z * pBoneCur->m_fScale;
	g_matTemp._41 = pQuatPos->m_vPos.x;
	g_matTemp._42 = pQuatPos->m_vPos.y;
	g_matTemp._43 = pQuatPos->m_vPos.z;

	//BYTE* pByte01 = (BYTE*)&pQuatPos->m_vScale;
	///BYTE* pByte02 = (BYTE*)&g_matTemp;

	//__asm
	//{
	//	mov		esi,	   pByte01	   	   // esi <- 
	//	mov		edi,	   pByte02		   	   	   // edi <- 최종 데이터가 저장되야할 데이터 포인터 세팅

	//	movss	xmm0,	  [esi]	  	   	   	   	   // xmm0 에 버텍스x~값을 세팅
	//	movss	xmm1,	  [esi+0x4]	  	   	   	   // xmm1 에 버텍스y~값 세팅
	//	movss	xmm2,	  [esi+0x8]	  	   	   	   // xmm2 에 버텍스z~값 세팅
	//	shufps	xmm0,	  xmm0, 0x0	  	   	   	   // xmm0의 4칸을, 버텍스x 로 채움
	//	shufps	xmm1,	  xmm1, 0x0	  	   	   	   // xmm1의 4칸을, 버텍스y 로 채움
	//	shufps	xmm2,	  xmm2, 0x0	  	   	   	   // xmm2의 4칸을, 버텍스z 로 채움

	//	movaps	xmm4,	  [edi]
	//	movaps	xmm5,	  [edi+0x10]	 	   // xmm4 에 매트릭스 21, 22, 23, 24 세팅
	//	movaps	xmm6,	  [edi+0x20]	 	   // xmm4 에 매트릭스 31, 32, 33, 34 세팅

	//	mulps	xmm4,	  xmm0	   	   	   	   // 버텍스 X와 매트릭스 11, 12, 13, 14 곱함
	//	mulps	xmm5,	  xmm1	   	   	   	   // 버텍스 Y와 매트릭스 21, 22, 23, 34 곱함
	//	mulps	xmm6,	  xmm2	   	   	   	   // 버텍스 Z와 매트릭스 31, 32, 33, 34 곱함

	//	movaps	[edi],		xmm4
	//	movaps	[edi+0x10],	xmm5
	//	movaps	[edi+0x20],	xmm6

	//	mov		esi,		pByte01
	//	mov		eax,		[esi]
	//	mov		[edi+0x30],	eax
	//	mov		eax,		[esi+0x4]
	//	mov		[edi+0x34],	eax
	//	mov		eax,		[esi+0x8]
	//	mov		[edi+0x38],	eax
	//}	

	//	Note : pBoneCur의 메트릭스를 계산. matCombined = matRot * matParent * matTrans
	D3DXMatrixMultiply( &pBoneCur->matCombined, &g_matTemp, &matCur );

	//	Note : 자식 프레임의 메트릭스 모두 계산.
	//
	DxBoneTrans *pBoneChild = pBoneCur->pBoneTransFirstChild;
	while( pBoneChild )
	{
		UpdateBones( pBoneChild, pBoneCur->matCombined );

		pBoneChild = pBoneChild->pBoneTransSibling;
	}

#endif
}

void DxSkeleton::CheckSphere( LPDIRECT3DDEVICEQ pd3dDevice, const char* szName )
{
	DxBoneTrans* pBoneTran = FindBone( szName );
	if( !pBoneTran )	return;

	D3DXVECTOR3 vPos( 0.f, 0.f, 0.f );
	vPos.x = pBoneTran->matCombined._41;
	vPos.y = pBoneTran->matCombined._42;
	vPos.z = pBoneTran->matCombined._43;

	pd3dDevice->SetRenderState( D3DRS_ZENABLE,		FALSE );
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,	FALSE );

	EDITMESHS::RENDERSPHERE( pd3dDevice, vPos, 0.006f*DxViewPort::GetInstance().GetDistance(), NULL, 0xffff0000 );

	pd3dDevice->SetRenderState( D3DRS_ZENABLE,		TRUE );
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,	TRUE );
}

void DxSkeleton::EditLineSphere( LPDIRECT3DDEVICEQ pd3dDevice )
{
	EditLineSphere( pd3dDevice, pBoneRoot );
}

void DxSkeleton::EditLineSphere( LPDIRECT3DDEVICEQ pd3dDevice, DxBoneTrans *pBoneCur )
{
	D3DXVECTOR3 vPos(0.f,0.f,0.f);
	vPos.x = pBoneCur->matCombined._41;
	vPos.y = pBoneCur->matCombined._42;
	vPos.z = pBoneCur->matCombined._43;

	EDITMESHS::RENDERSPHERE( pd3dDevice, vPos, 0.003f*DxViewPort::GetInstance().GetDistance() );

	//	Note : 자식 프레임의 메트릭스 모두 계산.
	DxBoneTrans *pBoneChild = pBoneCur->pBoneTransFirstChild;
	while ( pBoneChild != NULL )
	{
		D3DXVECTOR3 vPosChild(0.f,0.f,0.f);
		vPosChild.x = pBoneChild->matCombined._41;
		vPosChild.y = pBoneChild->matCombined._42;
		vPosChild.z = pBoneChild->matCombined._43;

		EDITMESHS::RENDERLINE( pd3dDevice, vPos, vPosChild );

		EditLineSphere( pd3dDevice, pBoneChild );
		pBoneChild = pBoneChild->pBoneTransSibling;
	}
}

BOOL DxSkeleton::IsCollision( D3DXVECTOR3& vFrom, D3DXVECTOR3& vLook, std::string& strName, const float fLength )
{
	float fBestDis = FLT_MAX;
	CheckCollision( vFrom, vLook, strName, fLength, fBestDis, pBoneRoot );

	if( strName.size() > 0 )	return TRUE;
	else						return FALSE;
}

void DxSkeleton::CheckCollision( D3DXVECTOR3& vFrom, D3DXVECTOR3& vLook, std::string& strName, const float fLength, float fBestDis, DxBoneTrans *pBoneCur )
{
	D3DXVECTOR3 vPos(0.f,0.f,0.f);
	vPos.x = pBoneCur->matCombined._41;
	vPos.y = pBoneCur->matCombined._42;
	vPos.z = pBoneCur->matCombined._43;

	// Note : 원에 픽킹 했는지 체크
	if( COLLISION::IsCollisionLineToSphere( vFrom, vLook, vPos, fLength ) )
	{
		const D3DXVECTOR3 vDis = vFrom - vPos;
		const float fDis = D3DXVec3Length( &vDis );

		if( fDis < fBestDis )
		{
			strName = pBoneCur->szName;
			fBestDis = fDis;
		}
	}

	// Note : 자식 프레임의 메트릭스 모두 계산.
	DxBoneTrans *pBoneChild = pBoneCur->pBoneTransFirstChild;
	while ( pBoneChild != NULL )
	{
		CheckCollision( vFrom, vLook, strName, fLength, fBestDis, pBoneChild );	
		pBoneChild = pBoneChild->pBoneTransSibling;
	}
}

HRESULT DxSkeleton::LoadFile ( const char *szFile, LPDIRECT3DDEVICEQ pd3dDevice )
{
	if (szFile == NULL)	return E_INVALIDARG;

	SAFE_DELETE(pBoneRoot);

	int cchFileName = (int)strlen(szFile);
	if (cchFileName < 2)	return E_FAIL;

	SAFE_DELETE_ARRAY(szXFileName);
	int nStrLen = cchFileName+1;
	szXFileName = new char[nStrLen];
	StringCchCopy( szXFileName, nStrLen, szFile );

	char szPathName[MAX_PATH] = "";
	StringCchCopy( szPathName, MAX_PATH, DxBoneCollector::GetInstance().GetPath() );
	StringCchCat( szPathName, MAX_PATH, szXFileName );

	//	Note : The frame hierarchy is read through YXCore (Assimp); d3dxof.dll has no 64-bit version.
	//
	yx::SModelFrame sRoot;
	std::string strError;
	if ( !yx::LoadFrameHierarchy ( szPathName, sRoot, strError ) )
	{
		CDebugSet::ToLogFile ( "ERROR : DxSkeleton::LoadFile() %s, %s", szPathName, strError.c_str() );
		return E_FAIL;
	}

	pBoneRoot = new DxBoneTrans;
	nStrLen = (int)strlen(__ROOT)+1;
	pBoneRoot->szName = new char[nStrLen];
	StringCchCopy( pBoneRoot->szName, nStrLen, __ROOT );

	for ( size_t i=0; i<sRoot.vecChildren.size(); ++i )
	{
		LoadBoneTrans ( pBoneRoot, sRoot.vecChildren[i], EMBODY_DEFAULT );
	}

	return S_OK;
}

void DxSkeleton::LoadBoneTrans ( DxBoneTrans* pParent, const yx::SModelFrame& sFrame, EMBODY emBody )
{
	//	Note : Unnamed frames are skipped together with their children.
	//
	if ( sFrame.strName.empty() )	return;

	DxBoneTrans* pBoneTransCur = new DxBoneTrans();
	pBoneTransCur->SetName ( const_cast<char*>( sFrame.strName.c_str() ) );

	switch( emBody )
	{
	case EMBODY_DEFAULT:
		if( sFrame.strName == "Bip01_Spine1" )
		{
			pBoneTransCur->m_emBody = EMBODY_UPBODY;
		}
		else
		{
			pBoneTransCur->m_emBody = EMBODY_DEFAULT;
		}
		break;

	case EMBODY_UPBODY:
		pBoneTransCur->m_emBody = EMBODY_UPBODY;
		break;
	};

	pBoneTransCur->matRot = pBoneTransCur->matRotOrig = D3DXMATRIX ( sFrame.matLocal );
	D3DXMatrixToSQT( pBoneTransCur->m_pQuatPosCUR.m_vScale, pBoneTransCur->m_pQuatPosCUR.m_vQuat, pBoneTransCur->m_pQuatPosCUR.m_vPos, pBoneTransCur->matRot );
	pBoneTransCur->m_pQuatPosPREV = pBoneTransCur->m_pQuatPosORIG = pBoneTransCur->m_pQuatPosCUR;

	BoneMap.insert ( std::make_pair(std::string(pBoneTransCur->szName),pBoneTransCur) );
	pParent->AddBoneTrans(pBoneTransCur);

	for ( size_t i=0; i<sFrame.vecChildren.size(); ++i )
	{
		LoadBoneTrans ( pBoneTransCur, sFrame.vecChildren[i], pBoneTransCur->m_emBody );
	}
}

DxBoneCollector::DxBoneCollector(void)
{
	memset( m_szPath, 0, sizeof(char)*MAX_PATH );
}

DxBoneCollector::~DxBoneCollector(void)
{
	CleanUp ();
}

void DxBoneCollector::OneTimeSceneInit ( char* szPath )
{
	StringCchCopy( m_szPath, MAX_PATH, szPath );
}

void DxBoneCollector::CleanUp ()
{
	std::for_each ( m_MapBone.begin(), m_MapBone.end(), std_afunc::DeleteMapObject() );
	m_MapBone.clear ();
}

DxSkeleton* DxBoneCollector::Find ( const char* szFile )
{
	BONEMAP_ITER iter = m_MapBone.find ( std::string(szFile) );
	if ( iter!=m_MapBone.end() )	return iter->second;

	return NULL;
}

DxSkeleton* DxBoneCollector::Load ( const char* _szFile, LPDIRECT3DDEVICEQ pd3dDevice )
{
	HRESULT hr = S_OK;
	DxSkeleton* pSkeleton;

	CString strFile(_szFile);
	strFile.MakeLower();

	//	Note : 이미 로드된 뼈다귀인지 검사후 처리.
	//
	pSkeleton = Find ( strFile.GetString() );
	if ( pSkeleton )
	{
		pSkeleton->dwRefCount++;
		return pSkeleton;
	}

	//CDebugSet::ToListView ( "DxSkeleton '%s' Load", strFile.GetString() );

	//	Note : 새로운 뼈다귀 생성.
	//
	pSkeleton = new DxSkeleton;
	hr = pSkeleton->LoadFile ( strFile.GetString(), pd3dDevice );
	if ( FAILED(hr) )	return NULL;
	pSkeleton->dwRefCount++;
	
	//	Note : 리스트에 삽입.
	//
	m_MapBone[std::string(pSkeleton->szXFileName)] = pSkeleton;

	return pSkeleton;
}

BOOL DxBoneCollector::PreLoad ( const char* szBoneList, LPDIRECT3DDEVICEQ pd3dDevice )
{
	CString strPath;
	strPath = GetPath();
	strPath += szBoneList;	

	CStringFile StrFile;
	if ( !StrFile.Open ( strPath.GetString() ) )	return FALSE;

	STRUTIL::ClearSeparator ();
	STRUTIL::RegisterSeparator ( "\t" );
	STRUTIL::RegisterSeparator ( " " );

	CString strLine;
	while ( StrFile.GetNextLine(strLine) )
	{
		if ( strLine.GetLength() == 0 )		continue;

		CStringArray *pStrArray = new CStringArray;
		STRUTIL::StringSeparate ( strLine, *pStrArray );

		if ( pStrArray->GetSize() != 1 )
		{
			SAFE_DELETE(pStrArray);
			continue;
		}

		CString strBone = pStrArray->GetAt(0);
		strBone.MakeLower();

		DxSkeleton* pSkeleton = Load ( strBone.GetString(), pd3dDevice );
		if ( !pSkeleton )
		{
			CString strMessage;
			strMessage.Format ( "DxBoneCollector::PreLoad() [%s] Load에 실패하였습니다.", strBone );
			MessageBox ( NULL, strMessage, "ERROR", MB_OK );

			SAFE_DELETE(pStrArray);
			continue;
		}

		m_PreLoadBoneMap.insert ( std::make_pair(std::string(strBone.GetString()),pSkeleton) );

		SAFE_DELETE(pStrArray);
	};

	return TRUE;
}

HRESULT DxBoneCollector::DoInterimClean ( LPDIRECT3DDEVICEQ pd3dDevice )
{
	BONELIST listInterimClean;
	
	//	Note : 제거할 리스트 작성.
	//
	{
		BONEMAP_ITER iter = m_MapBone.begin();
		BONEMAP_ITER iter_end = m_MapBone.end();
		for ( ; iter!=iter_end; ++iter )
		{
			//	Note : 사전 로딩 리스트에 있는 파일 제외.
			//
			BONEMAP_ITER found = m_PreLoadBoneMap.find ( (*iter).first );
			if ( found!=m_PreLoadBoneMap.end() )	continue;

			//	제거 리스트에 등록.
			listInterimClean.push_back ( (*iter).second );
		}
	}

	//	Note : 데이터 정리.
	//
	{
		BONELIST_ITER iter = listInterimClean.begin();
		BONELIST_ITER iter_end = listInterimClean.end();
		for ( ; iter!=iter_end; ++iter )
		{
			DxSkeleton* pBone = (*iter);
			std::string strname = pBone->szXFileName;

			//	데이터 삭제.
			SAFE_DELETE(pBone);

			//	로드 파일 리스트에서 제거.
			BONEMAP_ITER iter_del = m_MapBone.find ( strname );
			if ( iter_del!=m_MapBone.end() )		m_MapBone.erase ( iter_del );
		}
	}

	return S_OK;
}

