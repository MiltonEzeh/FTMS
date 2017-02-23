// SearchAndReplace.h: interface for the CSearchAndReplace class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SEARCHANDREPLACE_H__4A299314_BCC5_11D1_8F7F_00004B484B9E__INCLUDED_)
#define AFX_SEARCHANDREPLACE_H__4A299314_BCC5_11D1_8F7F_00004B484B9E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000


#include "RegExp.h"


class CSearchAndReplace  
{
public:
	CSearchAndReplace();
	virtual ~CSearchAndReplace();

	enum CaseModes
	{
		caseSensitive,
		forceUpperCase,
		forceLowerCase
	};


	CString Replace( const CString& source );
	bool Init( const CString& searchReplaceString, CaseModes mode = caseSensitive );

	bool IsValid() const
	{
		return m_isValid;
	}

	CString GetLastErrorString() const
	{
		return m_lastError;
	}

	CaseModes GetCurrentCaseMode() const
	{
		return m_currentCaseMode;
	}

private:
	void ModifyStringAccordingToCurrentCaseMode( CString& s ) const;
	void Clear();
	bool m_isValid;

	CString m_lastError;

	CaseModes m_currentCaseMode;

	// This structure represents a configured pair of regex->replace strings.
	// It also includes the regular expression object that contains a parsed version
	// of the regex string.
	struct RegExPair
	{
		CString m_regexStr;
		CString m_replaceStr;

		CRegExp m_regex;
	};

	RegExPair *m_replacementList;
	int m_replacementListLen;

};

#endif // !defined(AFX_SEARCHANDREPLACE_H__4A299314_BCC5_11D1_8F7F_00004B484B9E__INCLUDED_)
