// NTService.cpp
// 
// This is the main program file containing the entry point.

#include "stdafx.h"
#include "wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "ServHusk.h"
#include "PostProcess.h"

int main(int argc, char* argv[])
{
    // Create the service object
    CPostProcess ThisService;

    // Parse for standard arguments (install, uninstall, version etc.)
    if (!ThisService.ParseStandardArgs(argc, argv)) 
	{
		// Didn't find any standard args so start the service
        // Uncomment the DebugBreak line below to enter the debugger
        // when the service is started.
        //DebugBreak();
        ThisService.StartService();

    }

    // When we get here, the service has been stopped
	return ThisService.m_Status.dwWin32ExitCode;
}
