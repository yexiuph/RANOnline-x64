#include "pch.h"

#include "YXMeshHierarchy.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstring>
#include <map>
#include <vector>

namespace yx
{
	namespace
	{
		const DWORD MESH_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

		struct SVertex
		{
			D3DXVECTOR3	vPos;
			D3DXVECTOR3	vNor;
			D3DXVECTOR2	vTex;
		};

		//	aiMatrix4x4 is row-major with column vectors; D3DXMATRIX is row-major with row vectors.
		D3DXMATRIX ToD3DX ( const aiMatrix4x4& m )
		{
			return D3DXMATRIX ( m.a1, m.b1, m.c1, m.d1,
								m.a2, m.b2, m.c2, m.d2,
								m.a3, m.b3, m.c3, m.d3,
								m.a4, m.b4, m.c4, m.d4 );
		}

		D3DCOLORVALUE GetColor ( const aiMaterial* pMat, const char* szKey, unsigned int nType, unsigned int nIndex, float fAlpha )
		{
			aiColor4D c ( 0.f, 0.f, 0.f, fAlpha );
			aiGetMaterialColor ( pMat, szKey, nType, nIndex, &c );
			D3DCOLORVALUE v = { c.r, c.g, c.b, fAlpha };
			return v;
		}

		//	Texture name without its directory, as .x files store it.
		std::string GetTextureName ( const aiMaterial* pMat )
		{
			aiString strPath;
			if ( pMat->GetTexture ( aiTextureType_DIFFUSE, 0, &strPath ) != AI_SUCCESS )	return std::string();
			if ( strPath.length == 0 || strPath.data[0] == '*' )	return std::string();	// embedded textures aren't supported

			std::string str ( strPath.C_Str() );
			const size_t nSlash = str.find_last_of ( "/\\" );
			return nSlash == std::string::npos ? str : str.substr ( nSlash + 1 );
		}

		class CBuilder
		{
		public:
			CBuilder ( const aiScene* pScene, DWORD dwOptions, LPDIRECT3DDEVICE9 pDevice, LPD3DXALLOCATEHIERARCHY pAlloc ) :
				m_pScene(pScene), m_dwOptions(dwOptions), m_pDevice(pDevice), m_pAlloc(pAlloc)
			{
			}

			HRESULT BuildFrame ( const aiNode* pNode, LPD3DXFRAME* ppFrameOut )
			{
				*ppFrameOut = NULL;

				LPD3DXFRAME pFrame = NULL;
				HRESULT hr = m_pAlloc->CreateFrame ( pNode->mName.length ? pNode->mName.C_Str() : NULL, &pFrame );
				if ( FAILED(hr) )	return hr;
				*ppFrameOut = pFrame;

				pFrame->TransformationMatrix = ToD3DX ( pNode->mTransformation );

				if ( pNode->mNumMeshes )
				{
					hr = BuildMeshContainer ( pNode, &pFrame->pMeshContainer );
					if ( FAILED(hr) )	return hr;
				}

				return BuildChildren ( pNode, &pFrame->pFrameFirstChild );
			}

			//	Children are chained through pFrameSibling in file order.
			HRESULT BuildChildren ( const aiNode* pNode, LPD3DXFRAME* ppFirstOut )
			{
				LPD3DXFRAME* ppLink = ppFirstOut;
				for ( unsigned int i=0; i<pNode->mNumChildren; ++i )
				{
					HRESULT hr = BuildFrame ( pNode->mChildren[i], ppLink );
					if ( FAILED(hr) )	return hr;
					ppLink = &(*ppLink)->pFrameSibling;
				}
				return S_OK;
			}

