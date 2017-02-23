// AckHandler.cpp: implementation of the CAckHandler class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "InTransit.h"
#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "HuskMsg.h" // Event message ids
#include "ServHusk.h"
#include "AckHandler.h"
#include "Globals.h"
#include "Audit.h"

//////////////////////////////////////////////////////////////////////
// Constants
const char* c_ACK_ORIGINAL_DIR = "Original";	// PinICL PC0033290 (added c_ACK_ORIGINAL_DIR)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAckHandler::CAckHandler()
:CServiceHusk()
{
	m_AckHandlerSettings = NULL;

	m_ServiceType = "AckHandler";
}

CAckHandler::~CAckHandler()
{

}

void CAckHandler::GetServiceSpecificSettings()
{
	CString TempString;

	const char *defaultExp = ".*;&";

	m_AckHandlerSettings->m_SuccessSentDirectory = m_ConfigFile->ReadRegistryString("SuccessSentDirectory", m_ConfigFile->m_LinkRoot + "\\SuccessSent");
	m_AckHandlerSettings->m_SuccessSentRenameExp.Init( m_ConfigFile->ReadRegistryString("SuccessSentRenameExpression", defaultExp ) );

	m_AckHandlerSettings->m_UnsuccessSentDirectory = m_ConfigFile->ReadRegistryString("UnsuccessSentDirectory", m_ConfigFile->m_LinkRoot + "\\UnSuccessSent");
	m_AckHandlerSettings->m_UnsuccessSentRenameExp.Init( m_ConfigFile->ReadRegistryString("UnsuccessSentRenameExpression", defaultExp ) );

	m_AckHandlerSettings->m_SuccessfulAckDirectory = m_ConfigFile->ReadRegistryString("SuccessfulAckDirectory", m_ConfigFile->m_LinkRoot + "\\SuccessAck" );
	m_AckHandlerSettings->m_SuccessfulAckRenameExp.Init( m_ConfigFile->ReadRegistryString("SuccessfulAckRenameExpression", defaultExp ) );

	m_AckHandlerSettings->m_UnsuccessfulAckDirectory = m_ConfigFile->ReadRegistryString("UnsuccessfulAckDirectory", m_ConfigFile->m_LinkRoot + "\\UnSuccessAck");
	m_AckHandlerSettings->m_UnsuccessfulAckRenameExp.Init( m_ConfigFile->ReadRegistryString("UnsuccessfulAckRenameExpression", "(.*)\\.[aA][cC][kK];\\1.nak" ) );
	
	m_AckHandlerSettings->m_InTransitDirectory = m_ConfigFile->ReadRegistryString("InTransitDirectory", m_ConfigFile->m_LinkRoot + "\\PreProcess\\InTransit");

	TempString = m_ConfigFile->ReadRegistryString("AcknowledgementOverduePeriod", "120");

	m_AckHandlerSettings->m_OverduePeriod = CTimeSpan(0, 0, atoi(TempString), 0);

	m_AckHandlerSettings->m_endToEndRetryMax = m_ConfigFile->ReadRegistryInt("EndToEndRetryCount", "3" );

	m_AckHandlerSettings->m_ControlFileDirectory = m_ConfigFile->ReadRegistryString("ControlFileDirectory");
}

