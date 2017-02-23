// PostProcess.cpp: implementation of the CPostProcess class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "HuskMsg.h" // Event message ids
#include "ServHusk.h"
#include "PostProcess.h"
#include "Globals.h"
#include "CRC.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPostProcess::CPostProcess()
{
	m_PostProcessSettings = NULL;

	m_ServiceType = "PostProcess";

	m_readAndStoreWrapperBeforeProcessing = true;
	m_copyUnwrappedToWorking = true;
	m_restoreWrapperToOutputFile = false;
}

CPostProcess::~CPostProcess()
{
}

void CPostProcess::GetServiceSpecificSettings()
{
	m_PostProcessSettings->m_ArchiveDirectory = m_ConfigFile->ReadRegistryString("ArchiveDirectory", "");
	m_PostProcessSettings->m_ControlFileDirectory = m_ConfigFile->ReadRegistryString("ControlFileDirectory", m_ConfigFile->m_LinkRoot + "\\" + m_ServiceType + "\\Audit");

	CString TempString = m_ConfigFile->ReadRegistryString("DeliveredFileCreationDate", "Current");

	if ( TempString.CompareNoCase("Original") == 0 )
		m_PostProcessSettings->m_DeliveredOriginalDate = true;
	else
		m_PostProcessSettings->m_DeliveredOriginalDate = false; // default

	TempString = m_ConfigFile->ReadRegistryString("OverwriteFile", "OverwriteIfSame");

	if ( TempString.CompareNoCase("NoOverwrite") == 0 )
	{
		m_PostProcessSettings->m_OverwriteFile = m_PostProcessSettings->NoOverwrite;
	}
	else if ( TempString.CompareNoCase("AlwaysOverwrite") == 0 )
	{
		m_PostProcessSettings->m_OverwriteFile = m_PostProcessSettings->AlwaysOverwrite;
	}
	else // OverwriteIfSame - default
	{
		m_PostProcessSettings->m_OverwriteFile = m_PostProcessSettings->OverwriteIfSame;
	}

	m_PostProcessSettings->m_CheckCRC = m_ConfigFile->ReadRegistryBool("CheckCRC", "Yes");

	TempString = m_ConfigFile->ReadRegistryString("DeliveredFileName", "Original");

	if ( TempString.CompareNoCase("Current") == 0 )
		m_PostProcessSettings->m_DeliverOriginalFileName = false;
	else
		m_PostProcessSettings->m_DeliverOriginalFileName = true;

}

BOOL CPostProcess::CheckServiceSpecificSettings(bool Installing)
{
	if ( !m_PostProcessSettings->m_ControlFileDirectory.IsEmpty() )
	{
		if ( !CConfigSettings::CheckDirectory( this, "ControlFile", m_PostProcessSettings->m_ControlFileDirectory, CConfigSettings::required, Installing ) )
		{
			m_PostProcessSettings->m_WriteAuditInfo = false;
			return false;
		}
		else
			m_PostProcessSettings->m_WriteAuditInfo = true;
	}
	else
		m_PostProcessSettings->m_WriteAuditInfo = false;

	if ( !m_PostProcessSettings->m_ArchiveDirectory.IsEmpty() )
	{
		if ( !CConfigSettings::CheckDirectory( this, "ArchiveDirectory", m_PostProcessSettings->m_ArchiveDirectory, CConfigSettings::required, Installing ) )
		{
			m_PostProcessSettings->m_ArchiveData = false;
			return false;
		}
		else
			m_PostProcessSettings->m_ArchiveData = true;
	}
	else
		m_PostProcessSettings->m_ArchiveData = false;

	return true;
}

