// CRC.h: CRC header file
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GENERAL_H__DD4C6D75_97C2_11D1_889E_00004B481291__INCLUDED_)
#define AFX_GENERAL_H__DD4C6D75_97C2_11D1_889E_00004B481291__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000



// Check CRC on szFileName.  
//  Returns false if check failed.
bool CalculateCrc( DWORD& crc,  const CString& szFileName );

// Check CRC of buffer.
//  Returns false if check failed.
bool CalculateCrc( DWORD& crc, const void *buffer, DWORD buffSize );


#endif // !defined(AFX_GENERAL_H__DD4C6D75_97C2_11D1_889E_00004B481291__INCLUDED_)

