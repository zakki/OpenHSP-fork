//
//		Token analysis class ( HSP3 code generator )
//			onion software/onitama 2004/4
//
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "../hsp3/hsp3config.h"
#include "../hsp3/hsp3debug.h"
#include "../hsp3/hsp3struct.h"
#include "../hsp3/strnote.h"

#include "codegen_writer.h"
#include "comutil.h"
#include "label.h"
#include "membuf.h"
#include "supio.h"
#include "tagstack.h"
#include "token_def.h"

#include "errormsg.h"

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

#define GET_FI_SIZE() ( (int)( fi_buf->GetSize() / sizeof( HED_STRUCTDAT ) ) )
#define GET_FI( n ) ( ( (HED_STRUCTDAT *)fi_buf->GetBuffer() ) + ( n ) )
#define STRUCTDAT_INDEX_DUMMY ( (short)0x8000 )


void CCodeWriter::PutCS( int type, int value, int exflg )
{
	//		Register command code
	//		(HSP ver3.3以降用)
	//			type=0-0xfff ( -1 to debug line info )
	//			val=16,32bit length supported
	//
	int a;
	unsigned int v;
	v = (unsigned int)value;

	a = ( type & CSTYPE ) | exflg;
	if ( v < 0x10000 ) { // when 16bit encode
		cs_buf->Put( (short)( a ) );
		cs_buf->Put( (short)( v ) );
	} else { // when 32bit encode
		cs_buf->Put( (short)( 0x8000 | a ) );
		cs_buf->Put( (int)value );
	}
}


void CCodeWriter::PutCS( int type, double value, int exflg )
{
	//		Register command code (double)
	//
	PutCS( type, PutDS( value ), exflg );
}


void CCodeWriter::PutCSSymbol( int label_id, int exflag )
{
	//		まだ定義されていない関数の呼び出しがあったら仮登録する
	//
	int type = symtab->lb->GetType( label_id );
	int value = symtab->lb->GetOpt( label_id );
	if ( type == TYPE_MODCMD && value == -1 ) {
		int id = *(int *)symtab->lb->GetData2( label_id );
		symtab->tmp_lb->AddReference( id );

		HED_STRUCTDAT st = { STRUCTDAT_INDEX_DUMMY };
		st.otindex = label_id;
		value = GET_FI_SIZE();
		fi_buf->PutData( &st, sizeof( HED_STRUCTDAT ) );
		symtab->lb->SetOpt( label_id, value );
	}
	if ( ( ( exflag & EXFLG_1 ) != 0 ) && type != TYPE_VAR && type != TYPE_STRUCT ) {
		value &= 0xffff;
	}
	PutCS( type, value, exflag );
}


int CCodeWriter::GetCS()
{
	//		Get current CS index
	//
	return ( cs_buf->GetSize() ) >> 1;
}


int CCodeWriter::PutDS( double value )
{
	//		Register doubles to data segment
	//
	int i = ds_buf->GetSize();

	ds_buf->Put( value );
	return i;
}


int CCodeWriter::PutDS( char *str )
{
	//		Register strings to data segment (script string)
	//
	return PutDSStr( str, compopt->cg_utf8out() );
}


int CCodeWriter::PutDSBuf( char *str )
{
	//		Register strings to data segment (direct)
	//
	return PutDSStr( str, false );
}


int CCodeWriter::PutDSStr( char *str, bool converts_to_utf8 )
{
	//		Register strings to data segment (caching)

	char *p;

	// output as UTF8 format
	if ( converts_to_utf8 ) {
		p = ExecSCNV( str, SCNV_OPT_SJISUTF8 );
	} else {
		p = str;
		if ( compopt->pp_utf8 != 0 ) {
			if ( ( compopt->hed_cmpmode & CMPMODE_UTF8OUT ) == 0 ) {
				p = ExecSCNV( str, SCNV_OPT_UTF8SJIS );
			}
		}
	}

	int i = ds_buf->GetSize();

	if ( CG_optCode() ) {
		if ( *p != 0 ) {
			int i_cache = ds_buf->SearchIndexedData( p, -1 );
			if ( i_cache >= 0 ) {
				if ( CG_optInfo() ) {
					logger->Mesf( "#String pool:%s", p );
				}
				return i_cache;
			}
		}
	}

	ds_buf->IndexExclusive(); // 文字列はindexを登録する

	if ( converts_to_utf8 ) {
		ds_buf->PutData( p, (int)( strlen( p ) + 1 ) );
	} else {
		ds_buf->PutStr( p );
		ds_buf->Put( (char)0 );
	}
	return i;
}