BOOL CAckHandler::CheckServiceSpecificSettings(bool Installing)
{
	if (0 != stricmp(m_AckHandlerSettings->m_SuccessSentDirectory, c_ACK_ORIGINAL_DIR))		// PinICL PC0033290
		if ( !CConfigSettings::CheckDirectory( this, "SuccessSentDirectory", m_AckHandlerSettings->m_SuccessSentDirectory, CConfigSettings::required, Installing ) )
			return FALSE;

	if (0 != stricmp(m_AckHandlerSettings->m_UnsuccessSentDirectory, c_ACK_ORIGINAL_DIR))	// PinICL PC0033290
		if ( !CConfigSettings::CheckDirectory( this, "UnsuccessSentDirectory", m_AckHandlerSettings->m_UnsuccessSentDirectory, CConfigSettings::required, Installing ) )
			return FALSE;

	if (0 != stricmp(m_AckHandlerSettings->m_UnsuccessfulAckDirectory, c_ACK_ORIGINAL_DIR))	// PinICL PC0033290
		if ( !CConfigSettings::CheckDirectory( this, "UnsuccessfulAckDirectory", m_AckHandlerSettings->m_UnsuccessfulAckDirectory, CConfigSettings::required, Installing ) )
			return FALSE;

	if (0 != stricmp(m_AckHandlerSettings->m_SuccessfulAckDirectory, c_ACK_ORIGINAL_DIR))	// PinICL PC0033290
		if ( !CConfigSettings::CheckDirectory( this, "SuccessAckDirectory", m_AckHandlerSettings->m_SuccessfulAckDirectory, CConfigSettings::required, Installing ) )
			return FALSE;

	if ( !CConfigSettings::CheckDirectory( this, "InTransitDirectory", m_AckHandlerSettings->m_InTransitDirectory, CConfigSettings::required, Installing ) )
		return FALSE;

	// check if the regular expressions are OK!
	if ( !CheckValidRegEx( m_AckHandlerSettings->m_SuccessSentRenameExp, "SuccessSentRenameExpression" ) )
		return false;

	if ( !CheckValidRegEx( m_AckHandlerSettings->m_UnsuccessSentRenameExp, "UnsuccessSentRenameExpression" ) )
		return false;

	if ( !CheckValidRegEx( m_AckHandlerSettings->m_SuccessfulAckRenameExp, "SuccessfulAckRenameExpression" ) )
		return false;

	if( !CheckValidRegEx( m_AckHandlerSettings->m_UnsuccessfulAckRenameExp, "UnsuccessfulAckRenameExpression" ) )
		return false;

	if ( !m_AckHandlerSettings->m_ControlFileDirectory.IsEmpty() )
	{
		if ( !CConfigSettings::CheckDirectory( this, "ControlFile", m_AckHandlerSettings->m_ControlFileDirectory, CConfigSettings::required, Installing ) )
		{
			m_AckHandlerSettings->m_WriteAuditInfo = false;
			return false;
		}
		else
			m_AckHandlerSettings->m_WriteAuditInfo = true;
	}
	else
		m_AckHandlerSettings->m_WriteAuditInfo = false;

	return TRUE;
}

