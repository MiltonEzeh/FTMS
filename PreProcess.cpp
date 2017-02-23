// PreProcess.cpp: implementation of the CPreProcess class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "InTransit.h"
#include "wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "Audit.h"
#include "ServHusk.h"
#include "PreProcess.h"
#include "HuskMsg.h" // Event message ids
#include "Globals.h"
#include "CRC.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPreProcess::CPreProcess()
{
	m_ServiceType = "PreProcess";

	m_PreProcessSettings = NULL;

	m_readAndStoreWrapperBeforeProcessing = false;
	m_copyUnwrappedToWorking = false;
	m_restoreWrapperToOutputFile = false;
	m_FailureRetainInFile = true;
}

CPreProcess::~CPreProcess()
{
}

void CPreProcess::GetServiceSpecificSettings()
{
	m_PreProcessSettings->m_InTransitDirectory = m_ConfigFile->ReadRegistryString("InTransitDirectory", m_ConfigFile->m_LinkRoot + "\\" + m_ServiceType + "\\InTransit");
	m_PreProcessSettings->m_ControlFileDirectory = m_ConfigFile->ReadRegistryString("ControlFileDirectory", m_ConfigFile->m_LinkRoot + "\\" + m_ServiceType + "\\Audit");
	m_PreProcessSettings->m_ArchiveDirectory = m_ConfigFile->ReadRegistryString("ArchiveDirectory");

	m_PreProcessSettings->m_AddCRC = m_ConfigFile->ReadRegistryBool("AddCRC", "Yes");
}

BOOL CPreProcess::CheckServiceSpecificSettings(bool Installing)
{
	if ( !CConfigSettings::CheckDirectory( this, "InTransit", m_PreProcessSettings->m_InTransitDirectory, CConfigSettings::required, Installing ) )
		return false;

	if ( !m_PreProcessSettings->m_ControlFileDirectory.IsEmpty() )
	{
		if ( !CConfigSettings::CheckDirectory( this, "ControlFile", m_PreProcessSettings->m_ControlFileDirectory, CConfigSettings::required, Installing ) )
		{
			m_PreProcessSettings->m_WriteAuditInfo = false;
			return false;
		}
		else
		{
			m_PreProcessSettings->m_WriteAuditInfo = true;
		}
	}
	else
	{
		m_PreProcessSettings->m_WriteAuditInfo = false;
	}

	if ( !m_PreProcessSettings->m_ArchiveDirectory.IsEmpty() )
	{
		if ( !CConfigSettings::CheckDirectory( this, "ArchiveDirectory", m_PreProcessSettings->m_ArchiveDirectory, CConfigSettings::required, Installing ) )
		{
			m_PreProcessSettings->m_ArchiveData = false;
			return false;
		}
		else
		{
			m_PreProcessSettings->m_ArchiveData = true;
		}
	}
	else
	{
		m_PreProcessSettings->m_ArchiveData = false;
	}

	return true;
}