DWORD CPostProcess::ProcessFile(CString InPath, 
				 			    CString InFile, 
							    CString OutPath, 
							    CString OutFile)
{
	DebugMsg("ProcessFile: Obtaining wrapper info");

	// Get CRC info from wrapper -- the wrapper info
	// was aleady read from the file by ServiceHusk, above.
	DWORD WrapperCRC = atoi ( m_WrapperFile.GetWrapperCRC() );
	DWORD FileCRC = atoi ( m_WrapperFile.GetFileCRC() );

	// check the wrapper CRC
	DWORD uCRC = m_WrapperFile.GenerateWrapperCRC();

	if ( WrapperCRC != uCRC )
	{
		LogEvent(EVMSG_WRAPPER_CRC_MISMATCH);
		return EVMSG_WRAPPER_CRC_MISMATCH;
	}

	if ( m_PostProcessSettings->m_CheckCRC )
	{
		DebugMsg("Attempting to recalc file CRC");

		const CString fileName = OutPath + "\\" + OutFile;

		if ( !CalculateCrc( uCRC, fileName ) )
		{
			LogEvent(EVMSG_CRC_FAILED, fileName );
			return EVMSG_CRC_FAILED;
		}

		// store this calculated CRC
		SetDeliveredCRC( uCRC );

		if ( FileCRC != uCRC )
		{
			LogEvent(EVMSG_CRC_MISMATCH, fileName );
			return EVMSG_CRC_MISMATCH;
		}
	}

	return 0;
}

