//
//		Label Manager class
//			onion software/onitama 2002/2
//
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "label.h"

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

void CLabel::StrCase( char *str )
{
	//	string case to lower
	//
	unsigned char a1;
	unsigned char a2;
	unsigned char *ss;

	if ( casemode != 0 ) { // 大文字小文字を区別する
		return;
	}

	ss = (unsigned char *)str;
	while ( true ) {
		a1 = *ss;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 >= 0x80 ) {
			ss++;
			a1 = *ss++;
			if ( a1 == 0 ) {
				break;
			}
		} else {
			a2 = tolower( a1 );
			*ss++ = a2;
		}
	}
}


int CLabel::Regist( const std::string &name, int type, int opt )
{
	return Regist( name, type, opt, "", -1 );
}


int CLabel::Regist( const std::string &name, int type, int opt, const std::string &filename, int line )
{
	if ( name.empty() ) {
		return -1;
	}

	std::string canonical = name;
	StrCase( canonical.data() );
	int label_id = mem_lab.size();
	mem_lab.emplace_back();
	LABOBJ *lab = &mem_lab.back();
	lab->flag = 1;
	lab->type = type;
	lab->opt = opt;
	lab->eternal = 0;
	lab->ref = 0;
	lab->name = canonical;
	lab->data = nullptr;
	lab->data2 = nullptr;
	lab->rel = nullptr;
	lab->init = LAB_INIT_NO;
	lab->typefix = LAB_TYPEFIX_NONE;
	lab->def_file.clear();
	lab->def_line = -1;
	lab->skiplablist = false;
	SetDefinition( label_id, filename, line );

	labels.insert( std::make_pair( lab->name, label_id ) );
	return label_id;
}


void CLabel::SetEternal( int id )
{
	//		set eternal flag
	//
	mem_lab[id].eternal = 1;
}


int CLabel::GetEternal( int id )
{
	//		set eternal flag
	//
	return mem_lab[id].eternal;
}


void CLabel::SetFlag( int id, int val )
{
	//		set eternal flag
	//
	mem_lab[id].flag = val;
}


void CLabel::SetOpt( int id, int val )
{
	//		set option
	//
	mem_lab[id].opt = val;
}


void CLabel::SetData( int id, char *str )
{
	//		set data
	//
	LABOBJ *lab = &mem_lab[id];
	if ( str == nullptr ) {
		lab->data = nullptr;
		return;
	}
	lab->data = RegistTable( str, (int)strlen( str ) + 1 );
}


void CLabel::SetData2( int id, char *str, int size )
{
	//		set data
	//
	LABOBJ *lab = &mem_lab[id];
	if ( str == nullptr ) {
		lab->data2 = nullptr;
		return;
	}
	lab->data2 = RegistTable( str, size );
}


void CLabel::SetInitFlag( int id, int val )
{
	//		set init flag
	//
	mem_lab[id].init = (short)val;
}


void CLabel::SetForceType( int id, int val )
{
	//		set force type
	//
	mem_lab[id].typefix = (short)val;
}


int CLabel::Search( const std::string &oname )
{
	//		object name search
	//
	if ( mem_lab.empty() ) {
		return -1;
	}
	if ( oname.empty() ) {
		return -1;
	}

	std::string canonical = oname;
	StrCase( canonical.data() );

	std::pair<LabelMap::iterator, LabelMap::iterator> r = labels.equal_range( canonical );
	for ( auto it = r.first; it != r.second; ++it ) {
		LABOBJ *lab = &mem_lab[it->second];
		if ( lab->flag >= 0 ) {
			return it->second;
		}
	}

	return -1;
}


