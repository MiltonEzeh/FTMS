#include "stdafx.h"

#include "Wrapper.h"
#include "ConfigSettings.h"
#include "Acknowledgement.h"
#include "huskmsg.h"
#include "Globals.h"
#include "Crypto.h"
#include "DSigCode.h"

const CCryptoService::ServiceConfig CCryptoService::s_possibleConfigs[] =
{
	{ "Encrypt", "FileEncrypt.dll", "EncryptStart", "Encrypt", "EncryptStop" },
	{ "Decrypt", "FileEncrypt.dll", "DecryptStart", "Decrypt", "DecryptStop" },
	{ "Sign", "FileSign.dll", "SignStart", "Sign", "SignStop" },
	{ "Verify", "FileSign.dll", "VerifyStart", "Verify", "VerifyStop"},
	{ 0 }
};

CCryptoService::CCryptoService()
{
	m_ServiceType = "Crypto";

	m_CryptoSettings = NULL;

	m_config = 0;
	m_cryptoStarted = false;

	m_readAndStoreWrapperBeforeProcessing = true;
	m_copyUnwrappedToWorking = false;
	m_restoreWrapperToOutputFile = true;

	m_dllHandle = 0;
	m_startFunc = 0;
	m_actionFunc = 0;
	m_stopFunc = 0;
}

CCryptoService::~CCryptoService()
{
	if ( m_cryptoStarted )
	{
		// need to stop!
		DWORD r = m_stopFunc( m_CryptoSettings->m_cryptoContext );

		if ( r != 0 )
		{
			LogEvent( EVMSG_CRYPOGRAPHY_ERROR, EVCAT_SECURITY, "stop", m_config->m_configName, 
				GetCryptoErrorString( r ) );
		}
	}

	if ( m_dllHandle )
	{
		::FreeLibrary( m_dllHandle );
		m_dllHandle = 0;
	}

}

void CCryptoService::GetServiceSpecificSettings()
{
	m_CryptoSettings->m_cryptoContext = m_ConfigFile->ReadRegistryString("CryptoContext", CConfigSettings::s_DefaultSetting );
}

BOOL CCryptoService::CheckServiceSpecificSettings( bool Installing )
{
	if ( !m_config )
	{
		LogEvent( EVMSG_CRYPOGRAPHY_CONFIG_BAD );

		return false;
	}

	if ( !m_dllHandle )
	{
		// now try to load up the DLL
		m_dllHandle = ::LoadLibrary( m_config->m_dllName );

		if ( !m_dllHandle )
		{
			CString msg;
			msg.Format( "failed to load library %s with error %s",
				m_config->m_dllName, GetLastErrorString() );

			LogEvent( EVMSG_CRYPOGRAPHY_ERROR, EVCAT_SECURITY, "library load", m_config->m_configName, 
				msg );
			return false;
		}


		m_startFunc = (CryptoStartFunc)::GetProcAddress( m_dllHandle, m_config->m_startName ) ;
		m_stopFunc = (CryptoStopFunc)::GetProcAddress( m_dllHandle, m_config->m_stopName ) ;
		m_actionFunc = (CryptoActionFunc)::GetProcAddress( m_dllHandle, m_config->m_actionName ) ;

		if ( !m_startFunc || !m_stopFunc || !m_actionFunc )
		{
			CString msg;
			msg.Format( "library %s does not export expected functions; corrupt?",
				m_config->m_dllName );

			LogEvent( EVMSG_CRYPOGRAPHY_ERROR, EVCAT_SECURITY, "library load", m_config->m_configName, 
				msg );
			return false;
		}
	}

	// now start the crypto service
	DWORD r = m_startFunc( m_CryptoSettings->m_cryptoContext );

	if ( r != 0 )
	{
		LogEvent( EVMSG_CRYPOGRAPHY_ERROR, EVCAT_SECURITY, "start", m_config->m_configName, 
			GetCryptoErrorString( r ) );

		return false;
	}

	m_cryptoStarted = true;

	return true;
}

