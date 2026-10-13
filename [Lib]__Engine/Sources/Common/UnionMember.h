//
//	UnionMember.h
//
//	Many engine classes keep their property block in an anonymous union, next to an anonymous
//	struct that aliases its members:
//
//		union
//		{
//			struct { EFFCHAR_PROPERTY_BLUR m_Property; };
//			struct { DWORD m_dwFlag; int m_nBlend; float m_fFullLifeTemp; ... };
//		};
//
//	Visual C++ 2005, which this code was written for, ran the constructor of m_Property.  Since
//	Visual Studio 2015, MSVC neither constructs nor destroys members of an anonymous struct inside
//	a union (warnings C4587 / C4588, off by default).  The defaults set by the property constructor
//	are lost, and every field a loader does not overwrite keeps raw heap bytes -- e.g. the life time
//	of a weapon blur loaded from an older effect file, which then never fades out.
//
//	Owners call ConstructUnionMember ( m_Property ) as the first statement of their constructor,
//	which restores the old order: property defaults first, then the rest of the constructor body.
//
#pragma once

#include <new>

#pragma push_macro("new")
#undef new		// DEBUG_NEW would turn the placement new below into a debug allocation.

template < class T >
inline void ConstructUnionMember ( T& member )
{
	::new ( static_cast<void*> ( &member ) ) T;
}

//	Only needed where the member owns resources (std::string).
template < class T >
inline void DestroyUnionMember ( T& member )
{
	member.~T();
}

#pragma pop_macro("new")
