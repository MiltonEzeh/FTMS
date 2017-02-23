// NTService.cpp
// 
// This is the main program file containing the entry point.
#include "stdafx.h"
#include "wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "Audit.h"
#include "ServHusk.h"
#include "PreProcess.h"
#include "HuskMsg.h" // Event message ids

int main(int argc, char* argv[])
{
	// Create the service object
	CPreProcess ThisService;

	try
	{
		// Parse for standard arguments (install, uninstall, version etc.)
		if (!ThisService.ParseStandardArgs(argc, argv)) 
		{
			// Didn't find any standard args so start the service

			// Uncomment the DebugBreak line below to enter the debugger
			// when the service is started.
			//DebugBreak();

			ThisService.DebugMsg("About to start service");
			ThisService.StartService();
		}

	}
	catch (DWORD Error)
	{

		LPVOID lpMsgBuf;

		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					   NULL,
					   GetLastError(),
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					   (LPTSTR) &lpMsgBuf,
					   0,
					   NULL );

		CString ErrorString;
		ErrorString.Format( "Error %lu - %s", Error, (LPTSTR) lpMsgBuf );
		ThisService.DebugMsg( ErrorString );
		return ThisService.m_Status.dwWin32ExitCode;
	}
	catch (...)
	{
		LPVOID lpMsgBuf;

		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					   NULL,
					   GetLastError(),
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					   (LPTSTR) &lpMsgBuf,
					   0,
					   NULL );

		CString ErrorString;
		ErrorString.Format( "%s", (LPTSTR) lpMsgBuf );
		ThisService.DebugMsg( ErrorString );
		return ThisService.m_Status.dwWin32ExitCode;
	}

	return ThisService.m_Status.dwWin32ExitCode;
}
