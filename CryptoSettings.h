// CryptoSettings.h: interface for the CCryptoSettings class.
//
//////////////////////////////////////////////////////////////////////

class CCryptoSettings  
{
public:
	CCryptoSettings();
	virtual ~CCryptoSettings();

	// This is the cryptography context string that indicates the keyset
	// etc to be used by this service.
	CString m_cryptoContext;
};
