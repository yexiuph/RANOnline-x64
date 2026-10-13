#include "pch.h"

#include "YXModelLoader.h"
#include "YXXFileReader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstring>

#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif

namespace yx
{
	namespace
	{
		//	Assimp names the synthetic root it adds when a file has several top-level frames
		//	("$dummy_root") or meshes outside any frame ("$dummy_node").
		bool IsSyntheticRoot ( const aiNode* pNode )
		{
			return std::strcmp ( pNode->mName.C_Str(), "$dummy_root" ) == 0 ||
				   std::strcmp ( pNode->mName.C_Str(), "$dummy_node" ) == 0;
		}

		//	aiMatrix4x4 is row-major with column vectors; D3DXMATRIX is row-major with row vectors.
		void ToD3DXLayout ( const aiMatrix4x4& matIn, float (&matOut)[16] )
		{
			for ( unsigned int r = 0; r < 4; ++r )
			{
				for ( unsigned int c = 0; c < 4; ++c )
				{
					matOut[r*4 + c] = matIn[c][r];
				}
			}
		}

		bool HasExtension ( const char* szPath, const char* szExt )
		{
			const char* szDot = std::strrchr ( szPath, '.' );
			return szDot && _stricmp ( szDot, szExt ) == 0;
		}

		//	Same rules as DirectX : the last FrameTransformMatrix child sets the frame's matrix,
		//	nested Frame objects are children, references and every other object are ignored.
		void CopyFrame ( const SXObject& sObj, SModelFrame& sFrameOut )
		{
			sFrameOut.strName = sObj.strName;

			for ( size_t i=0; i<sObj.vecChildren.size(); ++i )
			{
				const SXObject& sChild = sObj.vecChildren[i];
				if ( sChild.bReference )	continue;

				if ( sChild.strType == "FrameTransformMatrix" && sChild.vecFloats.size() >= 16 )
				{
					std::memcpy ( sFrameOut.matLocal, sChild.vecFloats.data(), sizeof(sFrameOut.matLocal) );
				}
				else if ( sChild.strType == "Frame" )
				{
					sFrameOut.vecChildren.push_back ( SModelFrame() );
					CopyFrame ( sChild, sFrameOut.vecChildren.back() );
				}
			}
		}

		bool LoadXFrameHierarchy ( const char* szPath, SModelFrame& sRootOut, std::string& strErrorOut )
		{
			std::vector<SXObject> vecObjects;
			if ( !ReadXFile ( szPath, vecObjects, strErrorOut ) )	return false;

			for ( size_t i=0; i<vecObjects.size(); ++i )
			{
				if ( vecObjects[i].strType != "Frame" )	continue;

				sRootOut.vecChildren.push_back ( SModelFrame() );
				CopyFrame ( vecObjects[i], sRootOut.vecChildren.back() );
			}
			return true;
		}

		void CopyFrame ( const aiNode* pNode, SModelFrame& sFrameOut )
		{
			sFrameOut.strName.assign ( pNode->mName.C_Str(), pNode->mName.length );
			ToD3DXLayout ( pNode->mTransformation, sFrameOut.matLocal );

			sFrameOut.vecChildren.resize ( pNode->mNumChildren );
			for ( unsigned int i = 0; i < pNode->mNumChildren; ++i )
			{
				CopyFrame ( pNode->mChildren[i], sFrameOut.vecChildren[i] );
			}
		}
	}

	SModelFrame::SModelFrame ()
	{
		std::memset ( matLocal, 0, sizeof(matLocal) );
		matLocal[0] = matLocal[5] = matLocal[10] = matLocal[15] = 1.f;
	}

	bool LoadFrameHierarchy ( const char* szPath, SModelFrame& sRootOut, std::string& strErrorOut )
	{
		sRootOut = SModelFrame();
		strErrorOut.clear();

		if ( !szPath || !szPath[0] )
		{
			strErrorOut = "empty path";
			return false;
		}

		//	Assimp's .x importer merges an unnamed mesh frame into its parent and multiplies the
		//	parent's matrix by the child's, so .x files are read by YXCore's own reader instead.
		if ( HasExtension ( szPath, ".x" ) )
			return LoadXFrameHierarchy ( szPath, sRootOut, strErrorOut );

		//	Assimp converts every format to right-handed space; convert back to Direct3D's left-handed space.
		Assimp::Importer Importer;
		const aiScene* pScene = Importer.ReadFile ( szPath, aiProcess_MakeLeftHanded );
		if ( !pScene || !pScene->mRootNode )
		{
			strErrorOut = Importer.GetErrorString();
			return false;
		}

		const aiNode* pRoot = pScene->mRootNode;
		if ( IsSyntheticRoot ( pRoot ) )
		{
			sRootOut.vecChildren.resize ( pRoot->mNumChildren );
			for ( unsigned int i = 0; i < pRoot->mNumChildren; ++i )
			{
				CopyFrame ( pRoot->mChildren[i], sRootOut.vecChildren[i] );
			}
		}
		else
		{
			sRootOut.vecChildren.resize ( 1 );
			CopyFrame ( pRoot, sRootOut.vecChildren[0] );
		}

		return true;
	}
}
