//
//	[struct DXMATERIAL], JDH, (2002.12.02)
//		메터리얼과 텍스쳐, 메터리얼(텍스쳐) 속성값을 가지고 있는 구조체.
//		이들 데이터는 같이 있을 경우가 많으므로 이렇게 처리함.
//
#pragma once
#include <string>
#include "SerialLayout.h"

struct DXMATERIAL
{
	enum
	{
		FLAG_ALPHA_HARD		= 0x0001,
		FLAG_ALPHA_SOFT		= 0x0002,
	};

	DWORD				dwFlags;
	LPDIRECT3DTEXTUREQ	pTexture;
	std::string			strTexture;

	DXMATERIAL () :
		dwFlags(NULL),
		pTexture(NULL)
	{
		strTexture = "";
	}

	void RELEASE ();
};

struct DXMATERIAL_SPECULAR
{
	BOOL				bSpecUse;
	LPDIRECT3DTEXTUREQ	pSpecTex;
	char				szTexture[MAX_PATH];
	char				szSpecTex[MAX_PATH];

	DXMATERIAL_SPECULAR () :
		bSpecUse(FALSE),
		pSpecTex(NULL)
	{
		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
		memset(szSpecTex, 0, sizeof(char) * (MAX_PATH));
	}

	~DXMATERIAL_SPECULAR();
};

struct DXMATERIAL_CHAR_EFF_100
{
	BOOL				bEffUse;
	float				fMaterial_Power;
	LPDIRECT3DTEXTUREQ	pEffTex;
	char				szTexture[MAX_PATH];
	char				szEffTex[MAX_PATH];

	DXMATERIAL_CHAR_EFF_100()
		: pEffTex(NULL)
	{
		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
		memset(szEffTex, 0, sizeof(char) * (MAX_PATH));
	}
};

struct DXMATERIAL_CHAR_EFF
{
	BOOL				bEffUse;
	D3DMATERIALQ		d3dMaterial;
	LPDIRECT3DTEXTUREQ	pEffTex;
	char				szTexture[MAX_PATH];
	char				szEffTex[MAX_PATH];

	DXMATERIAL_CHAR_EFF() :
		bEffUse(FALSE),
		pEffTex(NULL)
	{
		d3dMaterial.Diffuse.r	= 1.f;
		d3dMaterial.Diffuse.g	= 1.f;
		d3dMaterial.Diffuse.b	= 1.f;
		d3dMaterial.Diffuse.a	= 1.f;
		d3dMaterial.Ambient		= d3dMaterial.Diffuse;
		d3dMaterial.Specular	= d3dMaterial.Diffuse;
		d3dMaterial.Emissive	= d3dMaterial.Diffuse;
		d3dMaterial.Power		= 14.f;

		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
		memset(szEffTex, 0, sizeof(char) * (MAX_PATH));
	}

	~DXMATERIAL_CHAR_EFF();
};

//	----------------------------------------------------------------------------------------------
//	On-disk (x86) images of the material structures above, see SerialLayout.h.
//	The texture pointer is a 4-byte slot that always loads as NULL; the load paths (re)load the
//	texture by name afterwards.  DXMATERIAL_SPECULAR and DXMATERIAL_CHAR_EFF_100 are only read
//	(old file versions); converting them leaves pSpecTex / pEffTex NULL, so the temporaries no
//	longer carry (and, for DXMATERIAL_SPECULAR, "release" in the destructor) a stale pointer.
//	----------------------------------------------------------------------------------------------
struct DXMATERIAL_SPECULAR_DISK
{
	BOOL					bSpecUse;
	SerialLayout::PTR32		pSpecTex;
	char					szTexture[MAX_PATH];
	char					szSpecTex[MAX_PATH];
};

struct DXMATERIAL_CHAR_EFF_100_DISK
{
	BOOL					bEffUse;
	float					fMaterial_Power;
	SerialLayout::PTR32		pEffTex;
	char					szTexture[MAX_PATH];
	char					szEffTex[MAX_PATH];
};

struct DXMATERIAL_CHAR_EFF_DISK
{
	BOOL					bEffUse;
	D3DMATERIALQ			d3dMaterial;
	SerialLayout::PTR32		pEffTex;
	char					szTexture[MAX_PATH];
	char					szEffTex[MAX_PATH];
};

static_assert ( sizeof(DXMATERIAL_SPECULAR_DISK) == 528, "x86 size of DXMATERIAL_SPECULAR" );
static_assert ( sizeof(DXMATERIAL_CHAR_EFF_100_DISK) == 532, "x86 size of DXMATERIAL_CHAR_EFF_100" );
static_assert ( sizeof(DXMATERIAL_CHAR_EFF_DISK) == 596, "x86 size of DXMATERIAL_CHAR_EFF" );

