// PostProcessSettings.cpp: implementation of the CPostProcessSettings class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PostProcessSettings.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPostProcessSettings::CPostProcessSettings()
{
	m_ArchiveDirectory.Empty();
	m_ArchiveData = false;

	m_ControlFileDirectory.Empty();
	m_WriteAuditInfo = false;

	m_DeliveredOriginalDate = false;
	m_DeliverOriginalFileName = true;

	m_CheckCRC = true;

	m_OverwriteFile = OverwriteIfSame;
}

CPostProcessSettings::~CPostProcessSettings()
{

}
