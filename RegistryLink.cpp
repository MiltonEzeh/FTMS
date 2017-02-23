
#include "stdafx.h"
#include "RegistryLink.h"
#include "ConfigSettings.h"

CRegistryLink::CRegistryLink()
{
//	m_RegistryRoot = "SYSTEM\\CurrentControlSet\\Services\\";
	m_RegistryRoot = "SOFTWARE\\ICL\\FTMS\\";
}

CRegistryLink::~CRegistryLink()
{

}


void CRegistryLink::SetAtBegin()
{
	m_Index = 0;
}


CString CRegistryLink::GetNext(CString & ServiceType)
{
	HKEY hRegistryLinkKey = 0;
	HKEY hRegistryServiceKey = 0;
	CString ServiceName;

	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, m_RegistryRoot, 0, KEY_READ,  &hRegistryLinkKey)	== ERROR_SUCCESS )
	{
		const DWORD maxStringLen = 1024;

		DWORD dwSize = maxStringLen;
		LPSTR valueName= "";
		FILETIME filetime;
		DWORD dwNULL = 0;
		long Ret;

		do 
		{
			ServiceName.Empty();

			Ret = RegEnumKeyEx(hRegistryLinkKey, m_Index, valueName, &dwSize, 0, NULL, &dwNULL, &filetime);
		
			m_Index++;

			if ( Ret == ERROR_SUCCESS )
			{
				CConfigSettings *c = new CConfigSettings;
				c->SetDefaultSettings(valueName, ServiceType, "");

				bool SharedLink = c->ReadRegistryBool("SharedLink", "Yes", true);

				delete c;

				if ( SharedLink )
				{
					CString RegName;
					RegName.Format("%s%s\\%s", m_RegistryRoot, valueName, ServiceType);

					if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegName, 0, KEY_READ, &hRegistryServiceKey) == ERROR_SUCCESS )
					{
						ServiceName = valueName;
						RegCloseKey(hRegistryServiceKey);
						break;
					}
				}
			}

			dwSize = maxStringLen;

		} while ( Ret != ERROR_NO_MORE_ITEMS);

        RegCloseKey(hRegistryLinkKey);
	}

	return ServiceName;
}
