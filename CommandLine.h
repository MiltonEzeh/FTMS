// CommandLine.h: interface for the CCommandLine class.
//
//////////////////////////////////////////////////////////////////////
#define FTMS_LINK_ROOT "C:\\FTMS"
#define FTMS_DEFAULT_USER "FTMS"
#define FTMS_DEFAULT_PASSWORD "mbiatp"

class CCommandLine
{
public:
	CCommandLine();
	virtual ~CCommandLine();

	bool ValidateCommandLine(int argc, char* argv[]);
	void DisplaySyntax();

	bool		m_Debug;

	bool		m_Install;
	bool		m_UnInstall;

	bool		m_ProcessMultiLinks;

	bool		m_AddRegistryLink;
	bool		m_RemoveRegistryLink;

	bool		m_Compress;
	bool		m_Decompress;

	bool		m_DisplaySyntax;

	CString		m_ServiceName;
	CString		m_StartupType;

	CString		m_User;
	CString		m_Password;

	CString		m_LinkRoot;

	CStringList m_DependentServices;

	CString		m_Crypto;
};
