//
// $History: Send.cpp $
// 
// *****************  Version 1  *****************
// User: Ezehm        Date: 19/06/03   Time: 11:25
// Created in $/Infrastructure/FTMS Baselines/Version 13/Source Code/SendNT
// Version 13 Software
// 
// *****************  Version 3  *****************
// User: Ezehm        Date: 6/02/03    Time: 14:22
// Updated in $/Infrastructure/FTMS Baselines/FTMS Version 11 upwards/Release/Source Code/SendNT
// Version 13 - Fixes to PC0080873 & 0081256
// 
// Milton 6.2.03
// 
// *****************  Version 2  *****************
// User: Ezehm        Date: 5/11/02    Time: 14:13
// Updated in $/Infrastructure/FTMS Baselines/FTMS Version 11 upwards/Release/Source Code/SendNT
// Version 12
// 
// *****************  Version 1  *****************
// User: Ezehm        Date: 5/11/02    Time: 11:45
// Created in $/Infrastructure/FTMS Baselines/FTMS Version 11 upwards/Release/Source Code/SendNT
// version 11
// 
// *****************  Version 23  *****************
// User: Barberp      Date: 2/01/01    Time: 13:37
// Updated in $/Infrastructure/FTMS Baselines/FTMS 19991029/Release/Source Code/SendNT
// 
// *****************  Version 22  *****************
// User: Barberp      Date: 1/12/00    Time: 10:22
// Updated in $/Infrastructure/FTMS Baselines/FTMS 19991029/Release/Source Code/SendNT
// Version 2.4.0.9
// 
// *****************  Version 21  *****************
// User: Barberp      Date: 24/11/00   Time: 15:36
// Updated in $/Infrastructure/FTMS Baselines/FTMS 19991029/Release/Source Code/SendNT
// 
// *****************  Version 20  *****************
// User: Barberp      Date: 26/11/99   Time: 14:50
// Updated in $/Infrastructure/FTMS Baselines/FTMS 19991029/Release/Source Code/SendNT
// 
// *****************  Version 18  *****************
// User: Ezehm        Date: 30/09/99   Time: 13:35
// Updated in $/Infrastructure/FTMS/Release/Source Code/SendNT
// No changes, zipped source files
// 
// 30/9/99
// 
// *****************  Version 17  *****************
// User: Ezehm        Date: 27/09/99   Time: 11:30
// Updated in $/Infrastructure/FTMS/Release/Source Code/SendNT
// Updated version number to 2.4.0.0
// 
// 27/09/99
// 
// *****************  Version 16  *****************
// User: Ezehm        Date: 27/09/99   Time: 10:23
// Updated in $/Infrastructure/FTMS/Release/Source Code/SendNT
// Updated version number to 2.3.0.0
// 
// 27.09.99
// 
// *****************  Version 15  *****************
// User: Willisj      Date: 22/06/99   Time: 11:34
// Updated in $/Infrastructure/FTMS/Release/Source Code/SendNT
// More derived service settings changes
// 
// *****************  Version 14  *****************
// User: Willisj      Date: 17/06/99   Time: 11:24
// Updated in $/Infrastructure/FTMS/Release/Source Code/SendNT
// Multiple service change
//

#include "stdafx.h"

#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "huskmsg.h"
#include "Globals.h"
#include "Send.h"

CSendService::CSendService()
:CServiceHusk()
{
	m_ServiceType = "Send";

	m_readAndStoreWrapperBeforeProcessing = true;
	m_copyUnwrappedToWorking = false;
	m_restoreWrapperToOutputFile = false;
}

CSendService::~CSendService()
{
}

/*
DWORD CALLBACK CopyProgressRoutine(
	LARGE_INTEGER TotalFileSize,
	LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER StreamSize,
	LARGE_INTEGER StreamBytesTransferred,
	DWORD dwStreamNumber,
	DWORD dwCallbackReason,
	HANDLE hSourceFile,
	HANDLE hDestinationFile,
	LPVOID lpData )
{

	CSendService *x = static_cast< CSendService* > ( lpData );

	// cast everything to longs, assume no file larger than 4 Gb.

	x->DebugMsg( 
		"CopyProgress: %d of %d bytes, stream: size = %d, bytes = %d, num = %d, reason = %d",
		TotalBytesTransferred.LowPart,
		TotalFileSize.LowPart,
		StreamSize.LowPart, StreamBytesTransferred.LowPart,
		dwStreamNumber, dwCallbackReason );

	return PROGRESS_CONTINUE;
}
*/

DWORD CSendService::ProcessFile(CString InPath,     CString InFile,
								CString WorkingDir, CString OutFile)
{
	
	CString fullInPath = InPath + '\\' + InFile;
	CString fullOutPath = WorkingDir + '\\' + OutFile;

	// now we need to copy the file from in to out

	BOOL cancelFlag = FALSE;

	DebugMsg( "Copying file from %s to %s...",
		fullInPath, fullOutPath );

//	CTime start = CTime::GetCurrentTime();

	BOOL success = 
		::CopyFileEx( fullInPath, fullOutPath, 0, 0, &cancelFlag, COPY_FILE_RESTARTABLE );

//	CTime end = CTime::GetCurrentTime();

	if ( !success )
	{
		DWORD err = ::GetLastError();
		DebugMsg( "CopyFileEx failed: %s -> %s, GetLastError returns 0x%x",
			fullInPath, fullOutPath, err );

		// translate error string
		CString errStr = GetErrorMessage( err, true );

		LogEvent( EVMSG_COPY_FAILED, fullInPath, fullOutPath, errStr );

		return EVMSG_COPY_FAILED;
	}
//	else
//	{
//		CTimeSpan s = end - start;
//		DebugMsg( "Copy completed successfully; took %d secs.", s.GetTotalSeconds() );
//	}

	// 0 = success
	return 0;
}

void CSendService::GetServiceSpecificSettings()
{
	// default RetryWait to 5 minutes
	m_ConfigFile->ReadRegistryInt("RetryWait", "300000" );
}

void CSendService::Initialize_ServiceSpecificSettings()
{
	m_ConfigFile->m_outDirSetting = CConfigSettings::validate;
	m_ConfigFile->m_workingDirSetting = CConfigSettings::validate;
	m_ConfigFile->m_ackInDirSetting = CConfigSettings::validate;
}