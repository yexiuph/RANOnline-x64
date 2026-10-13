//
//	YXModelVerify --meshes : builds each skin both with D3DXLoadMeshHierarchyFromX and with
//	yx::LoadMeshHierarchy (Assimp) and reports how the two hierarchies differ.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3dx9.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "YXMeshHierarchy.h"

namespace
{
	char* CopyName ( LPCSTR szName )
	{
		if ( !szName )	return NULL;
		const size_t nLen = std::strlen ( szName ) + 1;
		char* szCopy = new char[nLen];
		std::memcpy ( szCopy, szName, nLen );
		return szCopy;
	}

	//	Keeps just what the comparison needs.
	class CAllocate : public ID3DXAllocateHierarchy
	{
	public:
		STDMETHOD(CreateFrame)(THIS_ LPCSTR Name, LPD3DXFRAME* ppNewFrame)
		{
			D3DXFRAME* pFrame = new D3DXFRAME;
			std::memset ( pFrame, 0, sizeof(*pFrame) );
			pFrame->Name = CopyName ( Name );
			D3DXMatrixIdentity ( &pFrame->TransformationMatrix );
			*ppNewFrame = pFrame;
			return S_OK;
		}

		STDMETHOD(CreateMeshContainer)(THIS_ LPCSTR Name, CONST D3DXMESHDATA* pMeshData, CONST D3DXMATERIAL*, CONST D3DXEFFECTINSTANCE*,
										DWORD NumMaterials, CONST DWORD*, LPD3DXSKININFO pSkinInfo, LPD3DXMESHCONTAINER* ppNewMeshContainer)
		{
			D3DXMESHCONTAINER* pContainer = new D3DXMESHCONTAINER;
			std::memset ( pContainer, 0, sizeof(*pContainer) );
			pContainer->Name = CopyName ( Name );
			pContainer->MeshData = *pMeshData;
			if ( pContainer->MeshData.pMesh )	pContainer->MeshData.pMesh->AddRef();
			pContainer->NumMaterials = NumMaterials;
			pContainer->pSkinInfo = pSkinInfo;
			if ( pSkinInfo )	pSkinInfo->AddRef();
			*ppNewMeshContainer = pContainer;
			return S_OK;
		}

		STDMETHOD(DestroyFrame)(THIS_ LPD3DXFRAME pFrameToFree)
		{
			delete[] pFrameToFree->Name;
			delete pFrameToFree;
			return S_OK;
		}

		STDMETHOD(DestroyMeshContainer)(THIS_ LPD3DXMESHCONTAINER pContainer)
		{
			if ( pContainer->MeshData.pMesh )	pContainer->MeshData.pMesh->Release();
			if ( pContainer->pSkinInfo )		pContainer->pSkinInfo->Release();
			delete[] pContainer->Name;
			delete pContainer;
			return S_OK;
		}
	};

	struct SContainerInfo
	{
		std::string					strName;
		DWORD						dwMaterials;
		DWORD						dwVertices;
		DWORD						dwFaces;
		std::vector<std::string>	vecBones;
	};

	struct SHierarchyInfo
	{
		std::vector<std::string>	vecFrames;		// depth-first, unnamed frames as ""
		std::vector<SContainerInfo>	vecContainers;	// depth-first
	};

	void Collect ( const D3DXFRAME* pFrame, SHierarchyInfo& sInfo )
	{
		for ( ; pFrame; pFrame = pFrame->pFrameSibling )
		{
			sInfo.vecFrames.push_back ( pFrame->Name ? pFrame->Name : "" );

			for ( const D3DXMESHCONTAINER* pC = pFrame->pMeshContainer; pC; pC = pC->pNextMeshContainer )
			{
				SContainerInfo sC;
				sC.strName = pC->Name ? pC->Name : "";
				sC.dwMaterials = pC->NumMaterials;
				sC.dwVertices = pC->MeshData.pMesh ? pC->MeshData.pMesh->GetNumVertices() : 0;
				sC.dwFaces = pC->MeshData.pMesh ? pC->MeshData.pMesh->GetNumFaces() : 0;
				if ( pC->pSkinInfo )
				{
					for ( DWORD b=0; b<pC->pSkinInfo->GetNumBones(); ++b )
						sC.vecBones.push_back ( pC->pSkinInfo->GetBoneName(b) );
				}
				sInfo.vecContainers.push_back ( sC );
			}

			Collect ( pFrame->pFrameFirstChild, sInfo );
		}
	}

