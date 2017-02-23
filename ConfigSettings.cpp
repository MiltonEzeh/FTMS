// ConfigSettings.cpp: implementation of the ConfigSettings class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "wrapper.h"
#include "Acknowledgement.h"
#include "ConfigSettings.h"
#include "ServHusk.h"
#include "HuskMsg.h" // Event message ids
#include "Globals.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

const LPCSTR CConfigSettings::s_DefaultSetting = "THIS NEEDS TO BE SET";

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConfigSettings::CConfigSettings()
{
	m_inDirSetting = m_errorDirSetting
		= m_outDirSetting = m_workingDirSetting = required;

	m_ackInDirSetting = validate;
}

CConfigSettings::~CConfigSettings()
{

}

void CConfigSettings::SetDefaultSettings(const CString & LinkName,
										 const CString & ServiceType,
										 const CString & LinkRootDefault)
{
	m_LinkName = LinkName;
	m_ServiceType = ServiceType;

	if ( !LinkRootDefault.IsEmpty() )
	{
		CString Root = ReadRegistryString("RootDirectory", LinkRootDefault, true);
		m_LinkRoot = Root + "\\" + m_LinkName;
	}
}

void CConfigSettings::ReadVolatileSettings()
{
	m_LinkEnabled = ReadRegistryBool("LinkEnabled", "Yes", true, false, false);
	m_RefreshSettings = ReadRegistryBool("RefreshSettings", "No", false, false, false);
}

void CConfigSettings::ReadSettings()
{
	if ( m_LinkName.Right(6) == "TARGET" )
		m_ReceivingEnd = true;
	else
		m_ReceivingEnd = false;

	// Read ini/registry settings
	if ( m_ackInDirSetting != notRequired )
	{
		if ( m_ReceivingEnd )
			m_AckInDir = ReadRegistryString("AckInDir", m_LinkRoot + "\\Send\\In");
		else
			m_AckInDir = ReadRegistryString("AckInDir", m_LinkRoot + "\\AckHandler\\In");
	}

	m_AckInRenameExp.Init( ReadRegistryString("AckInRenameExpression", ".*;&.ack") );

	CString SourceId = GetSubString(m_LinkName, "-", 2);
	CString TargetId = GetSubString(m_LinkName, "-", 3);

	if ( SourceId.IsEmpty() ) SourceId = "XXX";
	if ( TargetId.IsEmpty() ) TargetId = "XXX";

	m_SourceSystemIdentity = ReadRegistryString("SourceSystemIdentity", SourceId, true);
	m_TargetSystemIdentity = ReadRegistryString("TargetSystemIdentity", TargetId, true);

	if ( m_inDirSetting != notRequired )
	{
		CString InDirectories = ReadRegistryString("InDirectory", m_LinkRoot + "\\" + m_ServiceType + "\\In");
		CString InPatterns = ReadRegistryString("InSearchPattern", "*.*" );

		// Multiple in directories can be specified, need to
		// split up string
		CStringList inDirList;

		SeperateParameter( InDirectories, inDirList );
		InDirectoryInfo tmp;

		tmp.m_lastSuccessfulScan = ::GetTickCount();
		tmp.m_errorCondition = false;

		m_InDirs.RemoveAll();
		m_InPats.RemoveAll();

		for ( POSITION p = inDirList.GetHeadPosition(); p; )
		{
			tmp.m_path = inDirList.GetNext(p);
			
			// this makes a copy of tmp
			m_InDirs.AddTail( tmp );
		}

		// Multiple regular expressions can be specified, need to
		// split up string
		SeperateParameter( InPatterns, m_InPats );

		m_inDirFailureThreshold = 1000 * ReadRegistryInt("InDirectoryFailureThreshold secs", "60" ) ;
	}

	if ( m_workingDirSetting != notRequired )
	{
		m_WorkingDirectory = ReadRegistryString("WorkingDirectory", m_LinkRoot + "\\" + m_ServiceType + "\\Working");
	}

	if ( m_outDirSetting != notRequired )
	{
		m_OutDirectory = ReadRegistryString("OutDirectory", s_DefaultSetting);
	}

	m_OutRenameExp.Init( ReadRegistryString("OutRenameExpression", ".*;&") );

	if ( m_errorDirSetting != notRequired )
	{
		m_ErrorDirectory = ReadRegistryString("ErrorDirectory", m_LinkRoot + "\\" + m_ServiceType + "\\Error");
	}

	m_ErrorRenameExp.Init( ReadRegistryString("ErrorRenameExpression", ".*;&.err" ) );
	m_ErrorLogRenameExp.Init( ReadRegistryString("ErrorLogRenameExpression", ".*;&.log") );

	// changed in version 16 to 5 retries
	// PC0095116
	m_RetryCount = ReadRegistryInt("RetryCount", "5");

	//changed in version 16 to 45000 ms
	// PC0095116
	m_RetryWait = ReadRegistryInt("RetryWait", "45000" );

	m_PollInterval = ReadRegistryInt("PollInterval", "5000" );

	m_WriteToWrapper = ReadRegistryBool("WriteToWrapper", "No");

	m_Sequencing = TRUE;
	CString TempString = ReadRegistryString("SequenceType", "Last modified");

	if ( TempString.CompareNoCase( "Created" ) == 0 )
	{
		m_SequenceType = Created;
	}
	else if ( TempString.CompareNoCase( "Last accessed" ) == 0 )
	{
		m_SequenceType = LastAccessed;
	}
	else
	{
		m_SequenceType = LastModified;
	}
	
	m_ProcessFileDLL = ReadRegistryString("ProcessFileDLL", "inline");

	m_RemoveWrapper = ReadRegistryBool("RemoveWrapper", "Yes" );

	m_StrictSequencing = ReadRegistryBool("StrictSequencing", "No" );

	// This enables the CRC check on writes to Sequent in PostProcess (defaults to TRUE)
	// (see ServiceHusk::MoveFileEx for details)
	
	if (m_ServiceType == "PostProcess")
	{
		m_CheckAfterWriteViaNfs = ReadRegistryBool("CheckAfterWriteViaNfs", "Yes" );
	}
	else
	{
		m_CheckAfterWriteViaNfs = FALSE;
	}

}

