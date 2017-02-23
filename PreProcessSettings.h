
class CPreProcessSettings  
{
public:
	CPreProcessSettings();
	virtual ~CPreProcessSettings();

	CString	m_InTransitDirectory;
	CString	m_ControlFileDirectory;
	CString	m_ArchiveDirectory;

	bool	m_WriteAuditInfo;
	bool	m_ArchiveData;
	bool	m_AddCRC;
};
