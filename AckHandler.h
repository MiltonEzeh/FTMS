// AckHandler.h: interface for the CAckHandler class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ACKHANDLER_H__E1BDF5C5_A6AC_11D1_8AA6_00004B4812B3__INCLUDED_)
#define AFX_ACKHANDLER_H__E1BDF5C5_A6AC_11D1_8AA6_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ServHusk.h"
#include "AckHandlerSettings.h"

//////////////////////////////////////////////////////////////////////
// Constants
extern "C" const char* c_ACK_ORIGINAL_DIR;	// PinICL PC0033290 (added c_ACK_ORIGINAL_DIR)

class CInTransit;
class CAcknowledgement;

class CAckHandler : public CServiceHusk 
{
public:
	CAckHandler();
	virtual ~CAckHandler();

	// Overloaded methods
	void GetServiceSpecificSettings();
	BOOL CheckServiceSpecificSettings(bool Installing);

	void New_ServiceSpecificSettings();
	void Initialize_ServiceSpecificSettings();
	void ClearArray_ServiceSpecificSettings();

	void Add_ServiceSpecificSettings();
	void Set_ServiceSpecificSettings(int i);

	void Delete_ServiceSpecificSettings();
	void Clear_ServiceSpecificSettings();

	DWORD ProcessFile(CString, CString, CString, CString);

	void SuccessfulPath(CString, BOOL, CString, CString);
	void CheckOverdueFiles();
	bool WriteDuplicatePostProcessAuditRecord(const CString& AckFileName);
	void CreateStrictSequenceFile(const CString & File);

private:
	// Data members
	CAckHandlerSettings* m_AckHandlerSettings;
	CPtrArray m_AckHandlerSettingsArray;
};

#endif // !defined(AFX_ACKHANDLER_H__E1BDF5C5_A6AC_11D1_8AA6_00004B4812B3__INCLUDED_)
