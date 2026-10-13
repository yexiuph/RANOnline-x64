#include "pch.h"

#include "YXXFileReader.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace yx
{
	namespace
	{
		//	Binary token ids from the DirectX .x file format reference.
		enum
		{
			BIN_NAME			= 1,
			BIN_STRING			= 2,
			BIN_INTEGER			= 3,
			BIN_GUID			= 5,
			BIN_INTEGER_LIST	= 6,
			BIN_FLOAT_LIST		= 7,
			BIN_OBRACE			= 10,
			BIN_CBRACE			= 11,
			BIN_TEMPLATE		= 31,
		};

		struct SToken
		{
			enum EMTYPE { NAME, STRING, INTEGER, FLOAT, GUID, OBRACE, CBRACE, TEMPLATE, OTHER, END, BAD };

			EMTYPE		emType;
			std::string	strText;	// NAME and STRING
			float		fValue;		// FLOAT

			SToken () : emType(END), fValue(0.f) {}
		};

		class CTokenizer
		{
		public:
			CTokenizer ( const std::vector<char>& vecData, bool bBinary, bool bDouble ) :
				m_pCur(vecData.data() + 16),
				m_pEnd(vecData.data() + vecData.size()),
				m_bBinary(bBinary),
				m_bDouble(bDouble),
				m_dwListLeft(0),
				m_bFloatList(false)
			{
			}

			SToken Next ()
			{
				return m_bBinary ? NextBinary() : NextText();
			}

		private:
			bool Has ( size_t nBytes ) const		{ return (size_t)(m_pEnd - m_pCur) >= nBytes; }

			uint16_t ReadWord ()
			{
				uint16_t w;
				std::memcpy ( &w, m_pCur, 2 );
				m_pCur += 2;
				return w;
			}

			uint32_t ReadDWord ()
			{
				uint32_t dw;
				std::memcpy ( &dw, m_pCur, 4 );
				m_pCur += 4;
				return dw;
			}

			SToken Bad ()
			{
				SToken sTok;
				sTok.emType = SToken::BAD;
				return sTok;
			}

			SToken NextBinary ()
			{
				SToken sTok;

				//	Lists are returned one element at a time.
				if ( m_dwListLeft )
				{
					--m_dwListLeft;
					if ( !m_bFloatList )
					{
						if ( !Has(4) )	return Bad();
						m_pCur += 4;
						sTok.emType = SToken::INTEGER;
						return sTok;
					}

					sTok.emType = SToken::FLOAT;
					if ( m_bDouble )
					{
						if ( !Has(8) )	return Bad();
						double d;
						std::memcpy ( &d, m_pCur, 8 );
						m_pCur += 8;
						sTok.fValue = (float)d;
					}
					else
					{
						if ( !Has(4) )	return Bad();
						std::memcpy ( &sTok.fValue, m_pCur, 4 );
						m_pCur += 4;
					}
					return sTok;
				}

				if ( m_pCur == m_pEnd )	return sTok;	// END
				if ( !Has(2) )			return Bad();

				const uint16_t wToken = ReadWord();
				switch ( wToken )
				{
				case BIN_NAME:
				case BIN_STRING:
					{
						const bool bName = ( wToken == BIN_NAME );
						if ( !Has(4) )	return Bad();
						const uint32_t dwLen = ReadDWord();
						if ( !Has(dwLen) )	return Bad();
						sTok.strText.assign ( m_pCur, dwLen );
						m_pCur += dwLen;
						if ( bName )
						{
							sTok.emType = SToken::NAME;
						}
						else
						{
							if ( !Has(2) )	return Bad();
							m_pCur += 2;	// ';' or ',' terminator
							sTok.emType = SToken::STRING;
						}
						return sTok;
					}

				case BIN_INTEGER:
					if ( !Has(4) )	return Bad();
					m_pCur += 4;
					sTok.emType = SToken::INTEGER;
					return sTok;

				case BIN_GUID:
					if ( !Has(16) )	return Bad();
					m_pCur += 16;
					sTok.emType = SToken::GUID;
					return sTok;

				case BIN_INTEGER_LIST:
				case BIN_FLOAT_LIST:
					m_bFloatList = ( wToken == BIN_FLOAT_LIST );
					if ( !Has(4) )	return Bad();
					m_dwListLeft = ReadDWord();
					sTok.emType = SToken::OTHER;
					return sTok;

				case BIN_OBRACE:	sTok.emType = SToken::OBRACE;	return sTok;
				case BIN_CBRACE:	sTok.emType = SToken::CBRACE;	return sTok;
				case BIN_TEMPLATE:	sTok.emType = SToken::TEMPLATE;	return sTok;

				default:
					//	Punctuation and template member types carry no payload.
					sTok.emType = SToken::OTHER;
					return sTok;
				}
			}

			static bool IsNameStart ( char c )	{ return std::isalpha((unsigned char)c) || c == '_'; }
			static bool IsNameChar ( char c )	{ return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.'; }

			SToken NextText ()
			{
				SToken sTok;

				for (;;)
				{
					while ( m_pCur < m_pEnd && ( std::isspace((unsigned char)*m_pCur) || *m_pCur == ',' || *m_pCur == ';' ) )
						++m_pCur;

					if ( m_pCur == m_pEnd )	return sTok;	// END

					//	Comments run to the end of the line.
					if ( *m_pCur == '#' || ( *m_pCur == '/' && m_pCur+1 < m_pEnd && m_pCur[1] == '/' ) )
					{
						while ( m_pCur < m_pEnd && *m_pCur != '\n' )	++m_pCur;
						continue;
					}
					break;
				}

				const char c = *m_pCur;

				if ( c == '{' )	{ ++m_pCur; sTok.emType = SToken::OBRACE; return sTok; }
				if ( c == '}' )	{ ++m_pCur; sTok.emType = SToken::CBRACE; return sTok; }

				if ( c == '<' )
				{
					while ( m_pCur < m_pEnd && *m_pCur != '>' )	++m_pCur;
					if ( m_pCur == m_pEnd )	return Bad();
					++m_pCur;
					sTok.emType = SToken::GUID;
					return sTok;
				}

				if ( c == '"' )
				{
					const char* pStart = ++m_pCur;
					while ( m_pCur < m_pEnd && *m_pCur != '"' )	++m_pCur;
					if ( m_pCur == m_pEnd )	return Bad();
					sTok.strText.assign ( pStart, m_pCur );
					++m_pCur;
					sTok.emType = SToken::STRING;
					return sTok;
				}

				if ( std::isdigit((unsigned char)c) || c == '-' || c == '+' ||
					 ( c == '.' && m_pCur+1 < m_pEnd && std::isdigit((unsigned char)m_pCur[1]) ) )
				{
					//	Text files don't mark member types, so every number is kept as a float.
					const char* pStart = m_pCur;
					while ( m_pCur < m_pEnd && ( std::isalnum((unsigned char)*m_pCur) || *m_pCur == '.' || *m_pCur == '-' || *m_pCur == '+' ) )
						++m_pCur;
					const std::string strNumber ( pStart, m_pCur );
					char* pParsed = NULL;
					sTok.fValue = (float)std::strtod ( strNumber.c_str(), &pParsed );
					if ( pParsed == strNumber.c_str() )	return Bad();
					sTok.emType = SToken::FLOAT;
					return sTok;
				}

				if ( IsNameStart(c) )
				{
					const char* pStart = m_pCur;
					while ( m_pCur < m_pEnd && IsNameChar(*m_pCur) )	++m_pCur;
					sTok.strText.assign ( pStart, m_pCur );
					sTok.emType = ( sTok.strText == "template" ) ? SToken::TEMPLATE : SToken::NAME;
					return sTok;
				}

				++m_pCur;	// [ ] ( ) . and other template punctuation
				sTok.emType = SToken::OTHER;
				return sTok;
			}

		private:
			const char*	m_pCur;
			const char*	m_pEnd;
			bool		m_bBinary;
			bool		m_bDouble;
			uint32_t	m_dwListLeft;
			bool		m_bFloatList;
		};

		//	Called after an object's type name; reads "[name] [<guid>] { ... }".
		bool ParseObject ( CTokenizer& Tok, SXObject& sObj, std::string& strError )
		{
			SToken sTok = Tok.Next();
			if ( sTok.emType == SToken::NAME )
			{
				sObj.strName = sTok.strText;
				sTok = Tok.Next();
			}
			if ( sTok.emType == SToken::GUID )
				sTok = Tok.Next();

			if ( sTok.emType != SToken::OBRACE )
			{
				strError = "expected '{' after " + sObj.strType + " " + sObj.strName;
				return false;
			}

			for (;;)
			{
				sTok = Tok.Next();
				switch ( sTok.emType )
				{
				case SToken::CBRACE:
					return true;

				case SToken::OBRACE:
					{
						//	Reference to another object : { Name } or { Name <guid> }.
						SXObject sRef;
						sRef.bReference = true;
						for ( sTok = Tok.Next(); sTok.emType != SToken::CBRACE; sTok = Tok.Next() )
						{
							if ( sTok.emType == SToken::NAME )	sRef.strName = sTok.strText;
							else if ( sTok.emType == SToken::END || sTok.emType == SToken::BAD )
							{
								strError = "unterminated reference in " + sObj.strType + " " + sObj.strName;
								return false;
							}
						}
						sObj.vecChildren.push_back ( sRef );
					}
					break;

				case SToken::NAME:
					{
						SXObject sChild;
						sChild.strType = sTok.strText;
						if ( !ParseObject ( Tok, sChild, strError ) )	return false;
						sObj.vecChildren.push_back ( sChild );
					}
					break;

				case SToken::FLOAT:
					sObj.vecFloats.push_back ( sTok.fValue );
					break;

				case SToken::END:
				case SToken::BAD:
					strError = "unexpected end of data in " + sObj.strType + " " + sObj.strName;
					return false;

				default:
					break;
				}
			}
		}

		bool SkipTemplate ( CTokenizer& Tok, std::string& strError )
		{
			int nDepth = 0;
			for (;;)
			{
				const SToken sTok = Tok.Next();
				if ( sTok.emType == SToken::OBRACE )		++nDepth;
				else if ( sTok.emType == SToken::CBRACE )
				{
					if ( --nDepth <= 0 )	return true;
				}
				else if ( sTok.emType == SToken::END || sTok.emType == SToken::BAD )
				{
					strError = "unterminated template";
					return false;
				}
			}
		}
	}

	bool ReadXFile ( const char* szPath, std::vector<SXObject>& vecObjectsOut, std::string& strErrorOut )
	{
		vecObjectsOut.clear();
		strErrorOut.clear();

		std::ifstream File ( szPath, std::ios::binary );
		if ( !File )
		{
			strErrorOut = "can't open file";
			return false;
		}
		const std::vector<char> vecData ( (std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>() );

		//	Header : "xof " version(4) format(4) float size(4), e.g. "xof 0303bin 0032".
		if ( vecData.size() < 16 || std::memcmp(vecData.data(), "xof ", 4) != 0 )
		{
			strErrorOut = "not a .x file";
			return false;
		}

		const std::string strFormat ( vecData.data() + 8, 4 );
		const std::string strFloat ( vecData.data() + 12, 4 );
		if ( strFormat != "txt " && strFormat != "bin " )
		{
			strErrorOut = "unsupported .x format '" + strFormat + "' (compressed files are not supported)";
			return false;
		}
		if ( strFloat != "0032" && strFloat != "0064" )
		{
			strErrorOut = "unsupported .x float size '" + strFloat + "'";
			return false;
		}

		CTokenizer Tok ( vecData, strFormat == "bin ", strFloat == "0064" );
		for (;;)
		{
			const SToken sTok = Tok.Next();
			switch ( sTok.emType )
			{
			case SToken::END:
				return true;

			case SToken::BAD:
				strErrorOut = "truncated or malformed data";
				return false;

			case SToken::TEMPLATE:
				if ( !SkipTemplate ( Tok, strErrorOut ) )	return false;
				break;

			case SToken::NAME:
				{
					SXObject sObj;
					sObj.strType = sTok.strText;
					if ( !ParseObject ( Tok, sObj, strErrorOut ) )	return false;
					if ( sObj.strType != "Header" )
						vecObjectsOut.push_back ( sObj );
				}
				break;

			default:
				break;
			}
		}
	}
}
