#pragma once

#include <d3dx9.h>
#include <string>

//
//	Assimp-backed replacement for D3DXLoadMeshHierarchyFromX.
//
//	Frames and mesh containers are created through the caller's ID3DXAllocateHierarchy, exactly as
//	D3DX does, so engine code that consumes D3DX hierarchies (skinning, materials, drawing) keeps
//	working. Any format Assimp reads (FBX, glTF, OBJ, DAE, 3DS, .x, ...) can be loaded.
//
//	Each frame that holds meshes gets one mesh container: Assimp's per-material parts are merged
//	back into a single ID3DXMesh whose attribute ids index the container's materials. Bones become
//	an ID3DXSkinInfo. Texture names are passed without their directory, like .x files store them.
//
//	For .x files prefer D3DX when existing data depends on vertex order (e.g. trace points saved in
//	.cps pieces): Assimp rebuilds vertices from faces, so the order differs from the file.
//

namespace yx
{
	bool IsXFile ( const char* szPath );

	HRESULT LoadMeshHierarchy ( const char* szPath, DWORD dwMeshOptions, LPDIRECT3DDEVICE9 pDevice,
								LPD3DXALLOCATEHIERARCHY pAlloc, LPD3DXFRAME* ppFrameRoot, std::string& strErrorOut );
}
