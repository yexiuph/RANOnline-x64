//
//	YXModelVerify : checks that YXCore (Assimp) reads .x frame hierarchies exactly like DirectX.
//
//	Every file is read three ways and the bone trees DxSkeleton would build are compared:
//	  - d3dxof    IDirectXFile, the loader the engine used before (Win32 only, no 64-bit DLL exists)
//	  - D3DX      ID3DXFile from d3dx9
//	  - YXCore    yx::LoadFrameHierarchy
//	Names, order and matrices must match bit for bit.
//
//	Usage : YXModelVerify <file or directory> [...]
//	        YXModelVerify --meshes <file or directory> [...]   (see YXMeshVerify.cpp)
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3dx9.h>
#include <rmxfguid.h>
#include <rmxftmpl.h>
#if !defined(_WIN64)
#include <dxfile.h>
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "YXModelLoader.h"

namespace
{
	struct SBone
	{
		std::string			strName;
		float				matLocal[16];
		bool				bHasMatrix;
		std::vector<SBone>	vecChildren;

		SBone () : bHasMatrix(false)
		{
			std::memset ( matLocal, 0, sizeof(matLocal) );
			matLocal[0] = matLocal[5] = matLocal[10] = matLocal[15] = 1.f;
		}
	};

	//	Same rules as DxSkeleton::LoadBoneTrans : unnamed frames are dropped with their children,
	//	the FrameTransformMatrix child sets the frame's matrix, every other object is ignored.

	void FromYX ( const yx::SModelFrame& sFrame, std::vector<SBone>& vecOut )
	{
		if ( sFrame.strName.empty() )	return;

		SBone sBone;
		sBone.strName = sFrame.strName;
		std::memcpy ( sBone.matLocal, sFrame.matLocal, sizeof(sBone.matLocal) );
		sBone.bHasMatrix = true;
		for ( size_t i=0; i<sFrame.vecChildren.size(); ++i )
			FromYX ( sFrame.vecChildren[i], sBone.vecChildren );

		vecOut.push_back ( sBone );
	}

	bool LoadYX ( const char* szPath, std::vector<SBone>& vecOut, std::string& strError )
	{
		yx::SModelFrame sRoot;
		if ( !yx::LoadFrameHierarchy ( szPath, sRoot, strError ) )	return false;

		for ( size_t i=0; i<sRoot.vecChildren.size(); ++i )
			FromYX ( sRoot.vecChildren[i], vecOut );
		return true;
	}

	void FromD3DX ( ID3DXFileData* pData, SBone* pParent, std::vector<SBone>& vecOut )
	{
		GUID type;
		if ( FAILED(pData->GetType(&type)) )	return;

		if ( type == TID_D3DRMFrameTransformMatrix )
		{
			SIZE_T cbSize = 0;
			const void* pMatrix = NULL;
			if ( pParent && SUCCEEDED(pData->Lock(&cbSize, &pMatrix)) )
			{
				std::memcpy ( pParent->matLocal, pMatrix, sizeof(pParent->matLocal) );
				pParent->bHasMatrix = true;
				pData->Unlock();
			}
			return;
		}

		if ( type != TID_D3DRMFrame )	return;

		SIZE_T cchName = 0;
		if ( FAILED(pData->GetName(NULL, &cchName)) || cchName <= 1 )	return;
		std::string strName ( cchName, '\0' );
		if ( FAILED(pData->GetName(&strName[0], &cchName)) )	return;
		strName.resize ( std::strlen(strName.c_str()) );
		if ( strName.empty() )	return;

		SBone sBone;
		sBone.strName = strName;

		SIZE_T cChildren = 0;
		pData->GetChildren ( &cChildren );
		for ( SIZE_T i=0; i<cChildren; ++i )
		{
			ID3DXFileData* pChild = NULL;
			if ( FAILED(pData->GetChild(i, &pChild)) )	continue;
			if ( !pChild->IsReference() )
				FromD3DX ( pChild, &sBone, sBone.vecChildren );
			pChild->Release();
		}

		vecOut.push_back ( sBone );
	}

