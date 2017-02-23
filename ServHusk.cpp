// NTService.cpp
//
// Implementation of CServiceHusk

#include "stdafx.h"
#include "HuskMsg.h" // Event message ids
#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "ServHusk.h"
#include "Globals.h"
#include "CRC.h"
#include "RegistryLink.h"
#include "fstream.h"

// static variables
CServiceHusk* CServiceHusk::s_pThis = NULL;

CServiceHusk::CServiceHusk()
: m_shutDownEvent( false, true )	// create the shutdown event initially blocked, manual reset
{
	m_ConfigFile = new CConfigSettings;

    DebugMsg("CServiceHusk::CServiceHusk()");

    // copy the address of the current object so we can access it from
    // the static member callback functions.
    // WARNING: This limits the application to only one CServiceHusk object.
	ASSERT( !s_pThis );
    s_pThis = this;

	m_readAndStoreWrapperBeforeProcessing = true;
	m_copyUnwrappedToWorking = false;
	m_restoreWrapperToOutputFile = true;
	m_FailureRetainInFile = false;

	m_ServiceType = "General Service";
    m_hEventSource = NULL;

    // set up the initial service status
    m_hServiceStatus = NULL;
    m_Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_Status.dwCurrentState = SERVICE_STOPPED;
    m_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    m_Status.dwWin32ExitCode = 0;
    m_Status.dwServiceSpecificExitCode = 0;
    m_Status.dwCheckPoint = 0;
    m_Status.dwWaitHint = 0;
    m_ShutdownRequested = false;
	m_DLLHandle = NULL;
}

CServiceHusk::~CServiceHusk()
{
    DebugMsg("CServiceHusk::~CServiceHusk()");

    if (m_hEventSource)
	{
        DeregisterEventSource(m_hEventSource);
		m_hEventSource = 0;
    }

	ASSERT( s_pThis == this );
	s_pThis = 0;

	delete m_ConfigFile;
	m_ConfigFile = 0;
	//delete[] m_inDirStates;
}

////////////////////////////////////////////////////////////////////////////////////////
// Default command line argument parsing

// Returns TRUE if it found an arg it recognised, FALSE if not
// Note: processing some arguments causes output to stdout to be generated.
bool CServiceHusk::ParseStandardArgs(int argc, char* argv[])
{
    DebugMsg("Calling ParseStandardArgs()");

	if ( ( _stricmp(argv[1], "-?") == 0 ) || ( _stricmp(argv[1], "/?") == 0 ) )
	{
		m_CommandLine.DisplaySyntax();
		return true;
	}

    // See if we have any command line args we recognise
    if ( argc < 2 )
	{
		DebugMsg("No service name specified!!!");
		LogEvent(EVMSG_NO_SERV_NAME);
		MessageBox(NULL, "No service name specified. Service will not start. Check the registry.", "FTMS Error message", 0x00200000L);
		return false;
	}

	m_CommandLine.m_ServiceName = argv[1];

	if ( !m_CommandLine.ValidateCommandLine(argc, argv) )
	{
		m_CommandLine.DisplaySyntax();
		return true;
	}

	if ( m_CommandLine.m_DisplaySyntax )
	{
		m_CommandLine.DisplaySyntax();
		return true;
	}

	Parse_ServiceSpecificSettings();

	if ( m_CommandLine.m_AddRegistryLink )
	{
		CreateRegistryDetails();
		printf("\"%s %s\" successfully added", m_CommandLine.m_ServiceName, m_ServiceType);
		return true;
	}

	if ( m_CommandLine.m_RemoveRegistryLink )
	{
		RemoveRegistryDetails();
		printf("\"%s %s\" successfully removed", m_CommandLine.m_ServiceName, m_ServiceType);
		return true;
	}

	if ( m_CommandLine.m_Install )
	{
		InstallService();
		return true;
	}

	if ( m_CommandLine.m_UnInstall )
	{
		UnInstallService();

		if ( !m_CommandLine.m_ProcessMultiLinks )
			RemoveRegistryDetails();

		return true;
	}

	// Return true if all processing has occurred and program is to exit.
	// This would be the case if a service was installed / uninstalled

	// Return false if to run as a service
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// Install/uninstall routines

// Test if the service is currently installed
bool CServiceHusk::IsInstalled()
{
    // Open the Service Control Manager
    SC_HANDLE hSCM = ::OpenSCManager(NULL, // local machine
                                     NULL, // ServicesActive database
                                     SC_MANAGER_ALL_ACCESS); // full access

    if (hSCM)
	{
        // Try to open the service
        SC_HANDLE hService = ::OpenService(hSCM,
                                           m_CommandLine.m_ServiceName,
                                           SERVICE_QUERY_CONFIG);

        if (hService)
		{
            ::CloseServiceHandle(hService);
			return true;
        }

        ::CloseServiceHandle(hSCM);
    }

    return false;
}

bool CServiceHusk::Install()
{
    // Open the Service Control Manager
    SC_HANDLE hSCM = ::OpenSCManager(NULL, // local machine
                                     NULL, // ServicesActive database
                                     SC_MANAGER_ALL_ACCESS); // full access

    if (!hSCM)
		return false;

    // Get the executable file path
    char szFilePath[_MAX_PATH];

    ::GetModuleFileName(NULL, szFilePath, sizeof(szFilePath));

	SC_HANDLE hService;

	// Store dependent service names into a double null terminated string
	// Format "<Service name>\0<Service name>\0\0"
	POSITION pos;
	int len = 1;
	CString s;

	for ( pos = m_CommandLine.m_DependentServices.GetHeadPosition(); pos != NULL; )
	{
		s = m_CommandLine.m_DependentServices.GetNext( pos );
		len += s.GetLength() + 1;
	}

	char * szServiceNames = new char [len];
	memset(szServiceNames, ' ', len);
	int offset = 0;

	for ( pos = m_CommandLine.m_DependentServices.GetHeadPosition(); pos != NULL; )
	{
		s = m_CommandLine.m_DependentServices.GetNext( pos );

		len = s.GetLength();

		strncpy( szServiceNames + offset, s, len );

		offset += len;

		memset(szServiceNames + offset, '\0', 1);

		offset++;
	}

	if ( m_CommandLine.m_User.FindOneOf("\\") == -1 )
		m_CommandLine.m_User = ".\\" + m_CommandLine.m_User;

	memset(szServiceNames + offset, '\0', 1);

	DWORD StartType;

	if ( m_CommandLine.m_StartupType == "Manual" )
	{
		StartType = SERVICE_DEMAND_START;
	}
	else if ( m_CommandLine.m_StartupType == "Disabled" )
	{
		StartType = SERVICE_DISABLED;
	}
	else
	{
		StartType = SERVICE_AUTO_START;
	}

    // Create the service
	if ( m_CommandLine.m_User.IsEmpty() || m_CommandLine.m_Password.IsEmpty() )
	{
		hService = ::CreateService(hSCM,
		 						   m_CommandLine.m_ServiceName,
								   m_CommandLine.m_ServiceName,
								   STANDARD_RIGHTS_REQUIRED,
								   SERVICE_WIN32_OWN_PROCESS,
								   StartType,
								   SERVICE_ERROR_IGNORE,
								   szFilePath,
								   NULL,
								   NULL,
								   szServiceNames,
								   NULL,
								   NULL);
	}
	else
	{
		hService = ::CreateService(hSCM,
								   m_CommandLine.m_ServiceName,
								   m_CommandLine.m_ServiceName,
								   STANDARD_RIGHTS_REQUIRED,
								   SERVICE_WIN32_OWN_PROCESS,
								   StartType,
								   SERVICE_ERROR_IGNORE,
								   szFilePath,
								   NULL,
								   NULL,
								   szServiceNames,
								   m_CommandLine.m_User,
								   m_CommandLine.m_Password);
	}

	delete [] szServiceNames;

    if (!hService)
	{
        ::CloseServiceHandle(hSCM);
        return false;
    }

    char szKey[256];
    HKEY hKey = NULL;

    strcpy(szKey, "SYSTEM\\CurrentControlSet\\Services\\");
    strcat(szKey, m_CommandLine.m_ServiceName);

	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey ) != ERROR_SUCCESS )
	{
        ::CloseServiceHandle(hService);
        ::CloseServiceHandle(hSCM);
        return false;
	}

	DWORD dwType = 0;

	unsigned char * RegistryValue = new unsigned char[200];
	memset(RegistryValue, ' ', 200);
	DWORD dwSize = 200;

	int lenServiceName = m_CommandLine.m_ServiceName.GetLength();
	char * ServiceName = new char[lenServiceName + 4];

	if ( RegQueryValueEx(hKey, "ImagePath", NULL, &dwType, RegistryValue, &dwSize) == ERROR_SUCCESS )
	{

		CString LinkName;

		if ( m_CommandLine.m_ProcessMultiLinks )
			LinkName = m_CommandLine.m_ServiceName;
		else
		{
			int pos = m_CommandLine.m_ServiceName.FindOneOf(" ");

			if ( pos != -1 )
				LinkName = m_CommandLine.m_ServiceName.Left(pos);
			else
				LinkName = m_CommandLine.m_ServiceName;
		}

		strcpy (ServiceName, " \"");
		strcat (ServiceName, LinkName);
		strcat (ServiceName, "\"");

		_mbscat (RegistryValue, (unsigned char *) ServiceName);

		if ( m_CommandLine.m_ProcessMultiLinks )
			_mbscat( RegistryValue, (unsigned char *) " -multi");

		if ( m_CommandLine.m_Compress )
			_mbscat( RegistryValue, (unsigned char *) " -compress");

		if ( m_CommandLine.m_Decompress )
			_mbscat( RegistryValue, (unsigned char *) " -decompress");

		if ( !m_CommandLine.m_Crypto.IsEmpty() )
		{
			_mbscat( RegistryValue, (unsigned char *) " -");
			_mbscat( RegistryValue, (unsigned char *) m_CommandLine.m_Crypto.GetBuffer(0));
		}


		long StringLength = _mbslen (RegistryValue);

		if ( RegSetValueEx(hKey,
					       "ImagePath",
							NULL,
							REG_SZ,
							RegistryValue,
							StringLength) != ERROR_SUCCESS )
		{
			delete [] ServiceName;
			delete [] RegistryValue;

			RegCloseKey(hKey);

			return false;
		}
	}
	else
	{
		delete [] ServiceName;
		delete [] RegistryValue;

		RegCloseKey(hKey);

		return false;
	}

	delete [] ServiceName;
	delete [] RegistryValue;

    RegCloseKey(hKey);

    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hSCM);

    // make registry entries to support logging messages
    // Add the source name as a subkey under the Application
    // key in the EventLog service portion of the registry.
    strcpy(szKey, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
    strcat(szKey, m_CommandLine.m_ServiceName);

    if (::RegCreateKey(HKEY_LOCAL_MACHINE, szKey, &hKey) != ERROR_SUCCESS)
	{
        return false;
    }

    // Add the Event ID message-file name to the 'EventMessageFile' subkey.
    ::RegSetValueEx(hKey,
                    "EventMessageFile",
                    0,
                    REG_EXPAND_SZ,
                    (CONST BYTE*)szFilePath,
                    strlen(szFilePath) + 1);

    ::RegSetValueEx(hKey,
                    "CategoryMessageFile",
                    0,
                    REG_EXPAND_SZ,
                    (CONST BYTE*)szFilePath,
                    strlen(szFilePath) + 1);

    // Set the supported types flags.
    DWORD dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

    ::RegSetValueEx(hKey,
                    "TypesSupported",
                    0,
                    REG_DWORD,
                    (CONST BYTE*)&dwData,
                     sizeof(DWORD));

	// tell event log we have 8 categories!
	dwData = 8;

    ::RegSetValueEx(hKey,
                    "CategoryCount",
                    0,
                    REG_DWORD,
                    (CONST BYTE*)&dwData,
                     sizeof(DWORD));

    ::RegCloseKey(hKey);

    LogEvent(EVMSG_INSTALLED, m_CommandLine.m_ServiceName);
	DebugMsg("Service Installed -  %s ", m_CommandLine.m_ServiceName);

    return true;
}

