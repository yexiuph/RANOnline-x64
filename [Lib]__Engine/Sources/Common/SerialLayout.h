//
//	SerialLayout.h
//
//	Asset files store several engine structures as raw memory images
//	( SFile.WriteBuffer ( &s, sizeof(s) ) ).  Those images were written by the x86 build, so the
//	x86 MSVC Release memory layout *is* the file format.  Structures whose members change size on
//	x64 -- raw pointers (4 -> 8 bytes) and std::string (24 -> 32 bytes) -- are therefore read and
//	written through an "on-disk mirror" that reproduces the x86 layout on every platform:
//
//		pointer member		->	SerialLayout::PTR32			( 4 bytes)
//		std::string member	->	SerialLayout::STRING_X86	(24 bytes)
//
//	Every mirror comes with SerialFromDisk() / SerialToDisk() overloads; ReadArray / WriteArray
//	stream an array of in-memory structures through its mirror.  Block-size headers that used to be
//	computed as sizeof(STRUCT)*n use sizeof(MIRROR)*n.
//
//	Next to each mirror, static_asserts check its size against the x86 size on every platform and,
//	where the original structure itself has the x86 layout (SERIAL_LAYOUT_NATIVE_X86[_STRING]),
//	check it against the original member by member -- compile-time proof that the Win32 build
//	reads and writes the same bytes as before.
//
//	Header-only, needs no Windows headers.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

//	1 where pointers are 4 bytes, i.e. where a pointer-only mirror must match its original exactly.
#if !defined(_WIN64)
	#define SERIAL_LAYOUT_NATIVE_X86			1
#else
	#define SERIAL_LAYOUT_NATIVE_X86			0
#endif

//	1 where std::string is also the 24-byte x86 Release image (_ITERATOR_DEBUG_LEVEL 0).
//	A Win32 Debug build (_ITERATOR_DEBUG_LEVEL 2, 28-byte std::string) reads and writes the
//	Release format through the mirrors as well.
#if SERIAL_LAYOUT_NATIVE_X86 && defined(_ITERATOR_DEBUG_LEVEL) && (_ITERATOR_DEBUG_LEVEL == 0)
	#define SERIAL_LAYOUT_NATIVE_X86_STRING		1
#else
	#define SERIAL_LAYOUT_NATIVE_X86_STRING		0
#endif