DWORD CPreProcess::ProcessFile(CString InPath, CString InFile, 
							  CString OutPath, CString OutFile)
{
	const CString fullInFileName = InPath + "\\" + InFile;

	// Check in transit dir, if files exist no processing required
	const CString inTransitFilename = m_PreProcessSettings->m_InTransitDirectory + "\\" + InFile;
	CInTransit inTrans( inTransitFilename );

	if ( !inTrans.Open ( true ) )
	{
		LogEvent(EVMSG_INTRANSIT_FAILED, m_PreProcessSettings->m_InTransitDirectory + "\\" + InFile);
		return EVMSG_INTRANSIT_FAILED;
	}

	if ( inTrans.IsInTransit() )
	{
		// Ignore file
		// As successpath does nothing
		// Ok to say that file has processed correctly
		//DebugMsg("%s: file is currently in transit", fullInFileName );

		// 25-11-1999 P Barber PinICL - PC0033709
		// CONTINUE_PROCESSING_LINK - instructs ServHusk to continue processing
		// this link, not to move onto next shared link.
		return CONTINUE_PROCESSING_LINK;
	}

	// check that we can open the incoming file
	{
		CFile FileShareChk;
		CFileException FileError;

		if ( m_ConfigFile->IsStrictSequencing() )
		{
			DebugMsg("%s: Waiting for no NT file locks", fullInFileName);

			while ( !FileShareChk.Open( fullInFileName, CFile::shareExclusive, &FileError ) )
			{
				if ( FileError.m_cause != CFileException::sharingViolation )
				{
					DebugMsg("file error %s, GetLastError: %s", fullInFileName, GetErrorMessage( GetLastError(), true ));
					LogEvent(EVMSG_IN_FILE_ERROR, fullInFileName, GetErrorMessage( GetLastError(), true ) );

					inTrans.Delete();

					return EVMSG_IN_FILE_ERROR;
				}
			}

			DebugMsg("%s: No NT File locks", fullInFileName);
		}
		else
		{
			if ( !FileShareChk.Open( fullInFileName, CFile::shareExclusive )  )
			{
				DebugMsg("%s: file currently locked, ignoring!!!", fullInFileName );
				inTrans.Delete();
				return 0;
			}
		}

		// This is being done to trap files locked that are accessed
		// via NFS hummingbird. This command will wait until no file 
		// access is occurring.
		DebugMsg("%s: Waiting for no NFS file locks", fullInFileName);

		FileShareChk.LockRange( 0, FileShareChk.GetLength() );

		DebugMsg("%s: No NFS file locks", fullInFileName);
	}

	inTrans.SetPath( InPath );
	inTrans.SetInTransit( false );
	inTrans.SetLastAttemptStartTime( CTime::GetCurrentTime() );

	if ( !inTrans.Write() )
	{
		DebugMsg("%s: Failed to write intransit record. %s", inTransitFilename, GetErrorMessage( GetLastError(), true ) );
		LogEvent(EVMSG_INTRANS_WRITE_FAILED, inTransitFilename, GetErrorMessage( GetLastError(), true ) );
		return EVMSG_INTRANS_WRITE_FAILED;
	}

	// Store control info
	// ------------------


	m_WrapperFile.SetAssociatedFile( fullInFileName );

	// Added P Barber 07-12-1999.  Ref: PinICL PC0034437 & PC0033404
	//
	// Tests the return code of SetOriginalFileDetails, logs and returns an error.
	// The retry logic will try the file again.
	// If the file attributes are not present in the wrapper the acknowledgement
	// is created without the fields also.  The absence of these fields causes an
	// Access Violation exception in AckHandler.
	if ( !m_WrapperFile.SetOriginalFileDetails() )	// This may include Machine name
	{
		DebugMsg("%s: Failed to get file attributes. %s", fullInFileName, GetErrorMessage( GetLastError(), true ) );
		LogEvent(EVMSG_GET_FILE_ATTRIB_FAILED, fullInFileName, GetErrorMessage( GetLastError(), true ) );
		return EVMSG_GET_FILE_ATTRIB_FAILED;
	}
	m_WrapperFile.SetPickupTime();
	m_WrapperFile.SetSuccessStatus();

	const CString fullOutFileName = OutPath + "\\" + OutFile;

	// Check if In file is read only
	// If so, make writeable and reset after file has been copied
	// to working directory.
	CFileStatus FileStatus;

	if ( CFile::GetStatus(fullInFileName, FileStatus) )
	{
		if ( FileStatus.m_attribute & CFile::readOnly )
		{
			DebugMsg("%s File is read-only. Making writable.", fullInFileName);

			FileStatus.m_attribute ^= CFile::readOnly;

			try
			{
				CFile::SetStatus(fullInFileName, FileStatus);
			}
			catch ( CFileException* e)
			{
				if ( e->m_cause == CFileException::accessDenied )
				{
					LogEvent(EVMSG_IN_ACCESS_DENIED, fullInFileName,
						    GetErrorMessage( e->m_lOsError, true ) );
 
					e->Delete();

					return EVMSG_IN_ACCESS_DENIED;
				}

				e->Delete();
			}
		}
	}

	DebugMsg("Copying %s to %s...", fullInFileName, fullOutFileName);


	// Copy in file to working dir
	if ( CopyFile(fullInFileName, fullOutFileName, FALSE) == 0 )
	{
		LogEvent(EVMSG_COPY_IN_WORK_FAILED,
				fullInFileName, fullOutFileName,
				GetLastErrorString() );

		return EVMSG_COPY_IN_WORK_FAILED;
	}

	DWORD fileCRC = 0;

	if ( m_PreProcessSettings->m_AddCRC )
	{
		// CRC of original file
		DebugMsg("Calculating CRC for %s...", fullOutFileName);

		if ( !CalculateCrc( fileCRC, fullOutFileName ) )
		{
			LogEvent(EVMSG_CRC_FAILED, fullOutFileName);
			return EVMSG_CRC_FAILED;
		}

		m_WrapperFile.SetFileCRC( fileCRC );

		// store the fact we have calulcated this
		SetDeliveredCRC( fileCRC );
	}

	// Generate the CRC for the wrapper 
	long uCRC = m_WrapperFile.GenerateWrapperCRC();
	m_WrapperFile.SetWrapperCRC(uCRC);

	// append wrapper to end of new file
	m_WrapperFile.SetAssociatedFile( fullOutFileName );

	DebugMsg("Adding wrapper to %s...", fullOutFileName);
	
	if ( !m_WrapperFile.Write() )
	{
		LogEvent(EVMSG_ADD_WRAPPER_FAILED, fullOutFileName);
		return EVMSG_ADD_WRAPPER_FAILED;
	}

	CTime timeProcessed = CTime::GetCurrentTime();

	CString ArchiveFile;

	if ( m_PreProcessSettings->m_ArchiveData ) 
	{
		ArchiveFile = m_PreProcessSettings->m_ArchiveDirectory + "\\" 
			        + GetJustFileName(InFile) +	'.'
					+ GetDateAndTimeAsString( timeProcessed );
	}

	// Write audit trail info if required
	if ( m_PreProcessSettings->m_WriteAuditInfo )
	{
		DebugMsg("Attempting to write audit info...");

		if ( !WriteAuditTrial(InPath, InFile, timeProcessed, fileCRC,
			GetJustFileName( ArchiveFile),
			GetJustPath( ArchiveFile ) ) )
		{
			LogEvent(EVMSG_AUDIT_REC_WRITE_FAILED, fullInFileName, GetLastErrorString());
			return EVMSG_AUDIT_REC_WRITE_FAILED;
		}
	}

	if ( m_PreProcessSettings->m_ArchiveData )
	{
		DebugMsg( "Archiving..." );

		// copy file to archive directory
		if ( !CopyFileCheck(InPath + "\\"+ InFile, ArchiveFile) )
		{
			LogEvent( EVMSG_ARCHIVE_FAILURE, InPath + "\\"+ InFile , ArchiveFile, GetErrorMessage( GetLastError(), true ));
			return EVMSG_ARCHIVE_FAILURE;
		}
	}

	DebugMsg("Moving file from working to out...");

	CString RenamedFileName = DoSearchReplace( m_ConfigFile->m_OutRenameExp, OutFile );

	DebugMsg("Creating unique name for file....");

	// Create unique file name (use datestamp)
	// This is pre-fixed to filename, this ensures that unique acks, error file, etc
	// are created.
	{
		CTime now = CTime::GetCurrentTime();
		CString strnow = GetDateAndTimeAsString( now );

		RenamedFileName = strnow + "." + RenamedFileName;

		// Store filename in intransit file
		inTrans.SetInProgressFileName( RenamedFileName );

		if ( !inTrans.Write() )
		{
			DebugMsg("%s: Failed to write intransit record. %s", inTransitFilename, GetErrorMessage( GetLastError(), true ) );
			LogEvent(EVMSG_INTRANS_WRITE_FAILED, inTransitFilename, GetErrorMessage( GetLastError(), true ) );
			return EVMSG_INTRANS_WRITE_FAILED;
		}
	}

	DebugMsg("Move file and control information to out directory...");

	// Move file and control info to out dir
	if ( MoveFileEx(fullOutFileName, m_ConfigFile->m_OutDirectory + "\\" + RenamedFileName,
					MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH )
					== 0 )
	{
		LogEvent(EVMSG_MOVE_OUT_FAILED, fullOutFileName, m_ConfigFile->m_OutDirectory + "\\" + OutFile);
		return EVMSG_MOVE_OUT_FAILED;
	}

	// Write success message to event log
	LogEvent(EVMSG_SUCCESS_DELIVERY, m_ConfigFile->m_OutDirectory + "\\" + OutFile);

	// wrap the intransit file
	inTrans.Close();		// need to close it first
	m_WrapperFile.SetAssociatedFile( inTransitFilename );
	// it may already be wrapped 
	if ( m_WrapperFile.IsWrapped() )
		m_WrapperFile.Delete();
	m_WrapperFile.Write();

	// finally, indicate file is in transit
	if ( !inTrans.Open() )
	{
		DebugMsg("Failed to open intransit file %s: GetLastError returns %s", inTransitFilename, GetErrorMessage( GetLastError(), true ) );
		LogEvent(EVMSG_INTRANSIT_FAILED, inTransitFilename);
		return EVMSG_INTRANSIT_FAILED;
	}

	inTrans.SetInTransit( true );

	if ( !inTrans.Write() )
	{
		DebugMsg("%s: Failed to write intransit record. %s", inTransitFilename, GetErrorMessage( GetLastError(), true ) );
		LogEvent(EVMSG_INTRANS_WRITE_FAILED, inTransitFilename, GetErrorMessage( GetLastError(), true ) );
		return EVMSG_INTRANS_WRITE_FAILED;
	}

	return 0;
}


