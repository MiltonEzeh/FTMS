// PreProcess.h: interface for the CPreProcess class.
//
//////////////////////////////////////////////////////////////////////

#include "PreProcessSettings.h"

class CPreProcess : public CServiceHusk  
{
public:
	CPreProcess();
	virtual ~CPreProcess();

	// Overloaded functions
	void GetServiceSpecificSettings();
	BOOL CheckServiceSpecificSettings(bool Installing);

	void New_ServiceSpecificSettings();
	void ClearArray_ServiceSpecificSettings();

	void Add_ServiceSpecificSettings();
	void Set_ServiceSpecificSettings(int i);

	void Delete_ServiceSpecificSettings();
	void Clear_ServiceSpecificSettings();

	DWORD ProcessFile(CString, 
					 CString, 
					 CString, 
					 CString);
	void SuccessfulPath(CString, BOOL, CString, CString);
	void FailurePath(CString, BOOL, CString, CString);
	bool WaitUntilDelivered();
	bool FileInTransit();
	bool FailFileInTransit();	// Added P Barber 01-02-2000.  Ref: PinICL PC0034206
	bool ReSendFile(const CString & FileToTest);

	// Classes own methods
	bool WriteAuditTrial(const CString& InPath, 
						 const CString& InFile,
						 const CTime&   auditTime, 
						 long			CRC,
						 const CString& ArchiveFile,
						 const CString& ArchivePath) const;

	CPreProcessSettings* m_PreProcessSettings;
	CPtrArray m_PreProcessSettingsArray;
};