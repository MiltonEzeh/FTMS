
class CRegistryLink  
{
public:
	CRegistryLink();
	virtual ~CRegistryLink();

	void SetAtBegin();
	CString GetNext(CString & ServiceType);

private:
	CString m_RegistryRoot;
	DWORD	m_Index;
};