void CPreProcess::SuccessfulPath(CString InFile,
								 BOOL    IsWrapped,
								 CString OutFile,
								 CString OutStatusString)
{
	// Do nothing
}

void CPreProcess::FailurePath(CString InFile,
							   BOOL IsWrapped,
							   CString OutFile,
							   CString OutStatusString)
{
	// Set this so that failure path will write wrapper and negative ack
	IsWrapped = TRUE;

	m_copyUnwrappedToWorking = true;

	// Indicate file is in transit. This is done so that the end to end
	// retry will work.
	// Check in transit dir, if files exist no processing required
	const CString inTransitFilename = m_PreProcessSettings->m_InTransitDirectory + "\\" + GetJustFileName( InFile );
	CInTransit inTrans( inTransitFilename );

	if ( !inTrans.Open() )
	{
		LogEvent(EVMSG_INTRANSIT_FAILED, inTransitFilename);
		return;
	}

	inTrans.SetInTransit( true );

	if ( !inTrans.Write() )
	{
		DebugMsg("%s: Failed to write intransit record.", inTransitFilename);
		LogEvent(EVMSG_INTRANS_WRITE_FAILED, inTransitFilename, GetErrorMessage( GetLastError(), true ) );
		return;
	}

	CServiceHusk::FailurePath(InFile, IsWrapped, OutFile, OutStatusString);

	m_copyUnwrappedToWorking = false;
}