int CLabel::SearchLocal( const std::string &global_name, const std::string &local_name )
{

	//		Retrieves the object name specified by 'global_name' and by 'local_name', first from the local scope and
	// then
	// from the global scope, and returns its ID on success or -1 if not found. 		'global_name' has to points to
	// the global name, and 'local_name' has to points to the local name; the module name should suffix the string of
	// 'local_name'.

	int i = Search( local_name );

	if ( i >= 0 && ( mem_lab[i].eternal == 0 ) ) {
		return i;
	}

	i = Search( global_name );

	if ( i >= 0 && ( mem_lab[i].eternal != 0 ) ) {
		return i;
	}

	return -1;
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CLabel::CLabel() : maxsymbol( def_maxsymbol ), token{ 0 }
{
	Reset();
}


CLabel::CLabel( int symmax ) : maxsymbol( symmax ), token{ 0 }
{
	Reset();
}


void CLabel::Reset()
{
	labels.clear();
	filenames.clear();
	mem_lab.clear();
	DisposeSymbolBuffer();
	MakeSymbolBuffer();
	casemode = 0;
}


CLabel::~CLabel()
{
	DisposeSymbolBuffer();
}


void CLabel::MakeSymbolBuffer()
{
	symbol.emplace_back();
	symbol.back().reserve( maxsymbol );
	symcur = 0;
}


void CLabel::DisposeSymbolBuffer()
{
	symbol.clear();
}


char *CLabel::ExpandSymbolBuffer( int size )
{
	int nsize = ( ( size + 7 ) >> 3 ) << 3;
	size = nsize;
	if ( symbol.empty() || ( symcur + size > symbol.back().capacity() ) ) {
		MakeSymbolBuffer();
	}
	size_t old_symcur = symcur;
	symcur += size;
	symbol.back().resize( symcur );
	return symbol.back().data() + old_symcur;
}


int CLabel::GetCount()
{
	return mem_lab.size();
}


int CLabel::GetFlag( int id )
{
	return mem_lab[id].flag;
}


int CLabel::GetOpt( int id )
{
	return mem_lab[id].opt;
}


int CLabel::GetType( int id )
{
	return mem_lab[id].type;
}


const std::string &CLabel::GetName( int id )
{
	return mem_lab[id].name;
}


char *CLabel::GetData( int id )
{
	return mem_lab[id].data;
}


char *CLabel::GetData2( int id )
{
	return mem_lab[id].data2;
}


LABOBJ *CLabel::GetLabel( int id )
{
	return &mem_lab[id];
}


int CLabel::GetInitFlag( int id )
{
	return (int)mem_lab[id].init;
}


int CLabel::GetForceType( int id )
{
	return (int)mem_lab[id].typefix;
}


char *CLabel::RegistTable( char *str, int size )
{
	//		シンボルテーブルにテーブルデータを登録
	//
	char *p;
	char *src;
	src = str;
	p = ExpandSymbolBuffer( size );
	memcpy( p, src, size );
	return p;
}


char *CLabel::GetListToken( char *str )
{
	char *p;
	char *dst;
	char a1;
	p = str;
	while ( true ) {
		a1 = *p;
		if ( a1 != 32 ) {
			break;
		}
		p++;
	}
	dst = token;
	while ( true ) {
		a1 = *p;
		if ( ( a1 == 0 ) || ( a1 == 32 ) ) {
			break;
		}
		*dst++ = a1;
		p++;
	}
	*dst = 0;
	return p;
}


int CLabel::RegistList( char **list, const std::string &modname )
{
	//		キーワードリストを登録する
	//
	char tmp[256];
	for ( int i = 1;; i++ ) {
		char *p = tmp;
		strcpy( p, list[i] );
		if ( p[0] != '$' ) {
			break;
		}
		p++;
		p = GetListToken( p );
		int opt = HtoI();
		p = GetListToken( p );
		int type = atoi( token );
		p = GetListToken( p );
		strcat( token, modname.c_str() );
		int id = Regist( token, type, opt );
		SetEternal( id );
	}
	return 0;
}


int CLabel::RegistList2( char **list, const std::string &modname )
{
	//		キーワードリストをword@modnameの代替マクロとして登録する
	//
	char tmp[256];
	for ( int i = 1;; i++ ) {
		char *p = tmp;
		strcpy( p, list[i] );
		if ( p[0] != '$' ) {
			break;
		}
		p++;
		p = GetListToken( p );
		int opt = HtoI();
		p = GetListToken( p );
		int type = atoi( token );
		p = GetListToken( p );
		// id = Regist( token, type, opt );

		int id = Regist( token, LAB_TYPE_PPINTMAC, 0 ); // 内部マクロとして定義
		strcat( token, modname.c_str() );
		SetData( id, token );
		SetEternal( id );
	}
	return 0;
}


int CLabel::RegistList3( char **list )
{
	//		キーワードリストを色分けテーブル用に登録する
	//
	char tmp[256];
	static int kwcnv[] = {

		LAB_TYPE_PPEX_PRECMD, // TYPE_MARK 0
		LAB_TYPE_PPMAC,		  // TYPE_VAR 1
		LAB_TYPE_PPEX_INTCMD, // TYPE_STRING 2
		LAB_TYPE_PPEX_INTCMD, // TYPE_DNUM 3
		LAB_TYPE_PPEX_INTCMD, // TYPE_INUM 4
		LAB_TYPE_PPEX_INTCMD, // TYPE_STRUCT 5
		LAB_TYPE_PPEX_INTCMD, // TYPE_XLABEL 6
		LAB_TYPE_PPEX_INTCMD, // TYPE_LABEL 7
		LAB_TYPE_PPEX_INTCMD, // TYPE_INTCMD 8
		LAB_TYPE_PPEX_INTCMD, // TYPE_EXTCMD 9
		LAB_TYPE_PPEX_INTCMD, // TYPE_EXTSYSVAR 10
		LAB_TYPE_PPEX_INTCMD, // TYPE_CMPCMD 11
		LAB_TYPE_PPEX_INTCMD, // TYPE_MODCMD 12
		LAB_TYPE_PPEX_INTCMD, // TYPE_INTFUNC 13
		LAB_TYPE_PPEX_INTCMD, // TYPE_SYSVAR 14
		LAB_TYPE_PPEX_INTCMD, // TYPE_PROGCMD 15
		LAB_TYPE_PPEX_INTCMD, // TYPE_DLLFUNC 16
		LAB_TYPE_PPEX_EXTCMD, // TYPE_DLLCTRL 17
		LAB_TYPE_PPEX_INTCMD, // TYPE_USERDEF 18

	};

	for ( int i = 1;; i++ ) {
		char *p = tmp;
		strcpy( p, list[i] );
		if ( p[0] != '$' ) {
			break;
		}
		p++;
		p = GetListToken( p );
		int opt = HtoI();
		p = GetListToken( p );
		int type = atoi( token );
		p = GetListToken( p );
		int id = Regist( token, kwcnv[type], opt );
		SetEternal( id );
	}
	return 0;
}


//-------------------------------------------------------------
//		For debug
//-------------------------------------------------------------

char *CLabel::Prt( char *str, const std::string &str2 )
{
	char *p;
	p = str;
	strcpy( str, str2.c_str() );
	p += strlen( str2.c_str() );
	*p++ = 13;
	*p++ = 10;
	return p;
}


int CLabel::HtoI()
{
	//	Convert token(hex) to int
	char *wp;
	char a1;
	int val;
	int b;
	val = 0;
	wp = token;
	while ( true ) {
		a1 = toupper( *wp );
		b = -1;
		if ( a1 == 0 ) {
			wp = nullptr;
			break;
		}
		if ( ( a1 >= 0x30 ) && ( a1 <= 0x39 ) ) {
			b = a1 - 0x30;
		}
		if ( ( a1 >= 0x41 ) && ( a1 <= 0x46 ) ) {
			b = a1 - 55;
		}
		if ( a1 == '_' ) {
			b = -2;
		}
		if ( b == -1 ) {
			break;
		}
		if ( b >= 0 ) {
			val = ( val << 4 ) + b;
		}
		wp++;
	}
	return val;
}


void CLabel::DumpLabel( char *str )
{
	char tmp[256];
	char *p;
	int a;
	p = str;
	p = Prt( p, "#Debug dump" );
	sprintf( tmp, "#Labels:%d", (int)mem_lab.size() );
	p = Prt( p, tmp );
	for ( size_t a = 0; a < mem_lab.size(); a++ ) {
		LABOBJ *lab = &mem_lab[a];
		sprintf( tmp, "#ID:%d (%s) flag:%d  type:%d  opt:%x", (int)a, lab->name.c_str(), lab->flag, lab->type,
				 lab->opt );
		p = Prt( p, tmp );
		//		lab = GetLabel( Search( lab->name) );
		//		sprintf( tmp,"#ID:%d (%s) flag:%d  type:%d  opt:%x",a,lab->name,lab->flag,lab->type,lab->opt );
		//		p=Prt( p,tmp );
	}
	*p = 0;
}


int CLabel::DumpHSPLabelById( int id, char *str, int option )
{
	char tmp[256];
	char *typem;
	char *p;
	p = str;

	LABOBJ *lab = &mem_lab[id];
	typem = nullptr;
	switch ( lab->type ) {
	case LAB_TYPE_PPEX_PRECMD:
		if ( ( option & LAB_DUMPMODE_RESCMD ) != 0 ) {
			typem = "pre|func";
		}
		break;
	case LAB_TYPE_PPEX_EXTCMD:
		if ( ( option & LAB_DUMPMODE_RESCMD ) != 0 ) {
			typem = "sys|func|1";
		}
		break;
	case LAB_TYPE_PPDLLFUNC:
		if ( ( option & LAB_DUMPMODE_DLLCMD ) != 0 ) {
			typem = "sys|func|2";
		}
		break;
	case LAB_TYPE_PPMODFUNC:
		if ( ( option & LAB_DUMPMODE_DLLCMD ) != 0 ) {
			typem = "sys|func|3";
		}
		break;
	case LAB_TYPE_PPMAC:
	case LAB_TYPE_PPVAL:
		if ( ( option & LAB_DUMPMODE_RESCMD ) != 0 ) {
			typem = "sys|macro";
		}
		break;
	default:
		if ( ( option & LAB_DUMPMODE_RESCMD ) != 0 ) {
			typem = "sys|func";
		}
		break;
	}
	if ( typem != nullptr ) {
		sprintf( tmp, "%s\t,%s", lab->name.c_str(), typem );
		p = Prt( p, tmp );
	}

	*p = 0;
	return ( p - str );
}


void CLabel::DumpHSPLabel( char *str, int option, int maxsize )
{
	char *p;
	char *p_limit;
	int a;
	int len;
	p = str;
	p_limit = p + maxsize;

	for ( size_t a = 0; a < mem_lab.size(); a++ ) {
		if ( p >= p_limit ) {
			break;
		}
		len = DumpHSPLabelById( a, p, option );
		p += len;
	}
	*p = 0;
}


void CLabel::AddReference( int id )
{
	//		参照回数を+1する
	//
	LABOBJ *lab = &mem_lab[id];
	lab->ref++;
}


int CLabel::GetReference( int id )
{
	//		参照回数を取得する(依存関係も含める)
	//
	int total;
	LABREL *rel;
	LABOBJ *lab = &mem_lab[id];
	total = lab->ref;
	rel = lab->rel;
	if ( rel != nullptr ) {
		while ( true ) {
			total += GetReference( rel->rel_id );
			if ( rel->link == nullptr ) {
				break;
			}
			rel = rel->link;
		}
	}
	return total;
}


int CLabel::SearchRelation( int id, int rel_id )
{
	//		ラベル依存の特定IDデータがあるかを検索
	//		(0=なし/1=あり)
	//
	LABREL *tmp;
	LABOBJ *lab = &mem_lab[id];
	tmp = lab->rel;
	if ( tmp == nullptr ) {
		return 0;
	}
	while ( true ) {
		if ( tmp->link == nullptr ) {
			break;
		}
		if ( tmp->rel_id == rel_id ) {
			return 1;
		}
		tmp = tmp->link;
	}
	return 0;
}


void CLabel::AddRelation( int id, int rel_id )
{
	//		ラベル依存のIDデータを追加する
	//
	LABREL *rel;
	LABREL *tmp;

	if ( id == rel_id ) {
		return; // 循環するようなデータは登録しない
	}

	rel = (LABREL *)ExpandSymbolBuffer( sizeof( LABREL ) );
	rel->link = nullptr;
	rel->rel_id = rel_id;

	LABOBJ *lab = &mem_lab[id];
	if ( lab->rel == nullptr ) {
		lab->rel = rel;
		return;
	}
	tmp = lab->rel;
	while ( true ) {
		if ( tmp->link == nullptr ) {
			break;
		}
		tmp = tmp->link;
	}
	tmp->link = rel;
}


void CLabel::AddRelation( const std::string &name, int rel_id )
{
	int i;
	i = Search( name );
	if ( i < 0 ) {
		return;
	}
	AddRelation( i, rel_id );
}


void CLabel::SetCaseMode( int flag )
{
	casemode = flag;
}

void CLabel::SetDefinition( int id, const std::string &filename, int line )
{
	if ( filename.empty() || line < 0 ) {
		return;
	}

	filenames.insert( filename );
	LABOBJ *const it = GetLabel( id );
	it->def_file = filename;
	it->def_line = line;
}

void CLabel::SetSkipLabList( int id )
{
	LABOBJ *const it = GetLabel( id );
	it->skiplablist = true;
}
