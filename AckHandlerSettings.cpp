// AckHandlerSettings.cpp: implementation of the CAckHandlerSettings class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AckHandlerSettings.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAckHandlerSettings::CAckHandlerSettings()
{
	m_SuccessSentDirectory.Empty();
	m_UnsuccessSentDirectory.Empty();
	m_SuccessfulAckDirectory.Empty();
	m_UnsuccessfulAckDirectory.Empty();

	m_InTransitDirectory.Empty();
	m_OverduePeriod = 120;

	m_endToEndRetryMax = 0;

	m_intransitValid = true;
	
	m_ControlFileDirectory.Empty();
	m_WriteAuditInfo = false;
}

CAckHandlerSettings::~CAckHandlerSettings()
{

}