DWORD CAckHandler::ProcessFile(CString InPath, CString InFile, 
							  CString OutPath, CString OutFile)
{
	// These two variables control where we should
	// put the sent file - the directory and the rename expression.
	// These change based on whether its a NACK or an ACK.
	CString sentDirectory;
	CSearchAndReplace *sentRenameExp = 0;

	// Similarly these control where the acknowledgement should be placed,
	// and what it should be called.
	CString ackDirectory;
	CSearchAndReplace *ackRenameExp = 0;

	const CString fullInFileName = InPath + "\\" + InFile;

	// Is file an acknowledgement
	DebugMsg("Check if this file is an ack file");
	CAcknowledgement ackFile;

	ackFile.Initialise(fullInFileName);

	if ( !ackFile.IsAcknowledgement() )
	{
		DebugMsg("File not an acknowledgement %s", fullInFileName);
		LogEvent( EVMSG_ACK_NOT, fullInFileName );
		return EVMSG_ACK_NOT;
	}

	ackFile.StoreOriginalFileDetails();

	if ( !ackFile.IsPositiveAck() && !ackFile.IsNegativeAck() )
	{
		// not a valid acknowledgment
		DebugMsg("File not a valid acknowledgement %s", fullInFileName);
		LogEvent(EVMSG_ACK_CORRUPT, fullInFileName);
		return EVMSG_ACK_CORRUPT;
	}

	// locate the intransit file
	CString originalFilename = ackFile.GetOriginalFileName();
	CString inTransitFilename = 
		m_AckHandlerSettings->m_InTransitDirectory + "\\" + originalFilename;

	CInTransit i ( inTransitFilename );

	bool InTransitFileError = false;

	if ( !i.Open() || !i.IsInTransit() )
	{
		InTransitFileError = true;
	}

	// Is this the correct acknowledgement for file in transit

	CString InProgressFileName = i.GetInProgressFileName();

	// If FTMS is unable to write to the audit file due to insufficient permissions, the In Progress File Name 
	// value in the intransit file will not be set. 
	// This means that the variable InProgressFileName [above] will be blank. 
	// By implication the GetUniqueID check below will fail, and an Unexpected Ack message
	// will be generated. From an operations point of view, warnings will be written to the 
	// event log, and only an Ack overdue error will signal the failure of the transferred file
	// As a work around, if InProgressFileName is blank, FTMS will proceed to generate a proper
	// NACK ....and thereby exhaust its retry count before reporting a failed transfer.

	DebugMsg( "Progress File Name - %s ", InProgressFileName);

	if (InProgressFileName != "")
	{

		if ( GetUniqueId( GetJustFileName(fullInFileName) ) != GetUniqueId ( InProgressFileName ) 
			|| InTransitFileError )
		{
			// Unexcepted acknowledgement received
			DebugMsg("Unexpected Acknowledgement received %s", fullInFileName);
			LogEvent(EVMSG_ACK_NO_MATCHING_FILE, fullInFileName);

			// Move file from in dir to error dir
			// rename if required
			CString ErrorFileName = DoSearchReplace(m_ConfigFile->m_ErrorRenameExp, fullInFileName );
			CString JustFileName = GetJustFileName(ErrorFileName);
			CString TempString;
			TempString.Format("%s\\%s", m_ConfigFile->m_ErrorDirectory, JustFileName);

			DebugMsg( "Moving to error directory, %s -> %s...", fullInFileName, TempString  );

			if ( !CopyFile(fullInFileName, TempString, FALSE ) )
			{
				LogEvent(EVMSG_COPY_FAILED, fullInFileName, TempString,
					GetErrorMessage( GetLastError(), true ) );
				return EVMSG_COPY_FAILED;

			}

			// return success, no need to re-try
			return 0;
		}
	}
	
	if ( ackFile.IsNegativeAck() )
	{
		char tmpBuf[30];

		DebugMsg("NACK: %s", InFile);

		// should we retry this file?
		i.IncrementAttemptCount();

		if ( i.GetAttemptCount() < m_AckHandlerSettings->m_endToEndRetryMax )
		{
			// yep, we need to retry!
			i.SetInTransit( false );

			LogEvent( EVMSG_ENDTOEND_RETRY, EVCAT_ENDTOEND,
				originalFilename, _itoa( i.GetAttemptCount(), tmpBuf, 10 ),
				ackFile.m_FailureReason );

			i.Write();

			if ( m_ConfigFile->IsStrictSequencing() )
			{
				CreateStrictSequenceFile(SEND_NEXT_FILE);
			}

			// return success
			return 0;
		}

		LogEvent( EVMSG_ENDTOEND_FAILED, EVCAT_ENDTOEND,
			originalFilename, _itoa( i.GetAttemptCount(), tmpBuf, 10 ),
			ackFile.m_FailureReason );

		sentDirectory = m_AckHandlerSettings->m_UnsuccessSentDirectory;
		sentRenameExp = &m_AckHandlerSettings->m_UnsuccessSentRenameExp;
		ackDirectory = m_AckHandlerSettings->m_UnsuccessfulAckDirectory;
		ackRenameExp = &m_AckHandlerSettings->m_UnsuccessfulAckRenameExp;
	}
	else
	{
		DebugMsg("ACK: %s", InFile );

		LogEvent( EVMSG_ENDTOEND_SUCCESS, EVCAT_ENDTOEND,
			originalFilename );

		sentDirectory = m_AckHandlerSettings->m_SuccessSentDirectory;
		sentRenameExp = &m_AckHandlerSettings->m_SuccessSentRenameExp;
		ackDirectory = m_AckHandlerSettings->m_SuccessfulAckDirectory;
		ackRenameExp = &m_AckHandlerSettings->m_SuccessfulAckRenameExp;
	}

	// make text copy of ack in relevant dir
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Changes made on 23/09/99, by Milton Ezeh
	//
	// Purpose: To return acknowledgements to the directory from which the data file was picked up.
	//
	// Line Removed: see below
	// CString RenamedFileName = ackDirectory + "\\" + DoSearchReplace( *ackRenameExp, RemoveUniqueId( InFile ) );
	//
	// Objective: The code will read the registry for the following values
	//
	// 1. SuccessAckDirectory
	// 2. UnSuccessAckDirectory
	// 3. SuccessSentDirectory
	// 4. UnSuccessSentDirectory
	//
	// If all these values are set to "Original" (c_ACK_ORIGINAL_DIR) the code will deliver the corresponding files
	// to the directory where the files originated. i.e. The InDirectory of Pre-Process
	// If the values are anything other than "Original" (c_ACK_ORIGINAL_DIR), the files are sent to the directories 
	// specified.
	///////////////////////////////////////////////////////////////////////////////////////////////


	/////////////////////////
	// code change
	// another change to make sure that acks are first written to a tmp file
	CString RenamedFileName;
	CString tmpRnFile;
	
	if(_stricmp(ackDirectory, c_ACK_ORIGINAL_DIR) == 0)		// PinICL PC0033290 (added c_ACK_ORIGINAL_DIR)
	{
		CString InDirectory = i.GetPath();
		RenamedFileName = InDirectory + "\\" + DoSearchReplace( *ackRenameExp, RemoveUniqueId( InFile ) );

		//tmpRnFile = InDirectory + "\\" + DoSearchReplace( *ackRenameExp, RemoveUniqueId( InFile ) ) + ".tmp";
		//tmpRnFile = InDirectory + "\\" + DoSearchReplace( *ackRenameExp, RemoveUniqueId( InFile ) ) + "tmp";

		// Create a temporary ack file name that wouldnt be picked up by APS_MONITOR. Rename once finished
		// Addressing PINICLS 91108, 91109, 90743, 91153 & 90024
		tmpRnFile = InDirectory + "\\ftmstempahfile.text.tmp";
	}
	else
	{
		RenamedFileName = ackDirectory + "\\" + DoSearchReplace( *ackRenameExp, RemoveUniqueId( InFile ) );
		//tmpRnFile = ackDirectory + "\\" + DoSearchReplace( *ackRenameExp, RemoveUniqueId( InFile ) ) + ".tmp";
		
		// create temporary ack file  
		tmpRnFile = ackDirectory + "\\ftmstempahfile.text.tmp";
	}
	// end of change
	/////////////////////////


	DebugMsg("Writing text acknowledgement ticket: %s", RenamedFileName);

	if ( !ackFile.TextFileWrite(tmpRnFile) )
	{
		LogEvent( EVMSG_ACK_FAILED_TEXT, tmpRnFile);
		return EVMSG_ACK_FAILED_TEXT;
	}

	// Write dupicate audit record as written by PostProcess at the
	// target end. This is done so that Pathway do not have to FTP to
	// the target platform to obtain audit information.
	if ( m_AckHandlerSettings->m_WriteAuditInfo && ackFile.IsPositiveAck() )
	{
		if ( !WriteDuplicatePostProcessAuditRecord(fullInFileName) )
		{
			LogEvent(EVMSG_AUDIT_REC_WRITE_FAILED, fullInFileName, GetLastErrorString());
			return EVMSG_AUDIT_REC_WRITE_FAILED;
		}
	}

	CString originalFilepath = i.GetPath() + "\\" + originalFilename;

	//////////////////
	// another change

	CString renamedFileName;
	if(_stricmp(sentDirectory, c_ACK_ORIGINAL_DIR) == 0)	// PinICL PC0033290 (added c_ACK_ORIGINAL_DIR)
	{
		CString InDirectory = i.GetPath();
		renamedFileName = InDirectory + "\\" + DoSearchReplace( *sentRenameExp, originalFilename );
	}
	else
	{
		renamedFileName = sentDirectory + "\\" + DoSearchReplace( *sentRenameExp, originalFilename );
	}

	// end of change
	/////////////////


	// ///////////////////////////////////////////
	// GM - 2000/07/05
	// PC0038897 part 1:
	//	1. If the directory into which we are tring to move the file
	//	(SuccessSent/UnSuccessSent) exists but the reason the file cant be moved is
	//	because it no longer exists in the InDirectory then do one of the following:
	//	- If the file has already been moved to the SuccessSent/UnSuccessSent
	//	directory then procede and delete the intransit file but log a warning
	//	stating that "The file <file name> was not found in the InDirectory as
	//	expected"
	//	- If the file is NOT in the SuccessSent/UnSuccessSent directory then procede
	//	and delete the intransit file but log an Error and fail the file stating that
	//	"The file <file name> was not found in the InDirectory as expected and
	//	appears to be lost". If it is strict sequencing then shut the link down in
	//	the normal way for a strict sequencing failure.


	//////////////////////////////////
	// rename file back to proper name
	//////////////////////////////////

	//CFile::Rename( tmpRnFile, RenamedFileName);

	DebugMsg("Renaming temp file. Rename %s to %s", tmpRnFile, RenamedFileName);

	if ( !::MoveFileEx(tmpRnFile, RenamedFileName,
		 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) )
	{
		LogEvent(EVMSG_SENT_FAILED, tmpRnFile, RenamedFileName, GetLastErrorString() );
		return EVMSG_SENT_FAILED;
	}

	// check if originalFilepath exists
	bool bOriginalFilepath = false;
	CFileFind FindOriginalFile;
	if ( FindOriginalFile.FindFile(originalFilepath, 0) )
			bOriginalFilepath = true;

	// check if renamedFileName already exists
	bool bRenamedFileNameExists = false;
	CFileFind FindRenamedFile;
	if ( FindRenamedFile.FindFile(renamedFileName, 0) )
			bRenamedFileNameExists = true;

	// part 1a
	// The InFile doesn't exist but a copy is present in the SuccSentDir
	if ( (!bOriginalFilepath) && (bRenamedFileNameExists) )
	{
		DebugMsg( "The orignal file path %s doesn't exist but a copy does exist in the (succsentdir) %s.",
			originalFilepath,
			renamedFileName );
		DebugMsg( "The file %s was not found in the InDirectory as expected.",
			originalFilepath );

		// log an event so we keep the user informed
		LogEvent( EVMSG_ACK_NOINFILE_ALREADYHAVESUCCSENTFILE, originalFilepath );

		// check and set strict sequencing
		if ( m_ConfigFile->IsStrictSequencing() )
		{
			if ( ackFile.IsPositiveAck() )
			{
				CreateStrictSequenceFile(SEND_NEXT_FILE);
			}
			else
			{
				CreateStrictSequenceFile(FAILURE_FILE);

				LogEvent(EVMSG_STRICT_SEQUENCING_ACK_ERROR, ackFile.m_FailureReason);

				m_ConfigFile->SetLinkToDisabled();
			}
		}

		// delete the InTransit file
		DebugMsg("Attempting to delete InTransit file");
		if ( !i.Delete() )
		{
			LogEvent(EVMSG_INTRANSIT_DEL_FAILED, inTransitFilename, GetLastErrorString());
			return EVMSG_INTRANSIT_DEL_FAILED;
		}

		return 0;
	}

	// part 1b
	// The InFile doesn't exist or does the sentFile.


	//********************************************************************************
	// ADDED BY MILTON - 14th July, 2004
	// As the code stands, there are no retry attempts if the original file or renamed
	// file go missing. 
	// CODE CHANGE FOR VERSION 16, will remove the return statement and allow
	// for retries. This will however apply to links with No strict sequencing
	// If strict sequencing is enabled, the link will be shut down
	// Fix for PC0103213
	//********************************************************************************

	if ( (!bOriginalFilepath) && (!bRenamedFileNameExists) )
	{
		DebugMsg( "The orignal file path %s doesn't exist and theres is no copy either in the (succsentdir) %s.",
			originalFilepath,
			renamedFileName );
		DebugMsg( "The file %s was not found in the InDirectory as expected and appears to be lost.",
			originalFilepath );

		// log an event so we keep the user informed
		LogEvent( EVMSG_ACK_NOINFILE_NOSUCCSENTFILE, originalFilepath );

		// set strict sequencing failure 

		/* ----------------------------
		// CHANGE: PC0081256 & 0080783
		// If both directories become unavailable
		// we should not be deleting the intransit
		// file if there is non strict
		// sequencing on the link
		// It leads to a duplicate transfer if the 
		// directories come back to life
		-------------------------------*/

		if ( m_ConfigFile->IsStrictSequencing() )
		{
			CreateStrictSequenceFile(FAILURE_FILE);

			LogEvent(EVMSG_STRICT_SEQUENCING_ACK_ERROR, ackFile.m_FailureReason);

			m_ConfigFile->SetLinkToDisabled();

			DebugMsg("Attempting to delete InTransit file");
			if ( !i.Delete() )
			{
				LogEvent(EVMSG_INTRANSIT_DEL_FAILED, inTransitFilename, GetLastErrorString());
				return EVMSG_INTRANSIT_DEL_FAILED;
			}
			// add a return here
			return 0;
		}

		/* move this bit of code into the strict sequencing section above
		// delete the InTransit file
		DebugMsg("Attempting to delete InTransit file");
		if ( !i.Delete() )
		{
			LogEvent(EVMSG_INTRANSIT_DEL_FAILED, inTransitFilename, GetLastErrorString());
			return EVMSG_INTRANSIT_DEL_FAILED;
		}
		*/

		// removed
		//return 0;
		// removed  
	}
	// GM - 2000/07/05
	// ///////////////////////////////////////////

	// ///////////////////////////////////////////
	// GM - 2000/07/05
	// PC0038897 part 2
	//	2. If the ackhandler is unable to move the file because the directory
	//	(SuccessSent/UnSuccessSent) into which we are trying to move the file does
	//	not exist then fail the file and disable the link. This is consistent with
	//	the action we take if we try to start the service and one of the directories
	//	is unavailable.

	DebugMsg( "Check that the following [un]succ. sent dir. exists...%s",  GetJustPath(renamedFileName) );
	if (!IsDirectory(GetJustPath(renamedFileName)))
	{
		DebugMsg( "Renaming original file %s to %s but directory %s doesn't exist.", 
			originalFilepath, 
			renamedFileName, 
			GetJustPath(renamedFileName) );
		
		if ( m_ConfigFile->IsStrictSequencing() )
		{
			CreateStrictSequenceFile(FAILURE_FILE);

			LogEvent(EVMSG_STRICT_SEQUENCING_ACK_ERROR, ackFile.m_FailureReason);
		}

		m_ConfigFile->SetLinkToDisabled();
//		Fix Incorrect event log message. P.Shah PC0038897 part 2
//		LogEvent( EVMSG_ACK_NOSUCCSENTDIREXISTS, originalFilepath );
		LogEvent( EVMSG_ACK_NOSUCCSENTDIREXISTS, GetJustPath(renamedFileName) );
		return EVMSG_ACK_NOSUCCSENTDIREXISTS;
	}
	// GM - 2000/07/05
	// ///////////////////////////////////////////

	
	DebugMsg("Renaming original file. Move %s to %s", originalFilepath, renamedFileName);

	if ( !::MoveFileEx(originalFilepath, renamedFileName,
		 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) )
	{
		LogEvent(EVMSG_SENT_FAILED, originalFilepath, renamedFileName, GetLastErrorString() );
		return EVMSG_SENT_FAILED;
	}

	if ( m_ConfigFile->IsStrictSequencing() )
	{
		if ( ackFile.IsPositiveAck() )
		{
			CreateStrictSequenceFile(SEND_NEXT_FILE);
		}
		else
		{
			CreateStrictSequenceFile(FAILURE_FILE);

			LogEvent(EVMSG_STRICT_SEQUENCING_ACK_ERROR, ackFile.m_FailureReason);

			m_ConfigFile->SetLinkToDisabled();
		}
	}

	// Moved P Barber 21-02-2000.  Ref: PinICL PC0038897
	// remove InTransit file
	DebugMsg("Attempting to delete InTransit file");
	if ( !i.Delete() )
	{
		LogEvent(EVMSG_INTRANSIT_DEL_FAILED, inTransitFilename, GetLastErrorString());
		return EVMSG_INTRANSIT_DEL_FAILED;
	}

	return 0;
}