		private:
			HRESULT BuildMeshContainer ( const aiNode* pNode, LPD3DXMESHCONTAINER* ppContainerOut )
			{
				//	Gather the triangle parts of this node; each part is one material subset.
				std::vector<const aiMesh*> vecParts;
				DWORD dwVertices = 0, dwFaces = 0;
				for ( unsigned int i=0; i<pNode->mNumMeshes; ++i )
				{
					const aiMesh* pPart = m_pScene->mMeshes[ pNode->mMeshes[i] ];
					if ( !(pPart->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) )	continue;
					vecParts.push_back ( pPart );
					dwVertices += pPart->mNumVertices;
					dwFaces += pPart->mNumFaces;
				}
				if ( vecParts.empty() || dwFaces == 0 )	return S_OK;

				const bool b32Bit = dwVertices > 0xFFFF;
				LPD3DXMESH pMesh = NULL;
				HRESULT hr = D3DXCreateMeshFVF ( dwFaces, dwVertices, m_dwOptions | (b32Bit ? D3DXMESH_32BIT : 0), MESH_FVF, m_pDevice, &pMesh );
				if ( FAILED(hr) )	return hr;

				SVertex* pVertices = NULL;
				void* pIndices = NULL;
				DWORD* pAttributes = NULL;
				pMesh->LockVertexBuffer ( 0, (void**)&pVertices );
				pMesh->LockIndexBuffer ( 0, &pIndices );
				pMesh->LockAttributeBuffer ( 0, &pAttributes );

				std::vector<DWORD> vecPartBase ( vecParts.size() );
				DWORD dwVertex = 0, dwFace = 0;
				for ( size_t p=0; p<vecParts.size(); ++p )
				{
					const aiMesh* pPart = vecParts[p];
					vecPartBase[p] = dwVertex;

					for ( unsigned int v=0; v<pPart->mNumVertices; ++v, ++dwVertex )
					{
						SVertex& sVtx = pVertices[dwVertex];
						sVtx.vPos = D3DXVECTOR3 ( pPart->mVertices[v].x, pPart->mVertices[v].y, pPart->mVertices[v].z );
						sVtx.vNor = pPart->HasNormals() ? D3DXVECTOR3 ( pPart->mNormals[v].x, pPart->mNormals[v].y, pPart->mNormals[v].z ) : D3DXVECTOR3 ( 0.f, 1.f, 0.f );
						sVtx.vTex = pPart->HasTextureCoords(0) ? D3DXVECTOR2 ( pPart->mTextureCoords[0][v].x, pPart->mTextureCoords[0][v].y ) : D3DXVECTOR2 ( 0.f, 0.f );
					}

					for ( unsigned int f=0; f<pPart->mNumFaces; ++f )
					{
						const aiFace& sFace = pPart->mFaces[f];
						if ( sFace.mNumIndices != 3 )	continue;
						for ( unsigned int k=0; k<3; ++k )
						{
							const DWORD dwIndex = vecPartBase[p] + sFace.mIndices[k];
							if ( b32Bit )	((DWORD*)pIndices)[dwFace*3 + k] = dwIndex;
							else			((WORD*)pIndices)[dwFace*3 + k] = (WORD)dwIndex;
						}
						pAttributes[dwFace++] = (DWORD)p;
					}
				}

				pMesh->UnlockAttributeBuffer ();
				pMesh->UnlockIndexBuffer ();
				pMesh->UnlockVertexBuffer ();

				std::vector<DWORD> vecAdjacency ( dwFaces * 3 );
				pMesh->GenerateAdjacency ( 0.f, &vecAdjacency[0] );

				//	Materials : one per part, in attribute order.
				std::vector<std::string> vecTextures ( vecParts.size() );
				std::vector<D3DXMATERIAL> vecMaterials ( vecParts.size() );
				for ( size_t p=0; p<vecParts.size(); ++p )
				{
					const aiMaterial* pMat = m_pScene->mMaterials[ vecParts[p]->mMaterialIndex ];
					float fOpacity = 1.f;
					aiGetMaterialFloat ( pMat, AI_MATKEY_OPACITY, &fOpacity );
					float fPower = 0.f;
					aiGetMaterialFloat ( pMat, AI_MATKEY_SHININESS, &fPower );

					D3DMATERIAL9& sMat = vecMaterials[p].MatD3D;
					sMat.Diffuse	= GetColor ( pMat, AI_MATKEY_COLOR_DIFFUSE, fOpacity );
					sMat.Ambient	= GetColor ( pMat, AI_MATKEY_COLOR_AMBIENT, 1.f );
					sMat.Specular	= GetColor ( pMat, AI_MATKEY_COLOR_SPECULAR, 1.f );
					sMat.Emissive	= GetColor ( pMat, AI_MATKEY_COLOR_EMISSIVE, 1.f );
					sMat.Power		= fPower;

					vecTextures[p] = GetTextureName ( pMat );
					vecMaterials[p].pTextureFilename = vecTextures[p].empty() ? NULL : &vecTextures[p][0];
				}

				//	Skinning : bones are merged across parts by name, in order of first use.
				LPD3DXSKININFO pSkinInfo = NULL;
				std::map<std::string, DWORD> mapBone;
				std::vector<const aiBone*> vecBoneFirst;
				std::vector< std::vector<DWORD> > vecBoneVertices;
				std::vector< std::vector<float> > vecBoneWeights;
				for ( size_t p=0; p<vecParts.size(); ++p )
				{
					const aiMesh* pPart = vecParts[p];
					for ( unsigned int b=0; b<pPart->mNumBones; ++b )
					{
						const aiBone* pBone = pPart->mBones[b];
						const std::string strName ( pBone->mName.C_Str() );
						std::map<std::string, DWORD>::iterator it = mapBone.find ( strName );
						DWORD dwBone;
						if ( it == mapBone.end() )
						{
							dwBone = (DWORD)vecBoneFirst.size();
							mapBone[strName] = dwBone;
							vecBoneFirst.push_back ( pBone );
							vecBoneVertices.push_back ( std::vector<DWORD>() );
							vecBoneWeights.push_back ( std::vector<float>() );
						}
						else
						{
							dwBone = it->second;
						}

						for ( unsigned int w=0; w<pBone->mNumWeights; ++w )
						{
							vecBoneVertices[dwBone].push_back ( vecPartBase[p] + pBone->mWeights[w].mVertexId );
							vecBoneWeights[dwBone].push_back ( pBone->mWeights[w].mWeight );
						}
					}
				}

				if ( !vecBoneFirst.empty() )
				{
					hr = D3DXCreateSkinInfoFVF ( dwVertices, MESH_FVF, (DWORD)vecBoneFirst.size(), &pSkinInfo );
					if ( FAILED(hr) )
					{
						pMesh->Release();
						return hr;
					}

					for ( DWORD b=0; b<(DWORD)vecBoneFirst.size(); ++b )
					{
						pSkinInfo->SetBoneName ( b, vecBoneFirst[b]->mName.C_Str() );
						const D3DXMATRIX matOffset = ToD3DX ( vecBoneFirst[b]->mOffsetMatrix );
						pSkinInfo->SetBoneOffsetMatrix ( b, &matOffset );
						if ( !vecBoneVertices[b].empty() )
							pSkinInfo->SetBoneInfluence ( b, (DWORD)vecBoneVertices[b].size(), &vecBoneVertices[b][0], &vecBoneWeights[b][0] );
					}
				}

				D3DXMESHDATA sMeshData;
				sMeshData.Type = D3DXMESHTYPE_MESH;
				sMeshData.pMesh = pMesh;

				const char* szMeshName = vecParts[0]->mName.length ? vecParts[0]->mName.C_Str() : NULL;
				hr = m_pAlloc->CreateMeshContainer ( szMeshName, &sMeshData, &vecMaterials[0], NULL, (DWORD)vecMaterials.size(),
													 &vecAdjacency[0], pSkinInfo, ppContainerOut );

				//	The container keeps its own references.
				if ( pSkinInfo )	pSkinInfo->Release();
				pMesh->Release();
				return hr;
			}