void CPostProcess::SuccessfulPath(CString InFile,
								  BOOL    IsWrapped,
								  CString OutFile,
								  CString OutStatusString)
{
	// rename file in working dir to original file name
	// stored in wrapper
	CString RenamedOutFile;
	CString AckFileName;

	DebugMsg("SuccesfulPath");

	CTime processedTime = CTime::GetCurrentTime();

	CString TempString = GetJustPath(OutFile);
	CString JustFileName;

	
	if ( m_PostProcessSettings->m_DeliverOriginalFileName )
	{
		JustFileName = GetJustFileName( m_WrapperFile.GetOriginalFileName() );
	}
	else
	{
		JustFileName = GetJustFileName(OutFile);

		// remove unique identifier
		JustFileName = RemoveUniqueId( JustFileName );

		JustFileName = DoSearchReplace( m_ConfigFile->m_OutRenameExp, JustFileName );
	}


	CString RenamedFile = TempString + "\\" + JustFileName;

	DebugMsg("About to rename file");
	if ( MoveFileEx(OutFile, RenamedFile, MOVEFILE_REPLACE_EXISTING ) == 0 )
	{
		LogEvent( EVMSG_RENAME_ORG_FAILED, OutFile, RenamedFile, GetErrorMessage( GetLastError(), true ) );
		return;
		
	}
	

	// set creation date using wrapper date

	// m_WrapperFile already contains file details


	m_WrapperFile.SetAssociatedFile(RenamedFile);

	// Set creation date of delivered file to creation date of original file
	if ( m_PostProcessSettings->m_DeliveredOriginalDate )
	{
		CString OriginalCreationDate = m_WrapperFile.GetDateStamp();

		if ( OriginalCreationDate.IsEmpty() || OriginalCreationDate.GetLength() != 14) 
		{
			LogEvent(EVMSG_WRAPPER_DATE, OriginalCreationDate, InFile);
		}
		else
		{
			int OrigYear = atoi( OriginalCreationDate.Mid(0, 4) );
			int OrigMonth = atoi( OriginalCreationDate.Mid(4, 2) );
			int OrigDay = atoi( OriginalCreationDate.Mid(6, 2) );
			int OrigHr = atoi( OriginalCreationDate.Mid(8, 2) );
			int OrigMin = atoi( OriginalCreationDate.Mid(10, 2) );
			int OrigSec = atoi( OriginalCreationDate.Mid(12, 2) );

			// Time stored in wrapper is UTC, convert to local time before
			// setting creation date
			CTime NewTime = ConvertUTCtoLocalTime(OrigYear, OrigMonth, OrigDay, OrigHr, OrigMin, OrigSec );

			CFileStatus FileStatus;


			CFile::GetStatus(RenamedFile, FileStatus);
			FileStatus.m_ctime = NewTime;
			FileStatus.m_mtime = NewTime;	// PinICL PC0037212, CP2354 - P Barber 01-02-2000
			CFile::SetStatus(RenamedFile, FileStatus);
		}
	}

	// move file from working dir to out dir


	RenamedOutFile = GetJustFileName(RenamedFile);
	RenamedOutFile = m_ConfigFile->m_OutDirectory + "\\" + RenamedOutFile;

	// Check "OverwriteFile" value from registry
	DWORD MoveFlags = MOVEFILE_COPY_ALLOWED;
	CFileStatus FileStatus;

	switch ( m_PostProcessSettings->m_OverwriteFile )
	{
	case m_PostProcessSettings->NoOverwrite:
		// no changes to flags required

		break;

	case m_PostProcessSettings->OverwriteIfSame:

		// Does file exist?
		if ( CFile::GetStatus(RenamedOutFile, FileStatus) )
		{
			DebugMsg( "%s: Target file exists and OverwriteIfSame set; checking if the same...",
				RenamedOutFile );

			if ( !IsDeliveredCRCvalid() )
			{
				// CRC not yet calculated - we need to do this
				DebugMsg( " -> calculating CRC for %s...", RenamedFile );
				
				DWORD tmp = 0;

				if ( CalculateCrc( tmp, RenamedFile ) )
					SetDeliveredCRC( tmp );
			}

			DWORD ExistingCRC = 0;

			DebugMsg( " -> calculating CRC for %s...", RenamedOutFile );

			if ( IsDeliveredCRCvalid() 
				&& CalculateCrc( ExistingCRC, RenamedOutFile ) 
				&& ( GetDeliveredCRC() == ExistingCRC ) )
			{
				
				DebugMsg("Deleting existing file %s", RenamedOutFile);

				if ( DeleteFile(RenamedOutFile) == 0 )
				{
					LogEvent(EVMSG_IN_DELETE_FAILED, RenamedOutFile, GetLastErrorString() );
				}

				MoveFlags |= MOVEFILE_REPLACE_EXISTING;
				LogEvent(EVMSG_OVERWRITE_SAME, RenamedOutFile);
			}
			else
			{
				DebugMsg(" -> CRC check failed, files differ or failure during check");
				LogEvent(EVMSG_OVERWRITE_DIFF, RenamedOutFile);

				// Send down standard failure path
				OutStatusString.Format("Unable to move %s to %s; different file already exists", OutFile, RenamedOutFile);
				FailurePath(InFile, IsWrapped, OutFile, OutStatusString);
				return;
			}
		}
		break;

	default:
		if ( CFile::GetStatus(RenamedOutFile, FileStatus) )
		{
			
			DebugMsg("Deleting existing file %s", RenamedOutFile);

			if ( DeleteFile(RenamedOutFile) == 0 )
			{
				LogEvent(EVMSG_IN_DELETE_FAILED, RenamedOutFile, GetLastErrorString() );
			}
		}

		MoveFlags |= MOVEFILE_REPLACE_EXISTING;
		break;
	}

	CString ArchiveFile;

	if ( m_PostProcessSettings->m_ArchiveData )
	{
		DebugMsg( "Archiving..." );

		// copy file to archive directory
		ArchiveFile = m_PostProcessSettings->m_ArchiveDirectory + "\\"
					+ GetJustFileName(RenamedFile) + '.'
					+ GetDateAndTimeAsString( processedTime );

		if ( !CopyFileCheck(RenamedFile, ArchiveFile) )
		{
			LogEvent( EVMSG_ARCHIVE_FAILURE, RenamedFile, ArchiveFile, GetErrorMessage( GetLastError(), true ));
			return;
		}
	}


	// do we need this message?
	DebugMsg( "Moving file %s -> %s", RenamedFile, RenamedOutFile );

	int Retries = 0;
	bool FileMovedSuccessfully = false;

	do
	{
		if ( Retries != 0 )
		{
			// wait for retry interval to expire
			DebugMsg( "RetryWait for %d ms starting...", m_ConfigFile->m_RetryWait );

			if ( m_shutDownEvent.Lock( m_ConfigFile->m_RetryWait ) )
			{
				// lock succeeded -- so shutdown IS requested!
				// exit loop without further retries
				break;
			}

			DebugMsg( "... RetryWait complete" );
		}


		if ( MoveFileEx(RenamedFile, RenamedOutFile, MoveFlags ) != 0 )
		{
			DebugMsg("File successfully moved %s -> %s", RenamedFile, RenamedOutFile);
			FileMovedSuccessfully = true;
			break;
		}
		else
		{
			if ( m_PostProcessSettings->m_OverwriteFile == m_PostProcessSettings->NoOverwrite
				&&
				GetLastError() == ERROR_ALREADY_EXISTS )
			{

				LogEvent(EVMSG_OVERWRITE_NO, RenamedOutFile);
			}
			else if (GetLastError() == ERROR_FILE_NOT_FOUND )
			{
				// In this case, we are unable to find the file we
				// want to move/copy
				// We will insert a 45 second wait in compliance
				// with what we now have for the ackhandler service
				// This hopefully will give Hummingbird enough time
				// to recover, and possibly find the file
				// 
				// If we still cant find the file after this wait, there
				// is nothing FTMS can do, than fail the transfer

				// Work around for PC0096724 - Version 16 25.08.2004

				DebugMsg("File un-successfully moved [completely] from %s -> %s: File in <path>\\ftmstmp not found", RenamedFile, RenamedOutFile);
				DebugMsg("Additional Retry Wait of 45 secs to find missing file - Allow Hummingbird etc settle down...");

				if ( m_shutDownEvent.Lock( 45000 ) )
				{
					// lock succeeded -- so shutdown IS requested!
					// exit loop without further retries
					break;
				}

				DebugMsg( "... Additional Wait period finished" );

			}
			else
			{
				
				
				//LogEvent(EVMSG_MOVE_OUT_FAILED, RenamedFile, RenamedOutFile, GetLastErrorString() );

			}

			Retries ++;

			OutStatusString = GetLastEventString();

			CString strRetries;
			strRetries.Format("%d", Retries);
			LogEvent(EVMSG_RETRY_WARNING, InFile, strRetries);

			// don't retry again if we're not running!
			if ( !m_ShutdownRequested )
				break;

		}

	} while (Retries < m_ConfigFile->m_RetryCount);

	if ( !FileMovedSuccessfully )
	{
		DebugMsg("FileMovedSuccessfully set to false");

		// Send down standard failure path
		OutStatusString.Format("Unable to move %s to %s: %s", OutFile, RenamedOutFile, GetLastErrorString());
		FailurePath(InFile, IsWrapped, OutFile, OutStatusString);
		return;
	}

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	// MILTON:
	// If we have no write access to the audit folder or file
	// we seem to loop indefinitely

	// Fix for PC0097255

	// NOTE: if NoOverwrite is set, the next transfer retry will fail
	// because it cant overwrite an already delivered file

	int AudRetries = 0;
	bool AuditWriteSuccessful = false;

	do
	{	
		if(AudRetries != 0)
		{
			
			DebugMsg("Audit Retry Count is %d: ", AudRetries);

			// wait for retry period to expire
			DebugMsg( "RetryWait for %d ms starting...", m_ConfigFile->m_RetryWait );

			if ( m_shutDownEvent.Lock( m_ConfigFile->m_RetryWait ) )
			{
				// lock succeeded -- so shutdown IS requested!
				// exit loop without further retries
				break;
			}

			DebugMsg( "... RetryWait complete" );
			
		}

		if ( m_PostProcessSettings->m_WriteAuditInfo )
		{
			DebugMsg( "Writing audit info..." );

			// write to audit file
			if ( !WriteAuditTrial( 
				GetJustPath(RenamedOutFile), 
				GetJustFileName(RenamedOutFile),
				processedTime,
				atoi(m_WrapperFile.GetFileCRC()),
				GetJustFileName( ArchiveFile ),
				GetJustPath( ArchiveFile ) ) )
			{

				DebugMsg( ".....Unable to write to audit file");
				LogEvent(EVMSG_AUDIT_REC_WRITE_FAILED, InFile, GetLastErrorString());
				// removes endless loop
				//return;
			}
			else
			{
				DebugMsg( "Writing Audit info successful...");
				AuditWriteSuccessful = true;
				break;
			}

			AudRetries ++;

			if(!m_ShutdownRequested)
				break;

		}
		
	}while (AudRetries < m_ConfigFile->m_RetryCount);


	if(!AuditWriteSuccessful)
	{
		DebugMsg("Unable to write to Audit File");
		DebugMsg("FileMovedSuccessfully set to false");

		// failure path
		OutStatusString.Format("Unable to write audit information for %s : %s", OutFile, GetLastErrorString());
		FailurePath(InFile, IsWrapped, OutFile, OutStatusString);
		return;
	}

	// end of  change
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////




	// Write successful event log message
	LogEvent(EVMSG_SUCCESS_DELIVERY, RenamedOutFile);

	// Create positive acknowledgement
	m_WrapperFile.SetSuccessStatus();



	RenamedFile = DoSearchReplace(m_ConfigFile->m_AckInRenameExp, m_WrapperFile.GetOriginalFileName() );
	JustFileName = GetJustFileName(RenamedFile);
	AckFileName.Format("%s\\%s.%s", m_ConfigFile->m_AckInDir, GetUniqueId( GetJustFileName(OutFile) ),
						JustFileName);

	CreateAckFile( AckFileName, RenamedOutFile, OutStatusString, true, ArchiveFile );

	// delete in file
	if ( DeleteFile(InFile) == 0 )	// Help says return no-zero if success, but i think it returns 0
	{
		LogEvent(EVMSG_IN_DELETE_FAILED, InFile, GetLastErrorString() );
		return;
	}

}

