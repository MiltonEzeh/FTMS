// HouseKeeper.cpp: implementation of the CHouseKeeper class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "HuskMsg.h" // Event message ids
#include "ServHusk.h"
#include "HouseKeeper.h"
#include "Globals.h"
#include "CRC.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHouseKeeper::CHouseKeeper()
:CServiceHusk()
{
	m_ServiceType = "HouseKeeper";

	m_HouseKeeperSettings = NULL;
}

CHouseKeeper::~CHouseKeeper()
{

}

void CHouseKeeper::GetServiceSpecificSettings()
{
	m_HouseKeeperSettings->m_DeletionParameters = m_ConfigFile->ReadRegistryString("DeletionParameters");
}

BOOL CHouseKeeper::CheckServiceSpecificSettings(bool Installing)
{
	// Added P Barber 08-12-1999 - PinICL PC0033873
	// Clear m_HouseKeeperSettings->m_ParamList before adding 
	// Deletion Parameters.  The list grows when a config refresh is performed
	// not replaced.
	m_HouseKeeperSettings->m_ParamList.RemoveAll();

	SeperateParameter( m_HouseKeeperSettings->m_DeletionParameters , m_HouseKeeperSettings->m_ParamList );

	if ( (m_HouseKeeperSettings->m_ParamList.GetCount() % 4) != 0 )
		return false;

	for ( POSITION pos = m_HouseKeeperSettings->m_ParamList.GetHeadPosition(); pos != NULL; )
	{
		POSITION PrePos = pos;

		// Directory
		CString a = m_HouseKeeperSettings->m_ParamList.GetNext( pos );

		if ( !CConfigSettings::CheckDirectory( this, "DeletionParameters", a, CConfigSettings::required, Installing ) )
			return false;

		PrePos = pos;

		// Pattern
		CString b = m_HouseKeeperSettings->m_ParamList.GetNext( pos );

		if ( b.IsEmpty() )
			m_HouseKeeperSettings->m_ParamList.SetAt( PrePos, "*.*" );

		PrePos = pos;

		// Hrs old
		CString c = m_HouseKeeperSettings->m_ParamList.GetNext( pos );

		if ( c.IsEmpty() )
			m_HouseKeeperSettings->m_ParamList.SetAt( PrePos, "72" );

		PrePos = pos;

		CString d = m_HouseKeeperSettings->m_ParamList.GetNext( pos );

		// Deletion test date (C/A/M)
		if ( d.CompareNoCase( "C" ) == 0 )
		{	
			m_HouseKeeperSettings->m_ParamList.SetAt( PrePos, "Created" );
		}
		else if ( d.CompareNoCase( "A" ) == 0 )
		{
			m_HouseKeeperSettings->m_ParamList.SetAt( PrePos, "LastAccessed" );
		}
		else
		{
			m_HouseKeeperSettings->m_ParamList.SetAt( PrePos, "LastModified" );
		}

	}

	return true;
}

void CHouseKeeper::PollDirectories()
{
	// Poll Directory
	//DebugMsg("Polling housekeeper directories");
	for ( POSITION ParamPos = m_HouseKeeperSettings->m_ParamList.GetHeadPosition(); ParamPos != NULL; )
	{
		CString Dir = m_HouseKeeperSettings->m_ParamList.GetNext( ParamPos );
		CString Pattern = m_HouseKeeperSettings->m_ParamList.GetNext( ParamPos );
		CString DirAndPattern;
		DirAndPattern.Format( "%s\\%s", Dir, Pattern );

		long Hrs = atoi ( m_HouseKeeperSettings->m_ParamList.GetNext( ParamPos ) );
		CString DelDate = m_HouseKeeperSettings->m_ParamList.GetNext( ParamPos );

		CStringList FileList;

		ListAllFiles( FileList, Dir, Pattern );

		for( POSITION CurrentPosition = FileList.GetHeadPosition(); CurrentPosition != NULL; )
		{
			DelFile( FileList.GetNext(CurrentPosition), Hrs, DelDate );

			// Added P Barber 23-03-2000 - PinICL PC0041248
			// housekeep at a leisurely pace
			// NOTE: however the PinICL describes 100% CPU
			// usage, we think that NFS Maestro is using the CPU when the housekeeper
			// gets the date/time info on a file it thinks it might delete,
			// but we put this in anyway for good measure.
			Sleep(1);
		}
	}
}