#if SERIAL_LAYOUT_NATIVE_X86
SERIAL_LAYOUT_ASSERT_SIZE ( DXMATERIAL_SPECULAR_DISK, DXMATERIAL_SPECULAR );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_SPECULAR_DISK, DXMATERIAL_SPECULAR, bSpecUse );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_SPECULAR_DISK, DXMATERIAL_SPECULAR, pSpecTex );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_SPECULAR_DISK, DXMATERIAL_SPECULAR, szTexture );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_SPECULAR_DISK, DXMATERIAL_SPECULAR, szSpecTex );
SERIAL_LAYOUT_ASSERT_SIZE ( DXMATERIAL_CHAR_EFF_100_DISK, DXMATERIAL_CHAR_EFF_100 );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_100_DISK, DXMATERIAL_CHAR_EFF_100, bEffUse );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_100_DISK, DXMATERIAL_CHAR_EFF_100, fMaterial_Power );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_100_DISK, DXMATERIAL_CHAR_EFF_100, pEffTex );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_100_DISK, DXMATERIAL_CHAR_EFF_100, szTexture );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_100_DISK, DXMATERIAL_CHAR_EFF_100, szEffTex );
SERIAL_LAYOUT_ASSERT_SIZE ( DXMATERIAL_CHAR_EFF_DISK, DXMATERIAL_CHAR_EFF );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_DISK, DXMATERIAL_CHAR_EFF, bEffUse );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_DISK, DXMATERIAL_CHAR_EFF, d3dMaterial );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_DISK, DXMATERIAL_CHAR_EFF, pEffTex );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_DISK, DXMATERIAL_CHAR_EFF, szTexture );
SERIAL_LAYOUT_ASSERT_MEMBER ( DXMATERIAL_CHAR_EFF_DISK, DXMATERIAL_CHAR_EFF, szEffTex );
#endif

inline void SerialFromDisk ( DXMATERIAL_SPECULAR& d, const DXMATERIAL_SPECULAR_DISK& s )
{
	d.bSpecUse	= s.bSpecUse;
	d.pSpecTex	= NULL;
	memcpy ( d.szTexture, s.szTexture, sizeof(d.szTexture) );
	memcpy ( d.szSpecTex, s.szSpecTex, sizeof(d.szSpecTex) );
}

inline void SerialFromDisk ( DXMATERIAL_CHAR_EFF_100& d, const DXMATERIAL_CHAR_EFF_100_DISK& s )
{
	d.bEffUse			= s.bEffUse;
	d.fMaterial_Power	= s.fMaterial_Power;
	d.pEffTex			= NULL;
	memcpy ( d.szTexture, s.szTexture, sizeof(d.szTexture) );
	memcpy ( d.szEffTex, s.szEffTex, sizeof(d.szEffTex) );
}

inline void SerialFromDisk ( DXMATERIAL_CHAR_EFF& d, const DXMATERIAL_CHAR_EFF_DISK& s )
{
	d.bEffUse		= s.bEffUse;
	d.d3dMaterial	= s.d3dMaterial;
	d.pEffTex		= NULL;
	memcpy ( d.szTexture, s.szTexture, sizeof(d.szTexture) );
	memcpy ( d.szEffTex, s.szEffTex, sizeof(d.szEffTex) );
}

inline void SerialToDisk ( DXMATERIAL_CHAR_EFF_DISK& d, const DXMATERIAL_CHAR_EFF& s )
{
	d.bEffUse		= s.bEffUse;
	d.d3dMaterial	= s.d3dMaterial;
	d.pEffTex		= SerialLayout::PtrToDisk ( s.pEffTex );
	memcpy ( d.szTexture, s.szTexture, sizeof(d.szTexture) );
	memcpy ( d.szEffTex, s.szEffTex, sizeof(d.szEffTex) );
}

//
//struct DXMATERIAL_CHAR_EFF
//{
//	BOOL				bEffUse;
//	D3DMATERIALQ		d3dMaterial;
//	DWORD				dwDiffuse;		// Optimize - 최적화 때문에 쓰인다.
//	LPDIRECT3DTEXTUREQ	pEffTex;
//	char				szTexture[MAX_PATH];
//	char				szEffTex[MAX_PATH];
//
//	DXMATERIAL_CHAR_EFF () :
//		bEffUse(FALSE),
//		pEffTex(NULL)
//	{
//		d3dMaterial.Diffuse.r	= 1.f;
//		d3dMaterial.Diffuse.g	= 1.f;
//		d3dMaterial.Diffuse.b	= 1.f;
//		d3dMaterial.Diffuse.a	= 1.f;
//		d3dMaterial.Ambient		= d3dMaterial.Diffuse;
//		d3dMaterial.Specular	= d3dMaterial.Diffuse;
//		d3dMaterial.Emissive	= d3dMaterial.Diffuse;
//		d3dMaterial.Power		= 14.f;
//
//		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
//		memset(szEffTex, 0, sizeof(char) * (MAX_PATH));
//	}
//
//	~DXMATERIAL_CHAR_EFF();
//};

