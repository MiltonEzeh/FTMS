// Decompress.h: interface for the CDecompress class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DECOMPRESS_H__C06CF935_A6EC_11D1_8AA6_00004B4812B3__INCLUDED_)
#define AFX_DECOMPRESS_H__C06CF935_A6EC_11D1_8AA6_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "DecompressSettings.h"

class CDecompress : public CServiceHusk  
{
public:
	CDecompress();
	virtual ~CDecompress();

	void GetServiceSpecificSettings();
	BOOL CheckServiceSpecificSettings(bool Installing);

	void Parse_ServiceSpecificSettings();

	void New_ServiceSpecificSettings();
	void ClearArray_ServiceSpecificSettings();

	void Add_ServiceSpecificSettings();
	void Set_ServiceSpecificSettings(int i);

	void Delete_ServiceSpecificSettings();
	void Clear_ServiceSpecificSettings();

	DWORD ProcessFile(CString, CString, CString, CString);

private:
	enum Mode
	{
		unknown,
		compress,
		decompress
	};

	Mode m_mode;

	CDecompressSettings* m_DecompressSettings;
	CPtrArray m_DecompressSettingsArray;
};

#endif // !defined(AFX_DECOMPRESS_H__C06CF935_A6EC_11D1_8AA6_00004B4812B3__INCLUDED_)