CString CConfigSettings::ReadRegistryString(LPCSTR valueName,
											LPCSTR defaultValue,
											bool LinkSettings,
											bool UpdateValue,
											bool CreateValue)
{
	CString ReturnString;

    DWORD dwType					= 0;
	DWORD dwSize					= 0;
	unsigned char* pRegistryValue	= NULL;
	
	CString RegistryKey;

	RegistryKey = REG_FTMS_ROOT;

	if ( _stricmp(valueName, "RefreshAllLinks" ) == 0 )
	{
		RegistryKey = "SYSTEM\\CurrentControlSet\\Services\\" + m_LinkName;
	}
	else
	{
		RegistryKey = RegistryKey + m_LinkName;

		if ( !LinkSettings )
			RegistryKey = RegistryKey + "\\" + m_ServiceType;
	}

	HKEY hRegistryKey = 0;

	// Added P Barber 10-12-1999 - PinICL PC0034838
	//
	// Get registry value size, allocate memory and THEN get value
	//

	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegistryKey, 0, KEY_QUERY_VALUE|KEY_WRITE,  &hRegistryKey)
		== ERROR_SUCCESS )
	{
        // Yes we are installed
		if (RegQueryValueEx(hRegistryKey,
						valueName,
						NULL,
						&dwType,
						NULL,
						&dwSize) == ERROR_SUCCESS)	// get value size
		{
			pRegistryValue = (unsigned char*)(malloc(sizeof(unsigned char) * dwSize));
			if (NULL != pRegistryValue)
			{
				memset(pRegistryValue, 0, sizeof(unsigned char) * dwSize);

				if (RegQueryValueEx(hRegistryKey,
								valueName,
								NULL,
								&dwType,
								pRegistryValue,
								&dwSize) == ERROR_SUCCESS)
				{
					ReturnString = pRegistryValue;

					if ( UpdateValue )
					{
						::RegSetValueEx(hRegistryKey, valueName, 0, REG_SZ, (LPBYTE) defaultValue, strlen(defaultValue) + 1);
					}
				}
				else
				{
					// error reading registry

					char szBuf[2048];
					sprintf(szBuf, "Error: Reading registry %s\\%s, Error no. %ld\n", (LPCTSTR)RegistryKey, valueName, GetLastError());
					OutputDebugString(szBuf);

					// DebugMsg("Error: Reading registry %s\\%s, Error no. %ld", (LPCTSTR)RegistryKey, valueName, GetLastError());
					// LogEvent(EVMSG_REG_READ_ERROR, RegistryKey, valueName, GetLastError());

					// NOTE:  Can not call DebugMsg or LogEvent from here!!!
					// Plus this function does not report success or failiure
					// to caller.
					// Therefore because of time constrants this will have to wait
					// for another day, SORRY!!!
					// The message has been left in HuskMsg.mc even though it is not used
				}
			}
			else
			{
				// memory alloc error

				char szBuf[2048];
				sprintf(szBuf, "Error: Could not allocate memory while reading registry value %s\\%s\n", RegistryKey, valueName);
				OutputDebugString(szBuf);

				// DebugMsg("Error: Could not allocate memory while reading registry value %s\\%s", RegistryKey, valueName);
				// LogEvent(EVMSG_MEM_ALLOC_ERROR);

				// NOTE:  Can not call DebugMsg or LogEvent from here!!!
				// Plus this function does not report success or failiure
				// to caller.
				// Therefore because of time constrants this will have to wait
				// for another day, SORRY!!!
				// The message has been left in HuskMsg.mc even though it is not used
			}
		}
		else
		{
			ReturnString = defaultValue;

			// and store the default value in the registry
			::RegSetValueEx(hRegistryKey, valueName, 0, REG_SZ, (LPBYTE) defaultValue, strlen(defaultValue)+1);
		}

        RegCloseKey(hRegistryKey);
    }
	else
	{
		ReturnString = defaultValue;

		if ( CreateValue )
		{
			DWORD Disposition = 0;

			if ( RegCreateKeyEx(HKEY_LOCAL_MACHINE, RegistryKey, 0, "", 0, KEY_ALL_ACCESS, NULL, &hRegistryKey, &Disposition) ==  ERROR_SUCCESS )
			{
				// and store the default value in the registry
				::RegSetValueEx(hRegistryKey, valueName, 0, REG_SZ, (LPBYTE) defaultValue, strlen(defaultValue)+1);

				RegCloseKey(hRegistryKey);
			}
		}
	}

	if (pRegistryValue)
		free(pRegistryValue);

	return ReturnString;
}