void CAckHandler::SuccessfulPath(CString InFile,
								 BOOL    IsWrapped,
								 CString OutFile,
								 CString OutStatusString)
{
	// when the AckHandler has successfully processed the files,
	// we need to delete the incoming file!
	
	DebugMsg( "SuccessfulPath: Deleting file %s", InFile );

	if ( !::DeleteFile( InFile ) )
	{
		LogEvent(EVMSG_IN_DELETE_FAILED, InFile, GetLastErrorString() );
	}

	return;
}

void CAckHandler::CheckOverdueFiles()
{
	if ( m_AckHandlerSettings->m_OverduePeriod.GetTotalSeconds() == 0 )
		return;

	CStringList CompleteFileList;

	ListAllFilesInDir( &CompleteFileList, m_AckHandlerSettings->m_InTransitDirectory );

    for( POSITION CurrentPosition = CompleteFileList.GetHeadPosition(); CurrentPosition; )
    {
		const CString fileName = CompleteFileList.GetNext(CurrentPosition).Mid(14);

		// now check the contents of the intransit file
		CInTransit i( fileName );

		if ( ! i.Open() )
		{
			// open failed for some reason -- perhaps PreProcess is still doing something?
			// Just ignore, it should sort itself out next time round.

			// go on to the next file round the loop
			continue;
		}

		if ( ! i.IsInTransit() )
		{
			// file not in transit, we have nothing to do with it.
		
			// go on to the next file round the loop
			continue;
		}

		CTime lastAttempt = i.GetLastAttemptStartTime();

		if ( lastAttempt + m_AckHandlerSettings->m_OverduePeriod >= CTime::GetCurrentTime() )
		{
			// file is not overdue; ignore
			continue;
		}

		// Get original file name from intransit file
		CString fn = i.GetFilename();
		LogEvent(EVMSG_OVERDUE_FILE, fn );

		CString originalFilepath = i.GetPath() + "\\" + fn;
		CString renamedFileName;

		// Added P Barber 21-02-2000.  Ref: PinICL PC0038897
		// check for original in UnsuccessSentDirectory, and only rename file.
		if(_stricmp(m_AckHandlerSettings->m_UnsuccessSentDirectory, c_ACK_ORIGINAL_DIR) == 0)
		{
			renamedFileName =	i.GetPath()
								+ "\\"
								+ DoSearchReplace( m_AckHandlerSettings->m_UnsuccessSentRenameExp, fn );
		}
		else
		{
			renamedFileName =	m_AckHandlerSettings->m_UnsuccessSentDirectory
								+ "\\"
								+ DoSearchReplace( m_AckHandlerSettings->m_UnsuccessSentRenameExp, fn );
		}

		// ///////////////////////////////////////////
		// PS - 22/11/2000
		// PC0038897 part 1:
		//	1. If the directory into which we are tring to move the file
		//	(SuccessSent/UnSuccessSent) exists but the reason the file cant be moved is
		//	because it no longer exists in the InDirectory then do one of the following:
		//	- If the file has already been moved to the SuccessSent/UnSuccessSent
		//	directory then procede and delete the intransit file but log a warning
		//	stating that "The file <file name> was not found in the InDirectory as
		//	expected"
		//	- If the file is NOT in the SuccessSent/UnSuccessSent directory then procede
		//	and delete the intransit file but log an Error and fail the file stating that
		//	"The file <file name> was not found in the InDirectory as expected and
		//	appears to be lost". If it is strict sequencing then shut the link down in
		//	the normal way for a strict sequencing failure.

		// check if originalFilepath exists
		bool bOriginalFilepath = false;
		CFileFind FindOriginalFile;
		if ( FindOriginalFile.FindFile(originalFilepath, 0) )
				bOriginalFilepath = true;

		// check if renamedFileName already exists
		bool bRenamedFileNameExists = false;
		CFileFind FindRenamedFile;
		if ( FindRenamedFile.FindFile(renamedFileName, 0) )
				bRenamedFileNameExists = true;

		// part 1a
		// The InFile doesn't exist but a copy is present in the SuccSentDir
		if ( (!bOriginalFilepath) && (bRenamedFileNameExists) )
		{
			DebugMsg( "The orignal file path %s doesn't exist but a copy does exist in the unsuccess sent directory %s.",
				originalFilepath,
				renamedFileName );
			DebugMsg( "1 The file %s was not found in the InDirectory as expected.",
				originalFilepath );

			// log an event so we keep the user informed 
			LogEvent( EVMSG_ACK_NOINFILE_ALREADYHAVEUNSUCCSENTFILE, originalFilepath );
			// delete the InTransit file
			DebugMsg("Attempting to delete InTransit file");
			if ( !i.Delete() )
			{
				LogEvent(EVMSG_INTRANSIT_DEL_FAILED, fileName, GetLastErrorString());
			}
			return;
		}

		// part 1b
		// The InFile doesn't exist nor does the sentFile.
		if ( (!bOriginalFilepath) && (!bRenamedFileNameExists) )
		{
			DebugMsg( "The orgiinal file path %s doesn't exist and theres is no copy in the unsuccess directory %s.",
				originalFilepath,
				renamedFileName );
			DebugMsg( "2 The file %s was not found in the InDirectory as expected and appears to be lost.",
				originalFilepath );

			// log an event so we keep the user informed
			LogEvent( EVMSG_ACK_NOINFILE_NOUNSUCCSENTFILE, originalFilepath );


			// delete the InTransit file
			DebugMsg("Attempting to delete InTransit file");
			if ( !i.Delete() )
			{
				LogEvent(EVMSG_INTRANSIT_DEL_FAILED, fileName, GetLastErrorString());
			}

			// set strict sequencing failure 
			if ( m_ConfigFile->IsStrictSequencing() )
			{
				CreateStrictSequenceFile(FAILURE_FILE);
				LogEvent(EVMSG_STRICT_SEQUENCING_ACK_ERROR, fn);
				m_ConfigFile->SetLinkToDisabled();
			}

			return ;
		}
		// ///////////////////////////////////////////
		// PS - 22/11/2000
		// PC0038897 part 2
		//	2. If the ackhandler is unable to move the file because the directory
		//	(SuccessSent/UnSuccessSent) into which we are trying to move the file does
		//	not exist then fail the file and disable the link. This is consistent with
		//	the action we take if we try to start the service and one of the directories
		//	is unavailable.

		DebugMsg( "Check that the following [un]succ. sent dir. exists...%s",  GetJustPath(renamedFileName) );
		if (!IsDirectory(GetJustPath(renamedFileName)))
		{
			DebugMsg( "Renaming original file %s to %s but directory %s doesn't exist.", 
				originalFilepath, 
				renamedFileName, 
				GetJustPath(renamedFileName) );
			
			if ( m_ConfigFile->IsStrictSequencing() )
			{
				CreateStrictSequenceFile(FAILURE_FILE);
				LogEvent(EVMSG_STRICT_SEQUENCING_ACK_ERROR, fn);
			}

			// delete the InTransit file
			DebugMsg("Attempting to delete InTransit file");
			if ( !i.Delete() )
			{
				LogEvent(EVMSG_INTRANSIT_DEL_FAILED, fileName, GetLastErrorString());
			}

			m_ConfigFile->SetLinkToDisabled();

			LogEvent( EVMSG_ACK_NOSUCCSENTDIREXISTS, GetJustPath(renamedFileName) );
			return ;
		}
///////////////////////////////////////////////////
		
		// move failed file into unsuccessful sent directory
		DebugMsg("Renaming original file. Move %s to %s", originalFilepath, renamedFileName);

		if ( !::MoveFileEx(originalFilepath, renamedFileName,
			 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) )
		{
			LogEvent(EVMSG_SENT_FAILED, originalFilepath, renamedFileName, GetLastErrorString() );
			// Added P Barber 21-02-2000.  Ref: PinICL PC0038897
			// return from function so that InTransit file is not deleted

			// P.Shah 23-11-2000 Ref: PinICL PC0038897
			// Need to delete the intransit file on overdue ack regardless of whether we can
			// move it to unsuccesssent directory. Hence commented out P.Barber bug fix

//			return;
		}

		// Added P Barber 21-02-2000.  Ref: PinICL PC0038897
		// disable link if strict sequencing
		if ( m_ConfigFile->IsStrictSequencing() )
		{
			CreateStrictSequenceFile(FAILURE_FILE);
			LogEvent(EVMSG_STRICT_SEQUENCING_ERROR, fn);
			m_ConfigFile->SetLinkToDisabled();
		}

		// remove InTransit file
		DebugMsg("Attempting to delete InTransit file");
		if ( !i.Delete() )
		{
			LogEvent(EVMSG_INTRANSIT_DEL_FAILED, fileName, GetLastErrorString());
			return;
		}
	}
}