	struct STally
	{
		int nFiles, nD3DXFailed, nYXFailed;
		int nSameFrames, nSameContainers, nSameMaterials, nSameFaces, nSameVertices, nSameBoneSet, nSameBoneOrder;
		STally () { std::memset ( this, 0, sizeof(*this) ); }
	};

	void CompareFile ( IDirect3DDevice9* pDevice, const std::string& strFile, STally& sTally, int& nExamples )
	{
		++sTally.nFiles;
		CAllocate Alloc;

		LPD3DXFRAME pRootD3DX = NULL;
		if ( FAILED(D3DXLoadMeshHierarchyFromXA(strFile.c_str(), D3DXMESH_SYSTEMMEM, pDevice, &Alloc, NULL, &pRootD3DX, NULL)) )
		{
			++sTally.nD3DXFailed;
			return;
		}

		std::string strError;
		LPD3DXFRAME pRootYX = NULL;
		if ( FAILED(yx::LoadMeshHierarchy(strFile.c_str(), D3DXMESH_SYSTEMMEM, pDevice, &Alloc, &pRootYX, strError)) )
		{
			++sTally.nYXFailed;
			std::printf ( "YX FAIL  %s : %s\n", strFile.c_str(), strError.c_str() );
			D3DXFrameDestroy ( pRootD3DX, &Alloc );
			return;
		}

		SHierarchyInfo a, b;
		Collect ( pRootD3DX, a );
		Collect ( pRootYX, b );

		//	D3DX keeps unnamed frames; compare the named ones.
		std::vector<std::string> vecNamedA, vecNamedB;
		for ( size_t i=0; i<a.vecFrames.size(); ++i )	if ( !a.vecFrames[i].empty() )	vecNamedA.push_back ( a.vecFrames[i] );
		for ( size_t i=0; i<b.vecFrames.size(); ++i )	if ( !b.vecFrames[i].empty() )	vecNamedB.push_back ( b.vecFrames[i] );
		const bool bFrames = vecNamedA == vecNamedB;

		bool bContainers = a.vecContainers.size() == b.vecContainers.size();
		bool bMaterials = bContainers, bFaces = bContainers, bVertices = bContainers, bBoneSet = bContainers, bBoneOrder = bContainers;
		for ( size_t i=0; bContainers && i<a.vecContainers.size(); ++i )
		{
			const SContainerInfo& ca = a.vecContainers[i];
			const SContainerInfo& cb = b.vecContainers[i];
			if ( ca.strName != cb.strName )			bContainers = false;
			if ( ca.dwMaterials != cb.dwMaterials )	bMaterials = false;
			if ( ca.dwFaces != cb.dwFaces )			bFaces = false;
			if ( ca.dwVertices != cb.dwVertices )	bVertices = false;
			if ( ca.vecBones != cb.vecBones )		bBoneOrder = false;
			std::vector<std::string> sa = ca.vecBones, sb = cb.vecBones;
			std::sort ( sa.begin(), sa.end() );
			std::sort ( sb.begin(), sb.end() );
			if ( sa != sb )							bBoneSet = false;
		}

		sTally.nSameFrames		+= bFrames;
		sTally.nSameContainers	+= bContainers;
		sTally.nSameMaterials	+= bContainers && bMaterials;
		sTally.nSameFaces		+= bContainers && bFaces;
		sTally.nSameVertices	+= bContainers && bVertices;
		sTally.nSameBoneSet		+= bContainers && bBoneSet;
		sTally.nSameBoneOrder	+= bContainers && bBoneOrder;

		if ( nExamples > 0 && !( bFrames && bContainers && bMaterials && bFaces && bVertices && bBoneOrder ) )
		{
			--nExamples;
			std::printf ( "DIFF     %s :", strFile.c_str() );
			if ( !bFrames )		std::printf ( " frames(%u vs %u named)", (unsigned)vecNamedA.size(), (unsigned)vecNamedB.size() );
			if ( !bContainers )	std::printf ( " containers(%u vs %u)", (unsigned)a.vecContainers.size(), (unsigned)b.vecContainers.size() );
			else
			{
				if ( !bMaterials )	std::printf ( " materials" );
				if ( !bFaces )		std::printf ( " faces" );
				if ( !bVertices )	std::printf ( " vertices(%u vs %u)", a.vecContainers[0].dwVertices, b.vecContainers[0].dwVertices );
				if ( !bBoneOrder )	std::printf ( bBoneSet ? " bone-order" : " bone-set" );
			}
			std::printf ( "\n" );
		}

		D3DXFrameDestroy ( pRootYX, &Alloc );
		D3DXFrameDestroy ( pRootD3DX, &Alloc );
	}