int CCodeWriter::PutDSBuf( char *str, int size )
{
	//		Register strings to data segment (direct)
	//
	int i;
	i = ds_buf->GetSize();
	ds_buf->PutData( str, size );
	return i;
}


int CCodeWriter::PutOT( int value )
{
	//		Register object temp
	//
	int i;
	i = ot_buf->GetSize() / sizeof( int );
	ot_buf->Put( value );
	return i;
}


void CCodeWriter::SetOT( int id, int value )
{
	//		Modify object temp
	//
	int *p;
	p = (int *)( ot_buf->GetBuffer() );
	p[id] = value;
}


void CCodeWriter::PutDI()
{
	//		Debug code register
	//
	//		mem_di formats :
	//			0-250           = offset to old mcs
	//			252,x(16)       = big offset
	//			253,x(24),y(16) = used val (x=mds ptr.)
	//			254,x(24),y(16) = new filename accepted (x=mds ptr.)
	//			255             = end of debug data
	//
	int ofs;
	ofs = (int)( GetCS() - cg_lastcs );
	if ( ofs <= 250 ) {
		di_buf->Put( (unsigned char)ofs );
	} else {
		di_buf->Put( (unsigned char)252 );
		di_buf->Put( (unsigned char)( ofs ) );
		di_buf->Put( (unsigned char)( ofs >> 8 ) );
	}
	cg_lastcs = GetCS();
}


void CCodeWriter::PutDI( int dbg_code, int a, int subid )
{
	//		special Debug code register
	//			in : -1=end of code
	//				254=(a=file ds ptr./subid=line num.)
	//
	if ( dbg_code < 0 ) {
		di_buf->Put( (unsigned char)255 );
		di_buf->Put( (unsigned char)255 );
	} else {
		di_buf->Put( (unsigned char)dbg_code );
		di_buf->Put( (unsigned char)( a ) );
		di_buf->Put( (unsigned char)( a >> 8 ) );
		di_buf->Put( (unsigned char)( a >> 16 ) );
		di_buf->Put( (unsigned char)( subid ) );
		di_buf->Put( (unsigned char)( subid >> 8 ) );
	}
}


void CCodeWriter::PutDIVars()
{
	//		Debug info register for vals
	//
	char vtmpname[256];

	strcpy( vtmpname, "I_" );

	for ( int a = 0; a < symtab->lb->GetNumEntry(); a++ ) {
		LABOBJ *lab = symtab->lb->GetLabel( a );
		char *p;
		if ( lab->type == TK_OBJ ) {
			switch ( lab->typefix ) {
			case LAB_TYPEFIX_INT:
				vtmpname[0] = 'I';
				p = vtmpname;
				strcpy( p + 2, lab->name );
				break;
			case LAB_TYPEFIX_DOUBLE:
				vtmpname[0] = 'D';
				p = vtmpname;
				strcpy( p + 2, lab->name );
				break;
			case LAB_TYPEFIX_NONE:
			default:
				p = lab->name;
				break;
			}
			int i = PutDS( p );
			PutDI( 253, i, lab->opt );
		}
	}
}


// ラベル名の情報を出力する
void CCodeWriter::PutDILabels()
{
	int num = ot_buf->GetSize() / sizeof( int );
	int *table = new int[num];
	for ( int i = 0; i < num; i++ ) {
		table[i] = -1;
	}
	for ( int i = 0; i < symtab->lb->GetNumEntry(); i++ ) {
		if ( symtab->lb->GetType( i ) == TYPE_LABEL ) {
			int id = symtab->lb->GetOpt( i );
			table[id] = i;
		}
	}
	di_buf->Put( (unsigned char)255 );
	for ( int i = 0; i < num; i++ ) {
		if ( table[i] == -1 ) {
			continue;
		}
		char *name = symtab->lb->GetName( table[i] );
		int dsPos = PutDSBuf( name );
		PutDI( 251, dsPos, i );
	}
	delete[] table;
}