DWORD CCryptoService::ProcessFile(CString InPath,     CString InFile,
								  CString WorkingDir, CString OutFile)
{
	const CString fullInPath = InPath + '\\' + InFile;
	const CString fullOutPath = WorkingDir + '\\' + OutFile;

	// At this point, the (possibly) wrapped file exists in the in directory.
	// We need to figure out how much of the file should be processed -
	// either the whole file (if not wrapped) or just the non-wrapped portion.
	CWrapper w;
	w.SetAssociatedFile( fullInPath );

	DWORD fileSize = 0;

	if ( w.IsWrapped() )
	{
		fileSize =  w.GetUnwrappedFileLength();
	}
	else
	{
		CFileStatus s;

		if ( !CFile::GetStatus( fullInPath, s ) )
		{
			LogEvent( EVMSG_REMOVE_WRAPPER_FAILED, fullInPath );
			return EVMSG_REMOVE_WRAPPER_FAILED;
		}

		fileSize = s.m_size;
	}

	DWORD r = m_startFunc( m_CryptoSettings->m_cryptoContext );

	if ( r != 0 )
	{
		LogEvent( EVMSG_CRYPOGRAPHY_ERROR, EVCAT_SECURITY, "start", m_config->m_configName, 
			GetCryptoErrorString( r ) );

		return false;
	}
	
	DebugMsg( "About to %s: file=\"%s\", crypt size=%d",
		m_config->m_configName, fullInPath, fileSize );

	r = m_actionFunc( m_CryptoSettings->m_cryptoContext,
							fullInPath, 0, fileSize, fullOutPath );

	DebugMsg( "%s complete on \"%s\"!",m_config->m_configName, fullInPath );

	if ( r != 0 )
	{
		// got failure!
		LogEvent( EVMSG_FILE_CRYPOGRAPHY_FAILED, EVCAT_SECURITY, m_config->m_configName, fullInPath,
			GetCryptoErrorString( r ) );

		return EVMSG_FILE_CRYPOGRAPHY_FAILED;
	}

	return 0;
}

CString CCryptoService::GetCryptoErrorString(DWORD errorNum)
{
	CString retVal;

	const int bufSize = 250;
	char buf[bufSize];

	char *err = GetDSErrorString( errorNum, buf, bufSize );

	if ( err )
		retVal.Format( "0x%x: %s", errorNum, err );
	else
		retVal.Format( "0x%x", errorNum );

	return retVal;
}

void CCryptoService::ClearArray_ServiceSpecificSettings()
{
	m_CryptoSettingsArray.SetSize(0);
}

void CCryptoService::New_ServiceSpecificSettings()
{
	m_CryptoSettings = new CCryptoSettings;
}

void CCryptoService::Add_ServiceSpecificSettings()
{
	m_CryptoSettingsArray.Add( m_CryptoSettings );
}

void CCryptoService::Set_ServiceSpecificSettings(int i)
{
	m_CryptoSettings = (CCryptoSettings*) m_CryptoSettingsArray.GetAt(i);
}

void CCryptoService::Delete_ServiceSpecificSettings()
{
	delete m_CryptoSettings;
}

void CCryptoService::Clear_ServiceSpecificSettings()
{
	for (int i = 0; i < m_CryptoSettingsArray.GetSize(); i++)
		delete (CCryptoSettings*) m_CryptoSettingsArray.GetAt(i);

	m_CryptoSettingsArray.RemoveAll();
}

void CCryptoService::Parse_ServiceSpecificSettings()
{
	const ServiceConfig *p = s_possibleConfigs;
	m_config = 0;

	while ( p->m_configName )
	{
		if ( m_CommandLine.m_Crypto.CompareNoCase( p->m_configName ) == 0 )
		{
			// we got a match!
			m_config = p;
			m_ServiceType = p->m_configName;
			break;
		}
		p++;
	}
}