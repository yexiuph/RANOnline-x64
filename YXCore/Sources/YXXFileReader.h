#pragma once

#include <string>
#include <vector>

//
//	Minimal DirectX .x reader (text and binary, 32 or 64-bit floats; compressed files are rejected).
//
//	It parses data objects generically so unknown objects are skipped correctly, and keeps the
//	numeric data of each object in file order. Assimp's .x importer can't be used for frame
//	hierarchies: it folds unnamed mesh frames into their parent and changes the parent's matrix.
//

namespace yx
{
	struct SXObject
	{
		std::string				strType;		// Template name, e.g. "Frame".
		std::string				strName;		// Empty when the object has no name.
		bool					bReference;		// "{ Name }" reference; only strName is set.
		std::vector<float>		vecFloats;		// Float data in file order.
		std::vector<SXObject>	vecChildren;	// Nested objects and references in file order.

		SXObject () : bReference(false) {}
	};

	//	Returns the top-level data objects of the file (templates and the header are skipped).
	bool ReadXFile ( const char* szPath, std::vector<SXObject>& vecObjectsOut, std::string& strErrorOut );
}