// 引数名の情報を出力する
void CCodeWriter::PutDIParams()
{
	di_buf->Put( (unsigned char)255 );
	for ( int i = 0; i < symtab->lb->GetNumEntry(); i++ ) {
		if ( symtab->lb->GetType( i ) == TYPE_STRUCT ) {
			int id = symtab->lb->GetOpt( i );
			if ( id < 0 ) {
				continue;
			}
			char *name = symtab->lb->GetName( i );
			int dsPos = PutDSBuf( name );
			PutDI( 251, dsPos, id );
		}
	}
}


char *CCodeWriter::GetDS( int ptr )
{
	int i;
	char *p;
	i = ds_buf->GetSize();
	if ( ptr >= i ) {
		return nullptr;
	}
	p = ds_buf->GetBuffer();
	p += ptr;
	return p;
}


/*
	rev 54
	mingw : warning : i は未初期化で使用されうる
	に対処。
*/

int CCodeWriter::PutLIB( int flag, char *name )
{
	int a;
	int i = -1;
	int p;
	HED_LIBDAT lib;
	HED_LIBDAT *l;
	p = li_buf->GetSize() / sizeof( HED_LIBDAT );
	l = (HED_LIBDAT *)li_buf->GetBuffer();

	if ( flag == LIBDAT_FLAG_DLL ) {
		if ( *name != 0 ) {
			for ( a = 0; a < p; a++ ) {
				if ( l->flag == flag ) {
					if ( strcmp( GetDS( l->nameidx ), name ) == 0 ) {
						return a;
					}
				}
				l++;
			}
			i = PutDSBuf( name );
		} else {
			i = -1;
		}
	}
	if ( flag == LIBDAT_FLAG_COMOBJ ) {
		COM_GUID guid;
		if ( ConvertIID( &guid, name ) != 0 ) {
			return -1;
		}
		i = PutDSBuf( (char *)&guid, sizeof( COM_GUID ) );
	}

	lib.flag = flag;
	lib.nameidx = i;
#ifdef PTR64BIT
	lib.p_hlib = 0;
#else
	lib.hlib = NULL;
#endif
	lib.clsid = -1;
	li_buf->PutData( &lib, sizeof( HED_LIBDAT ) );
	// logger->Mesf( "LIB#%d:%s",flag,name );

	return p;
}


void CCodeWriter::SetLIBIID( int id, char *clsid )
{
	HED_LIBDAT *l;
	l = (HED_LIBDAT *)li_buf->GetBuffer();
	l += id;
	if ( *clsid == 0 ) {
		l->clsid = -1;
	} else {
		l->clsid = PutDSBuf( clsid );
	}
}


int CCodeWriter::PutStructParam( short mptype, int extype )
{
	int size;
	int i;
	STRUCTPRM prm;

	i = mi_buf->GetSize() / sizeof( STRUCTPRM );

	prm.mptype = mptype;
	if ( extype == STRUCTPRM_SUBID_STID ) {
		prm.subid = (short)GET_FI_SIZE();
	} else {
		prm.subid = extype;
	}
	prm.offset = cg_stsize;

	size = 0;
	switch ( mptype ) {
	case MPTYPE_INUM:
	case MPTYPE_STRUCT:
		size = sizeof( int );
		break;
	case MPTYPE_LOCALVAR:
		size = sizeof( PVal );
		break;
	case MPTYPE_DNUM:
		size = sizeof( double );
		break;
	case MPTYPE_FLOAT:
		size = sizeof( float );
		break;
	case MPTYPE_LOCALSTRING:
	case MPTYPE_STRING:
	case MPTYPE_LABEL:
	case MPTYPE_PPVAL:
	case MPTYPE_PBMSCR:
	case MPTYPE_PVARPTR:
	case MPTYPE_IOBJECTVAR:
	case MPTYPE_LOCALWSTR:
	case MPTYPE_FLEXSPTR:
	case MPTYPE_FLEXWPTR:
	case MPTYPE_PTR_REFSTR:
	case MPTYPE_PTR_EXINFO:
	case MPTYPE_PTR_DPMINFO:
	case MPTYPE_NULLPTR:
		// XXX 32bit版換算でax出力する
		// size = sizeof(char *);
		size = 4;
		break;
	case MPTYPE_SINGLEVAR:
	case MPTYPE_ARRAYVAR:
		size = sizeof( MPVarData );
		break;
	case MPTYPE_MODULEVAR:
	case MPTYPE_IMODULEVAR:
	case MPTYPE_TMODULEVAR:
		size = sizeof( MPModVarData );
		break;
	default:
		return i;
	}
	cg_stsize += size;
	cg_stnum++;
	mi_buf->PutData( &prm, sizeof( STRUCTPRM ) );
	return i;
}


