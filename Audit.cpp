// Audit.cpp: implementation of the CAudit class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Audit.h"
#include "CRC.h"
#include "Globals.h"

//#define AUDIT_SYSTEM_ID_SIZE			3

#define AUDIT_HEADER_SOURCE_ID_SIZE		4
#define AUDIT_HEADER_TARGET_ID_SIZE		4
#define AUDIT_HEADER_SERVICE_ID_SIZE	4
#define AUDIT_HEADER_RECORD_TOTAL_SIZE	8
#define AUDIT_HEADER_DATESTAMP_SIZE		14

//#define AUDIT_RECORD_NUM_OF_FIELDS		9

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAudit::CAudit() 
: m_CRC(0)
{
}

CAudit::~CAudit()
{

}

BOOL CAudit::OpenFile()
{
	// If does does not exist will need to create
	if ( !IsFileFound() )
	{
		if ( !CreateFile() )
		{
			return FALSE;
		}
	}
	else
	{
		if ( !m_DiskFile.Open(m_FileName, CFile::modeReadWrite ) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CAudit::IsFileFound()
{
	CFileStatus FileStatus;

	GetFileName();

	if( CFile::GetStatus( m_FileName, FileStatus ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

CString CAudit::GetFileName()
{
	// Get todays date and time
	CTime TodaysDate = CTime::GetCurrentTime();
	CString strTodaysDate;

	strTodaysDate = TodaysDate.FormatGmt("%Y%m%d");

	m_FileName.Format("%s0000%s0000.CTL",
					  m_SystemID,
					  strTodaysDate);

	m_FileName = m_AuditDirectory + "\\" + m_FileName;

	return m_FileName;
}

BOOL CAudit::CreateFile()
{
	try
	{
		if ( !m_DiskFile.Open(m_FileName, CFile::modeCreate | CFile::modeReadWrite ) )
		{
			return FALSE;
		}

		if ( !WriteHeader() )
		{
			return FALSE;
		}

		return TRUE;
	}
	catch (CFileException* e)
	{
		e->Delete();
		return FALSE;
	}
}

void CAudit::CloseFile()
{
	m_DiskFile.Close();
}

bool CAudit::WriteRecord(const CString& dataFileName, const CString& dataPath)
{
	try
	{
		CFileStatus fileStatus;

		if( !CFile::GetStatus( dataPath + "\\" + dataFileName, fileStatus ) )
		{
			return FALSE;
		}

		const CString dataFileDateStamp = GetDateAndTimeAsString( fileStatus.m_ctime );
		const long dataFileSize = fileStatus.m_size ;

		const CString machineName = GetMachineName(dataPath);

		CString	OutString;
		CString	blank;

		OutString.Format(
			"|%s|%s|%s|%s|%s|%s|%s|%d|%d|%s|%s|\r\n", 
			GetDateAndTimeAsString( m_recordDateStamp ),
			dataFileName,
			dataPath,
			machineName,
			dataFileDateStamp,
			blank,		// data file record total
			blank,		// max record size
			dataFileSize,
			m_CRC,
			m_ArchiveFile,
			m_ArchivePath);

		m_DiskFile.SeekToEnd();
		m_DiskFile.Write( OutString, OutString.GetLength() );

		if ( !AddToRecordCount() )
		{
			return FALSE;
		}

		return TRUE;
	}
	catch (CFileException* e)
	{
		e->Delete();
		return FALSE;
	}
}
bool CAudit::WriteRecord(CTime DateProcessed,
						 const CString& DataFileName,
						 const CString& DataPath,
						 const CString& MachineName,
						 CTime DataFileCreationTime,
						 long DataFileSize,
						 long DataFileCRC,
						 const CString& ArchiveFile,
						 const CString& ArchivePath)
{
	CString	OutString;
	CString blank;

	OutString.Format(
		"|%s|%s|%s|%s|%s|%s|%s|%d|%d|%s|%s|\r\n", 
		GetDateAndTimeAsString( DateProcessed ),
		DataFileName,
		DataPath,
		MachineName,
		GetDateAndTimeAsString( DataFileCreationTime ),
		blank,		// data file record total
		blank,		// max record size
		DataFileSize,
		DataFileCRC,
		ArchiveFile,
		ArchivePath);

	m_DiskFile.SeekToEnd();
	m_DiskFile.Write( OutString, OutString.GetLength() );

	if ( !AddToRecordCount() )
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CAudit::WriteHeader()
{
	try
	{
		// Creation date
		CFileStatus FileStatus;

		if( !CFile::GetStatus( m_FileName, FileStatus ) )
		{
			return FALSE;
		}

		CString auditFileDateStamp = GetDateAndTimeAsString( FileStatus.m_ctime );

		// Write record
		CString OutString;
		//////////////////////////////////////////////////////////////
		//
		// BUG fix: added precision to the format string. to nail the
		// maximum field length to 4 - Milton Ezeh, 24/9/99
		// 
		//////////////////////////////////////////////////////////////
		OutString.Format("%-4.4s%-4.4s%-4.4s%08d%14s\r\n", 
			m_SourceID,
			m_TargetID,
			m_ServiceID,
			0,
			auditFileDateStamp );

		m_DiskFile.SeekToEnd();
		m_DiskFile.Write(OutString, OutString.GetLength());

		return TRUE;
	}
	catch (CFileException FileException)
	{
		return FALSE;
	}
}

BOOL CAudit::AddToRecordCount()
{
	try
	{
		CString OutString;
		LONG	FilePointer = 0;
		LONG	RecordTotalPosition = 0;
		LONG	BytesRead = 0;
		LONG	lRecordTotal = 0;

		RecordTotalPosition = AUDIT_HEADER_SOURCE_ID_SIZE
							+ AUDIT_HEADER_TARGET_ID_SIZE
							+ AUDIT_HEADER_SERVICE_ID_SIZE;

		FilePointer = m_DiskFile.Seek(RecordTotalPosition, CFile::begin);

		if ( FilePointer != RecordTotalPosition )
		{
			return FALSE;
		}

		// Read record total
		char RecordTotal [8];

		memset(RecordTotal, NULL, sizeof(RecordTotal) / sizeof(RecordTotal[0]) );

		BytesRead = m_DiskFile.Read(RecordTotal, 8);

		if ( BytesRead != 8 )
		{
			return FALSE;
		}

		lRecordTotal = atol( RecordTotal );

		// TODO Add 1 to total
		lRecordTotal++;

		OutString.Format("%08d", lRecordTotal);

		FilePointer = m_DiskFile.Seek(RecordTotalPosition, CFile::begin);

		if ( FilePointer != RecordTotalPosition )
		{
			return FALSE;
		}

		m_DiskFile.Write(OutString, 8);

		return TRUE;
	}
	catch ( CFileException FileException )
	{
		return FALSE;
	}
}

