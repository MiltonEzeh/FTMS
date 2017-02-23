
#ifndef __SEND_H__
#define __SEND_H__

#include "ServHusk.h"
#include "CryptoSettings.h"

class CCryptoService : public CServiceHusk
{
public:

	CCryptoService();
	virtual ~CCryptoService();

	void GetServiceSpecificSettings();
	BOOL CheckServiceSpecificSettings(bool Installing);

	void Parse_ServiceSpecificSettings();

	void New_ServiceSpecificSettings();
	void ClearArray_ServiceSpecificSettings();

	void Add_ServiceSpecificSettings();
	void Set_ServiceSpecificSettings(int i);

	void Delete_ServiceSpecificSettings();
	void Clear_ServiceSpecificSettings();

	DWORD ProcessFile (CString, CString, CString, CString);

private:
	static CString GetCryptoErrorString( DWORD errorNum );

	// This service can be configured to use one of four different
	// cryptography function sets: Encrypt, Decrypt, Sign, Verify.

	// Definition of the function sets:
	typedef DWORD (*CryptoStartFunc)( const char *context_ID );
	typedef DWORD (*CryptoStopFunc)( const char *context_ID );
	typedef DWORD (*CryptoActionFunc)( const char *context_ID,
			          				   const char *in_file,
							                 long offset,
							                 long len,
							           const char *out_file);

	// definition of a particular function set
	struct ServiceConfig
	{
		// string which the user configures in the registry
		// to indicate this configuration.
		LPCSTR m_configName;	

		// name of the DLL this implementation is stored in
		LPCSTR m_dllName;

		// the function set -- first the function name,
		// then a value that can be filled in at run time with
		// the actual function pointer
		LPCSTR m_startName;
		LPCSTR m_actionName;
		LPCSTR m_stopName;
	};

	// this static array contains the set of functions that we
	// support (set to four in this release, I expect.) Add new
	// entries in the implementation file to support new types
	// of cryptography actions.
	static const ServiceConfig s_possibleConfigs[];

	// and, finally, a pointer to one entry within s_possibleConfigs
	// that indicates the configuration currently in use by this
	// instance of the class.
	const ServiceConfig* m_config;

	// is the current crypto service active? (i.e. startFunc called)
	bool m_cryptoStarted;

	// information about the currently active service
	HINSTANCE m_dllHandle;
	CryptoStartFunc m_startFunc;
	CryptoActionFunc m_actionFunc;
	CryptoStopFunc m_stopFunc;

	CCryptoSettings* m_CryptoSettings;
	CPtrArray m_CryptoSettingsArray;
};

#endif

