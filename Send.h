
#ifndef __SEND_H__
#define __SEND_H__

#include "ServHusk.h"

class CSendService : public CServiceHusk
{
public:

	CSendService();
	virtual ~CSendService();

	void Initialize_ServiceSpecificSettings();

	DWORD ProcessFile (CString, CString, CString, CString);

protected:
	virtual void GetServiceSpecificSettings();
};

#endif