bool CAckHandler::WriteDuplicatePostProcessAuditRecord(const CString& AckFileName)
{
	CAudit a;

	a.SetSourceID(m_ConfigFile->m_SourceSystemIdentity);
	a.SetTargetID(m_ConfigFile->m_TargetSystemIdentity);
	a.SetServiceID("FTMS");

	a.SetSystemID(m_ConfigFile->m_TargetSystemIdentity);
	a.SetAuditDirectory(m_AckHandlerSettings->m_ControlFileDirectory);

	if ( !a.OpenFile() )
	{
		DebugMsg("Failed to open audit file");
		return false;
	}

	CAcknowledgement ack;

	ack.Initialise(AckFileName);
	ack.StoreOriginalFileDetails();

	CString strTimeProcessed = ack.GetDeliveredDateStamp();// this is processed time
	CString FileName = ack.GetDeliveredFileName();
	CString Path = ack.GetDeliveredPath();
	CString MachineID = ack.GetDeliveredMachineID();
	CString strCreationTime = ack.GetOriginalDateStamp();
	long FileSize = atoi( ack.GetDeliveredFileSize() );
	long FileCRC = atoi ( ack.GetDeliveredCRC() );
	CString ArchiveFile = GetJustFileName( ack.GetArchiveFileName() );
	CString ArchivePath = GetJustPath( ack.GetArchiveFileName() );

	CTime TimeProcessed = ConvertUTCtoLocalTime( atoi( strTimeProcessed.Mid(0, 4) ),
												 atoi( strTimeProcessed.Mid(4, 2) ),
												 atoi( strTimeProcessed.Mid(6, 2) ),
												 atoi( strTimeProcessed.Mid(8, 2) ),
												 atoi( strTimeProcessed.Mid(10, 2) ),
												 atoi( strTimeProcessed.Mid(12, 2) ) );

	CTime FileCreationTime = ConvertUTCtoLocalTime( atoi( strCreationTime.Mid(0, 4) ),
												    atoi( strCreationTime.Mid(4, 2) ),
													atoi( strCreationTime.Mid(6, 2) ),
													atoi( strCreationTime.Mid(8, 2) ),
													atoi( strCreationTime.Mid(10, 2) ),
													atoi( strCreationTime.Mid(12, 2) ) );

	if ( !a.WriteRecord(TimeProcessed, FileName, Path, MachineID, FileCreationTime, FileSize, FileCRC,
		ArchiveFile, ArchivePath ) )
	{
		DebugMsg("Failed to write audit record");
		return false;
	}

	a.CloseFile();

	return true;
}

