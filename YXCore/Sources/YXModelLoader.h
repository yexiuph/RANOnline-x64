#pragma once

#include <string>
#include <vector>

//
//	YXCore model loading. .x files are read by YXCore's own reader so the result matches
//	DirectX exactly; every other format (FBX, glTF, OBJ, DAE, ...) goes through Assimp.
//
//	Nothing here exposes Assimp or Direct3D types, so the engine only needs this header.
//	Matrices use the D3DXMATRIX memory layout (row-major, row vectors, translation in
//	elements 12..14) and can be copied straight into a D3DXMATRIX.
//

namespace yx
{
	struct SModelFrame
	{
		std::string					strName;		// Empty for unnamed frames; they are kept so callers can decide.
		float						matLocal[16];	// Transform relative to the parent frame.
		std::vector<SModelFrame>	vecChildren;	// In file order.

		SModelFrame ();
	};

	//	Reads the frame hierarchy of a model file (meshes and animations are ignored).
	//	sRootOut is an unnamed identity frame whose children are the file's top-level frames.
	//	Matrices are left-handed like Direct3D; for .x files they are exactly the values in the file.
	bool LoadFrameHierarchy ( const char* szPath, SModelFrame& sRootOut, std::string& strErrorOut );
}