bool CPreProcess::WriteAuditTrial(const CString& InPath,      const CString& InFile, 
								  const CTime& auditTime,     long crc,
								  const CString& ArchiveFile, const CString& ArchivePath) const
{
	CAudit a;

	a.SetSourceID(m_ConfigFile->m_SourceSystemIdentity);
	a.SetTargetID(m_ConfigFile->m_TargetSystemIdentity);
	a.SetServiceID("FTMS");

	a.SetSystemID(m_ConfigFile->m_SourceSystemIdentity);
	a.SetAuditDirectory(m_PreProcessSettings->m_ControlFileDirectory);
	a.SetCRC( crc );
	a.SetRecordDateStamp( auditTime );
	a.SetArchiveFile( ArchiveFile );
	a.SetArchivePath( ArchivePath );

	if ( !a.OpenFile() )
	{
		DebugMsg("Failed to open audit file");
		return FALSE;
	}

	if ( !a.WriteRecord(InFile, InPath) )
	{
		DebugMsg("Failed to write audit record");
		return FALSE;
	}

	a.CloseFile();

	return TRUE;
}

bool CPreProcess::WaitUntilDelivered()
{
	// If strict sequencing is required, this function will
	// wait until the file has been fully transfered before
	// returning control so that the next file can be processed.

	if ( m_ShutdownRequested ) // shutdown requested
		return false;

	CFileFind FileFind;
	bool Found = false;
	CString DelFile;

	DelFile = SEND_NEXT_FILE;

	DebugMsg("Waiting until file has been delivered!!!");

	do
	{
		BOOL b = FileFind.FindFile(m_PreProcessSettings->m_InTransitDirectory + "\\*.*", 0);

		while ( b )
		{
			// Get next file
			b = FileFind.FindNextFile();

			// If not a directory and not "." or ".."
			if ( !FileFind.MatchesMask(FILE_ATTRIBUTE_DIRECTORY) && !FileFind.IsDots() )
			{

				if ( m_ShutdownRequested ) // shutdown requested
					return false;

				if ( FileFind.GetFileName() == FAILURE_FILE )
				{
					DebugMsg("Failure file found.");

					DelFile = FAILURE_FILE;

					Found = true;
					break;
				}

				if ( FileFind.GetFileName() == SEND_NEXT_FILE )
				{
					Found = true;
					break;
				}

			}
		}

		// Give Ack Handler chance to close file
		// and don't hog all the cpu while looping
		// Don't actually sleep: wait on the (normally blocking) shutdown event.  
		if ( m_shutDownEvent.Lock( 1000 ) )
		{
			// lock succeeded -- so shutdown IS requested!
			return false;
		}

		if ( m_CommandLine.m_ProcessMultiLinks )
			return true;

	} while ( !Found );

	DebugMsg("Ready to send next file");

	if ( !DeleteFile( m_PreProcessSettings->m_InTransitDirectory + "\\" + DelFile ) )
	{
		DebugMsg("Failed to delete %s -> %s", 
			m_PreProcessSettings->m_InTransitDirectory + "\\" + DelFile,
			GetErrorMessage( GetLastError(), true ));

		LogEvent(EVMSG_INTRANSIT_DEL_FAILED,
			m_PreProcessSettings->m_InTransitDirectory + "\\" + DelFile,
			GetErrorMessage( GetLastError(), true ));

		return false;
	}

	if ( DelFile == FAILURE_FILE )
		return false;
	else
		return true;

}

