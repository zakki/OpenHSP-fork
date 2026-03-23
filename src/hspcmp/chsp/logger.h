//
//	logger.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

class CLabel;
class CMemBuf;

#define SCNVBUF_DEFAULTSIZE 0x8000
#define SCNV_OPT_NONE 0
#define SCNV_OPT_SJISUTF8 1
#define SCNV_OPT_UTF8SJIS 2


//  util class
class CLogger
{
	// friend class CToken;
public:
	explicit CLogger( const std::shared_ptr<CMemBuf> &buf ) : errbuf( buf )
	{
	}
	~CLogger()
	{
	}

	void Error( const char *mes );
	void LineError( int line, const char *fname );
	void SetError( const char *mes );
	void Mes( const char *mes );
	void Mesf( const char *format, ... );
	void SetErrorBuf( const std::shared_ptr<CMemBuf> &buf );

	void ClearError()
	{
		*errtmp = 0;
	}

protected:
	//		Data
	//
	std::shared_ptr<CMemBuf> errbuf;
	char errtmp[128]; // temp for error message
};
