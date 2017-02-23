// ConfigSettings.h: interface for the ConfigSettings class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONFIGSETTINGS_H__A6F2E633_98BF_11D1_8A7B_00004B4812B3__INCLUDED_)
#define AFX_CONFIGSETTINGS_H__A6F2E633_98BF_11D1_8A7B_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SearchAndReplace.h"

//const CString DISK_FTMS_ROOT = "C:\\FTMS";
#define REG_FTMS_ROOT "SOFTWARE\\ICL\\FTMS\\";

class CServiceHusk;


class CConfigSettings  
{

public:

	CConfigSettings();
	virtual ~CConfigSettings();

	void SetDefaultSettings(const CString & LinkName,
						    const CString & ServiceType,
							const CString & LinkRoot);

	void ReadVolatileSettings();
	void ReadSettings();

	CString ReadRegistryString(LPCSTR valueName,
							   LPCSTR defaultValue = "",
							   bool LinkSettings = false,
							   bool UpdateValue = false,
							   bool CreateValue = true);

	bool	ReadRegistryBool(LPCSTR valueName,
							 LPCSTR defaultValue = "",
							 bool LinkSettings = false,
							 bool UpdateValue = false,
							 bool CreateValue = true);

	int		ReadRegistryInt(LPCSTR valueName,
							LPCSTR defaultValue = "");

	bool IsValid(CServiceHusk* Service, bool Installing = false);

	bool IsLinkEnabled();
	void SetLinkToDisabled();

	bool IsRefreshSettings();
	void SetNoRefreshSettings();

	bool IsStrictSequencing();

	bool RemoveRegistryLink();

	bool RefreshAllLinks();
	void SetNoRefreshAllLinks();

	CString m_AckInDir;
	CSearchAndReplace m_AckInRenameExp;

	CString m_SourceSystemIdentity;
	CString m_TargetSystemIdentity;

	CString m_WorkingDirectory;

	CString m_OutDirectory;
	CSearchAndReplace m_OutRenameExp;


	CString m_ErrorDirectory;
	CSearchAndReplace m_ErrorRenameExp;
	CSearchAndReplace m_ErrorLogRenameExp;


	int		m_RetryCount;
	int		m_RetryWait;

	int		m_PollInterval;
	DWORD	m_StartTime;

	bool	m_Sequencing;

	enum SequenceType
	{
		Created,
		LastModified,
		LastAccessed
	};

	SequenceType m_SequenceType;
	bool	m_WriteToWrapper;

	CString m_LinkName;
	CString m_ServiceType;
	CString m_LinkRoot;

	struct InDirectoryInfo
	{
		CString m_path;
		DWORD m_lastSuccessfulScan;
		bool m_errorCondition;
	};

	CList<InDirectoryInfo, InDirectoryInfo&> m_InDirs;

	// this is the interval for which the In directory scan must
	// have failed for before we raise an error
	long m_inDirFailureThreshold;

	CStringList m_InPats;

	CString m_ProcessFileDLL;

	bool	m_RemoveWrapper;

	// These members can be set to indicate which directory settings
	// should be checked for validity.
	enum DirectoryCheckSetting
	{
		// Directory setting is required and service should fail
		// to start if not present.
		required = 1,	

		// Directory setting should be validated and a warning raised
		// if invalid, but the service should still startup.
		validate,

		// Directory setting not required.  Do not read from registry.
		notRequired
	};


	DirectoryCheckSetting m_inDirSetting;
	DirectoryCheckSetting m_outDirSetting;
	DirectoryCheckSetting m_workingDirSetting;
	DirectoryCheckSetting m_ackInDirSetting;
	DirectoryCheckSetting m_errorDirSetting;

	static bool CheckDirectory( CServiceHusk* pService, 
		LPCSTR dirName, LPCSTR dirPath, 
		DirectoryCheckSetting setting = required,
		bool Installing = false);

	static const LPCSTR s_DefaultSetting;

	// This enables the CRC check on writes to Sequent in PostProcess
	// (see ServiceHusk::MoveFileEx for details)
	
	bool m_CheckAfterWriteViaNfs;

	bool m_ReceivingEnd;

private:

	bool m_LinkEnabled;
	bool m_RefreshSettings;
	bool m_StrictSequencing;
};

#endif // !defined(AFX_CONFIGSETTINGS_H__A6F2E633_98BF_11D1_8A7B_00004B4812B3__INCLUDED_)
