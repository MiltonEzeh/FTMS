// PostProcessSettings.h: interface for the CPostProcessSettings class.
//
//////////////////////////////////////////////////////////////////////
#include "Audit.h"

class CPostProcessSettings  
{
public:
	CPostProcessSettings();
	virtual ~CPostProcessSettings();

	CString		m_ArchiveDirectory;
	bool		m_ArchiveData;

	CString		m_ControlFileDirectory;
	bool		m_WriteAuditInfo;

	CAudit		m_AuditFile;

	bool		m_DeliveredOriginalDate;
	bool		m_DeliverOriginalFileName;

	//CString		m_OverwriteFile;
	bool		m_CheckCRC;

	enum OverwriteOptions
	{
		NoOverwrite,
		OverwriteIfSame,
		AlwaysOverwrite
	};

	OverwriteOptions m_OverwriteFile;
};