bool CConfigSettings::ReadRegistryBool(LPCSTR valueName, LPCSTR defaultValue, bool LinkSettings, bool UpdateValue, bool CreateValue)
{
	CString RetVal = ReadRegistryString(valueName, defaultValue, LinkSettings, UpdateValue, CreateValue);

	if ( RetVal.CompareNoCase("No") == 0 || RetVal == "0" )
		return false;
	else
		return true;
}

int CConfigSettings::ReadRegistryInt(LPCSTR valueName, LPCSTR defaultValue)
{
	CString RetVal = ReadRegistryString(valueName, defaultValue);

	return atoi( RetVal );
}

bool CConfigSettings::IsValid(CServiceHusk* pService, bool Installing)
{
	// Check all directories exist
	if ( !CheckDirectory( pService, "AckInDir",  m_AckInDir, m_ackInDirSetting, Installing ) )
		return false;

	if ( !CheckDirectory( pService, "WorkingDirectory", m_WorkingDirectory, m_workingDirSetting, Installing ) )
		return false;

	if ( !CheckDirectory( pService, "OutDirectory", m_OutDirectory, m_outDirSetting, Installing ) )
		return false;

	if ( !CheckDirectory( pService, "ErrorDirectory", m_ErrorDirectory, m_errorDirSetting, Installing ) )
		return false;

	// Loop though all in directories ans check for existence
	POSITION CurrentPosition;

	for ( CurrentPosition = m_InDirs.GetHeadPosition(); CurrentPosition != NULL; )
	{
		if ( !CheckDirectory( pService, "InDirectory", m_InDirs.GetNext(CurrentPosition).m_path, m_inDirSetting, Installing ) )
			return false;
	}

	// check for errors in rename expressions
	if ( !pService->CheckValidRegEx( m_OutRenameExp, "OutRenameExpression" ) )
		return false;

	if ( !pService->CheckValidRegEx( m_ErrorLogRenameExp, "ErrorLogRenameExpression" ) )
		return false;

	if ( !pService->CheckValidRegEx( m_ErrorRenameExp, "ErrorRenameExpression" ) )
		return false;
	
	if ( !pService->CheckValidRegEx( m_AckInRenameExp, "AckInRenameExpression" ) )
		return false;

	return TRUE;
}

