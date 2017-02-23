// ntservice.h
//
// Definitions for CServiceHusk
//

#ifndef __SERVICE_HUSK_H__
#define __SERVICE_HUSK_H__

#pragma once

#include "ConfigSettings.h"
#include "CommandLine.h"

#define SERVICE_CONTROL_USER 128
#define INLINE_CODE "inline"
#define SEND_NEXT_FILE "OkToSendNextFile"
#define FAILURE_FILE "FailureShutLinkDown"

// 25-11-1999 P Barber PinICL - PC0033709
// CONTINUE_PROCESSING_LINK - instructs ServHusk to continue processing
// this link, not to move onto next shared link.
// ---------------------------------------------------------------------
// NOTE: THIS VALUE MUST BE DIFFERENT FROM ALL MESSAGE ID's in HuskMsg.h
#define CONTINUE_PROCESSING_LINK 0x00000010L

#define severity(x) (((x) >> 30) & 0x3)
#define remove_severity(x) ( (x) & 0x3fffffff )

class CServiceHusk
{
public:
	typedef BOOL (*DLLHANDLE)(CString InPath,
							  CString InFile,
							  CString InWorkingDir,
							  CString InConfigFile,
							  CString OutFile);

    CServiceHusk();
    virtual ~CServiceHusk();

	// Methods to be overloaded in derived classes
	virtual void StoreLinkConfiguration();
	virtual void ClearWorkingDirectory();

	virtual DWORD ProcessFile(CString InPath, 
							 CString InFile, 
							 CString OutPath, 
							 CString OutFile);

	virtual void SuccessfulPath(CString, BOOL, CString, CString);
	virtual void FailurePath(CString, BOOL, CString, CString);
	virtual void GetServiceSpecificSettings();
	virtual BOOL CheckServiceSpecificSettings(bool Installing = false);

	// This is where any service specific command line settings get done.
	virtual void Parse_ServiceSpecificSettings(){};

	// This will be used to create (new) the service specific object.
	virtual void New_ServiceSpecificSettings(){};
	// This is where any default setting for m_ConfigFile-> type initialization.
	virtual void Initialize_ServiceSpecificSettings(){};
	// This is where the service specific ptr array will get set to 0.
	virtual void ClearArray_ServiceSpecificSettings(){};

	// This will be used to add an item to the service specific array.
	virtual void Add_ServiceSpecificSettings(){};
	// This will be used to set an item in the service specific array.
	virtual void Set_ServiceSpecificSettings(int i){};

	// This will delete an object of service specific type.
	virtual void Delete_ServiceSpecificSettings(){};
	// This will clear the service specific array and delete all objects.
	virtual void Clear_ServiceSpecificSettings(){};

	virtual BOOL CheckDLLFunctionExists();
	virtual void CheckOverdueFiles();
	virtual bool WaitUntilDelivered();
	virtual bool FileInTransit();
	virtual bool FailFileInTransit();	// Added P Barber 01-02-2000.  Ref: PinICL PC0034206
	virtual bool ReSendFile(const CString & FileToTest);

	virtual void PollDirectories();
	virtual bool ListAllFilesInDir(CStringList* outFileList, const CString& dir ) const;

	bool CheckValidRegEx( const CSearchAndReplace& r, LPCSTR regexName );

	// FTMS utility type methods
	bool CreateAckFile(	const CString& AckFileName,
						const CString& DeliveredFileName,
						const CString& FailureReason,
						bool isPositive,
						const CString& ArchiveFileName = ""	) const;

	// Service Methods
    bool ParseStandardArgs(int argc, char* argv[]);

	bool InstallService();
    bool IsInstalled();
    bool Install();
	void CreateRegistryDetails();
	void RemoveRegistryDetails();

	bool UnInstallService();
    bool Uninstall();

    BOOL StartService();
    BOOL Initialize();

	bool GetLinkSettings();
	virtual BOOL ServiceStartUp();

	void ProcessLink();
	
	// These methods can be overloaded if required
    virtual void SetStatus(DWORD dwState);
    virtual BOOL OnUserControl(DWORD dwOpcode);
	virtual void Run();
    virtual void OnStop();
    virtual void OnInterrogate();
    virtual void OnPause();
    virtual void OnContinue();
    virtual void OnShutdown();