int CCodeWriter::PutStructParamTag()
{
	int i;
	STRUCTPRM prm;

	i = mi_buf->GetSize() / sizeof( STRUCTPRM );

	prm.mptype = MPTYPE_STRUCTTAG;
	prm.subid = (short)GET_FI_SIZE();
	prm.offset = -1;

	cg_stnum++;
	mi_buf->PutData( &prm, sizeof( STRUCTPRM ) );
	return i;
}


void CCodeWriter::PutStructStart()
{
	cg_stnum = 0;
	cg_stsize = 0;
	cg_stptr = mi_buf->GetSize() / sizeof( STRUCTPRM );
}


int CCodeWriter::PutStructEnd( int i, char *name, int libindex, int otindex, int funcflag )
{
	//		STRUCTDATを登録する(モジュール用)
	//
	HED_STRUCTDAT st;
	st.index = libindex;
	st.nameidx = PutDSBuf( name );
	st.subid = i;
	st.prmindex = cg_stptr;
	st.prmmax = cg_stnum;
	st.funcflag = funcflag;
	st.size = cg_stsize;
	if ( otindex < 0 ) {
		st.otindex = 0;
		st.subid = otindex;
	} else {
		st.otindex = otindex;
	}
	*GET_FI( i ) = st;
	// logger->Mesf( "#%d : %s(LIB%d) prm%d size%d ot%d", i, name, libindex, cg_stnum, cg_stsize, otindex );
	return i;
}


int CCodeWriter::PutStructEnd( char *name, int libindex, int otindex, int funcflag )
{
	int i = GET_FI_SIZE();
	fi_buf->PreparePtr( sizeof( HED_STRUCTDAT ) );
	return PutStructEnd( i, name, libindex, otindex, funcflag );
}

int CCodeWriter::PutStructEndDll( char *name, int libindex, int subid, int otindex )
{
	//		STRUCTDATを登録する(DLL用)
	//
	int i;
	HED_STRUCTDAT st;
	i = GET_FI_SIZE();
	st.index = libindex;
	if ( name[0] == '*' ) {
		st.nameidx = -1;
	} else {
		st.nameidx = PutDSBuf( name );
	}
	st.subid = subid;
	st.prmindex = cg_stptr;
	st.prmmax = cg_stnum;
	// st.proc = NULL;
	st.funcflag = 0;
	st.size = cg_stsize;
	st.otindex = otindex;
	fi_buf->PutData( &st, sizeof( HED_STRUCTDAT ) );
	// logger->Mesf( "#%d : %s(LIB%d) prm%d size%d ot%d", i, name, libindex, cg_stnum, cg_stsize, otindex );
	return i;
}


void CCodeWriter::PutHPI( short flag, short option, char *libname, char *funcname )
{
	HPIDAT hpi;
	hpi.flag = flag;
	hpi.option = option;
	hpi.libname = PutDSBuf( libname );
	hpi.funcname = PutDSBuf( funcname );
#ifndef PTR64BIT
	hpi.libptr = NULL;
#else
	hpi.p_libptr = 0;
#endif
	hpi_buf->PutData( &hpi, sizeof( HPIDAT ) );
}


