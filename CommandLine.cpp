// CommandLine.cpp: implementation of the CCommandLine class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "CommandLine.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCommandLine::CCommandLine()
{
	m_Debug = false;
	m_Install = false;
	m_UnInstall = false;
	m_ProcessMultiLinks = false;
	m_AddRegistryLink = false;
	m_RemoveRegistryLink = false;
	m_Compress = false;
	m_Decompress = false;
	m_DisplaySyntax = false;

	m_ServiceName.Empty();
	m_StartupType.Empty();
	m_Crypto.Empty();

	m_User = FTMS_DEFAULT_USER;
	m_Password = FTMS_DEFAULT_PASSWORD;

	m_LinkRoot = FTMS_LINK_ROOT;

	m_DependentServices.RemoveAll();
}

CCommandLine::~CCommandLine()
{

}

bool CCommandLine::ValidateCommandLine(int argc, char* argv[])
{
	int LoginPos = 0, DependencyPos = 0, LinkRootPos = 0, StartupPos = 0;

	for (int loop = 2; loop < argc; ++loop)
	{ 
		if ( _stricmp(argv[loop], "-debug" ) == 0 )
		{
			m_Debug = true;
		}
		else if ( _stricmp(argv[loop], "-i" ) == 0 )
		{
			m_Install = true;
		}
		else if ( _stricmp(argv[loop], "-u") == 0 ) 
		{
			m_UnInstall = true;
		}
		else if ( _stricmp(argv[loop], "-l") == 0 ) 
		{
			if ( loop > argc - 3 )
			{
				printf("ERROR: -l <User name and password must be specified.>\n");
				return false;
			}

			LoginPos = loop;
		}
		else if ( LoginPos != 0 && loop == LoginPos + 1 )
		{
			m_User = argv[loop];
		}
		else if ( LoginPos != 0 && loop == LoginPos + 2 )
		{
			m_Password = argv[loop];
		}
		else if ( _stricmp(argv[loop], "-d") == 0 )
		{
			if ( loop > argc - 2 )
			{
				printf("ERROR: -d <At least one Dependent service name must be specified.>\n");
				return false;
			}

			DependencyPos = loop;
		}
		else if ( DependencyPos != 0 && loop == DependencyPos + 1 )
		{
			// need to store in a string list
			m_DependentServices.AddTail( argv[loop] );
		}
		else if ( _stricmp(argv[loop], "-root") == 0 )
		{
			if ( loop > argc - 2 )
			{
				printf("ERROR: -root <Path must be specified.>\n");
				return false;
			}

			LinkRootPos = loop;
		}
		else if ( LinkRootPos != 0 && loop == LinkRootPos + 1 )
		{
			m_LinkRoot = argv[loop];

			if ( m_LinkRoot.Right(1) == "\\" )
			{
				int len = m_LinkRoot.GetLength();
				m_LinkRoot = m_LinkRoot.Left(len - 1);
			}
		}
		else if ( _stricmp(argv[loop], "-s") == 0 )
		{
			if ( loop > argc - 2 )
			{
				printf("ERROR: -s <Startup type must be specified.>");
				return false;
			}

			StartupPos = loop;
		}
		else if ( StartupPos != 0 && loop == StartupPos + 1 )
		{
			m_StartupType = argv[loop];

			if ( m_StartupType.CompareNoCase("Automatic") == 0
				 || m_StartupType.IsEmpty() )
			{
				m_StartupType = "Automatic";
			}
			else if ( m_StartupType.CompareNoCase("Manual") == 0 )
			{
				m_StartupType = "Manual";
			}
			else if ( m_StartupType.CompareNoCase("Disabled") == 0 )
			{
				m_StartupType = "Disabled";
			}
			else 
			{
				printf("ERROR: -s <Startup type not valid.>");
				return false;
			}
		}
		else if ( _stricmp(argv[loop], "-multi") == 0 )
		{
			m_ProcessMultiLinks = true;
		}
		else if ( _stricmp(argv[loop], "-add" ) == 0 )
		{
			m_AddRegistryLink = true;
			m_ProcessMultiLinks = true;
		}
		else if ( _stricmp(argv[loop], "-remove" ) == 0 )
		{
			m_RemoveRegistryLink = true;
		}
		else if ( _stricmp(argv[loop], "-compress" ) == 0 )
		{
			m_Compress = true;
		}
		else if ( _stricmp(argv[loop], "-decompress" ) == 0 )
		{
			m_Decompress = true;
		}
		else if ( _stricmp(argv[loop], "-sign" ) == 0 )
		{
			m_Crypto = "Sign";
		}
		else if ( _stricmp(argv[loop], "-verify" ) == 0 )
		{
			m_Crypto = "Verify";
		}
		else if ( _stricmp(argv[loop], "-encrypt" ) == 0 )
		{
			m_Crypto = "Encrypt";
		}
		else if ( _stricmp(argv[loop], "-decrypt" ) == 0 )
		{
			m_Crypto = "Decrypt";
		}
		else if ( _stricmp(argv[loop], "-?" ) == 0 )
		{
			m_DisplaySyntax = true;
		}
	}

	return true;
}

void CCommandLine::DisplaySyntax()
{
	printf("\n");
	printf("SYNTAX: <exe name> <service name> optional parameters...\n");
	printf("\n");
	printf("Optional Parameters\n");
	printf("\n");
	printf("Installation parameters\n");
	printf("-i                             - Install service.\n");
	printf("-l <user> <password>           - Sets user that the service is to run as.\n");
	printf("-d <service name>              - Sets dependant services. Platforms with many\n");
	printf("                                 links may need this set to 'messenger',\n");
	printf("                                 otherwise it tends to time-out. Multiple -d's\n");
	printf("                                 may be specified.\n");
	printf("-root <path>                   - Used to specify non-standard root path.\n");
	printf("-s <automatic/manual/disabled> - Service startup type.\n");
	printf("-add                           - Add link configuration to registry.\n");
	printf("-remove                        - Remove link configuration from registry.\n");
	printf("\n");
	printf("Uninstall parameters\n");
	printf("-u                             - Uninstall service.\n");
	printf("\n");
	printf("Runtime parameters\n");
	printf("-debug                         - Debug mode for development.\n");
	printf("\n");
	printf("Installation and Runtime parameters\n");
	printf("-multi                         - Run in multiple link mode.\n");
	printf("-compress                      - Compress mode.\n");
	printf("-decompress                    - Decompress mode.\n");
	printf("-sign                          - Sign mode.\n");
	printf("-verify                        - Verify mode.\n");
	printf("-encrypt                       - Encrypt mode.\n");
	printf("-decrypt                       - Decrypt mode.\n");
	printf("\n");
	printf("Misc. parameters\n");
	printf("-?                             - Display this help.\n");
}