bool CServiceHusk::Uninstall()
{
    // Open the Service Control Manager
    SC_HANDLE hSCM = ::OpenSCManager(NULL, // local machine
                                     NULL, // ServicesActive database
                                     SC_MANAGER_ALL_ACCESS); // full access

    if (!hSCM)
		return false;

    SC_HANDLE hService = ::OpenService(hSCM, m_CommandLine.m_ServiceName, DELETE);

    if (hService)
	{
        if (::DeleteService(hService))
		{
            LogEvent(EVMSG_REMOVED, m_CommandLine.m_ServiceName);
			::CloseServiceHandle(hService);
			::CloseServiceHandle(hSCM);

			return true;
		}
		else
		{
            LogEvent(EVMSG_NOTREMOVED, m_CommandLine.m_ServiceName);

	        ::CloseServiceHandle(hService);
		    ::CloseServiceHandle(hSCM);

			return false;
		}
    }

    ::CloseServiceHandle(hSCM);

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////
// Logging functions

// This function makes an entry into the application event log
void CServiceHusk::LogEvent(
	DWORD dwID, WORD category,
	const char* pszS1,
	const char* pszS2,
	const char* pszS3)
{
	// If out of disk space shut service down
	if ( ::GetLastError() == 112 && m_ConfigFile->IsLinkEnabled() )
	{
		DebugMsg("Out of disk space, shutting service down!!!!");
		m_ConfigFile->SetLinkToDisabled();
		LogEvent(EVMSG_DISK_FULL, GetLastErrorString());
	}

	const int maxParams = 4;
    const char* ps[maxParams+1];

	CString temp;

	if ( m_ConfigFile )
	{
		temp.Format("%s %s", m_ConfigFile->m_LinkName, m_ServiceType);
		ps[0] = temp;
	}
	else
		if ( !m_CommandLine.m_ProcessMultiLinks )
		{
			temp.Format("%s %s", m_CommandLine.m_ServiceName, m_ServiceType);
			ps[0] = temp;
		}
		else
			ps[0] = m_CommandLine.m_ServiceName;

    ps[1] = pszS1;
    ps[2] = pszS2;
    ps[3] = pszS3;
	ps[4] = 0;

    int iStr = 0;

    for (int i = 0; i < maxParams; i++)
	{
        if (ps[i] != NULL)
			iStr++;
    }

	CString EventSource;

	EventSource = m_CommandLine.m_ServiceName;

	if ( !m_CommandLine.m_ProcessMultiLinks)
		// Possible bug - Event Source becomes <Link Name> PreProcess PreProcess
		EventSource.Format("%s %s", m_CommandLine.m_ServiceName, m_ServiceType);
		//EventSource.Format("%s", m_CommandLine.m_ServiceName);


    // Check the event source has been registered and if
    // not then register it now
    if (!m_hEventSource)
	{
        m_hEventSource = ::RegisterEventSource(NULL,         // local machine
                                               EventSource); // source name
    }

	// Get user name
	char name [256];
	DWORD size = sizeof( name );
	PSID psid = NULL;

	if ( GetUserName ( name, & size ) != 0 )
	{
		static BYTE sid [100];

		psid = (PSID) & sid;

		DWORD dsid = 100;

		char dom [80];

		DWORD ddom = sizeof( dom );

		SID_NAME_USE snu;

		if ( !LookupAccountName (0, name, psid, & dsid, dom, & ddom, & snu) )
			psid = NULL;
	}

	DWORD dwSeverity = severity(dwID);
	int EventSeverity = (int) dwSeverity;

	if ( EventSeverity == 3 )
	{
		EventSeverity = 1;
	}
	else if ( EventSeverity == 1 )
	{
		EventSeverity = 4;
	}

	// Write to event log
    if (m_hEventSource)
	{
        ::ReportEvent(m_hEventSource,
					  EventSeverity,
                      category,
                      dwID,
                      psid,
                      iStr,
                      0,
                      ps,
                      NULL);
    }

	// and output it to the debug output
	{
		LPVOID lpMsgBuf = 0;

		const DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_HMODULE ;

		DWORD dwRet = ::FormatMessage(
			dwFlags,
			::GetModuleHandle( NULL ),
			dwID,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			const_cast<va_list*>( ps ) );

		if ( dwRet != 0 )
		{
			m_lastEventString.Format( "%d: %s", remove_severity(dwID), (LPCSTR) lpMsgBuf );
			m_lastEventString.TrimRight();

			LocalFree( lpMsgBuf );
		}
		else
			m_lastEventString.Format( "%d: [FormatMessage failed]", remove_severity(dwID) );

		DebugMsg( m_lastEventString );
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////
// Service startup and registration

// a hack for neat shutdown when running as normal app --
BOOL WINAPI HandlerRoutine( DWORD dwCtrlType )
{
	CServiceHusk::Handler( SERVICE_CONTROL_STOP );

	return TRUE;
}


BOOL CServiceHusk::StartService()
{
	// This is a hack to enable debugging as an normal app
	if ( m_CommandLine.m_Debug )
	{
		::SetConsoleCtrlHandler( HandlerRoutine, TRUE );

		Run();

		return TRUE;
	}

    SERVICE_TABLE_ENTRY st[] =
	{
		{ (LPSTR) (LPCSTR) m_CommandLine.m_ServiceName, ServiceMain},
        {NULL, NULL}
    };

    DebugMsg("Calling StartServiceCtrlDispatcher()");

    BOOL b = ::StartServiceCtrlDispatcher(st);

    DebugMsg("Returned from StartServiceCtrlDispatcher()");

    return b;
}

// static member function (callback)
void CServiceHusk::ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Get a pointer to the C++ object
    CServiceHusk* pService = s_pThis;

    pService->DebugMsg("Entering CServiceHusk::ServiceMain()");
    pService->DebugMsg("Pointer = %lu", pService);

    // Register the control request handler
    pService->m_Status.dwCurrentState = SERVICE_START_PENDING;

    pService->m_hServiceStatus = RegisterServiceCtrlHandler(pService->m_CommandLine.m_ServiceName,
                                                           Handler);

    if (pService->m_hServiceStatus == NULL)
	{
        pService->LogEvent(EVMSG_CTRLHANDLERNOTINSTALLED);
        return;
    }

    // Start the initialisation
    if (pService->Initialize())
	{
        // Do the real work.
        // When the Run function returns, the service has stopped.
        pService->m_Status.dwWin32ExitCode = 0;
        pService->m_Status.dwCheckPoint = 0;
        pService->m_Status.dwWaitHint = 0;

        pService->Run();
    }

    // Tell the service manager we are stopped
    pService->DebugMsg("Leaving CServiceHusk::ServiceMain()");

	pService->LogEvent(EVMSG_STOPPED);
    pService->SetStatus(SERVICE_STOPPED);

}

///////////////////////////////////////////////////////////////////////////////////////////
// status functions

void CServiceHusk::SetStatus(DWORD dwState)
{
    DebugMsg("CServiceHusk::SetStatus(%lu -> %lu)", m_Status.dwCurrentState, dwState);

	if ( m_Status.dwCurrentState != dwState )
	{
		m_Status.dwCurrentState = dwState;

		if ( m_hServiceStatus )
			SetServiceStatus(m_hServiceStatus, &m_Status);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
// Service initialization

BOOL CServiceHusk::Initialize()
{
    DebugMsg("Entering CServiceHusk::Initialize()");

    // Start the initialization
    SetStatus(SERVICE_START_PENDING);

    // Set final state
    m_Status.dwWin32ExitCode = GetLastError();
    m_Status.dwCheckPoint = 0;
    m_Status.dwWaitHint = 0;

    LogEvent(EVMSG_STARTED);
    SetStatus(SERVICE_RUNNING);

    DebugMsg("Leaving CServiceHusk::Initialize()");

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// main function to do the real work of the service

// This function performs the main work of the service.
// When this function returns the service has stopped.
void CServiceHusk::Run()
{
	StoreLinkConfiguration();

	DebugMsg("Number of %s's found -> %lu", m_ServiceType, m_ConfigFileArray.GetSize() );

    while ( !m_ShutdownRequested )
	{

		if (m_ConfigFileArray.GetSize() == 0 )
		{
			// If no links in registry wait 5 seconds
			// this is so that we don't hog the cpu.
			if ( m_shutDownEvent.Lock( 5000 ) )
			{
				// block was successful, time to exit loop!
				DebugMsg("No link wait complete");
			}
		}
		else
		{
			for (int i = 0; i < m_ConfigFileArray.GetSize(); i++)
			{
				m_ConfigFile = (CConfigSettings*) m_ConfigFileArray.GetAt(i);

				Set_ServiceSpecificSettings(i);

				m_ConfigFile->ReadVolatileSettings();

				WaitPollInterval();

//				DebugMsg("Processing %s link", m_ConfigFile->m_LinkName);

				if ( !m_ConfigFile->IsLinkEnabled() )
				{
					DebugMsg("Link not enabled -> %s", m_ConfigFile->m_LinkName);
					continue;
				}

				if ( m_ConfigFile->IsRefreshSettings() )
				{
					DebugMsg("Refreshing settings -> %s", m_ConfigFile->m_LinkName);

					if ( !GetLinkSettings() )
					{
						DebugMsg("Failed to refresh settings -> %s", m_ConfigFile->m_LinkName);

						m_ConfigFile->SetLinkToDisabled();
						continue;
					}

					DebugMsg("Setting Refresh settings to No -> %s", m_ConfigFile->m_LinkName);
					m_ConfigFile->SetNoRefreshSettings();
				}

				ProcessLink();
			}	// end for
		}

		if ( m_CommandLine.m_ProcessMultiLinks )
		{
			CString StoredLinkName;

			if ( m_ConfigFileArray.GetSize() != 0 )
				StoredLinkName = m_ConfigFile->m_LinkName;

			m_ConfigFile->m_LinkName = m_CommandLine.m_ServiceName;

			if ( m_ConfigFile->RefreshAllLinks() )
			{
				DebugMsg("Refreshing all links...");

				// delete previous settings before storing new settings
				m_ConfigFile->SetNoRefreshAllLinks();

				for (int i = 0; i < m_ConfigFileArray.GetSize(); i++)
					delete (CConfigSettings*) m_ConfigFileArray.GetAt(i);

				m_ConfigFile = 0;

				m_ConfigFileArray.RemoveAll();

				Clear_ServiceSpecificSettings();

				m_ConfigFile = new CConfigSettings;

				// Get new settings
				StoreLinkConfiguration();

				DebugMsg("Number of %s's found -> %lu", m_ServiceType, m_ConfigFileArray.GetSize() );
			}
			else
			{
				if ( m_ConfigFileArray.GetSize() != 0 )
					m_ConfigFile->m_LinkName = StoredLinkName;
			}
		}

	} // end while

	// Release DLL from memory
	if ( m_DLLHandle )
		::FreeLibrary(m_DLLHandle);

	for (int i = 0; i < m_ConfigFileArray.GetSize(); i++)
		delete (CConfigSettings*) m_ConfigFileArray.GetAt(i);

	m_ConfigFile = 0;

	m_ConfigFileArray.RemoveAll();

	Clear_ServiceSpecificSettings();
}

//////////////////////////////////////////////////////////////////////////////////////
// Control request handlers

// static member function (callback) to handle commands from the
// service control manager
void CServiceHusk::Handler(DWORD dwOpcode)
{
    // Get a pointer to the object
    CServiceHusk* pService = s_pThis;

    pService->DebugMsg("CServiceHusk::Handler(%lu)", dwOpcode);

    switch (dwOpcode)
	{
    case SERVICE_CONTROL_STOP: // 1

        pService->SetStatus(SERVICE_STOP_PENDING);

        pService->OnStop();

        break;

    case SERVICE_CONTROL_PAUSE: // 2

        pService->OnPause();
        break;

    case SERVICE_CONTROL_CONTINUE: // 3

        pService->OnContinue();
        break;

    case SERVICE_CONTROL_INTERROGATE: // 4

        pService->OnInterrogate();
        break;

    case SERVICE_CONTROL_SHUTDOWN: // 5

        pService->OnShutdown();
        break;

    default:
        if ( !( (dwOpcode >= SERVICE_CONTROL_USER)
			&& ( pService->OnUserControl(dwOpcode) ) ) )
		{
			CString s;
			s.Format( "Opcode = 0x%x", dwOpcode );
            pService->LogEvent(EVMSG_BADREQUEST, s);
        }

        break;
	}

    pService->DebugMsg("CServiceHusk::Handler complete" );
}

void CServiceHusk::StoreLinkConfiguration()
{
    DebugMsg("Entering StoreLinkConfiguration");

	m_ConfigFileArray.SetSize(0);

	ClearArray_ServiceSpecificSettings();

	CRegistryLink RegistryLink;

	if ( m_CommandLine.m_ProcessMultiLinks )
		RegistryLink.SetAtBegin();

	CString LinkName;

	bool FirstTime = true;

	do
	{
		if ( m_CommandLine.m_ProcessMultiLinks )
			LinkName = RegistryLink.GetNext(m_ServiceType);
		else
			LinkName = m_CommandLine.m_ServiceName;

		if ( !LinkName.IsEmpty() )
		{
			// Remove dummy object created in constructor
			if ( FirstTime )
			{
				delete m_ConfigFile;
				m_ConfigFile = 0;

				FirstTime = false;
			}

			m_ConfigFile = new CConfigSettings;

			New_ServiceSpecificSettings();

			Initialize_ServiceSpecificSettings();

			m_ConfigFile->SetDefaultSettings(LinkName, m_ServiceType, m_CommandLine.m_LinkRoot);

			if ( !GetLinkSettings() )
			{
				DebugMsg("GetLinkSettings failed");
				m_ConfigFile->SetLinkToDisabled();
			}

			m_ConfigFileArray.Add(m_ConfigFile);

			Add_ServiceSpecificSettings();

			// this only needs to be run on startup
			DebugMsg("Clearing working directories");

			ClearWorkingDirectory();

			// this only needs to be run on startup
			if ( !ServiceStartUp() )
			{
				DebugMsg("Service startup failed!!!");
				m_ConfigFile->SetLinkToDisabled();
				return;
			}

			// this only needs to be run on startup
			if ( !CheckDLLFunctionExists() )
			{
				DebugMsg("Check DLL function failed!!!");
				m_ConfigFile->SetLinkToDisabled();
				return;
			}

			if ( !m_CommandLine.m_ProcessMultiLinks )
				break;
		}
	}
	while ( !LinkName.IsEmpty() );
}

// Called when the service control manager wants to stop the service
void CServiceHusk::OnStop()
{
    DebugMsg("CServiceHusk::OnStop()");

	m_ShutdownRequested = true;
	m_shutDownEvent.SetEvent();
}

// called when the service is interrogated
void CServiceHusk::OnInterrogate()
{
    DebugMsg("CServiceHusk::OnInterrogate()");
}

// called when the service is paused
void CServiceHusk::OnPause()
{
    DebugMsg("CServiceHusk::OnPause()");
}

// called when the service is continued
void CServiceHusk::OnContinue()
{
    DebugMsg("CServiceHusk::OnContinue()");
}

// called when the service is shut down
void CServiceHusk::OnShutdown()
{
    DebugMsg("CServiceHusk::OnShutdown()");
}

// called when the service gets a user control message
BOOL CServiceHusk::OnUserControl(DWORD dwOpcode)
{
    DebugMsg("CServiceHusk::OnUserControl(%8.8lXH)", dwOpcode);
    return FALSE; // say not handled
}


////////////////////////////////////////////////////////////////////////////////////////////
// Debugging support

void CServiceHusk::DebugMsg(const char* pszFormat, ...) const
{
    char buf[1024];

	CString displayname;

	if ( m_ConfigFile )
		displayname.Format("%s %s", m_ConfigFile->m_LinkName, m_ServiceType);
	else
		if ( !m_CommandLine.m_ProcessMultiLinks )
			displayname.Format("%s %s", m_CommandLine.m_ServiceName, m_ServiceType);
		else
			displayname = m_CommandLine.m_ServiceName;

	sprintf(buf, "[%s](%lu): ", displayname, GetCurrentThreadId());

	va_list arglist;
	va_start(arglist, pszFormat);
    vsprintf( &buf[ strlen(buf) ], pszFormat, arglist);
	va_end(arglist);

    strcat(buf, "\n");

    OutputDebugString(buf);
}


void CServiceHusk::PollDirectories()
{
	CStringList CompleteFileList;

	POSITION CurrentPosition;

	DWORD RetVal = 0;
	CString OutStatusString;
	CString InFullName;
	CString OutFullName;
	CString RenamedFileName;
	CString JustFileName;

	// Build up file list from all directories specified

	// This will create a complete list of files in 'CompleteFileList'
	// Each item is stored in a string with the following format
	// "<date stamp><path><filename>"
	for ( CurrentPosition = m_ConfigFile->m_InDirs.GetHeadPosition(); CurrentPosition != NULL; )
	{
		CConfigSettings::InDirectoryInfo& c = m_ConfigFile->m_InDirs.GetNext( CurrentPosition );

		if ( !ListAllFilesInDir( &CompleteFileList, c.m_path ) )
		{
			// save the error code before it gets overwritten
			DWORD errorCode = ::GetLastError();

			// has error been reported?
			if ( !c.m_errorCondition )
			{
				// no - is time now due to report (bear in mind timer wrap-around)
				DWORD now = ::GetTickCount();

				DebugMsg( "ListAllFilesInDir failed: %s, last successful scan was %d ms ago",
					c.m_path, now - c.m_lastSuccessfulScan );

				if ( ( c.m_lastSuccessfulScan > now ) ||
					( c.m_lastSuccessfulScan + m_ConfigFile->m_inDirFailureThreshold < now ) )
				{
					c.m_errorCondition = true;

					LogEvent(EVMSG_CONFIG_DIR_INVALID,
						"InDirectory", c.m_path, GetErrorMessage(errorCode, true) );
				}
			}

		}
		else
		{
			if ( c.m_errorCondition )
			{
				// reset period
				c.m_errorCondition = false;
				DebugMsg( "ListAllFilesInDir recovered: %s", c.m_path );
			}

			c.m_lastSuccessfulScan = ::GetTickCount();
		}
	}

	// Loop though file list and perform some process on each file.
    for( CurrentPosition = CompleteFileList.GetHeadPosition(); CurrentPosition != NULL; )
    {

		CString ItemString = CompleteFileList.GetNext(CurrentPosition);

		CString InPath = GetJustPath( ItemString.Mid(14) );
		CString InFile = GetJustFileName( ItemString.Mid(14) );

		// If a file exists in the intransit directory then this service has
		// probably been stopped and restarted before file has been fully
		// transfered, all files should be ignored until there are no files
		// in transit
		if ( m_ConfigFile->IsStrictSequencing() )
		{
			// Added P Barber 01-02-2000.  Ref: PinICL PC0034206
			// Test for FAILURE_FILE in InTransit directory,
			// if present shutdown link
			if ( !FailFileInTransit() )
			{
				if ( FileInTransit() )
				{
					if ( !ReSendFile(InFile) )
					{
						// if IsRunning is false, logic error has occured in
						// ReSendFile, so service needs to be shut down
						if ( m_ConfigFile->IsLinkEnabled() )
						{
							if ( !WaitUntilDelivered() )
							{
								if ( m_ShutdownRequested )
									break;

								if ( m_ConfigFile->IsLinkEnabled() )
								{		// Failure file has been generated

									// shut down 'strict sequencing' link due to a previous error or warning
									LogEvent(EVMSG_STRICT_SEQUENCING_ERROR, InPath + "\\" + InFile);

									m_ConfigFile->SetLinkToDisabled();

									break;
								}
								else	// Shutdown has been requested
								{
									break;
								}
							}
						}

						// Force a re-poll here. The most likely way to get here is if the service
						// is stopped and re-started when a file is intransit. As the file in transit
						// will be picked up to be processed when the service is restarted, ignore
						// current file and re-poll
						break;
					}
				}
			}
			else	// disable link because of FAILURE_FILE in InTransit
			{
				// shut down 'strict sequencing' link due to a previous error or warning
				LogEvent(EVMSG_STRICT_SEQUENCING_ERROR, InPath + "\\" + InFile);

				m_ConfigFile->SetLinkToDisabled();

				break;
			}
		}

		if ( m_ConfigFile->IsStrictSequencing() )
		{
			CFile FileShareChk;
			CFileException FileError;

			DebugMsg("%s: Waiting for no NT file locks", InPath + "\\" + InFile);

			while ( !FileShareChk.Open( InPath + "\\" + InFile, CFile::shareExclusive, &FileError ) )
			{
				if ( FileError.m_cause != CFileException::sharingViolation )
				{
					DebugMsg("file error %s, GetLastError: %s", InPath + "\\" + InFile, GetErrorMessage( GetLastError(), true ));
					LogEvent(EVMSG_IN_FILE_ERROR, InPath + "\\" + InFile, GetErrorMessage( GetLastError(), true ) );

					m_ConfigFile->SetLinkToDisabled();
					break;
				}
			}

			if ( !m_ConfigFile->IsLinkEnabled() )
				break;

			DebugMsg("%s: No NT File locks", InPath + "\\" + InFile);

		}
		else
		{
			CFile FileShareChk ;
			if ( !FileShareChk.Open( InPath + "\\" + InFile, CFile::shareExclusive )  )
			{
				DebugMsg("%s: file currently locked (Polling), ignoring!!!", InPath + "\\" + InFile );
				continue ;
			}
		}

		// Working file name -- always the same as incoming filename
		CString OutFile = InFile;

		bool wrapperWasStored = false;

		// clear down the wrapper
		m_WrapperFile.Clear();

		// clear down the CRC info
		ClearDeliveredCRC();

		// are we to processing wrappers?
		if ( m_readAndStoreWrapperBeforeProcessing )
		{
			// Associated file with wrapper object
			m_WrapperFile.SetAssociatedFile(InPath + "\\" + InFile);

			// does file have wrapper?
			if ( m_WrapperFile.IsWrapped() )
			{
				// yes, store info
				m_WrapperFile.StoreOriginalFileDetails();

				// and remember that we have stored it
				wrapperWasStored = true;
			}
		}

		// Retry ProcessFile Number of times specified in configuration file
		int Retries = 0;

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

			// are we to copy un-wrapped file into working directory?
			if ( m_copyUnwrappedToWorking )
			{
				CString workingFile = m_ConfigFile->m_WorkingDirectory + "\\" + InFile;

				if ( !CopyFile(InPath + "\\" + InFile, workingFile, FALSE) )
				{
					// If unable to copy to working directory log event so that
					// service will shut down if a 'Out of disk space' error.
					// Otherwise will fail in ProcessFile, and go down the
					// standard failure path.
					LogEvent(EVMSG_COPY_IN_WORK_FAILED, InPath + "\\" + InFile,
														workingFile,
														GetLastErrorString() );
				}
				else
				{
					// now check to see if we need to unwrap the file
					m_WrapperFile.SetAssociatedFile( workingFile );

					if ( m_WrapperFile.IsWrapped() )
					{
						if ( !m_WrapperFile.Delete() )
						{
							LogEvent(EVMSG_REMOVE_WRAPPER_FAILED, InPath + "\\" + InFile);
						}
					}
				}
			}

			RetVal = ProcessFile(InPath,
   							     InFile,
								 m_ConfigFile->m_WorkingDirectory,
								 OutFile);

			// 25-11-1999 P Barber PinICL - PC0033709
			// added *** || RetVal == CONTINUE_PROCESSING_LINK ***
			if ( RetVal == 0 || RetVal == CONTINUE_PROCESSING_LINK)
			{
				break;
			}
			else
			{
				Retries ++;

				OutStatusString = GetLastEventString();

				CString strRetries;
				strRetries.Format("%d", Retries);
				LogEvent(EVMSG_RETRY_WARNING, InPath + "\\" + InFile, strRetries);

				// don't retry again if we're not running!
				if ( !m_ConfigFile->IsLinkEnabled() )
					break;
			}

		} while (Retries < m_ConfigFile->m_RetryCount);

		InFullName.Format( "%s\\%s", InPath, InFile );
		OutFullName.Format( "%s\\%s", m_ConfigFile->m_WorkingDirectory, OutFile );

		// 25-11-1999 P Barber PinICL - PC0033709
		// added *** || RetVal == CONTINUE_PROCESSING_LINK ***
		if ( RetVal == 0 || RetVal == CONTINUE_PROCESSING_LINK )
		{
			
			SuccessfulPath(InFullName, wrapperWasStored, OutFullName, OutStatusString);

			// PC0114749 
			// This stops the application from going into a crazy loop!!!
			// when it cant rename a file

			if(GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_ACCESS_DENIED)
			{
				FailurePath(InFullName, wrapperWasStored, OutFullName, OutStatusString);
			}
			// end of addition

		}
		else
		{
			FailurePath(InFullName, wrapperWasStored, OutFullName, OutStatusString);

			if ( m_ConfigFile->IsStrictSequencing() )
			{
				// shut down 'strict sequencing' link due to a previous error or warning
				LogEvent(EVMSG_STRICT_SEQUENCING_ERROR, InPath + "\\" + InFile);
				m_ConfigFile->SetLinkToDisabled();
				break;
			}
		}

		
		if ( m_ConfigFile->IsStrictSequencing() )
		{
			if ( !WaitUntilDelivered() )
			{
				if ( m_ConfigFile->IsLinkEnabled() )
				{	// Failure file has been generated

					// shut down 'strict sequencing' link due to a previous error or warning
					LogEvent(EVMSG_STRICT_SEQUENCING_ERROR, InPath + "\\" + InFile);
					m_ConfigFile->SetLinkToDisabled();
					break;
				}
				else
				{	// Shutdown has been requested
					break;
				}
			}

			// If any files exist in the intransit directory, will
			// probably need to re-send current file. The easiest way
			// to achieve this is to jump out here and to re-poll.
			if ( FileInTransit() )
			{
				DebugMsg("Strict Sequencing - Waiting, moving on to next link");
				break;
			}
		}
		// 25-11-1999 P Barber PinICL - PC0033709
		// added *** || RetVal == CONTINUE_PROCESSING_LINK ***
		if ( m_CommandLine.m_ProcessMultiLinks && RetVal != CONTINUE_PROCESSING_LINK )
		{
			break;
		}

    }

}

bool CServiceHusk::ListAllFilesInDir(CStringList* FileList, const CString& ListDir ) const
{
	// Create object
	CFileFind FileFind;

	// List all files in directory specified. Looping
	// for each regular expression specified.
	for ( POSITION pos = m_ConfigFile->m_InPats.GetHeadPosition(); pos != NULL; )
	{
		// Set search criteria
		CString pattern = m_ConfigFile->m_InPats.GetNext(pos);
		CString FileWildCard;
		FileWildCard.Format("%s\\%s", ListDir, pattern);

		BOOL FileFound = FileFind.FindFile(FileWildCard, 0);

		if ( !FileFound )
		{
			// no files found -- is this an error?
			if ( ::GetLastError() != ERROR_FILE_NOT_FOUND )
				// yes it is an error - return to caller
				return false;
		}

		while (FileFound)
		{
			// Get next file
			FileFound = FileFind.FindNextFile();

			// Is file a directory
			BOOL FileIsDir = FileFind.MatchesMask(FILE_ATTRIBUTE_DIRECTORY);
			// Pragmatic check of FileFind's name matching
			BOOL NameOK = CheckNameOK(FileFind.GetFilePath(), pattern);

			// If not a directory and not "." or ".." (and not an invalid name),
			//  get Datestamp and filename then add to array
			if (!FileFind.IsDots() && !FileIsDir && NameOK)
			{
				// Filename and path
				CString FileName = FileFind.GetFilePath();
				CTime tmpTime;

				// Datestamp YYYYMMDDhhmmss
				if ( m_ConfigFile->m_SequenceType == CConfigSettings::Created )
				{
					FileFind.GetCreationTime(tmpTime);
				}
				else if ( m_ConfigFile->m_SequenceType == CConfigSettings::LastAccessed )
				{
					FileFind.GetLastAccessTime(tmpTime);
				}
				else
				{
					FileFind.GetLastWriteTime(tmpTime);
				}

				CString DateStamp = GetDateAndTimeAsString( tmpTime );

				// Search for file in list.
				// If already exists, do not add to list
				POSITION posInFileList = FileList->Find(DateStamp + FileName);

				// Does not exist, so add to list
				if (posInFileList == NULL)
				{
					// add to list in the format
					// YYYYMMDDhhmmss<file path><filename>
					FileList->AddTail(DateStamp + FileName);

					if (m_ConfigFile->m_Sequencing)
					{
						POSITION SortPos;
						POSITION tmpPos;
						POSITION CheckPos;
						POSITION MinusOnePos;
						CString  tmpString;

						for (SortPos = FileList->GetTailPosition(); SortPos != NULL; )
						{
							CheckPos = SortPos;

							CString CheckingFile = FileList->GetPrev(SortPos);

							MinusOnePos = SortPos;
							tmpPos = SortPos;

							if ( MinusOnePos != NULL)
							{
								// check if needs swapping
								CString MinusOneFile;

								MinusOneFile = FileList->GetPrev(tmpPos);

								if (CheckingFile < MinusOneFile)
								{
									// swap
									FileList->SetAt(CheckPos, MinusOneFile);
									FileList->SetAt(MinusOnePos, CheckingFile);
								}
							}
						}
					}
				}
			}
		}
	}

	return true;
}

DWORD CServiceHusk::ProcessFile(CString InPath,
							    CString InFile,
							    CString OutPath,
							    CString OutFile)
{
    DebugMsg("CServiceHusk::ProcessFile()");

	CString root;

	root = REG_FTMS_ROOT;
	root = root + m_ConfigFile->m_LinkName + m_ServiceType;

	UINT ReturnStatus = m_ProcessFilePointer(root,
											 InPath, InFile,
											 OutPath, OutFile);

	if ( ReturnStatus == 1 )
		return 0;
	else
		return 1;
}


void CServiceHusk::ClearWorkingDirectory()
{
	if ( m_ConfigFile->m_WorkingDirectory.IsEmpty() )
		return;

	BOOL FileFound = TRUE;

	CFileFind* KillFile = new CFileFind;

	CString KillPath = m_ConfigFile->m_WorkingDirectory + "\\*.*";

	if ( KillFile->FindFile(KillPath, 0) )
	{
		while ( FileFound )
		{
			FileFound = KillFile->FindNextFile();

			CString delfile = KillFile->GetFilePath();

			if ( !KillFile->IsDots() && !KillFile->IsReadOnly() && !KillFile->IsDirectory() )
			{
				try
				{
					CFile::Remove( delfile );
				}

				catch ( CFileException* e )
				{
					TCHAR    szCause[255];

					e->GetErrorMessage(szCause, 255);

					DebugMsg( "Remove file %s failed: %s", delfile, szCause );

					e->Delete();
				}
			}
		}
	}

	delete KillFile;
}

BOOL CServiceHusk::ServiceStartUp()
{
	return TRUE;
}

void CServiceHusk::SuccessfulPath(CString InFile,
								  BOOL    wrapperWasStored,
								  CString OutFile,
								  CString OutStatusString)
{
	if ( wrapperWasStored && m_restoreWrapperToOutputFile )
	{
		m_WrapperFile.SetSuccessStatus();
		m_WrapperFile.SetAssociatedFile( OutFile );

		// move succesful info to wrapper
		if ( !m_WrapperFile.Write() )
		{
			LogEvent(EVMSG_ADD_WRAPPER_FAILED, OutFile);
			return;
		}
	}

	CString RenamedFileName = DoSearchReplace( m_ConfigFile->m_OutRenameExp, OutFile );

	CString FullFileName = m_ConfigFile->m_OutDirectory + '\\' + GetJustFileName( RenamedFileName );

	if ( MoveFileEx(OutFile,
					FullFileName,
					MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH )
					== 0 )
	{
		DWORD err = GetLastError();


		LogEvent( EVMSG_MOVE_OUT_FAILED, OutFile,
			FullFileName, GetErrorMessage( err, true ) );


		return;
	}

	// Write success message to event log
	LogEvent(EVMSG_SUCCESS_DELIVERY, FullFileName);

	// Del in file
	if ( DeleteFile(InFile) == 0 )
	{
		LogEvent(EVMSG_IN_DELETE_FAILED, InFile,
			GetErrorMessage( GetLastError(), true ) );
		return;
	}
}

void CServiceHusk::FailurePath(CString InFile,
							   BOOL wrapperWasStored,
							   CString OutFile,
							   CString OutStatusString)
{
	CString RenamedFileName;
	CString JustFileName;
	CString TempString;
	CString AckFileName;

	if ( wrapperWasStored )
		m_WrapperFile.SetFailureStatus();

	// Create Negative Acknowledgement - this uses a lot of information from the
	// wrapper -- if input file was not wrapped, this will be blank.  But still
	// better than nothing.
	if ( !m_ConfigFile->m_AckInDir.IsEmpty() )
	{
		DebugMsg( "Generating negative acknowledgement for %s...", m_WrapperFile.GetOriginalFileName() );

		RenamedFileName = DoSearchReplace(m_ConfigFile->m_AckInRenameExp, m_WrapperFile.GetOriginalFileName() );
		JustFileName = GetJustFileName(RenamedFileName);
		AckFileName.Format("%s\\%s.%s", m_ConfigFile->m_AckInDir, GetUniqueId( GetJustFileName(OutFile) ),
						JustFileName);

		if ( !CreateAckFile( AckFileName, InFile, OutStatusString, false ) )
		{
			LogEvent(EVMSG_ACK_FAILED, "Negative", AckFileName, OutFile );
			return;
		}
	}
	else
	{
		// generate error
		LogEvent( EVMSG_ACK_RETURN_LEG_FAILURE, InFile, OutStatusString );
	}

	// Write error to event log
	LogEvent(EVMSG_ERROR, InFile);

	// Move file from in dir to error dir
	// rename if required
	RenamedFileName = DoSearchReplace(m_ConfigFile->m_ErrorRenameExp, InFile );
	JustFileName = GetJustFileName(RenamedFileName);
	TempString.Format("%s\\%s", m_ConfigFile->m_ErrorDirectory, JustFileName);

	if ( m_FailureRetainInFile )
	{
		DebugMsg( "Copying to error directory, %s -> %s...", InFile, TempString  );

		if ( CopyFile(InFile, TempString, FALSE) == 0 )
		{
			LogEvent(EVMSG_COPY_FAILED, InFile, TempString,
				GetErrorMessage( GetLastError(), true ) );
			return;
		}
	}
	else
	{
		DebugMsg( "Moving to error directory, %s -> %s...", InFile, TempString  );
		

		if ( !MoveFileEx(InFile, TempString, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING ) == 0 )
		{
			
			// Check if file exists before outputing this message
			DWORD dwAttr = GetFileAttributes(TempString);
			if(dwAttr == 0xffffffff)
			{
				LogEvent(EVMSG_MOVE_ERROR_FAILED, InFile, TempString,
					GetErrorMessage( GetLastError(), true ) );
				DebugMsg( "MOVEFILEEX CHECK");
			}
			return;
			
		}
	}

	// create error file in error dir, which is copy of acknowledgement
	RenamedFileName = DoSearchReplace(m_ConfigFile->m_ErrorLogRenameExp, InFile );
	JustFileName = GetJustFileName(RenamedFileName);
	TempString.Format("%s\\%s", m_ConfigFile->m_ErrorDirectory, JustFileName);

	DebugMsg("Generating error file %s...", TempString );

	// This is not a ack file, but a error file
	// but contents is the same as a ack file so using ack class to produce
	if ( !CreateAckFile( TempString, InFile, OutStatusString, FALSE ) )
	{
		LogEvent(EVMSG_ERROR_LOG_FAILED, TempString);
		return;
	}

	if ( m_copyUnwrappedToWorking )
	{
		CString DelFileName = GetJustFileName(InFile);

		DelFileName = m_ConfigFile->m_WorkingDirectory + "\\" + DelFileName;

		CFileStatus FileStatus;

		if( CFile::GetStatus( DelFileName, FileStatus ) )
		{
			if ( DeleteFile(DelFileName) == 0 )
			{
				LogEvent(EVMSG_WORKING_DELETE_FAILED, m_ConfigFile->m_WorkingDirectory + "\\" + InFile,
					GetErrorMessage( GetLastError(), true ) );
				return;
			}
		}
	}

	DebugMsg("Failure path complete.");

}

bool CServiceHusk::CreateAckFile(
	const CString& AckFileName,
	const CString& DeliveredFileName,
	const CString& FailureReason,
	bool isPositive,
	const CString& ArchiveFileName ) const
{
	CAcknowledgement ackFile;

	// Create acknowledgement
	ackFile.Initialise(AckFileName);

	// Set data members
	if ( m_ConfigFile )
		ackFile.m_ServiceID.Format("%s %s", m_ConfigFile->m_LinkName, m_ServiceType);
	else
		if ( !m_CommandLine.m_ProcessMultiLinks )
			ackFile.m_ServiceID.Format("%s %s", m_CommandLine.m_ServiceName, m_ServiceType);
		else
			ackFile.m_ServiceID = m_CommandLine.m_ServiceName;

	if ( isPositive )
		ackFile.SetPositiveStatus();
	else
		ackFile.SetNegativeStatus();

	ackFile.m_FailureReason = FailureReason;

	// Original file
	const CString FullFileName = m_WrapperFile.GetOriginalFileName();

	ackFile.m_OriginalFileName = GetJustFileName( FullFileName );
	ackFile.m_OriginalPath = GetJustPath( FullFileName );

	ackFile.m_OriginalMachineID = m_WrapperFile.GetMachineName();

	ackFile.m_OriginalFileSize = m_WrapperFile.GetOriginalFileSize();
	ackFile.m_OriginalDateStamp = m_WrapperFile.GetDateStamp();
	ackFile.m_OriginalCRC = m_WrapperFile.GetFileCRC();

	// Delivered File
	ackFile.m_DeliveredFileName = GetJustFileName( DeliveredFileName );
	ackFile.m_DeliveredPath = GetJustPath( DeliveredFileName );

	ackFile.m_DeliveredMachineID = GetMachineName( DeliveredFileName );

	CFileStatus FileStatus;

	if( CFile::GetStatus( DeliveredFileName, FileStatus ) )
		ackFile.m_DeliveredFileSize.Format( "%lu", FileStatus.m_size );

	ackFile.m_DeliveredDateStamp = GetDateAndTimeAsString( CTime::GetCurrentTime() );

	DebugMsg( "CreateAckFile: deliveredCRCisValid = %s",
		IsDeliveredCRCvalid() ? "Yes" : "No" );

	if ( IsDeliveredCRCvalid() )
		ackFile.m_DeliveredCRC.Format("%lu", GetDeliveredCRC() );

	ackFile.SetArchiveFileName( ArchiveFileName );

	// Write details to file
	ackFile.Write();

	return TRUE;
}



void CServiceHusk::GetServiceSpecificSettings()
{

}

BOOL CServiceHusk::CheckServiceSpecificSettings(bool Installing)
{
	return true;
}

BOOL CServiceHusk::CheckDLLFunctionExists()
{
	// Load DLL that does work
	if (m_ConfigFile->m_ProcessFileDLL.IsEmpty())
		return FALSE;

	if (m_ConfigFile->m_ProcessFileDLL == INLINE_CODE )
		return TRUE;

	m_DLLHandle = LoadLibrary(m_ConfigFile->m_ProcessFileDLL);

	if (m_DLLHandle != NULL)
	{
	   m_ProcessFilePointer = (DLLHANDLE)GetProcAddress(m_DLLHandle,
											   "DLLProcessFile");
	   if (!m_ProcessFilePointer)
		   return FALSE;
	}
	else
		return FALSE;

	return TRUE;
}

void CServiceHusk::CheckOverdueFiles()
{
}

CString CServiceHusk::DoSearchReplace(CSearchAndReplace & r, const CString & source)
{
	CString retVal = r.Replace( source );

	if ( retVal.IsEmpty() )
	{
		LogEvent( EVMSG_EXPRESSION_NO_MATCH, source, r.GetLastErrorString() );

		retVal = source;
	}

	return retVal;
}

bool CServiceHusk::CheckValidRegEx(const CSearchAndReplace & r, LPCSTR regexName)
{
	if ( !r.IsValid() )
	{
		LogEvent(EVMSG_BAD_EXPRESSION,
			regexName, r.GetLastErrorString() );

		return false;
	}

	return true;
}

bool CServiceHusk::InstallService()
{
	// Request to install.
	if ( IsInstalled() )
	{
		printf("%s is already installed\n", m_CommandLine.m_ServiceName);
		return false;
	}

	// Try and install the copy that's running
	if ( !Install() )
	{
		printf("%s failed to install. Error %d\n", m_CommandLine.m_ServiceName, GetLastError());
		return false;
	}

	printf("%s was successfully installed\n", m_CommandLine.m_ServiceName);

	if ( !m_CommandLine.m_ProcessMultiLinks )
	{
		int pos = m_CommandLine.m_ServiceName.FindOneOf(" ");

		if ( pos != -1 )
			m_CommandLine.m_ServiceName = m_CommandLine.m_ServiceName.Left(pos);

		CreateRegistryDetails();
	}

	return true;
}

void CServiceHusk::CreateRegistryDetails()
{
	New_ServiceSpecificSettings();

	Initialize_ServiceSpecificSettings();

	m_ConfigFile->SetDefaultSettings(m_CommandLine.m_ServiceName,
									 m_ServiceType,
									 m_CommandLine.m_LinkRoot);

	if ( m_CommandLine.m_ProcessMultiLinks )
		m_ConfigFile->ReadRegistryBool("SharedLink", "Yes", true);
	else
		m_ConfigFile->ReadRegistryBool("SharedLink", "No", true);

	GetServiceSpecificSettings();
	m_ConfigFile->ReadSettings();
	m_ConfigFile->IsValid( this, true );
	CheckServiceSpecificSettings(true);

	Delete_ServiceSpecificSettings();
}

void CServiceHusk::RemoveRegistryDetails()
{
	CString LinkName;

	if ( m_CommandLine.m_ProcessMultiLinks )
		LinkName = m_CommandLine.m_ServiceName;
	else
	{
		int pos = m_CommandLine.m_ServiceName.FindOneOf(" ");

		if ( pos != -1 )
			LinkName = m_CommandLine.m_ServiceName.Left(pos);
		else
			LinkName = m_CommandLine.m_ServiceName;
	}

	m_ConfigFile->SetDefaultSettings(LinkName,
									 m_ServiceType,
									 m_CommandLine.m_LinkRoot);

	m_ConfigFile->RemoveRegistryLink();
}

bool CServiceHusk::UnInstallService()
{
	if ( !IsInstalled() )
	{
		printf("%s is not installed\n", m_CommandLine.m_ServiceName);
		return false;
	}

	// Try and remove the copy that's installed
	if ( Uninstall() )
	{
		// Get the executable file path
		char szFilePath[_MAX_PATH];

		::GetModuleFileName(NULL, szFilePath, sizeof(szFilePath));

		printf("%s was successfully removed\n", m_CommandLine.m_ServiceName);
	}
	else
	{
		printf("Could not remove %s. Error %d\n", m_CommandLine.m_ServiceName, GetLastError());
		return false;
	}

	return true;
}

BOOL CServiceHusk::MoveFileEx(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName,DWORD dwFlags)
{
	// If file is to be moved to a differentr drive, and copy allowed has been specified
	// create a temporary directory to copy file to, then move to destination.
	// This is required so that when files are delivered to the destination, only fully
	// written files are made available.





	// Added P Barber 10-02-2000.  Ref: PinICL PC0038107
	// guarantee that a move perfomed as a copy and delete operation
	// is flushed to disk before the function returns
	dwFlags |=  MOVEFILE_WRITE_THROUGH;



	DebugMsg("Copying DACL");
	// move doesn't change permissions on file, so change them before transfer, then
	// permissions are appropriate to target directory immediately on arrival
	const CString & lpDir = GetJustPath(lpNewFileName);

	if ( !CopyDACL(lpDir, lpExistingFileName) ) // set file permissions to those of directory
	{
		DebugMsg("Failed to copy DACL");

		DWORD dwAttr = GetFileAttributes(lpExistingFileName);
		DWORD dwAttr1 = GetFileAttributes(lpNewFileName);
		if((dwAttr == 0xffffffff) || (dwAttr1 == 0xffffffff))
		{
			LogEvent(EVMSG_MOVE_ERROR,
				lpExistingFileName,
				lpNewFileName,
				GetErrorMessage( GetLastError(), true ) );
			
		}

		return FALSE;

	}

	if (dwFlags & MOVEFILE_COPY_ALLOWED)
	{
		dwFlags ^= MOVEFILE_COPY_ALLOWED;

		DebugMsg("Attempting to move without COPY attribute %s -> %s", lpExistingFileName, lpNewFileName);

		if ( !::MoveFileEx(lpExistingFileName, lpNewFileName, dwFlags) )
		{
			if ( GetLastError() != 17 )	// Only interested in connot move to
			{							// a diferent disk error
				DebugMsg("Failed to move %s -> %s, GetLastError -> %s", lpExistingFileName, lpNewFileName, GetErrorMessage( GetLastError(), true ));


				LogEvent(EVMSG_MOVE_ERROR,
					     lpExistingFileName,
						 lpNewFileName,
						 GetErrorMessage( GetLastError(), true ) );
				return FALSE;

			}

			DebugMsg("Move across drives requested. Copying File...");

			// Check that there is enough free space to copy file.
			// NFS to sequent hangs when out of disk space!!!!
			// Pick up file times for restoring after copy destroys.
			CFileStatus FileStatus;
			DWORD SpaceRequired = 0;
			CTime CreatedTime, ModifiedTime, AccessedTime;

			DebugMsg("Getting file size");

			if( CFile::GetStatus( lpExistingFileName, FileStatus ) )
			{
				DebugMsg("Successfully got file size - %s ", lpExistingFileName);

				SpaceRequired = FileStatus.m_size;
				CreatedTime = FileStatus.m_ctime;
				ModifiedTime = FileStatus.m_mtime;
				AccessedTime = FileStatus.m_atime;
			}

			ULARGE_INTEGER lpFreeBytes;
			ULARGE_INTEGER lpTotalBytes;

			LONG fileSize = SpaceRequired;

			DebugMsg("Checking free disk space");

			if ( GetDiskFreeSpaceEx(lpDir,
									&lpFreeBytes,
									&lpTotalBytes,
									NULL) )
			{
				// only test against lowpart as GetStatus only returns 32 bits
				// if high bit is set, assume enough space is avaliable
				if ( (lpFreeBytes.HighPart == 0) && (SpaceRequired > lpFreeBytes.LowPart) )
				{
					DebugMsg("Not enough free disk space on destination -> %s", lpDir);

					LogEvent(EVMSG_COPY_FAILED,
							 lpExistingFileName,
							 lpNewFileName,
							 "Not enough free disk space on destination, Unable to copy file." );
					SetLastError(ERROR_DISK_FULL);
					return FALSE;
				}

				DebugMsg("Enough free space %i", lpFreeBytes.LowPart);
			}

			DebugMsg("Checking FTMStmp exists");

			CString path = GetJustPath(lpNewFileName) + "\\FTMStmp";

			if ( !IsDirectory(path) )
			{
				// createdirectory
				if ( !CreateDirectory(path, NULL) )
				{
					DebugMsg("Failed to create directory %s", path);
					LogEvent(EVMSG_DIR_CREATE_FAILED, path);
					return FALSE;
				}

				DebugMsg("%s created", path);
			}

			path = path + "\\" + GetJustFileName(lpNewFileName);

			DebugMsg("Copying file %s -> %s", lpExistingFileName, path);

			if ( !CopyFile(lpExistingFileName, path, FALSE) )
			{
				DebugMsg("Failed to copy file %s -> %s, GetLastError -> %s",lpExistingFileName, path, GetErrorMessage( GetLastError(), true ));

				LogEvent(EVMSG_COPY_FAILED,
					     lpExistingFileName,
						 path,
						 GetErrorMessage( GetLastError(), true ) );
				return FALSE;
			}

		// Only check file sizes if not copying to a different machine.
			// PC0999996 - PostProcess seems to have problems when
			// checking the size of files across NFS Maestro
			//
			//
			// We will however add a wait period here too
			//
			// In a nutshell, if files are moved across the same machine
			// we just check the file sizes to ensure integrity
			// If they are copied across machines we use a CRC check

			//DebugMsg("File size is: %d", SpaceRequired);

			CString iTargetDir = GetJustPath(lpNewFileName) + "\\";
			if (GetDriveType(iTargetDir) == DRIVE_FIXED)
			{
				DebugMsg("Wait for a 10 seconds to ensure that copy is complete");

				// Wait for a 10 seconds to ensure that copy is complete

				if ( m_shutDownEvent.Lock( 10000 ) )
				{
					// block was successful, time to exit loop
				}

				DebugMsg("Copy wait complete");

				DebugMsg("Checking size of delivered file");

				// Check that copied file is the same size as the original
				// if not fail
				// restore file times
				if( CFile::GetStatus( lpExistingFileName, FileStatus ) )
				{
					DebugMsg("Getting size of initial file");
					LONG ExistingSize = FileStatus.m_size;

					if( CFile::GetStatus( path, FileStatus ) )
					{
						DebugMsg("Getting size of delivered file");
						LONG pathSize = FileStatus.m_size;

						if ( ExistingSize != pathSize )
						{
							DebugMsg("File only partially written -> %s, GetLastError -> %s", path, GetErrorMessage( GetLastError(), true ));

							LogEvent(EVMSG_COPY_FAILED,
									 lpExistingFileName,
									 path,
									 "Disk full. File only partially written." );
							return FALSE;
						}

						// restore file times destroyed by copy
						DebugMsg("Restoring File times");
						FileStatus.m_ctime = CreatedTime;
						FileStatus.m_mtime = ModifiedTime;
						FileStatus.m_atime = AccessedTime;
						CFile::SetStatus(path, FileStatus);
					}
				}

			}
			else
			{
				// file copied across to a different machine, wait a bit
				if(fileSize > 1048576)
				{
					DebugMsg("Wait for a 30 seconds to ensure that copy is complete");
					DebugMsg("File size greater than 1MB");

					// Wait for a 30 seconds to ensure that copy is complete

					if ( m_shutDownEvent.Lock( 30000 ) )
					{
						// block was successful, time to exit loop
					}

					DebugMsg("Copy wait complete");
				}
				else
				{
					DebugMsg("Wait for a 10 seconds to ensure that copy is complete");

					//Wait for a 10 seconds to ensure that copy is complete

					if ( m_shutDownEvent.Lock( 10000 ) )
					{
						// block was successful, time to exit loop
					}

					DebugMsg("Copy wait complete");
				}

			}

			/////////////////////////////////
			//
			// Check file size removed - END
			//
			//////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////
			//
			// THIS IS A PRAGMATIC FIX INSERTED FOR RELEASE 2 ONLY.  IT IS DESIGNED TO CHECK,
			// BY CRC THAT ANY FILE COPIED VIA NFS TO A SEQUENT IS VALID AFTER THE WRITE.
			// Inserting it here and invoking it only when the target file is not a fixed disc,
			// therefore assumed to be a network drive (as given by GetDriveType) is believed
			// sufficiently precise to identify only "writes to Sequent" given the actual usage
			// of MoveFileEx and the actual architectures of the links existing at Release 2.
			//

			DebugMsg("Retrieving registry setting for CheckAfterWriteViaNFS");
			if (m_ConfigFile->m_CheckAfterWriteViaNfs) /* Only Post Process can have CheckAfterWriteViaNfs Enabled */
			{
				DWORD crcSource, crcTarget;
				DebugMsg("Getting Path of new file name %s ", lpNewFileName);
				CString TargetDir = GetJustPath(lpNewFileName) + "\\";
				DebugMsg("Target Directory is %s :", TargetDir);
				DebugMsg("Getting Drive Type");

				//####################################################################
				// this <GetDriveType> might be the cause of the problem with PC099996
				//####################################################################

				if (GetDriveType(TargetDir) != DRIVE_FIXED)
				{
					DebugMsg("Write via NFS. Checking CRC on files %s and %s", lpExistingFileName, path);

					if ( !CalculateCrc( crcSource, lpExistingFileName ) )
					{
						LogEvent(EVMSG_CRC_FAILED, lpExistingFileName );
						return FALSE;
					}
					if ( !CalculateCrc( crcTarget, path ) )
					{
						LogEvent(EVMSG_CRC_FAILED, path );
						return FALSE;
					}
					if ( crcSource != crcTarget	)
					{
						LogEvent(EVMSG_COPY_FAILED,
							lpExistingFileName,
							path,
							"no error, but FTMS CheckAfterWriteViaNfs detected CRC mismatch" );
						return FALSE;
					}
				}
			}
			// END OF PRAGMATIC FIX INSERTED FOR RELEASE 2 ONLY.
			// See CConfigSettings for details m_CheckAfterWriteViaNfs

			DebugMsg("Moving file %s -> %s", path, lpNewFileName);



			if ( !::MoveFileEx(path, lpNewFileName, dwFlags) )
			{


				DebugMsg("Failed to move %s -> %s, GetLastError -> %s", path, lpNewFileName, GetErrorMessage( GetLastError(), true ) );

				LogEvent(EVMSG_MOVE_ERROR,
				     path,
					 lpNewFileName,
					 GetErrorMessage( GetLastError(), true ) );

				/////////////////////////////////////////////////////////////
				// Added to simulate file not found error in PC0096724
				// No longer required, but left to just in case! MILTON
				//
				// SetLastError(ERROR_FILE_NOT_FOUND);
				/////////////////////////////////////////////////////////////

				return FALSE;

			}

			DebugMsg("Deleting %s", lpExistingFileName);

			if ( !DeleteFile(lpExistingFileName) )
			{
				LogEvent(EVMSG_IN_DELETE_FAILED, lpExistingFileName, GetLastErrorString() );
				return FALSE;
			}
		}

		dwFlags ^= MOVEFILE_COPY_ALLOWED;

		return TRUE;

	}

	return ::MoveFileEx(lpExistingFileName, lpNewFileName, dwFlags);
}

bool CServiceHusk::WaitUntilDelivered()
{
	// Do nothing, will be overloaded in derived class
	return true;
}

bool CServiceHusk::FileInTransit()
{
	// Do nothing, will be overloaded in derived class
	return false;
}

// Added P Barber 01-02-2000.  Ref: PinICL PC0034206
bool CServiceHusk::FailFileInTransit()
{
	// Do nothing, will be overloaded in derived class
	return false;
}

bool CServiceHusk::ReSendFile(const CString & FileToTest)
{
	// Do nothing, will be overloaded in derived class
	return false;
}

bool CServiceHusk::CopyFileCheck(const CString & source, const CString & dest)
{
	// Firstly copy file to a temporary directory, then
	// move to the destination. This ensures that if the destination is
	// being polled, only fully written files are picked up.

	// Check that there is enough free space to copy file.
	// NFS hangs when out of disk space!!!!
	CFileStatus FileStatus;
	DWORD SpaceRequired = 0;

	if( CFile::GetStatus( source, FileStatus ) )
		SpaceRequired = FileStatus.m_size;

	ULARGE_INTEGER lpFreeBytes;
	ULARGE_INTEGER lpTotalBytes;

	const CString & lpDir = GetJustPath(dest);

	if ( GetDiskFreeSpaceEx(lpDir,
							&lpFreeBytes,
							&lpTotalBytes,
							NULL) )
	{
		// only test against lowpart as GetStatus only returns 32 bits
		// if high bit is set, assume enough space is avaliable
		if ( (lpFreeBytes.HighPart == 0) && (SpaceRequired > lpFreeBytes.LowPart) )
		{
			DebugMsg("Not enough free disk space on destination -> %s", lpDir);

			LogEvent(EVMSG_COPY_FAILED,
					 source,
					 dest,
					 "Not enough free disk space on destination, Unable to copy file." );

			SetLastError(ERROR_DISK_FULL);

			return false;
		}
	}

	CString path = GetJustPath(dest) + "\\FTMStmp";

	if ( !IsDirectory(path) )
	{
		// createdirectory
		if ( !CreateDirectory(path, NULL) )
		{
			DebugMsg("CFC - Failed to create directory %s", path);
			LogEvent(EVMSG_DIR_CREATE_FAILED, path);
			return false;
		}
	}

	path = path + "\\" + GetJustFileName(dest);

	DebugMsg("Copying file %s -> %s", source, path);

	if ( !CopyFile(source, path, FALSE) )
	{
		LogEvent(EVMSG_COPY_FAILED,
				 source,
				 path,
				 GetErrorMessage( GetLastError(), true ) );
		return false;
	}

	// Check that copied file is the same size as the original
	// if not fail
	if( CFile::GetStatus( source, FileStatus ) )
	{
		LONG ExistingSize = FileStatus.m_size;

		if( CFile::GetStatus( path, FileStatus ) )
		{
			LONG pathSize = FileStatus.m_size;

			if ( ExistingSize != pathSize )
			{
				DebugMsg("File only partially written -> %s", path);

				LogEvent(EVMSG_COPY_FAILED,
						 source,
						 path,
						 "Disk full. File only partially written." );
				return false;
			}
		}
	}

	DebugMsg("Moving file %s -> %s", path, dest);

	if ( !::MoveFileEx(path, dest, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) )
	{
		/*
		LogEvent(EVMSG_MOVE_ERROR,
				 path,
				 dest,
				 GetErrorMessage( GetLastError(), true ) );
		return false;
		*/

		LogEvent(EVMSG_MOVE_ERROR,
				 path,
				 dest,
				 "CopyFileCheck Failed" );
		return false;
	}

	return true;
}

BOOL CServiceHusk::CopyDACL(const CString & Source, const CString & Target)
{
// CopyDACL takes the discretionary permissions of the Source directory (or file)
// and applies them to the Target file (or directory). Any errors on FALSE return are in
// GetLastError().
	BOOL bRes;
	PSECURITY_DESCRIPTOR pSecurityDescriptor = 0;
	DWORD nLengthNeeded = 9999;   // address of required size of buffer

	// This call will fail and return the size of buffer required
	bRes = GetFileSecurity(
		Source,       // address of string for file name
		DACL_SECURITY_INFORMATION,  // requested information
		pSecurityDescriptor,        // address of security descriptor
		0,            // size of security descriptor buffer ZERO USED TO FIND SIZE
		&nLengthNeeded);   // address of required size of buffer

	if ((pSecurityDescriptor = malloc (nLengthNeeded)) == 0)
		return FALSE;

	bRes = GetFileSecurity(
		Source,       // address of string for file name
		DACL_SECURITY_INFORMATION,  // requested information
		pSecurityDescriptor,        // address of security descriptor
		nLengthNeeded,            // size of security descriptor buffer
		&nLengthNeeded);   // address of required size of buffer

	if (bRes != TRUE)
	{
		free (pSecurityDescriptor);
		return FALSE;
	}

	bRes =  SetFileSecurity(
		Target,  // address of string for filename
		DACL_SECURITY_INFORMATION,	// type of information to set
		pSecurityDescriptor        // address of security descriptor
		);

	if (bRes != TRUE)
	{
		free (pSecurityDescriptor);
		return FALSE;
	}

	free (pSecurityDescriptor);
	return TRUE;
}

bool CServiceHusk::GetLinkSettings()
{
	DebugMsg("Reading general config settings");

	m_ConfigFile->ReadVolatileSettings();

	m_ConfigFile->ReadSettings();

	DebugMsg("Checking general config settings");

	if ( !m_ConfigFile->IsValid( this ) )
	{
		DebugMsg("General config in-correct!!!");
		m_ConfigFile->SetLinkToDisabled();
		return false;
	}

	DebugMsg("Reading service specific settings");
	GetServiceSpecificSettings();

	if ( !CheckServiceSpecificSettings() )
	{
		DebugMsg("Service specific settings incorrect!!!");
		m_ConfigFile->SetLinkToDisabled();
		return false;
	}

	return true;
}

void CServiceHusk::ProcessLink()
{
	PollDirectories();

	CheckOverdueFiles();
}

void CServiceHusk::WaitPollInterval()
{
	// Calcualate how long we should wait before next processing
	DWORD endTime = ::GetTickCount();

	DWORD sleepTime = m_ConfigFile->m_PollInterval;

	// GetTickCount returns the number of milliseconds since the system
	// was started.  Every 50 days or so, it wraps round.  So, if the
	// endTime is less than the start time, we've wrapped.
	if ( endTime < m_ConfigFile->m_StartTime )
	{
		// 50 days is up!
		// Play on the side of caution, we may have been very busy.
		// So don't sleep at all in this case.
		sleepTime = 0;
	}
	else
	{
		DWORD duration = endTime - m_ConfigFile->m_StartTime;

		if ( duration > sleepTime )
		{
			// processing took more than the whole period, so don't sleep at all.
			sleepTime = 0;
		}
		else
			sleepTime -= duration;
	}

//	TRACE( "sleepTime = %d\n", sleepTime );
//	DebugMsg( "sleepTime = %d\n", sleepTime );

	if ( sleepTime )
	{
		// Don't actually sleep: instead wait on the (normally blocking) shutdown
		// event.
		if ( m_shutDownEvent.Lock( sleepTime ) )
		{
			// block was successful, time to exit loop!
			DebugMsg("WaitPollInterval complete");
		}
	}

	m_ConfigFile->m_StartTime = ::GetTickCount();
}