	bool LoadD3DX ( const char* szPath, std::vector<SBone>& vecOut, std::string& strError )
	{
		ID3DXFile* pFile = NULL;
		ID3DXFileEnumObject* pEnum = NULL;
		bool bOK = false;

		if ( SUCCEEDED(D3DXFileCreate(&pFile)) &&
			 SUCCEEDED(pFile->RegisterTemplates(D3DRM_XTEMPLATES, D3DRM_XTEMPLATE_BYTES)) &&
			 SUCCEEDED(pFile->CreateEnumObject(szPath, D3DXF_FILELOAD_FROMFILE, &pEnum)) )
		{
			SIZE_T cChildren = 0;
			pEnum->GetChildren ( &cChildren );
			for ( SIZE_T i=0; i<cChildren; ++i )
			{
				ID3DXFileData* pData = NULL;
				if ( FAILED(pEnum->GetChild(i, &pData)) )	continue;
				FromD3DX ( pData, NULL, vecOut );
				pData->Release();
			}
			bOK = true;
		}
		else
		{
			strError = "ID3DXFile could not open the file";
		}

		if ( pEnum )	pEnum->Release();
		if ( pFile )	pFile->Release();
		return bOK;
	}

#if !defined(_WIN64)
	void FromDXOF ( IDirectXFileData* pData, SBone* pParent, std::vector<SBone>& vecOut )
	{
		const GUID* pType = NULL;
		if ( FAILED(pData->GetType(&pType)) )	return;

		if ( *pType == TID_D3DRMFrameTransformMatrix )
		{
			DWORD cbSize = 0;
			void* pMatrix = NULL;
			if ( pParent && SUCCEEDED(pData->GetData(NULL, &cbSize, &pMatrix)) )
			{
				std::memcpy ( pParent->matLocal, pMatrix, sizeof(pParent->matLocal) );
				pParent->bHasMatrix = true;
			}
			return;
		}

		if ( *pType != TID_D3DRMFrame )	return;

		//	Same test as the old DxSkeleton::LoadBoneTrans.
		DWORD cchName = 0;
		if ( FAILED(pData->GetName(NULL, &cchName)) || cchName == 0 )	return;
		std::string strName ( cchName + 1, '\0' );
		if ( FAILED(pData->GetName(&strName[0], &cchName)) )	return;
		strName.resize ( std::strlen(strName.c_str()) );

		SBone sBone;
		sBone.strName = strName;

		IDirectXFileObject* pChild = NULL;
		while ( SUCCEEDED(pData->GetNextObject(&pChild)) )
		{
			IDirectXFileData* pChildData = NULL;
			if ( SUCCEEDED(pChild->QueryInterface(IID_IDirectXFileData, (void**)&pChildData)) )
			{
				FromDXOF ( pChildData, &sBone, sBone.vecChildren );
				pChildData->Release();
			}
			pChild->Release();
		}

		vecOut.push_back ( sBone );
	}

	bool LoadDXOF ( const char* szPath, std::vector<SBone>& vecOut, std::string& strError )
	{
		IDirectXFile* pFile = NULL;
		IDirectXFileEnumObject* pEnum = NULL;
		bool bOK = false;

		if ( SUCCEEDED(DirectXFileCreate(&pFile)) &&
			 SUCCEEDED(pFile->RegisterTemplates((void*)D3DRM_XTEMPLATES, D3DRM_XTEMPLATE_BYTES)) &&
			 SUCCEEDED(pFile->CreateEnumObject((void*)szPath, DXFILELOAD_FROMFILE, &pEnum)) )
		{
			IDirectXFileData* pData = NULL;
			while ( SUCCEEDED(pEnum->GetNextDataObject(&pData)) )
			{
				FromDXOF ( pData, NULL, vecOut );
				pData->Release();
			}
			bOK = true;
		}
		else
		{
			strError = "d3dxof could not open the file";
		}

		if ( pEnum )	pEnum->Release();
		if ( pFile )	pFile->Release();
		return bOK;
	}
#endif

	struct SStats
	{
		int nBones;
		int nNoMatrix;
		SStats () : nBones(0), nNoMatrix(0) {}
	};

	//	Returns an empty string when both trees are identical.
	std::string Compare ( const std::vector<SBone>& vecA, const std::vector<SBone>& vecB, const std::string& strPath, SStats& sStats )
	{
		char szBuf[512];
		if ( vecA.size() != vecB.size() )
		{
			sprintf_s ( szBuf, "%s: child count %u vs %u", strPath.c_str(), (unsigned)vecA.size(), (unsigned)vecB.size() );
			return szBuf;
		}

		for ( size_t i=0; i<vecA.size(); ++i )
		{
			const SBone& a = vecA[i];
			const SBone& b = vecB[i];
			const std::string strHere = strPath + "/" + a.strName;

			if ( a.strName != b.strName )
				return strPath + ": name '" + a.strName + "' vs '" + b.strName + "'";

			if ( std::memcmp(a.matLocal, b.matLocal, sizeof(a.matLocal)) != 0 )
			{
				for ( int k=0; k<16; ++k )
				{
					if ( std::memcmp(&a.matLocal[k], &b.matLocal[k], sizeof(float)) != 0 )
					{
						sprintf_s ( szBuf, "%s: matrix[%d] %.9g vs %.9g", strHere.c_str(), k, a.matLocal[k], b.matLocal[k] );
						return szBuf;
					}
				}
			}

			++sStats.nBones;
			if ( !a.bHasMatrix )	++sStats.nNoMatrix;

			const std::string strChild = Compare ( a.vecChildren, b.vecChildren, strHere, sStats );
			if ( !strChild.empty() )	return strChild;
		}

		return std::string();
	}

	struct SSummary
	{
		int nFiles, nMatch, nMismatch, nFailed, nBones, nNoMatrixFiles;
		SSummary () : nFiles(0), nMatch(0), nMismatch(0), nFailed(0), nBones(0), nNoMatrixFiles(0) {}
	};

