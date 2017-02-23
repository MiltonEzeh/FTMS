// PostProcess.h: interface for the CPostProcess class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_POSTPROCESS_H__5DAAF865_A450_11D1_8AA5_00004B4812B3__INCLUDED_)
#define AFX_POSTPROCESS_H__5DAAF865_A450_11D1_8AA5_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ServHusk.h"
#include "PostProcessSettings.h"

class CPostProcess : public CServiceHusk  
{
public:
	CPostProcess();
	virtual ~CPostProcess();

	// Overloaded methods
	void GetServiceSpecificSettings();
	BOOL CheckServiceSpecificSettings(bool Installing);

	void New_ServiceSpecificSettings();
	void ClearArray_ServiceSpecificSettings();

	void Add_ServiceSpecificSettings();
	void Set_ServiceSpecificSettings(int i);

	void Delete_ServiceSpecificSettings();
	void Clear_ServiceSpecificSettings();

	DWORD ProcessFile(CString, CString, CString, CString);

	bool WriteAuditTrial(const CString& InPath, 
						 const CString& InFile, 
						 const CTime& auditTime,
						 long crc,
						 const CString& ArchiveFile,
						 const CString& ArchivePath) const;

	void SuccessfulPath(CString, BOOL, CString, CString);

	// data members
	CPostProcessSettings* m_PostProcessSettings;
	CPtrArray m_PostProcessSettingsArray;
};

#endif // !defined(AFX_POSTPROCESS_H__5DAAF865_A450_11D1_8AA5_00004B4812B3__INCLUDED_)