int CCodeWriter::SaveStringMap( const char *fname )
{
	//      Output String Map
	//
	int max;
	int i;
	int ofs;
	int ptr;
	int chksum;
	unsigned char *p;
	unsigned char *mem;
	unsigned char a1;
	CMemBuf outbuf;

	outbuf.PutStrf( ";      strmap v0.5 %s", fname );
	outbuf.PutCR();

	max = ds_buf->GetIndexBufferSize();
	mem = (unsigned char *)ds_buf->GetBuffer();
	for ( i = 0; i < max; i++ ) {
		ptr = ds_buf->GetIndex( i );
		p = mem + ptr;
		chksum = 0;
		ofs = 0;
		while ( true ) {
			a1 = p[ofs++];
			if ( a1 == 0 ) {
				break;
			}
			chksum += ofs + ( (int)a1 ) * 7;
		}
		outbuf.PutStrf( "&&&&dsmap:%d,%d,%d", ofs, ptr, chksum );
		outbuf.PutCR();
		outbuf.PutStr( (char *)p );
		outbuf.PutCR();
	}

	return outbuf.SaveFile( fname );
}

//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------
CCodeWriter::CCodeWriter( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log,
						  std::shared_ptr<CSymbolTable> s )
	: CCompilerUtil( compopt ), logger( std::move( log ) ), symtab( std::move( s ) )
{
	cs_buf = std::make_unique<CMemBuf>();
	ds_buf = std::make_unique<CMemBuf>();
	ot_buf = std::make_unique<CMemBuf>();
	di_buf = std::make_unique<CMemBuf>();
	li_buf = std::make_unique<CMemBuf>();
	fi_buf = std::make_unique<CMemBuf>();
	mi_buf = std::make_unique<CMemBuf>();
	fi2_buf = std::make_unique<CMemBuf>();
	hpi_buf = std::make_unique<CMemBuf>();
}

CCodeWriter::~CCodeWriter() = default;

