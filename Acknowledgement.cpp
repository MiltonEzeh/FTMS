// Acknowledgement.cpp: implementation of the CAcknowledgement class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Acknowledgement.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAcknowledgement::CAcknowledgement()
{
	m_AckKey = ACK_KEY;
}

CAcknowledgement::~CAcknowledgement()
{
}


//////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////
BOOL CAcknowledgement::Initialise(const CString& FileName)
{
	m_FileName = FileName;

	return TRUE;
}

BOOL CAcknowledgement::OpenFile(BOOL OverwriteFile)
{
	DWORD OpenFlags;

	if ( OverwriteFile )
	{
		OpenFlags = CFile::modeCreate | CFile::modeReadWrite;
	}
	else
	{
		OpenFlags = CFile::modeReadWrite;
	}

	// Open file
	if( !m_DiskFile.Open( m_FileName, OpenFlags ) )
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CAcknowledgement::CloseFile()
{
	m_DiskFile.Close();

	return TRUE;
}


BOOL CAcknowledgement::Read(CString &ReadString, int FieldNumber)
{
	char FileChar[1];
	LONG BytesRead = 0;
	LONG FilePointer = 0;
	LONG OffSet = 0;
	LONG FieldStart = 0;
	LONG FieldEnd = 0;
	LONG FieldSize = 0;
	LONG BeginningOfFile = 0;
	LONG EndOfFile = 0;
	int FieldsRead = 0;

	if ( !OpenFile(FALSE) )
	{
		return FALSE;
	}

	EndOfFile = m_DiskFile.SeekToEnd();

	memset(FileChar, NULL, sizeof(FileChar) / sizeof(FileChar[0]) );

	while (FieldsRead <= FieldNumber && OffSet < EndOfFile )
	{
		do
		{
			FilePointer = m_DiskFile.Seek( OffSet, CFile::begin );

			BytesRead = m_DiskFile.Read( FileChar, 1 );

			if (BytesRead != 1)
			{
				CloseFile();
				return FALSE;
			}

			OffSet += 1;

			if ( FieldNumber == 1 && OffSet > 22 )
			{
				OffSet = EndOfFile + 1;
			}

		} while ( FileChar[0] != '|' && OffSet < EndOfFile );

		if (OffSet <= EndOfFile)
		{
			if (FieldsRead < FieldNumber)
			{
				FieldStart = FilePointer;
			}
			else
			{
				FieldEnd = FilePointer;
			}

			FieldsRead++;
		}
	}

	// Beginning of file reached, field not found
	if (FieldStart == 0 && FieldEnd == 0)
	{
		CloseFile();
		return FALSE;
	}

	if ( FieldEnd < FieldStart )
	{
		CloseFile();
		return FALSE;
	}

	FilePointer = m_DiskFile.Seek( FieldStart + 1, CFile::begin );

	FieldSize = FieldEnd - (FieldStart + 1);

	char *buf = ReadString.GetBuffer( FieldSize+1 );
	BytesRead = m_DiskFile.Read( buf, FieldSize );
	ReadString.ReleaseBuffer(FieldSize);

	if (BytesRead != FieldSize)
	{
		ReadString.Empty();
		CloseFile();
		return FALSE;
	}

	CloseFile();

	return TRUE;
}

BOOL CAcknowledgement::Write()
{

	if ( !OpenFile(TRUE) )
	{
		return FALSE;
	}

	m_AckKey = ACK_KEY;

	CString OutString;

	OutString.Format("|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|", 
					m_AckKey,
					m_ServiceID,
					m_ProcessStatus,
					m_FailureReason,

					m_OriginalFileName,
					m_OriginalPath,
					m_OriginalMachineID,
					m_OriginalFileSize,
					m_OriginalDateStamp,
					m_OriginalCRC,

					m_DeliveredFileName,
					m_DeliveredPath,
					m_DeliveredMachineID,
					m_DeliveredFileSize,
					m_DeliveredDateStamp,
					m_DeliveredCRC,
					
					m_ArchiveFileName);

	m_DiskFile.SeekToEnd();
	m_DiskFile.Write(OutString, OutString.GetLength() );

	m_DiskFile.Flush();

	CloseFile();

	return TRUE;
}

BOOL CAcknowledgement::TextFileWrite( const CString& textFileName ) const
{
	try
	{
		CStdioFile file( textFileName, 
			CFile::modeCreate | CFile::modeReadWrite | CFile::typeText );
		
		CString OutString;

		// ServiceID
		OutString.Format("DeliveredServiceID= %s\n", m_ServiceID);
		file.WriteString( OutString );

		// ProcessStatus
		OutString.Format("DeliveredStatus= %s\n", m_ProcessStatus );
		file.WriteString( OutString );

		// FailureReason
		OutString.Format("FailureReason= %s\n", m_FailureReason );
		file.WriteString( OutString );

		// OriginalFileName
		OutString.Format("OriginalFileName= %s\n", m_OriginalFileName );
		file.WriteString( OutString );

		// OriginalPath
		OutString.Format("OriginalPath= %s\n", m_OriginalPath);
		file.WriteString( OutString );

		// OriginalMachineId
		OutString.Format("OriginalMachineID= %s\n", m_OriginalMachineID);
		file.WriteString( OutString );

		// OriginalFileSize
		OutString.Format("OriginalFileSize= %s\n", m_OriginalFileSize);
		file.WriteString( OutString );

		// OriginalCreationStamp
		OutString.Format("OriginalCreationDate= %s/%s/%s-%s:%s:%s\n", 
												m_OriginalDateStamp.Mid(6,2),
												m_OriginalDateStamp.Mid(4,2),
												m_OriginalDateStamp.Mid(0,4),
												m_OriginalDateStamp.Mid(8,2),
												m_OriginalDateStamp.Mid(10,2),
												m_OriginalDateStamp.Mid(12,2));
		file.WriteString( OutString );

		// OriginalCRC
		OutString.Format("OriginalCheckSum= %s\n", m_OriginalCRC);
		file.WriteString( OutString );

		// DeliveredFileName
		OutString.Format("DeliveredFileName= %s\n", m_DeliveredFileName);
		file.WriteString( OutString );

		// DeliveredPath
		OutString.Format("DeliveredPath= %s\n", m_DeliveredPath);
		file.WriteString( OutString );

		// DeliveredMachineID
		OutString.Format("DeliveredMachineID= %s\n", m_DeliveredMachineID);
		file.WriteString( OutString );

		// DeliveredFileSize
		OutString.Format("DeliveredFileSize= %s\n", m_DeliveredFileSize);
		file.WriteString( OutString );

		// DeliveredDateTime
		 
		// MILTON
		// Maybe we need to add GMT to the end of this string
		//
		// PC0118642 - Raised because time is in GMT

		// This fix has been regressed beacuse the LFS applications cannot 
		// handle the extra information - GMT - contained in the DeliveredDateTime
		// stamp [070905] - PC0124542
		
		// REMOVED
		/*
		OutString.Format("DeliveredDateTime= %s/%s/%s-%s:%s:%s %s\n", 
												m_DeliveredDateStamp.Mid(6,2),
												m_DeliveredDateStamp.Mid(4,2),
												m_DeliveredDateStamp.Mid(0,4),
												m_DeliveredDateStamp.Mid(8,2),
												m_DeliveredDateStamp.Mid(10,2),
												m_DeliveredDateStamp.Mid(12,2),
												" - GMT");

		*/

		OutString.Format("DeliveredDateTime= %s/%s/%s-%s:%s:%s\n", 
												m_DeliveredDateStamp.Mid(6,2),
												m_DeliveredDateStamp.Mid(4,2),
												m_DeliveredDateStamp.Mid(0,4),
												m_DeliveredDateStamp.Mid(8,2),
												m_DeliveredDateStamp.Mid(10,2),
												m_DeliveredDateStamp.Mid(12,2));

		file.WriteString( OutString );

		// DeliveredCRC
		OutString.Format("DeliveredCheckSum= %s\n", m_DeliveredCRC);
		file.WriteString( OutString );

		file.Flush();
	}
	catch ( CFileException* f )
	{
		f->Delete();
		return FALSE;
	}

	return TRUE;
}

BOOL CAcknowledgement::IsAcknowledgement()
{
	if ( !Read(m_AckKey, 1) ) return FALSE;

	if ( m_AckKey == ACK_KEY )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL CAcknowledgement::IsPositiveAck()
{
	if ( !Read(m_ProcessStatus, 3) ) return FALSE;

	if ( m_ProcessStatus == ACK_POSITIVE )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL CAcknowledgement::IsNegativeAck()
{
	if ( !Read(m_ProcessStatus, 3) ) return FALSE;

	if ( m_ProcessStatus == ACK_NEGATIVE )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL CAcknowledgement::StoreOriginalFileDetails()
{
	// This will read an acknowledge file
	// and store details in data members
	if ( IsAcknowledgement() )
	{
		Read( m_ServiceID, 2);
		Read( m_ProcessStatus, 3);
		Read( m_FailureReason, 4);

		Read( m_OriginalFileName, 5);
		Read( m_OriginalPath, 6);
		Read( m_OriginalMachineID, 7);
		Read( m_OriginalFileSize, 8);		
		Read( m_OriginalDateStamp, 9);
		Read( m_OriginalCRC, 10);

		Read( m_DeliveredFileName, 11);
		Read( m_DeliveredPath, 12);
		Read( m_DeliveredMachineID, 13);
		Read( m_DeliveredFileSize, 14);
		Read( m_DeliveredDateStamp, 15);
		Read( m_DeliveredCRC, 16);

		Read( m_ArchiveFileName, 17);
	}

	return TRUE;
}

CString CAcknowledgement::GetOriginalFileName()
{
	return m_OriginalFileName;
}

void CAcknowledgement::SetPositiveStatus()
{
	m_ProcessStatus = ACK_POSITIVE;
}

void CAcknowledgement::SetNegativeStatus()
{
	m_ProcessStatus = ACK_NEGATIVE;
}

CString CAcknowledgement::GetDeliveredFileName()
{
	return m_DeliveredFileName;
}

CString CAcknowledgement::GetDeliveredPath()
{
	return m_DeliveredPath;
}

CString CAcknowledgement::GetDeliveredMachineID()
{
	return m_DeliveredMachineID;
}

CString CAcknowledgement::GetDeliveredDateStamp()
{
	return m_DeliveredDateStamp;
}

CString CAcknowledgement::GetDeliveredFileSize()
{
	return m_DeliveredFileSize;
}

CString CAcknowledgement::GetDeliveredCRC()
{
	return m_DeliveredCRC;
}

CString CAcknowledgement::GetOriginalDateStamp()
{
	return m_OriginalDateStamp;
}

CString CAcknowledgement::GetArchiveFileName()
{
	return m_ArchiveFileName;
}

void CAcknowledgement::SetArchiveFileName(const CString& ArchiveFileName)
{
	m_ArchiveFileName = ArchiveFileName;
}