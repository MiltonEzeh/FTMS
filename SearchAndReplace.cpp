// SearchAndReplace.cpp: implementation of the CSearchAndReplace class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "SearchAndReplace.h"

#include <ctype.h>

#ifdef _USE_LOCAL_GLOBALS

#include "LocalGlobals.h"

#else

#include "Globals.h"

#endif // _USE_LOCAL_GLOBALS

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSearchAndReplace::CSearchAndReplace()
{
	m_isValid = false;
	m_replacementList = 0;
	m_replacementListLen = 0;
}

CSearchAndReplace::~CSearchAndReplace()
{
	Clear();
}

bool CSearchAndReplace::Init( const CString& paramSearchReplaceString, CaseModes mode /*= caseSensitive */)
{
	// tidy up the class
	Clear();

	m_currentCaseMode = mode;

	// make copies of parameters so we can modify
	CString searchReplace = paramSearchReplaceString;

	ModifyStringAccordingToCurrentCaseMode( searchReplace );

	// split the search and replace strings into separate items
	CStringList searchParams;
	SeperateParameter( searchReplace, searchParams );

	if ( searchParams.IsEmpty() )
	{
		m_lastError = "Empty search string." ;
		return false;
	}

	// check even number of parameters
	if ( ( searchParams.GetCount() % 2 ) != 0)
	{
		m_lastError = "Need even number of items in search and replace strings.";
		return false;
	}

	// allocate replacement list

	m_replacementListLen = searchParams.GetCount() / 2;
	m_replacementList = new RegExPair[ m_replacementListLen ];
	
	RegExPair *curPair = m_replacementList;

	// for each parameter
	for ( POSITION searchPos = searchParams.GetHeadPosition(); searchPos; curPair++ )
	{
		curPair->m_regexStr = searchParams.GetNext( searchPos );
		curPair->m_replaceStr = searchParams.GetNext( searchPos );

		// parse regex
		if ( !curPair->m_regex.RegComp( curPair->m_regexStr ) )
		{
			// compilation failed!
			m_lastError.Format( _TEXT("Invalid regular expression: \"%s\""), curPair->m_regexStr );
			return false;
		}
	}

	m_isValid = true;
	return true;
}

CString CSearchAndReplace::Replace(const CString & paramSource )
{
	CString retVal;

	if ( !IsValid() )
	{
		m_lastError = "Replace called without successful Init.";
		return retVal;
	}

	CString source = paramSource;

	ModifyStringAccordingToCurrentCaseMode( source );

	// now compare source against each of the expressions we have
	for ( int i = 0; i < m_replacementListLen; i++ )
	{
		if ( m_replacementList[i].m_regex.RegFind( source ) == 0 )
		{
			// found a match -- at position zero
			// now replace
			TCHAR *replacement = m_replacementList[i].m_regex.GetReplaceString( m_replacementList[i].m_replaceStr );

			if ( replacement )
			{
				// copy to return value
				retVal = replacement;

				// delete temporary
				delete[] replacement;

				// and return
				return retVal;

			}

		}

	}

	return retVal;
}


void CSearchAndReplace::Clear()
{
	delete[] m_replacementList;
	m_replacementList = 0;
	m_replacementListLen = 0;
	m_isValid = false;
	m_lastError.Empty();
}


void CSearchAndReplace::ModifyStringAccordingToCurrentCaseMode(CString & s) const
{
	// obey case sensativity
	switch ( GetCurrentCaseMode() )
	{
	case forceUpperCase:
		s.MakeUpper();
		break;

	case forceLowerCase:
		s.MakeLower();
		break;

	default:
		// do not modify
		break;
	}
		
}

