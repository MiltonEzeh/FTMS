// Decompress.cpp: implementation of the CDecompress class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "ServHusk.h"
#include "Decompress.h"
#include "HuskMsg.h"
#include "Globals.h"
#include "CRC.h"

static const char *defaultExtension = ".pz";

	// default to adding the defaultExtension
static const char *compressDefaultRenameExp = ".*;&.pz";

	// default to removing the defaultExtension; otherwise leave alone
static const char *decompressDefaultRenameExp = "(.*)\\.[pP][zZ];\\1;.*;&";


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDecompress::CDecompress()
{
	m_DecompressSettings = NULL;

	m_readAndStoreWrapperBeforeProcessing = true;
	m_copyUnwrappedToWorking = true;
	m_restoreWrapperToOutputFile = true;
}

CDecompress::~CDecompress()
{

}

void CDecompress::GetServiceSpecificSettings()
{
	m_DecompressSettings->m_GZipDirectory = m_ConfigFile->ReadRegistryString("GZipDirectory", "c:\\FTMS\\Software");

	m_DecompressSettings->m_isAsymmetric = m_ConfigFile->ReadRegistryBool("IsAsymmetic", "No" );

	if ( m_mode == decompress )
	{
		m_ConfigFile->ReadRegistryString("OutRenameExpression", decompressDefaultRenameExp );
	}
	else if ( m_mode == compress )
	{
		m_ConfigFile->ReadRegistryString("OutRenameExpression", compressDefaultRenameExp );
	}
}

BOOL CDecompress::CheckServiceSpecificSettings(bool Installing)
{
	if ( !CConfigSettings::CheckDirectory( this, "GZipDirectory", m_DecompressSettings->m_GZipDirectory, CConfigSettings::required, Installing ) )
		return false;

	if ( m_mode == unknown )
	{
		LogEvent( EVMSG_COMPRESSION_CONFIG_BAD );
		return false;
	}

	return true;
}