		private:
			const aiScene*			m_pScene;
			DWORD					m_dwOptions;
			LPDIRECT3DDEVICE9		m_pDevice;
			LPD3DXALLOCATEHIERARCHY	m_pAlloc;
		};

		bool IsSyntheticRoot ( const aiNode* pNode )
		{
			return std::strcmp ( pNode->mName.C_Str(), "$dummy_root" ) == 0;
		}
	}

	bool IsXFile ( const char* szPath )
	{
		const char* szDot = szPath ? std::strrchr ( szPath, '.' ) : NULL;
		return szDot && _stricmp ( szDot, ".x" ) == 0;
	}

	HRESULT LoadMeshHierarchy ( const char* szPath, DWORD dwMeshOptions, LPDIRECT3DDEVICE9 pDevice,
								LPD3DXALLOCATEHIERARCHY pAlloc, LPD3DXFRAME* ppFrameRoot, std::string& strErrorOut )
	{
		strErrorOut.clear();
		if ( !szPath || !pDevice || !pAlloc || !ppFrameRoot )	return E_INVALIDARG;
		*ppFrameRoot = NULL;

		//	ConvertToLeftHanded = left-handed coordinates, top-left texture origin, clockwise front faces.
		Assimp::Importer Importer;
		Importer.SetPropertyInteger ( AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE );
		const aiScene* pScene = Importer.ReadFile ( szPath,
			aiProcess_ConvertToLeftHanded |
			aiProcess_Triangulate |
			aiProcess_SortByPType |
			aiProcess_JoinIdenticalVertices |
			aiProcess_GenSmoothNormals |
			aiProcess_LimitBoneWeights );
		if ( !pScene || !pScene->mRootNode )
		{
			strErrorOut = Importer.GetErrorString();
			return E_FAIL;
		}

		CBuilder Builder ( pScene, dwMeshOptions, pDevice, pAlloc );

		//	Several top-level frames are returned as a sibling chain, like D3DX does.
		HRESULT hr = IsSyntheticRoot ( pScene->mRootNode ) ?
			Builder.BuildChildren ( pScene->mRootNode, ppFrameRoot ) :
			Builder.BuildFrame ( pScene->mRootNode, ppFrameRoot );

		if ( FAILED(hr) )
		{
			if ( *ppFrameRoot )
			{
				D3DXFrameDestroy ( *ppFrameRoot, pAlloc );
				*ppFrameRoot = NULL;
			}
			strErrorOut = "building the D3DX hierarchy failed";
		}
		return hr;
	}
}