void CHouseKeeper::ListAllFiles( CStringList & FileList, const CString & Dir, const CString & Pattern )
{
	CFileFind* FileFind = new CFileFind;

	CString FileWildCard;

	// Version 17 change
	// PC0117737
	// This is a temp file created by the ackhandler service. If the housekeeper
	// service is set to poll everything in the folder where this file is created
	// we could have a problem
	// Asking the housekeeper service to ignore files with this name.
	CString FileToIgnore = "ftmstempahfile.text.tmp";
	// End of change

	FileWildCard.Format("%s\\%s", Dir, Pattern);

	BOOL FileFound = FileFind->FindFile(FileWildCard, 0);

	while ( FileFound )
	{
			FileFound = FileFind->FindNextFile();

			if ( !FileFind->IsDots() && !FileFind->MatchesMask(FILE_ATTRIBUTE_DIRECTORY) )
			{
				// need to add something here to make sure that you dont pick up
				// files created by the ackhandler [ftmstempahfile.text.tmp]
				// PC0117737
				if (FileFind->GetFileName() != FileToIgnore)
				{
					FileList.AddTail( FileFind->GetFilePath() );
				}
			}
		
	}

	FileFind->Close();

	delete FileFind;
}


void CHouseKeeper::DelFile(const CString & DelFileName, long Hrs, const CString & DelDate )
{
	CFileStatus FileStatus;

	if ( !CFile::GetStatus( DelFileName, FileStatus ) )
	{
		
		// Version 17
		// PC0117116 - Changed Log event to Information, there is no need to have a warning 
		// or error - Milton 08-03-2005
		// Also added GetLastErrorString parameter
		DebugMsg("Unable to obtain status information for the specified file -> %s", DelFileName);
		LogEvent(EVMSG_HOUSEKEEPER_DEL_FAILED, DelFileName, GetLastErrorString());	
		return;
	}

	CTime Now = CTime::GetCurrentTime();
	CTime FileTime;

	if ( DelDate == "Created" )
		FileTime = FileStatus.m_ctime;

	else if ( DelDate == "LastAccessed" )
		FileTime = FileStatus.m_atime;

	else // LastModified
		FileTime = FileStatus.m_mtime;

	CTimeSpan Age = Now - FileTime;
	LONG HrsOld = Age.GetTotalHours();

	if ( HrsOld > Hrs )
	{
		if ( DeleteFile( DelFileName ) == 0 )
		{
			//LogEvent(EVMSG_HOUSEKEEPER_DEL_FAILED, DelFileName);
			// Changed to report error message to event log - PC0090885
			DebugMsg("Unable to delete the specified file -> %s", DelFileName);
			LogEvent(EVMSG_HOUSEKEEPER_DEL_FAILED, DelFileName, GetLastErrorString());
			return;
		}
		else
		{
			DebugMsg("Deleted the specified file -> %s", DelFileName);
			LogEvent(EVMSG_HOUSEKEEPER_DEL_SUCCESS, DelFileName);
			return;
		}
	}
}

void CHouseKeeper::ClearArray_ServiceSpecificSettings()
{
	m_HouseKeeperSettingsArray.SetSize(0);
}

void CHouseKeeper::New_ServiceSpecificSettings()
{
	m_HouseKeeperSettings = new CHouseKeeperSettings;
}

void CHouseKeeper::Add_ServiceSpecificSettings()
{
	m_HouseKeeperSettingsArray.Add( m_HouseKeeperSettings );
}

void CHouseKeeper::Set_ServiceSpecificSettings(int i)
{
	m_HouseKeeperSettings = (CHouseKeeperSettings*) m_HouseKeeperSettingsArray.GetAt(i);
}

void CHouseKeeper::Delete_ServiceSpecificSettings()
{
	delete m_HouseKeeperSettings;
}

void CHouseKeeper::Clear_ServiceSpecificSettings()
{
	for (int i = 0; i < m_HouseKeeperSettingsArray.GetSize(); i++)
		delete (CHouseKeeperSettings*) m_HouseKeeperSettingsArray.GetAt(i);

	m_HouseKeeperSettingsArray.RemoveAll();
}

void CHouseKeeper::Initialize_ServiceSpecificSettings()
{
	m_ConfigFile->m_inDirSetting = CConfigSettings::notRequired;
	m_ConfigFile->m_outDirSetting = CConfigSettings::notRequired;
	m_ConfigFile->m_workingDirSetting = CConfigSettings::notRequired;
	m_ConfigFile->m_ackInDirSetting = CConfigSettings::notRequired;
	m_ConfigFile->m_errorDirSetting = CConfigSettings::notRequired;
}
