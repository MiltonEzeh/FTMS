// DecompressSettings.h: interface for the CDecompressSettings class.
//
//////////////////////////////////////////////////////////////////////

class CDecompressSettings  
{
public:
	CDecompressSettings();
	virtual ~CDecompressSettings();

	// Data members
	CString m_GZipDirectory;

	bool m_isAsymmetric;
};