	void CollectFiles ( const std::string& strPath, std::vector<std::string>& vecFiles )
	{
		const DWORD dwAttr = GetFileAttributesA ( strPath.c_str() );
		if ( dwAttr == INVALID_FILE_ATTRIBUTES )	return;
		if ( !(dwAttr & FILE_ATTRIBUTE_DIRECTORY) )
		{
			vecFiles.push_back ( strPath );
			return;
		}

		WIN32_FIND_DATAA fd;
		HANDLE hFind = FindFirstFileA ( (strPath + "\\*").c_str(), &fd );
		if ( hFind == INVALID_HANDLE_VALUE )	return;
		do
		{
			if ( std::strcmp(fd.cFileName, ".") == 0 || std::strcmp(fd.cFileName, "..") == 0 )	continue;
			const std::string strChild = strPath + "\\" + fd.cFileName;
			if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )	CollectFiles ( strChild, vecFiles );
			else if ( yx::IsXFile ( fd.cFileName ) )				vecFiles.push_back ( strChild );
		}
		while ( FindNextFileA(hFind, &fd) );
		FindClose ( hFind );
	}
}

int RunMeshVerify ( int argc, char** argv )
{
	HWND hWnd = CreateWindowA ( "STATIC", "YXModelVerify", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, NULL, NULL, NULL, NULL );
	IDirect3D9* pD3D = Direct3DCreate9 ( D3D_SDK_VERSION );
	if ( !hWnd || !pD3D )
	{
		std::printf ( "can't create a window or Direct3D 9\n" );
		return 2;
	}

	D3DPRESENT_PARAMETERS sPP;
	std::memset ( &sPP, 0, sizeof(sPP) );
	sPP.Windowed = TRUE;
	sPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
	sPP.BackBufferFormat = D3DFMT_UNKNOWN;
	sPP.hDeviceWindow = hWnd;

	IDirect3DDevice9* pDevice = NULL;
	if ( FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &sPP, &pDevice)) )
	{
		std::printf ( "can't create a Direct3D 9 device\n" );
		pD3D->Release();
		return 2;
	}

	std::vector<std::string> vecFiles;
	for ( int i=2; i<argc; ++i )
		CollectFiles ( argv[i], vecFiles );

	STally sTally;
	int nExamples = 15;
	for ( size_t i=0; i<vecFiles.size(); ++i )
		CompareFile ( pDevice, vecFiles[i], sTally, nExamples );

	const int nBoth = sTally.nFiles - sTally.nD3DXFailed - sTally.nYXFailed;
	std::printf ( "\nfiles %d : D3DX failed %d, Assimp failed %d, compared %d\n", sTally.nFiles, sTally.nD3DXFailed, sTally.nYXFailed, nBoth );
	std::printf ( "  same named frames (depth-first) : %d\n", sTally.nSameFrames );
	std::printf ( "  same mesh containers            : %d\n", sTally.nSameContainers );
	std::printf ( "  same material counts            : %d\n", sTally.nSameMaterials );
	std::printf ( "  same face counts                : %d\n", sTally.nSameFaces );
	std::printf ( "  same vertex counts              : %d\n", sTally.nSameVertices );
	std::printf ( "  same bone sets                  : %d\n", sTally.nSameBoneSet );
	std::printf ( "  same bone order                 : %d\n", sTally.nSameBoneOrder );

	pDevice->Release();
	pD3D->Release();
	DestroyWindow ( hWnd );
	return sTally.nYXFailed ? 1 : 0;
}
