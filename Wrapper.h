// Wrapper.h: interface for the Wrapper class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WRAPPER_H__149DB5F1_97E3_11D1_8A7A_00004B4812B3__INCLUDED_)
#define AFX_WRAPPER_H__149DB5F1_97E3_11D1_8A7A_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

// Footer Record constants
#define WRAPPER_KEY				"FTMSWrapper"
#define WRAPPER_NUM_OF_FIELDS	9
#define WRAPPER_SUCCESS			"Success"
#define WRAPPER_FAILURE			"Failure"

class CWrapper  
{
public:
	CWrapper();
	virtual ~CWrapper();

	bool SetAssociatedFile(const CString& FileName);
	bool Write();
	bool Delete();

	bool IsWrapped();

	void Clear();

	DWORD GetUnwrappedFileLength();

	CString GetOriginalFileSize() const
	{
		return m_OriginalFileSize;
	}

	CString GetOriginalFileName() const
	{
		return m_OriginalFileName;
	}

	CString GetDateStamp() const
	{
		return m_DateStamp;
	}

	CString GetFileCRC() const
	{
		return m_FileCRC;
	}

	CString GetMachineName() const
	{
		return m_MachineName;
	}

	CString GetPickupTime() const
	{
		return m_PickupTime;
	}

	CString GetWrapperCRC() const
	{
		return m_WrapperCRC;
	}

	bool IsEmpty() const
	{
		return m_isEmpty;
	}

	void SetFileCRC(DWORD CheckSum);
	void SetWrapperCRC(DWORD CheckSum);
	void SetSuccessStatus();
	void SetFailureStatus();
	void SetPickupTime();
	void SetOriginalFileSize(LONG FileSize);
	bool SetOriginalFileDetails();
	bool StoreOriginalFileDetails();

	// this function generates a CRC for all the information in this
	// wrapper -- excluding the wrapper CRC of course!
	DWORD GenerateWrapperCRC() const;

private:
	bool FindField( int FieldNumber, long& fieldStart, long& fieldLength );

	bool OpenFile();
	void CloseFile();
	bool Read(CString &ReadString, int FieldNumber);

	// Data members
	CString			m_AssociatedFile;
	CFile			m_DiskFile;
	bool			m_FileIsOpen;
	

	// data file fields
	CString			m_WrapperKey;
	CString			m_OriginalFileSize;
	CString			m_OriginalFileName;
	CString			m_DateStamp;
	CString			m_FileCRC;
	CString			m_MachineName;
	CString			m_PickupTime;
	CString         m_WrapperCRC;
	CString			m_ProcessStatus;

	bool m_isEmpty;
};

#endif // !defined(AFX_WRAPPER_H__149DB5F1_97E3_11D1_8A7A_00004B4812B3__INCLUDED_)
