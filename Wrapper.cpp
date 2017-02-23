// Wrapper.cpp: implementation of the Wrapper class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Wrapper.h"
#include "CRC.h"
#include "Globals.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CWrapper::CWrapper()
{
	m_WrapperKey = WRAPPER_KEY;
	m_FileIsOpen = false;
	m_isEmpty = true;
}

CWrapper::~CWrapper()
{
}

//////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////
bool CWrapper::SetAssociatedFile(const CString& AssociatedFile)
{
	// todo: close file is associated file is different
	if ( AssociatedFile != m_AssociatedFile )
	{
		m_AssociatedFile = AssociatedFile;
		CloseFile();
	}

	return true;
}

bool CWrapper::OpenFile()
{
	if ( m_FileIsOpen )
		return true;

	// Open file
	if( !m_DiskFile.Open( m_AssociatedFile, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeReadWrite ) )
		return false;

	m_FileIsOpen = true;

	return true;
}

void CWrapper::CloseFile()
{
	if ( m_FileIsOpen )
	{
		m_DiskFile.Close();
		m_FileIsOpen = false;
	}
}


bool CWrapper::Read(CString& ReadString, int FieldNumber)
{
	if ( !OpenFile() )
		return false;

	long FieldStart, FieldSize;

	if ( !FindField( FieldNumber, FieldStart, FieldSize ) )
	{
		CloseFile();
		return false;
	}

	m_DiskFile.Seek( FieldStart + 1, CFile::begin );

	char *buf = ReadString.GetBuffer( FieldSize );
	long BytesRead = m_DiskFile.Read( buf, FieldSize );
	ReadString.ReleaseBuffer( BytesRead );

	if (BytesRead != FieldSize)
	{
		CloseFile();
		ReadString.Empty();
		return false;
	}

	CloseFile();

	return true;
}

bool CWrapper::Write()
{
	if ( IsWrapped() )
	{
		ASSERT( FALSE );
	}

	if ( !OpenFile() )
	{
		return FALSE;
	}

	m_WrapperKey = WRAPPER_KEY;

	CString OutString;
	OutString.Format("|%s|%s|%s|%s|%s|%s|%s|%s|%s|", m_ProcessStatus,
													 m_WrapperCRC,
													 m_PickupTime,
													 m_MachineName,
													 m_FileCRC,
													 m_DateStamp,
													 m_OriginalFileName,
													 m_OriginalFileSize,
													 m_WrapperKey);

	m_DiskFile.SeekToEnd();
	m_DiskFile.Write(OutString, OutString.GetLength() );

	CloseFile();

	// Write to end of file
	return true;
}

bool CWrapper::Delete()
{
	if ( !OpenFile() )
	{
		return false;
	}

	DWORD FileLength = GetUnwrappedFileLength();

	if ( m_DiskFile.GetLength() > FileLength ) // Can only make file smaller.
	{
		m_DiskFile.SetLength(FileLength);
	}
	else
	{
		CloseFile();
		return false;
	}

	CloseFile();

	return true;
}