void CAckHandler::CreateStrictSequenceFile(const CString & File)
{
	CFile SendNextFile;

	UINT flags = CFile::modeReadWrite | CFile::shareDenyNone
			   | CFile::modeCreate    | CFile::modeNoTruncate;

	if ( SendNextFile.Open( m_AckHandlerSettings->m_InTransitDirectory + "\\" + File, flags ) == 0 )
	{
		DebugMsg("Failed to create OkToSend");
		LogEvent(EVMSG_STRICT_SEQ_FILE_ERROR, m_AckHandlerSettings->m_InTransitDirectory + "\\" + File,
			     GetErrorMessage( GetLastError(), true ) );

		LogEvent(EVMSG_STRICT_SEQUENCING_ERROR, m_AckHandlerSettings->m_InTransitDirectory + "\\" + File );

		m_ConfigFile->SetLinkToDisabled();
		return;
	}
}

void CAckHandler::ClearArray_ServiceSpecificSettings()
{
	m_AckHandlerSettingsArray.SetSize(0);
}

void CAckHandler::New_ServiceSpecificSettings()
{
	m_AckHandlerSettings = new CAckHandlerSettings;
}

void CAckHandler::Add_ServiceSpecificSettings()
{
	m_AckHandlerSettingsArray.Add( m_AckHandlerSettings );
}

void CAckHandler::Set_ServiceSpecificSettings(int i)
{
	m_AckHandlerSettings = (CAckHandlerSettings*) m_AckHandlerSettingsArray.GetAt(i);
}

void CAckHandler::Delete_ServiceSpecificSettings()
{
	delete m_AckHandlerSettings;
}

void CAckHandler::Clear_ServiceSpecificSettings()
{
	for ( int i = 0; i < m_AckHandlerSettingsArray.GetSize(); i++ )
	{
		delete (CAckHandlerSettings*) m_AckHandlerSettingsArray.GetAt(i);
	}

	m_AckHandlerSettingsArray.RemoveAll();
}

void CAckHandler::Initialize_ServiceSpecificSettings()
{
	m_ConfigFile->m_outDirSetting = CConfigSettings::notRequired;
}