bool CPostProcess::WriteAuditTrial(const CString& InPath, 
								   const CString& InFile, 
								   const CTime& auditTime,
								   long crc,
								   const CString& ArchiveFile,
								   const CString& ArchivePath) const
{
	CAudit a;

	a.SetSourceID(m_ConfigFile->m_SourceSystemIdentity);
	a.SetTargetID(m_ConfigFile->m_TargetSystemIdentity);
	a.SetServiceID("FTMS");

	a.SetSystemID(m_ConfigFile->m_TargetSystemIdentity);
	a.SetAuditDirectory(m_PostProcessSettings->m_ControlFileDirectory);
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
	// added by Milton
	/*
	if( !a.CloseFile())
	{
		return TRUE;
	}
	*/

	//a.CloseFile();

	return TRUE;
}

void CPostProcess::ClearArray_ServiceSpecificSettings()
{
	m_PostProcessSettingsArray.SetSize(0);
}

void CPostProcess::New_ServiceSpecificSettings()
{
	m_PostProcessSettings = new CPostProcessSettings;
}

void CPostProcess::Add_ServiceSpecificSettings()
{
	m_PostProcessSettingsArray.Add( m_PostProcessSettings );
}

void CPostProcess::Set_ServiceSpecificSettings(int i)
{
	m_PostProcessSettings = (CPostProcessSettings*)	m_PostProcessSettingsArray.GetAt(i);
}

void CPostProcess::Delete_ServiceSpecificSettings()
{
	delete m_PostProcessSettings;
}

void CPostProcess::Clear_ServiceSpecificSettings()
{
	for (int i = 0; i < m_PostProcessSettingsArray.GetSize(); i++)
		delete (CPostProcessSettings*) m_PostProcessSettingsArray.GetAt(i);

	m_PostProcessSettingsArray.RemoveAll();
}