	void VerifyFile ( const std::string& strFile, SSummary& sSum )
	{
		++sSum.nFiles;

		std::string strError;
		std::vector<SBone> vecYX, vecD3DX;
		if ( !LoadYX(strFile.c_str(), vecYX, strError) )
		{
			++sSum.nFailed;
			std::printf ( "FAIL     %s : YXCore : %s\n", strFile.c_str(), strError.c_str() );
			return;
		}
		if ( !LoadD3DX(strFile.c_str(), vecD3DX, strError) )
		{
			++sSum.nFailed;
			std::printf ( "FAIL     %s : %s\n", strFile.c_str(), strError.c_str() );
			return;
		}

		SStats sStats;
		std::string strDiff = Compare ( vecD3DX, vecYX, "", sStats );
		if ( strDiff.empty() )
		{
			//	ID3DXFile reports the matrix presence, which Assimp can't; count frames that had none.
			SStats sD3DX;
			Compare ( vecD3DX, vecD3DX, "", sD3DX );
			if ( sD3DX.nNoMatrix )
			{
				++sSum.nNoMatrixFiles;
				std::printf ( "NOTE     %s : %d frame(s) without FrameTransformMatrix\n", strFile.c_str(), sD3DX.nNoMatrix );
			}
		}

#if !defined(_WIN64)
		if ( strDiff.empty() )
		{
			std::vector<SBone> vecDXOF;
			if ( !LoadDXOF(strFile.c_str(), vecDXOF, strError) )
			{
				++sSum.nFailed;
				std::printf ( "FAIL     %s : %s\n", strFile.c_str(), strError.c_str() );
				return;
			}
			SStats sDXOF;
			strDiff = Compare ( vecDXOF, vecYX, "", sDXOF );
			if ( !strDiff.empty() )	strDiff = "d3dxof vs YXCore " + strDiff;
		}
		else
		{
			strDiff = "D3DX vs YXCore " + strDiff;
		}
#else
		if ( !strDiff.empty() )	strDiff = "D3DX vs YXCore " + strDiff;
#endif

		if ( strDiff.empty() )
		{
			++sSum.nMatch;
			sSum.nBones += sStats.nBones;
		}
		else
		{
			++sSum.nMismatch;
			std::printf ( "MISMATCH %s : %s\n", strFile.c_str(), strDiff.c_str() );
		}
	}

	bool HasXExtension ( const char* szName )
	{
		const char* szExt = std::strrchr ( szName, '.' );
		return szExt && _stricmp ( szExt, ".x" ) == 0;
	}

	void VerifyPath ( const std::string& strPath, SSummary& sSum )
	{
		const DWORD dwAttr = GetFileAttributesA ( strPath.c_str() );
		if ( dwAttr == INVALID_FILE_ATTRIBUTES )
		{
			std::printf ( "missing  %s\n", strPath.c_str() );
			return;
		}

		if ( !(dwAttr & FILE_ATTRIBUTE_DIRECTORY) )
		{
			VerifyFile ( strPath, sSum );
			return;
		}

		WIN32_FIND_DATAA fd;
		HANDLE hFind = FindFirstFileA ( (strPath + "\\*").c_str(), &fd );
		if ( hFind == INVALID_HANDLE_VALUE )	return;
		do
		{
			if ( std::strcmp(fd.cFileName, ".") == 0 || std::strcmp(fd.cFileName, "..") == 0 )	continue;

			const std::string strChild = strPath + "\\" + fd.cFileName;
			if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )		VerifyPath ( strChild, sSum );
			else if ( HasXExtension(fd.cFileName) )					VerifyFile ( strChild, sSum );
		}
		while ( FindNextFileA(hFind, &fd) );
		FindClose ( hFind );
	}
}

int RunMeshVerify ( int argc, char** argv );	// YXMeshVerify.cpp

int main ( int argc, char** argv )
{
	if ( argc < 2 )
	{
		std::printf ( "usage: YXModelVerify <file or directory> [...]           frame hierarchies vs DirectX\n" );
		std::printf ( "       YXModelVerify --meshes <file or directory> [...]  skins : D3DX vs YXCore/Assimp\n" );
		return 2;
	}

	if ( std::strcmp ( argv[1], "--meshes" ) == 0 )
		return RunMeshVerify ( argc, argv );

#if defined(_WIN64)
	std::printf ( "x64 build : comparing YXCore against D3DX (d3dxof has no 64-bit DLL)\n" );
#else
	std::printf ( "Win32 build : comparing YXCore against D3DX and d3dxof\n" );
#endif

	SSummary sSum;
	for ( int i=1; i<argc; ++i )
		VerifyPath ( argv[i], sSum );

	std::printf ( "\nfiles %d : match %d, mismatch %d, failed %d (%d bones compared, %d files with frames lacking a matrix)\n",
				  sSum.nFiles, sSum.nMatch, sSum.nMismatch, sSum.nFailed, sSum.nBones, sSum.nNoMatrixFiles );

	return ( sSum.nMismatch || sSum.nFailed ) ? 1 : 0;
}
