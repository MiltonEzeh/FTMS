// Acknowledgement.h: interface for the CAcknowledgement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ACKNOWLEDGEMENT_H__1B332B91_9E2D_11D1_8A9F_00004B4812B3__INCLUDED_)
#define AFX_ACKNOWLEDGEMENT_H__1B332B91_9E2D_11D1_8A9F_00004B4812B3__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define ACK_KEY				"FTMSAcknowledgement"
#define ACK_NUM_OF_FIELDS	16
#define ACK_POSITIVE		"Success"
#define ACK_NEGATIVE		"Failure"

class CAcknowledgement  
{
public:
	CAcknowledgement();
	virtual ~CAcknowledgement();

	BOOL Initialise(const CString& FileName);
	BOOL Read(CString &ReadString, int FieldNumber);
	BOOL Write();
	BOOL TextFileWrite( const CString& textFileName ) const;
	BOOL IsAcknowledgement();
	BOOL IsPositiveAck();
	BOOL IsNegativeAck();

	BOOL OpenFile(BOOL OverwriteFile);
	BOOL CloseFile();

	BOOL StoreOriginalFileDetails();
	CString GetOriginalFileName();

	CString GetDeliveredFileName();
	CString GetDeliveredPath();
	CString GetDeliveredMachineID();
	CString GetDeliveredDateStamp();
	CString GetDeliveredFileSize();
	CString GetDeliveredCRC();
	CString GetOriginalDateStamp();

	CString GetArchiveFileName();
	void SetArchiveFileName(const CString& ArchiveFileName);

	void SetPositiveStatus();
	void SetNegativeStatus();

	// Data members
	CString			m_FileName;
	CFile			m_DiskFile;

	// Data File Fields
	CString			m_AckKey;
	CString			m_ServiceID;
	CString			m_ProcessStatus;
	CString			m_FailureReason;

	CString			m_OriginalFileName;
	CString			m_OriginalPath;
	CString			m_OriginalMachineID;
	CString			m_OriginalFileSize;		
	CString			m_OriginalDateStamp;
	CString			m_OriginalCRC;

	CString			m_DeliveredFileName;
	CString			m_DeliveredPath;
	CString			m_DeliveredMachineID;
	CString			m_DeliveredFileSize;
	CString			m_DeliveredDateStamp;
	CString			m_DeliveredCRC;

	CString			m_ArchiveFileName;
};

#endif // !defined(AFX_ACKNOWLEDGEMENT_H__1B332B91_9E2D_11D1_8A9F_00004B4812B3__INCLUDED_)