    // static member functions
    static void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
    static void WINAPI Handler(DWORD dwOpcode);

	//static void CALLBACK PollDirectory(UINT,UINT,DWORD, DWORD, DWORD);
	//static void ListAllFilesInDir(CStringList*, CString);
	void DebugMsg(const char* pszFormat, ...) const;
    void LogEvent(DWORD dwID,
						const char* pszS1 = NULL,
						const char* pszS2 = NULL,
						const char* pszS3 = NULL)
	{
		LogEvent( dwID, 0, pszS1, pszS2, pszS3 );
	}

    void LogEvent(DWORD dwID, WORD category,
						const char* pszS1 = NULL,
						const char* pszS2 = NULL,
						const char* pszS3 = NULL);

	CString GetLastEventString() const
	{
		return m_lastEventString;
	}

private:
	void WaitPollInterval();

protected:
	BOOL MoveFileEx(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName,DWORD dwFlags);
	bool CopyFileCheck(const CString & source, const CString & dest);

    // data members
public:
    SERVICE_STATUS m_Status;
	CString m_ServiceType;

	CCommandLine m_CommandLine;

protected:

    SERVICE_STATUS_HANDLE m_hServiceStatus;

    bool m_ShutdownRequested;

	HINSTANCE m_DLLHandle;
	DLLHANDLE m_ProcessFilePointer;

	CWrapper m_WrapperFile;

	CConfigSettings* m_ConfigFile;
	CPtrArray m_ConfigFileArray;

	// These two values are used to cache the calculated CRC value of the delivered
	// file
private:
	DWORD m_deliveredCRC;
	bool m_deliveredCRCisValid;

protected:
	// Call these functions to set!
	void SetDeliveredCRC( DWORD crc )
	{
		m_deliveredCRC = crc;
		m_deliveredCRCisValid = true;
	}

	void ClearDeliveredCRC()
	{
		m_deliveredCRCisValid = false;
#ifdef _DEBUG
		// dummy value
		m_deliveredCRC = 0xeeeeeeee;
#endif
	}

	bool IsDeliveredCRCvalid() const { return m_deliveredCRCisValid; }
	DWORD GetDeliveredCRC() const
	{
		ASSERT( IsDeliveredCRCvalid() );
		return m_deliveredCRC;
	}

private:
	// "mutable" means that this value can change, even in "const" member functions
	mutable CString m_lastEventString;
    mutable HANDLE m_hEventSource;

    // static data
    static CServiceHusk* s_pThis; // nasty hack to get object ptr

protected:
	BOOL CopyDACL(const CString& Source, const CString& Target);
	CString DoSearchReplace( CSearchAndReplace& r, const CString& source );

	// The following bools can be set by derived classes to modify the behaviour
	// of the husk.

	// m_readAndStoreWrapperBeforeProcessing: If true, the husk will read the wrapper
	//   information from the in file before calling ProcessFile, storing it in m_WrapperFile.
	//   The in file is not modified.  If the in file is not wrapped, no action is taken.
	//		Default: true
	bool m_readAndStoreWrapperBeforeProcessing;

	// m_copyUnwrappedToWorking: if true, the husk will ensure an unwrapped copy
	//   of the current file is present in the working directory before calling
	//   ProcessFile. If the input file is unwrapped, a straight copy is stored in 
	//   the working dir.  If wrapped, it is copied and then the wrapper is removed.
	//		Default: false
	bool m_copyUnwrappedToWorking;

	// m_restoreWrapperToOutputFile: if true, and if the husk read and stored a wrapper
	//   before processing (as directed by m_readAndStoreWrapperBeforeProcessing),
	//   then this exact wrapper is restored to the file (after processing) in the working
	//   directory, before moving to the output directory.  No action is taken if a
	//   wrapper was not stored before processing.  (By implication, therefore, this
	//   value is ignored if m_readAndStoreWrapperBeforeProcessing is false.)
	//		Default: true
	bool m_restoreWrapperToOutputFile;

	// m_FailureRetainInFile: if true, the husk will copy the in file to the error
	//   directory, instead of moving the file when a error occurs. This is only set
	//   to true by the Pre-Process at the momment.
	//   if false, the husk will move the in file to the error directory.
	//		Default: false
	bool m_FailureRetainInFile;

	CEvent m_shutDownEvent;
};

#endif // _NTSERVICE_H_
