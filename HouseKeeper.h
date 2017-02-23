// HouseKeeper.h: interface for the CHouseKeeper class.
//
//////////////////////////////////////////////////////////////////////

#if !defined __HOUSEKEEPER_H__
#define __HOUSEKEEPER_H__

#include "ServHusk.h"
#include "HouseKeeperSettings.h"

class CHouseKeeper : public CServiceHusk  
{
public:
	CHouseKeeper();
	virtual ~CHouseKeeper();

	virtual void GetServiceSpecificSettings();
	virtual BOOL CheckServiceSpecificSettings( bool Installing );

	void New_ServiceSpecificSettings();
	void Initialize_ServiceSpecificSettings();
	void ClearArray_ServiceSpecificSettings();

	void Add_ServiceSpecificSettings();
	void Set_ServiceSpecificSettings(int i);

	void Delete_ServiceSpecificSettings();
	void Clear_ServiceSpecificSettings();

	void PollDirectories();
	void ListAllFiles(CStringList & FileList, const CString & Dir, const CString & Pattern);

	void DelFile( const CString & DelFile, long Hrs, const CString & DelDate );

private:
	CHouseKeeperSettings* m_HouseKeeperSettings;
	CPtrArray m_HouseKeeperSettingsArray;
};

#endif 