int CCodeWriter::Write( const char *oname, int mode, int cg_valcnt, int cg_varhpi, int cg_putvars )
{
	int res = 0;
	CMemBuf optbuf;	  // オプション文字列用バッファ
	CMemBuf exoptbuf; // 拡張オプション用バッファ
	CMemBuf axbuf;

	HSPHED hsphed;
	int sz_hed;
	int sz_opt;
	int cs_size;
	int ds_size;
	int ot_size;
	int di_size;
	int li_size;
	int fi_size;
	int mi_size;
	int fi2_size;
	int hpi_size;
	int sz_exopt;

	int orgcs = GetCS();
	PutCS( TYPE_PROGCMD, 0x11, EXFLG_1 ); // 終了コードを最後に入れる
	int otlabel = PutOT( orgcs );
	PutCS( TYPE_PROGCMD, 0, EXFLG_1 );
	PutCS( TYPE_LABEL, otlabel, 0 );

	if ( compopt->cg_debug() ) {
		PutDI();
	}
	if ( compopt->cg_debug() || ( cg_putvars != 0 ) ) {
		PutDIVars();
	}
	if ( compopt->cg_debug() ) {
		PutDILabels();
		PutDIParams();
	}
	PutDI( -1, 0, 0 ); // デバッグ情報終端

	sz_hed = sizeof( HSPHED );
	memset( &hsphed, 0, sz_hed );
	hsphed.bootoption = 0;
	hsphed.runtime = 0;

	if ( ( compopt->hed_option & HEDINFO_RUNTIME ) != 0 ) {
		optbuf.PutStr( compopt->hed_runtime );
	}
	sz_opt = optbuf.GetSize();
	if ( sz_opt != 0 ) {
		while ( true ) {
			int adjsize = ( sz_opt + 15 ) & 0xfff0;
			if ( adjsize == sz_opt ) {
				break;
			}
			optbuf.Put( (char)0 );
			sz_opt = optbuf.GetSize();
		}
		hsphed.bootoption |= HSPHED_BOOTOPT_RUNTIME;
		hsphed.runtime = sz_hed;
		sz_hed += sz_opt;
		if ( ( compopt->hed_option & HEDINFO_UTF8 ) != 0 ) {
			hsphed.bootoption |= HSPHED_BOOTOPT_UTF8; // ランタイムはUTF8を使用する
		}
		if ( ( compopt->hed_option & HEDINFO_HSP64 ) != 0 ) {
			hsphed.bootoption |= HSPHED_BOOTOPT_HSP64; // ランタイムは64bitで動作する
		}
	}

	//		デバッグウインドゥ表示
	if ( ( mode & COMP_MODE_DEBUGWIN ) != 0 ) {
		hsphed.bootoption |= HSPHED_BOOTOPT_DEBUGWIN;
	}
	//		起動オプションの設定
	if ( compopt->hed_autoopt_timer >= 0 ) {
		// awaitが使用されていない場合はマルチメディアタイマーを無効にする(自動設定)
		if ( compopt->hed_autoopt_timer == 0 ) {
			hsphed.bootoption |= HSPHED_BOOTOPT_NOMMTIMER;
		}
	} else {
		// 設定されたオプションに従ってマルチメディアタイマーを無効にする
		if ( ( compopt->hed_option & HEDINFO_NOMMTIMER ) != 0 ) {
			hsphed.bootoption |= HSPHED_BOOTOPT_NOMMTIMER;
		}
	}

	if ( ( compopt->hed_option & HEDINFO_NOGDIP ) != 0 ) {
		hsphed.bootoption |= HSPHED_BOOTOPT_NOGDIP; // GDI+による描画を無効にする
	}
	if ( ( compopt->hed_option & HEDINFO_FLOAT32 ) != 0 ) {
		hsphed.bootoption |= HSPHED_BOOTOPT_FLOAT32; // 実数を32bit floatとして処理する
	}
	if ( ( compopt->hed_option & HEDINFO_ORGRND ) != 0 ) {
		hsphed.bootoption |= HSPHED_BOOTOPT_ORGRND; // 標準の乱数発生を使用する
	}
	if ( ( compopt->hed_option & HEDINFO_IORESUME ) != 0 ) {
		hsphed.bootoption |= HSPHED_BOOTOPT_IORESUME; // ファイルI/Oエラーを無視して処理を続行する
	}

	//		文字列テーブルの作成
	if ( compopt->hed_autoopt_strexchange > 0 ) {
		int dspool_size = ds_buf->GetIndexBufferSize();
		if ( dspool_size != 0 ) {
			exoptbuf.Put( (int)( HSPHED_EXOPTION_TAG_DSINDEX ) + ( dspool_size << 16 ) );
			exoptbuf.PutData( ds_buf->GetIndexBuffer(), dspool_size * sizeof( int ) );
		}
	}
	//		型固定情報の作成
	if ( compopt->cg_debug() ) {
		for ( int i = 0; i < symtab->lb->GetCount(); i++ ) {
			if ( symtab->lb->GetType( i ) == TYPE_VAR ) {
				int fixedvalue = symtab->lb->GetForceType( i );
				if ( fixedvalue != LAB_TYPEFIX_NONE ) {
					int bodysize = sizeof( int ) * 2;
					// logger->Mes(symtab->lb->GetName(i));
					exoptbuf.Put( (int)( HSPHED_EXOPTION_TAG_VARFIX ) + ( bodysize << 16 ) );
					exoptbuf.Put( symtab->lb->GetOpt( i ) );
					exoptbuf.Put( fixedvalue );
				}
			}
		}
	}

	if ( exoptbuf.GetSize() != 0 ) {
		//	exoptの終端
		exoptbuf.Put( (int)HSPHED_EXOPTION_TAG_NONE );
	}

	sz_exopt = exoptbuf.GetSize();

	cs_size = cs_buf->GetSize();
	ds_size = ds_buf->GetSize();
	ot_size = ot_buf->GetSize();
	di_size = di_buf->GetSize();

	li_size = li_buf->GetSize();
	fi_size = fi_buf->GetSize();
	mi_size = mi_buf->GetSize();
	fi2_size = fi2_buf->GetSize();
	hpi_size = hpi_buf->GetSize();

	hsphed.h1 = 'H';
	hsphed.h2 = 'S';
	hsphed.h3 = 'P';
	hsphed.h4 = '3';
	hsphed.version = 0x0360;	// version3.5
	hsphed.max_val = cg_valcnt; // max count of VAL Object
	hsphed.allsize = sz_hed + cs_size + ds_size + ot_size + di_size;
	hsphed.allsize += li_size + fi_size + mi_size + fi2_size + hpi_size + sz_exopt;

	hsphed.pt_cs = sz_hed;					  // ptr to Code Segment
	hsphed.max_cs = cs_size;				  // size of CS
	hsphed.pt_ds = sz_hed + cs_size;		  // ptr to Data Segment
	hsphed.max_ds = ds_size;				  // size of DS
	hsphed.pt_ot = hsphed.pt_ds + ds_size;	  // ptr to Object Temp
	hsphed.max_ot = ot_size;				  // size of OT
	hsphed.pt_dinfo = hsphed.pt_ot + ot_size; // ptr to Debug Info
	hsphed.max_dinfo = di_size;				  // size of DI

	hsphed.pt_linfo = hsphed.pt_dinfo + di_size; // ptr to Debug Info
	hsphed.max_linfo = li_size;					 // size of LINFO

	hsphed.pt_finfo = hsphed.pt_linfo + li_size; // ptr to Debug Info
	hsphed.max_finfo = fi_size;					 // size of FINFO

	hsphed.pt_minfo = hsphed.pt_finfo + fi_size; // ptr to Debug Info
	hsphed.max_minfo = mi_size;					 // size of MINFO

	hsphed.pt_finfo2 = hsphed.pt_minfo + mi_size; // ptr to Debug Info
	hsphed.max_finfo2 = fi2_size;				  // size of FINFO2

	hsphed.pt_hpidat = hsphed.pt_finfo2 + fi2_size; // ptr to Debug Info
	hsphed.max_hpi = hpi_size;						// size of HPIDAT
	hsphed.max_varhpi = cg_varhpi;					// Num of Vartype Plugins

	hsphed.pt_sr = sizeof( HSPHED ); // ptr to Option Segment
	hsphed.max_sr = sz_opt;			 // size of Option Segment
	hsphed.pt_exopt = hsphed.pt_hpidat + hpi_size;
	hsphed.max_exopt = sz_exopt;


	axbuf.PutData( &hsphed, sizeof( HSPHED ) );
	if ( sz_opt != 0 ) {
		axbuf.PutData( optbuf.GetBuffer(), sz_opt );
	}

	if ( cs_size != 0 ) {
		axbuf.PutData( cs_buf->GetBuffer(), cs_size );
	}
	if ( ds_size != 0 ) {
		axbuf.PutData( ds_buf->GetBuffer(), ds_size );
	}
	if ( ot_size != 0 ) {
		axbuf.PutData( ot_buf->GetBuffer(), ot_size );
	}
	if ( di_size != 0 ) {
		axbuf.PutData( di_buf->GetBuffer(), di_size );
	}

	if ( li_size != 0 ) {
		axbuf.PutData( li_buf->GetBuffer(), li_size );
	}
	if ( fi_size != 0 ) {
		axbuf.PutData( fi_buf->GetBuffer(), fi_size );
	}
	if ( mi_size != 0 ) {
		axbuf.PutData( mi_buf->GetBuffer(), mi_size );
	}
	if ( fi2_size != 0 ) {
		axbuf.PutData( fi2_buf->GetBuffer(), fi2_size );
	}
	if ( hpi_size != 0 ) {
		axbuf.PutData( hpi_buf->GetBuffer(), hpi_size );
	}
	if ( sz_exopt != 0 ) {
		axbuf.PutData( exoptbuf.GetBuffer(), sz_exopt );
	}
	if ( compopt->cg_strmap() ) {
		if ( *oname != 0 ) {
			res = SaveStringMap( oname );
		}
	} else {
		if ( *oname != 0 ) {
			res = axbuf.SaveFile( oname );
		}
	}
	if ( res < 0 ) {
#ifdef JPNMSG
		logger->Mes( "#出力ファイルを書き込めません" );
#else
		logger->Mes( "#Can't write output file." );
#endif
	} else {
		int n_mod;
		int n_hpi;
		n_hpi = hpi_buf->GetSize() / sizeof( HPIDAT );
		n_mod = fi_buf->GetSize() / sizeof( HED_STRUCTDAT );
		logger->Mesf( "#Code size (%d) String data size (%d) param size (%d)", cs_size, ds_size, mi_buf->GetSize() );
		logger->Mesf( "#Vars (%d) Labels (%d) Modules (%d) Libs (%d) Plugins (%d)", cg_valcnt, ot_size >> 2, n_mod,
					  li_size, n_hpi );
		if ( sz_exopt != 0 ) {
			logger->Mesf( "#Output extra data field (%d).", sz_exopt );
		}
		logger->Mesf( "#No error detected. (total %d bytes)", hsphed.allsize );
		res = 0;
	}

	return res;
}