bool CWrapper::IsWrapped()
{
	if ( !Read(m_WrapperKey, 1) ) 
		return false;

	if ( m_WrapperKey == WRAPPER_KEY )
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CWrapper::SetFileCRC(DWORD CheckSum)
{
	m_FileCRC.Format("%lu", CheckSum);
	m_isEmpty = false;
}

void CWrapper::SetWrapperCRC(DWORD CheckSum)
{
	m_WrapperCRC.Format("%lu", CheckSum);
	m_isEmpty = false;
}

void CWrapper::SetSuccessStatus()
{
	m_ProcessStatus = WRAPPER_SUCCESS;
	m_isEmpty = false;
}

void CWrapper::SetFailureStatus()
{
	m_ProcessStatus = WRAPPER_FAILURE;
	m_isEmpty = false;
}

void CWrapper::SetPickupTime()
{
	CTime Now = CTime::GetCurrentTime();

	m_PickupTime = GetDateAndTimeAsString( Now );
	m_isEmpty = false;
}

void CWrapper::SetOriginalFileSize(LONG FileSize)
{
	m_OriginalFileSize.Format("%lu", FileSize);
	m_isEmpty = false;
}

bool CWrapper::SetOriginalFileDetails()
{
	CFileStatus FileStatus;

	m_OriginalFileName = m_AssociatedFile;

	if ( !CFile::GetStatus( m_OriginalFileName, FileStatus ) )
	{
		return FALSE;
	}

	m_OriginalFileSize.Format( "%lu", FileStatus.m_size );

	CTime DateStamp = FileStatus.m_ctime;	// creation time
	m_DateStamp = GetDateAndTimeAsString( DateStamp );

	TCHAR ConnectionName [MAX_PATH] = {0};

	DWORD size = sizeof( ConnectionName );

	GetComputerName(ConnectionName, &size);

	m_MachineName = ConnectionName;

	m_isEmpty = false;

	return TRUE;
}

bool CWrapper::StoreOriginalFileDetails()
{
	if ( IsWrapped() )
	{
		Read(m_ProcessStatus, 9);
		Read(m_WrapperCRC, 8);
		Read(m_PickupTime, 7);
		Read(m_MachineName, 6);
		Read(m_FileCRC, 5);
		Read(m_DateStamp, 4);
		Read(m_OriginalFileName, 3);
		Read(m_OriginalFileSize, 2);
		m_isEmpty = false;
		return true;
	}

	return false;
}

DWORD CWrapper::GetUnwrappedFileLength()
{
	// The original file length must be calculated this way because
	// the file preceding the wrapper is not always the same size as specified
	// in the wrapper. i.e. If a file has been compressed, the compressed
	// file will not be the same size as the original file.
	bool weOpenedFile = false;
	if ( !m_FileIsOpen )
	{
		if ( !OpenFile() )
			return -1;

		weOpenedFile = true;
	}

	// find the first field in the file
	long FieldStart, FieldLength;

	bool ret = FindField( WRAPPER_NUM_OF_FIELDS, FieldStart, FieldLength ) ;

	if ( weOpenedFile )
	{
		CloseFile();
	}

	if ( ret )
		return FieldStart ;
	else
		return -1;
}

bool CWrapper::FindField( int FieldNumber, long& FieldStart, long& FieldLength )
{
	ASSERT( FieldNumber <= WRAPPER_NUM_OF_FIELDS );

	FieldStart = FieldLength = 0;

	if ( !m_FileIsOpen )
		return false;

	long EndOfFile = m_DiskFile.SeekToEnd();
	long BeginningOfFile = 0 - EndOfFile - 1;

	long OffSet = -1;
	int FieldsRead = 0;

	long FieldEnd = 0;

	long FilePointer = 0;

	char FileChar;

	while (FieldsRead <= FieldNumber && OffSet > BeginningOfFile )
	{
		// search through file for next vertical bar
		do
		{
			FilePointer = m_DiskFile.Seek( OffSet, CFile::end );

			// read one byte in from the file
			if ( m_DiskFile.Read( &FileChar, 1 ) != 1 )
				return false;

			OffSet--;

			// don't search all the way through the file
			// if searching for field number 1!
			// (14 is a value that is longer than the length of
			// the wrapper key and everything else)
			if ( (FieldNumber == 1) && (OffSet < -14) )
				return false;

		} while ( (FileChar != '|') && (OffSet > BeginningOfFile) );

		if (OffSet > BeginningOfFile )
		{
			if (FieldsRead < FieldNumber)
			{
				FieldEnd = FilePointer;
			}
			else
			{
				FieldStart = FilePointer;
			}

			FieldsRead++;
		}
	}

	// Beginning of file reached, field not found
	if (FieldStart == 0 && FieldEnd == 0)
	{
		return false;
	}

	FieldLength = FieldEnd - (FieldStart + 1);

	return true;
}

void CWrapper::Clear()
{
	CloseFile();

	m_AssociatedFile.Empty();

	m_WrapperKey.Empty();
	m_OriginalFileSize.Empty();
	m_OriginalFileName.Empty();
	m_DateStamp.Empty();
	m_FileCRC.Empty();
	m_MachineName.Empty();
	m_PickupTime.Empty();
	m_WrapperCRC.Empty();
	m_ProcessStatus.Empty();

	m_isEmpty = true;

}

DWORD CWrapper::GenerateWrapperCRC() const
{
	// build the wrapper string in memory -- blanking out the wrapper CRC info!
	CString OutString;
	OutString.Format("|Success|0|%s|%s|%s|%s|%s|%s|%s|", 
		 m_PickupTime,
		 m_MachineName,
		 m_FileCRC,
		 m_DateStamp,
		 m_OriginalFileName,
		 m_OriginalFileSize,
		 WRAPPER_KEY );

	// now generate a CRC
	DWORD crc;

	CalculateCrc( crc, (LPCSTR) OutString, OutString.GetLength() );

	return crc;
}
