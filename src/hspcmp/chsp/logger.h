#pragma once

#include <memory>

class CMemBuf;

//  util class
class CLogger
{
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
