// AckHandlerSettings.h: interface for the CAckHandlerSettings class.
//
//////////////////////////////////////////////////////////////////////
#include "SearchAndReplace.h"

class CAckHandlerSettings  
{
public:
	CAckHandlerSettings();
	virtual ~CAckHandlerSettings();

	// data members
	CString				m_SuccessSentDirectory;
	CSearchAndReplace	m_SuccessSentRenameExp;

	CString				m_UnsuccessSentDirectory;
	CSearchAndReplace	m_UnsuccessSentRenameExp;
	
	CString				m_SuccessfulAckDirectory;
	CSearchAndReplace	m_SuccessfulAckRenameExp;

	CString				m_UnsuccessfulAckDirectory;
	CSearchAndReplace	m_UnsuccessfulAckRenameExp;

	CString				m_InTransitDirectory;
	CTimeSpan			m_OverduePeriod;	

	int					m_endToEndRetryMax;

	bool				m_intransitValid;

	CString				m_ControlFileDirectory;
	bool				m_WriteAuditInfo;

};