
#include "stdafx.h"
#include "PreProcessSettings.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPreProcessSettings::CPreProcessSettings()
{
	m_InTransitDirectory.Empty();
	m_ControlFileDirectory.Empty();
	m_ArchiveDirectory.Empty();

	m_WriteAuditInfo = false;
	m_ArchiveData = false;
	m_AddCRC = true;
}

CPreProcessSettings::~CPreProcessSettings()
{

}