bool CPreProcess::FileInTransit()
{
	CFileFind FileFind;
	int NumFiles = 0;

	BOOL b = FileFind.FindFile(m_PreProcessSettings->m_InTransitDirectory + "\\*.*", 0);

	while ( b )
	{
		// Get next file
		b = FileFind.FindNextFile();

		// If not a directory and not "." or ".."
		if ( !FileFind.MatchesMask(FILE_ATTRIBUTE_DIRECTORY) && !FileFind.IsDots() )
		{
			// Added P Barber 01-02-2000.  Ref: PinICL PC0034206
			// ALSO test that file is not FAILURE_FILE
			if ( FileFind.GetFileName() != SEND_NEXT_FILE && FileFind.GetFileName() != FAILURE_FILE )
				NumFiles++;
		}
	}

	if ( NumFiles == 0 )
		return false;
	else
		return true;

}

// Added P Barber 01-02-2000.  Ref: PinICL PC0034206
// Test for FAILURE_FILE in InTransit
bool CPreProcess::FailFileInTransit()
{
	CFileFind FileFind;
	if (FileFind.FindFile(m_PreProcessSettings->m_InTransitDirectory + "\\" + FAILURE_FILE, 0))
		return true;
	else
		return false;
}

bool CPreProcess::ReSendFile(const CString & FileToTest)
{
	CString inTransitFilename = m_PreProcessSettings->m_InTransitDirectory + "\\" + FileToTest;

	CInTransit inTrans( inTransitFilename );

	if ( inTrans.Open ( false ) )
	{
		if ( inTrans.IsInTransit() )
			return false;
		else
			return true;
	}

	// InDirectory files are out of sequence...
	LogEvent(EVMSG_STRICT_SEQUENCING_INDIR_ERROR, m_PreProcessSettings->m_InTransitDirectory + "\\" + FileToTest);

	// If get this far, intransit file is not for file being tested
	// therefore something has got out of step, raise error
	// and shutdown
	m_ConfigFile->SetLinkToDisabled();

	return false;
}

void CPreProcess::ClearArray_ServiceSpecificSettings()
{
	m_PreProcessSettingsArray.SetSize(0);
}

void CPreProcess::New_ServiceSpecificSettings()
{
	m_PreProcessSettings = new CPreProcessSettings;
}

void CPreProcess::Add_ServiceSpecificSettings()
{
	m_PreProcessSettingsArray.Add( m_PreProcessSettings );
}

void CPreProcess::Set_ServiceSpecificSettings(int i)
{
	m_PreProcessSettings = (CPreProcessSettings*) m_PreProcessSettingsArray.GetAt(i);
}

void CPreProcess::Delete_ServiceSpecificSettings()
{
	delete m_PreProcessSettings;
}

void CPreProcess::Clear_ServiceSpecificSettings()
{
	for (int i = 0; i < m_PreProcessSettingsArray.GetSize(); i++)
		delete (CPreProcessSettings*) m_PreProcessSettingsArray.GetAt(i);

	m_PreProcessSettingsArray.RemoveAll();
}