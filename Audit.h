// Audit.h: interface for the CAudit class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_AUDIT_H__CD083513_A78F_11D1_8AA6_00004B4812B3__INCLUDED_)
#define AFX_AUDIT_H__CD083513_A78F_11D1_8AA6_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000


class CAudit  
{
public:
	CAudit();
	virtual ~CAudit();

	BOOL OpenFile();
	void CloseFile();
	bool WriteRecord(const CString& DataFile, const CString& DataPath);

	bool WriteRecord(CTime DateProcessed,        const CString& DataFileName,
					 const CString& DataPath,    const CString& MachineName,
					 CTime DataFileCreationTime, long DataFileSize,
					 long DataFileCRC,		     const CString& ArchiveFile,
					 const CString& ArchivePath);

	void SetSystemID(const CString& SystemID)
	{
		m_SystemID = SystemID;
	}

	void SetAuditDirectory(const CString& AuditDirectory)
	{
		m_AuditDirectory = AuditDirectory;
	}

	void SetSourceID(const CString& SourceID)
	{
		m_SourceID = SourceID;
	}
		
	void SetTargetID(const CString& TargetID)
	{
		m_TargetID = TargetID;
	}

	void SetServiceID(const CString& ServiceID)
	{
		m_ServiceID = ServiceID;
	}

	void SetCRC( long crc )
	{
		m_CRC = crc;
	}

	void SetRecordDateStamp( const CTime& t )
	{
		m_recordDateStamp = t;
	}

	void SetArchiveFile( const CString& ArchiveFile )
	{
		m_ArchiveFile = ArchiveFile;
	}

	void SetArchivePath( const CString& ArchivePath )
	{
		m_ArchivePath = ArchivePath;
	}

private:
	BOOL IsFileFound();
	BOOL CreateFile();
	BOOL WriteHeader();
	BOOL AddToRecordCount();
	CString GetFileName();

	// Data members
	CFile	m_DiskFile;

	CString m_AuditDirectory;		// Audit file path
	CString m_FileName;				// Audit filename

	CString m_SystemID;				// 3 chars

	// Header file data members
	CString	m_SourceID;				// 4 chars
	CString m_TargetID;				// 4 chars
	CString m_ServiceID;			// 4 chars

	// Record data members
	long m_CRC;					// 4 hex
	CTime m_recordDateStamp;
	CString m_ArchiveFile;
	CString m_ArchivePath;
};

#endif // !defined(AFX_AUDIT_H__CD083513_A78F_11D1_8AA6_00004B4812B3__INCLUDED_)