bool CConfigSettings::CheckDirectory(CServiceHusk* pService, 
									 LPCSTR dirName,
									 LPCSTR dirPath, 
									 DirectoryCheckSetting setting,
									 bool Installing /* = false */ )
{
	if ( setting == notRequired )
		return true;

	if ( Installing && setting == required )
	{
		if (CreateDirTree(dirPath) )	 // create directory
		{
			pService->LogEvent(EVMSG_DIR_CREATED, dirPath);
		}
		else
		{
			pService->LogEvent(EVMSG_DIR_CREATE_FAILED, dirPath);
		}

		return true;
	}

	// this nasty expression means -
	//  if we're validating and dirPath is NULL or an empty string...
	if ( (setting == validate) && ( (!dirPath) || (!*dirPath) ) )
		return true;

	if ( !IsDirectory( dirPath ) )
	{
		pService->LogEvent(EVMSG_CONFIG_DIR_INVALID_WARN, dirName, dirPath, GetLastErrorString() );
	}

	return true;
}

bool CConfigSettings::IsLinkEnabled()
{
	return m_LinkEnabled;
}

void CConfigSettings::SetLinkToDisabled()
{
	m_LinkEnabled = ReadRegistryBool("LinkEnabled", "No", true, true);
}

bool CConfigSettings::IsRefreshSettings()
{
	return m_RefreshSettings;
}

void CConfigSettings::SetNoRefreshSettings()
{
	m_RefreshSettings = ReadRegistryBool("RefreshSettings", "No", false, true);
}

bool CConfigSettings::IsStrictSequencing()
{
	return m_StrictSequencing;
}

bool CConfigSettings::RemoveRegistryLink()
{
	CString		RegistryKey;

	RegistryKey = REG_FTMS_ROOT;
	RegistryKey = RegistryKey + m_LinkName + "\\" + m_ServiceType;

	if ( RegDeleteKey(HKEY_LOCAL_MACHINE, RegistryKey) != ERROR_SUCCESS )
		return false;

	RegistryKey = REG_FTMS_ROOT;
	RegistryKey = RegistryKey + m_LinkName;

	// Not interested in return value, as probably failed to delete
	// due to subkeys existing and only want to delete this key if
	// no subkeys exist - NT only behaviour
	RegDeleteKey(HKEY_LOCAL_MACHINE, RegistryKey);

	return true;
}

bool CConfigSettings::RefreshAllLinks()
{
	return ReadRegistryBool("RefreshAllLinks", "No", true, false);
}

void CConfigSettings::SetNoRefreshAllLinks()
{
	ReadRegistryBool("RefreshAllLinks", "No", true, true);
}