DWORD CDecompress::ProcessFile(CString InPath,     CString InFile,
							   CString WorkingDir, CString OutFile)
{
	// given the Service Husk settings in the constructor, at this point
	// we are guaranteed to have an unwrapped copy of the input file in
	// working directory.

	const CString fullWorkingFilename = WorkingDir + "\\" + OutFile;

	// in asymmetric mode, we need to check the CRC of this file
	if ( m_DecompressSettings->m_isAsymmetric )
	{
		if ( m_WrapperFile.IsEmpty() )
		{
			DebugMsg( "Cannot check input CRC; input file not wrapped" );
		}
		else if ( m_WrapperFile.GetFileCRC().IsEmpty() )
		{
			DebugMsg( "Cannot check input CRC; CRC not calculated for input file" );
		}
		else
		{
			DWORD FileCRC = atoi ( m_WrapperFile.GetFileCRC() );

			// calculate CRC of file before decompress
			DWORD FileInCRC = 0;

			DebugMsg( "Calculating CRC for %s...", fullWorkingFilename );

			if ( !CalculateCrc( FileInCRC, fullWorkingFilename ) )
			{
				LogEvent(EVMSG_CRC_FAILED, fullWorkingFilename );
				return EVMSG_CRC_FAILED;
			}

			SetDeliveredCRC( FileInCRC );

			if ( FileInCRC != FileCRC )
			{
				DebugMsg("File CRC'c do not match");
				LogEvent(EVMSG_CRC_MISMATCH, fullWorkingFilename );
				return EVMSG_CRC_MISMATCH;
			}
		}
	}
	
	// now, work around gzip's insistance on renaming things.  We don't want
	// to use gzip's scheme, but rather our own scheme as defined in the rename
	// expressions.

	CString nameBeforeZip = fullWorkingFilename;
	CString nameAfterZip = fullWorkingFilename;

	if ( decompress == m_mode ) 
	{
		// make name before have the default extension
		nameBeforeZip += defaultExtension;
	}
	else
	{
		// the name after will end up getting the default extension
		nameAfterZip += defaultExtension;
	}

	if ( nameBeforeZip != fullWorkingFilename )
	{
		// we need to rename!
		if ( 0 == ::MoveFileEx( fullWorkingFilename, nameBeforeZip, MOVEFILE_REPLACE_EXISTING ) )
		{
			LogEvent( EVMSG_MOVE_ERROR, fullWorkingFilename, nameBeforeZip, GetLastErrorString() );
			return EVMSG_MOVE_ERROR;
		}
	}

	// now build up the gzip command line
	CString cmdLine;
	cmdLine.Format( "%s\\gzip -f%s -S %s \"%s\"",
		m_DecompressSettings->m_GZipDirectory, (m_mode == decompress) ? "d" : " ",
		defaultExtension, nameBeforeZip );

	STARTUPINFO	si = { sizeof si };
	PROCESS_INFORMATION pi = { 0 };

	DebugMsg("Running command: \"%s\"", cmdLine );

	if ( !CreateProcess(NULL,
				  (LPSTR)(LPCSTR) cmdLine,
				  NULL,
				  NULL,
				  TRUE,
				  NORMAL_PRIORITY_CLASS,
				  NULL,
				  NULL,
				  &si,
				  &pi) )
	{
		// add some debug information here

		// PC0117399 - Version 17
		// First check if gzip.exe exists
		

		// Since there is no API function that checks whether a file exists in the file system, 
		// we create a simple function that can check if file exists
		//
		// Full path of executable has been hard coded for debug purposes
		// Change this later
		//

		//LPSTR gzipFileName = "C:\\FTMS\\Software\\GZip.exe";

		// Build path to gzip exectuable
		CString gzipFileName;
		gzipFileName.Format( "%s\\gzip.exe",m_DecompressSettings->m_GZipDirectory);

		// Get file attributes, if this fails we assume the file doesnt exist
		DWORD dwAttr = GetFileAttributes(gzipFileName);
		if (dwAttr == 0xffffffff)
		{
			DebugMsg("Gzip file not found");
			LogEvent( EVMSG_GZIP_FILE_NOTFOUND, GetLastErrorString() );
		}
		else 
		{
			LogEvent( EVMSG_COMPRESSION_EXECUTE_FAILED, 
			( m_mode == decompress ) ? "decompress" : "compress",
			InFile, GetLastErrorString() );
		}

		// delete the working file!
		::DeleteFile( nameBeforeZip );

		return EVMSG_COMPRESSION_EXECUTE_FAILED;
	}

    ::WaitForSingleObject( pi.hProcess, INFINITE );
  

	DWORD ExitCode = 0;
	::GetExitCodeProcess( pi.hProcess, &ExitCode );

	if ( ExitCode != 0 )
	{
		DebugMsg("Unable to decompress file");
		char buf[20];

		// delete the working file!
		::DeleteFile( nameBeforeZip );

		LogEvent( EVMSG_COMPRESSION_FAILED, 
			( m_mode == decompress ) ? "decompress" : "compress",
			InFile, _itoa( ExitCode, buf, 10 ) );

		return EVMSG_COMPRESSION_FAILED;
	}
    if (pi.hProcess != NULL)
		::CloseHandle(pi.hProcess);

    if (pi.hThread != NULL)
		::CloseHandle(pi.hThread);

	// now possibly rename the output file
	if ( nameAfterZip != fullWorkingFilename )
	{
		// we need to rename!
		if ( !::MoveFileEx( nameAfterZip, fullWorkingFilename, MOVEFILE_REPLACE_EXISTING ) )
		{
			LogEvent( EVMSG_MOVE_ERROR, nameAfterZip, fullWorkingFilename, GetLastErrorString() );
			return EVMSG_MOVE_ERROR;
		}
	}

	// that's everything done -- except we need to update CRC for non-symmetric
	// configurations.
	// in asymmetric mode, we need to check the CRC of this file
	if ( m_DecompressSettings->m_isAsymmetric )
	{
		if ( m_WrapperFile.IsEmpty() )
		{
			DebugMsg( "Cannot update CRC information; input file not wrapped" );
		}
		else if ( m_WrapperFile.GetFileCRC().IsEmpty() )
		{
			DebugMsg( "Cannot check input CRC; CRC not calculated for input file" );
		}
		else
		{
			// calculate CRC of file after compress
			DWORD newCRC = 0;

			DebugMsg( "Realculating CRC..." );

			if ( !CalculateCrc( newCRC, fullWorkingFilename ) )
			{
				LogEvent(EVMSG_CRC_FAILED, fullWorkingFilename );
				return EVMSG_CRC_FAILED;
			}

			m_WrapperFile.SetFileCRC( newCRC );

			// and set the new file size
			CFileStatus s;

			CFile::GetStatus( fullWorkingFilename, s );

			m_WrapperFile.SetOriginalFileSize( s.m_size );

			// and finally recalculate the wrapper CRC
			DWORD wrapCRC = m_WrapperFile.GenerateWrapperCRC();
			m_WrapperFile.SetWrapperCRC(wrapCRC);

			// ServiceHusk will write this wrapper info to target
		}
	}

	return 0;
}

void CDecompress::ClearArray_ServiceSpecificSettings()
{
	m_DecompressSettingsArray.SetSize(0);
}

void CDecompress::New_ServiceSpecificSettings()
{
	m_DecompressSettings = new CDecompressSettings;
}

void CDecompress::Add_ServiceSpecificSettings()
{
	m_DecompressSettingsArray.Add( m_DecompressSettings );
}

void CDecompress::Set_ServiceSpecificSettings(int i)
{
	m_DecompressSettings = (CDecompressSettings*) m_DecompressSettingsArray.GetAt(i);
}
void CDecompress::Delete_ServiceSpecificSettings()
{
	delete m_DecompressSettings;
}

void CDecompress::Clear_ServiceSpecificSettings()
{
	for (int i = 0; i < m_DecompressSettingsArray.GetSize(); i++)
	{
		delete (CDecompressSettings*) m_DecompressSettingsArray.GetAt(i);
	}

	m_DecompressSettingsArray.RemoveAll();
}

void CDecompress::Parse_ServiceSpecificSettings()
{
	if ( m_CommandLine.m_Compress )
	{
		m_ServiceType = "Compress";
		m_mode = compress;
	}
	else if ( m_CommandLine.m_Decompress )
	{
		m_ServiceType = "Decompress";
		m_mode = decompress;
	}
	else
		m_mode = unknown;
}