//	Compile-time proof that MIRROR has the layout of ORIGINAL (use under the macros above).
#define SERIAL_LAYOUT_ASSERT_SIZE( MIRROR, ORIGINAL )											\
	static_assert ( sizeof(MIRROR) == sizeof(ORIGINAL),											\
		#MIRROR " must have the size of " #ORIGINAL )

#define SERIAL_LAYOUT_ASSERT_MEMBER( MIRROR, ORIGINAL, MEMBER )									\
	static_assert ( offsetof(MIRROR, MEMBER) == offsetof(ORIGINAL, MEMBER) &&					\
					sizeof(((MIRROR*)0)->MEMBER) == sizeof(((ORIGINAL*)0)->MEMBER),				\
		#MIRROR "::" #MEMBER " must have the offset and size of " #ORIGINAL "::" #MEMBER )

namespace SerialLayout
{
	//	4-byte slot that held a pointer in the x86 image.
	typedef unsigned long	PTR32;		// same type as DWORD

	//	The value of a pointer slot means nothing to a reader (it is an address in the process that
	//	wrote the file), so every SerialFromDisk() yields NULL for it.  Writers store the pointer
	//	truncated to 32 bits only so that the Win32 build keeps writing the bytes it always wrote.
	inline PTR32 PtrToDisk ( const void* p )
	{
		return static_cast<PTR32> ( reinterpret_cast<std::uintptr_t> ( p ) );
	}

	//	Image of std::basic_string<char> in the MSVC (v143) x86 build with _ITERATOR_DEBUG_LEVEL 0:
	//		union _Bxty { char _Buf[16]; char* _Ptr; } _Bx;		// +0
	//		size_t _Mysize;										// +16	length
	//		size_t _Myres;										// +20	capacity
	//	_Myres <= 15 : small-string mode, the text and its terminating NUL are stored inline in _Buf.
	//	_Myres >  15 : large mode, _Buf starts with a pointer to a heap block of the writing process.
	struct STRING_X86
	{
		char			buf[16];
		unsigned long	size;
		unsigned long	capacity;
	};

	static_assert ( sizeof(STRING_X86) == 24, "x86 MSVC Release std::string image is 24 bytes" );
#if SERIAL_LAYOUT_NATIVE_X86_STRING
	static_assert ( sizeof(std::string) == sizeof(STRING_X86), "STRING_X86 must match this toolset's std::string" );
#endif

	const unsigned long STRING_X86_SMALL_CAPACITY = 15;

	//	A small-string image gives back its text (size bytes).
	//
	//	A large-mode image (capacity > 15) carries no text at all, only a pointer into the heap of
	//	the process that wrote the file.  The legacy x86 code copied that image straight into a live
	//	std::string, leaving it pointing at an address that is foreign to the reader -- undefined
	//	behaviour: c_str() reads arbitrary memory, and assigning to or destroying the string writes
	//	to / frees that address.  The text was never stored, so it cannot be recovered: a large image
	//	(and a malformed small one whose length exceeds 15) decodes to an empty string and the stored
	//	pointer is never dereferenced.  An empty name makes the texture load / lookup that uses it fail
	//	softly instead of crashing.
	inline std::string DecodeString ( const STRING_X86& d )
	{
		if ( d.capacity <= STRING_X86_SMALL_CAPACITY && d.size <= STRING_X86_SMALL_CAPACITY )
			return std::string ( d.buf, d.size );

		return std::string ();
	}

	//	Writes the small-string image of s (text, NUL, zero fill, capacity 15): what the x86 build
	//	wrote for any string of up to 15 characters.  A longer string cannot be represented -- the
	//	legacy x86 writer stored its heap pointer, which no reader can follow -- so it is written as
	//	an empty string (the value DecodeString() yields for such a legacy record) rather than as a
	//	pointer that would crash older readers.
	inline void EncodeString ( STRING_X86& d, const std::string& s )
	{
		memset ( &d, 0, sizeof(d) );
		d.capacity = STRING_X86_SMALL_CAPACITY;

		const size_t nLen = s.size();
		if ( nLen <= STRING_X86_SMALL_CAPACITY )
		{
			memcpy ( d.buf, s.data(), nLen );
			d.size = static_cast<unsigned long> ( nLen );
		}
	}

	//	Reads dwNum records of the on-disk type DISK and converts them with SerialFromDisk().
	template < class DISK, class STREAM, class T >
	inline void ReadArray ( STREAM& SFile, T* pDst, unsigned long dwNum )
	{
		if ( !dwNum )	return;

		std::vector<DISK> vecDisk ( dwNum );
		SFile.ReadBuffer ( &vecDisk[0], static_cast<unsigned long> ( sizeof(DISK) * dwNum ) );

		for ( unsigned long i=0; i<dwNum; ++i )
			SerialFromDisk ( pDst[i], vecDisk[i] );
	}

	//	Converts dwNum in-memory records with SerialToDisk() and writes them as one block.
	template < class DISK, class STREAM, class T >
	inline void WriteArray ( STREAM& SFile, const T* pSrc, unsigned long dwNum )
	{
		if ( !dwNum )	return;

		std::vector<DISK> vecDisk ( dwNum );
		for ( unsigned long i=0; i<dwNum; ++i )
			SerialToDisk ( vecDisk[i], pSrc[i] );

		SFile.WriteBuffer ( &vecDisk[0], static_cast<unsigned long> ( sizeof(DISK) * dwNum ) );
	}
}
