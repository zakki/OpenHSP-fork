//
//	HSP3 any variable type plugin
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../../../hsp3/hsp3struct.h"

#ifdef _WIN32
#define HSPANY_API extern "C" __declspec(dllexport) void __stdcall
#else
#define HSPANY_API extern "C" void
#endif

#ifndef HSPANY_TYPE_NAME
#define HSPANY_TYPE_NAME "any"
#endif

typedef struct
{
	short flag;
	short reserved;
	union
	{
		int i;
		double d;
		int64_t l;
		char *s;
	} value;
} AnyValue;

static HSPEXINFO *g_exinfo;
static int g_any_flag;

static int conv_i;
static int64_t conv_l;
static double conv_d;
static char conv_s[400];
static AnyValue conv_any;

static char *any_alloc( size_t size )
{
	if ( g_exinfo != NULL && g_exinfo->HspFunc_malloc != NULL ) {
		return g_exinfo->HspFunc_malloc( size );
	}
	return (char *)malloc( size );
}

static char *any_expand( char *ptr, size_t size )
{
	if ( g_exinfo != NULL && g_exinfo->HspFunc_expand != NULL ) {
		return g_exinfo->HspFunc_expand( ptr, size );
	}
	return (char *)realloc( ptr, size );
}

static void any_free( void *ptr )
{
	if ( ptr == NULL ) return;
	if ( g_exinfo != NULL && g_exinfo->HspFunc_free != NULL ) {
		g_exinfo->HspFunc_free( ptr );
		return;
	}
	free( ptr );
}

static void *HspVarAny_GetBlockPtr( const AnyValue *value, int *size );

static void AnyValue_Clear( AnyValue *value )
{
	if ( value->flag == HSPVAR_FLAG_STR ) {
		any_free( value->value.s );
	}
	value->flag = HSPVAR_FLAG_NONE;
	value->reserved = 0;
	value->value.l = 0;
}

static void AnyValue_SetTyped( AnyValue *value, int type, const void *ptr )
{
	if ( type == g_any_flag ) {
		const AnyValue *src = (const AnyValue *)ptr;
		type = src->flag;
		ptr = HspVarAny_GetBlockPtr( src, NULL );
	}

	AnyValue_Clear( value );

	switch( type ) {
	case HSPVAR_FLAG_INT:
		value->flag = HSPVAR_FLAG_INT;
		value->value.i = *(const int *)ptr;
		break;
	case HSPVAR_FLAG_DOUBLE:
		value->flag = HSPVAR_FLAG_DOUBLE;
		value->value.d = *(const double *)ptr;
		break;
	case HSPVAR_FLAG_INT64:
		value->flag = HSPVAR_FLAG_INT64;
		value->value.l = *(const int64_t *)ptr;
		break;
	case HSPVAR_FLAG_STR:
		{
		int size = (int)strlen( (const char *)ptr ) + 1;
		value->flag = HSPVAR_FLAG_STR;
		value->value.s = any_alloc( size );
		memcpy( value->value.s, ptr, size );
		break;
		}
	case HSPVAR_FLAG_NONE:
		break;
	default:
		throw HSPVAR_ERROR_TYPEMISS;
	}
}

static void *HspVarAny_GetBlockPtr( const AnyValue *value, int *size )
{
	switch( value->flag ) {
	case HSPVAR_FLAG_INT:
		if ( size ) *size = sizeof(int);
		return (void *)&value->value.i;
	case HSPVAR_FLAG_DOUBLE:
		if ( size ) *size = sizeof(double);
		return (void *)&value->value.d;
	case HSPVAR_FLAG_INT64:
		if ( size ) *size = sizeof(int64_t);
		return (void *)&value->value.l;
	case HSPVAR_FLAG_STR:
		if ( size ) *size = (int)strlen( value->value.s ) + 1;
		return value->value.s;
	default:
		if ( size ) *size = sizeof(int);
		conv_i = 0;
		return &conv_i;
	}
}

static int HspVarAny_GetValueType( const AnyValue *value )
{
	if ( value->flag == HSPVAR_FLAG_NONE ) return HSPVAR_FLAG_INT;
	return value->flag;
}

static PDAT *HspVarAny_GetPtr( PVal *pval )
{
	return (PDAT *)(((AnyValue *)(pval->pt)) + pval->offset);
}

static void *HspVarAny_Cnv( const void *buffer, int flag )
{
	conv_any.flag = (short)flag;
	conv_any.reserved = 0;

	switch( flag ) {
	case HSPVAR_FLAG_INT:
		conv_any.value.i = *(const int *)buffer;
		return &conv_any;
	case HSPVAR_FLAG_DOUBLE:
		conv_any.value.d = *(const double *)buffer;
		return &conv_any;
	case HSPVAR_FLAG_INT64:
		conv_any.value.l = *(const int64_t *)buffer;
		return &conv_any;
	case HSPVAR_FLAG_STR:
		conv_any.value.s = (char *)buffer;
		return &conv_any;
	default:
		if ( flag == g_any_flag ) return (void *)buffer;
		throw HSPVAR_ERROR_TYPEMISS;
	}
}

static void *HspVarAny_CnvCustom( const void *buffer, int flag )
{
	const AnyValue *value = (const AnyValue *)buffer;
	void *ptr;

	if ( flag == g_any_flag ) return (void *)buffer;

	ptr = HspVarAny_GetBlockPtr( value, NULL );
	switch( flag ) {
	case HSPVAR_FLAG_STR:
		if ( value->flag == HSPVAR_FLAG_INT ) {
			sprintf( conv_s, "%d", *(int *)ptr );
			return conv_s;
		}
		if ( value->flag == HSPVAR_FLAG_DOUBLE ) {
			sprintf( conv_s, "%f", *(double *)ptr );
			return conv_s;
		}
		if ( value->flag == HSPVAR_FLAG_INT64 ) {
			sprintf( conv_s, "%lld", (long long)(*(int64_t *)ptr) );
			return conv_s;
		}
		if ( value->flag == HSPVAR_FLAG_STR ) return ptr;
		sprintf( conv_s, "%d", 0 );
		return conv_s;
	case HSPVAR_FLAG_INT:
		if ( value->flag == HSPVAR_FLAG_INT ) return ptr;
		if ( value->flag == HSPVAR_FLAG_DOUBLE ) {
			conv_i = (int)(*(double *)ptr);
			return &conv_i;
		}
		if ( value->flag == HSPVAR_FLAG_INT64 ) {
			conv_i = (int)(*(int64_t *)ptr);
			return &conv_i;
		}
		if ( value->flag == HSPVAR_FLAG_STR ) {
			conv_i = atoi( (char *)ptr );
			return &conv_i;
		}
		conv_i = 0;
		return &conv_i;
	case HSPVAR_FLAG_DOUBLE:
		if ( value->flag == HSPVAR_FLAG_DOUBLE ) return ptr;
		if ( value->flag == HSPVAR_FLAG_INT ) {
			conv_d = (double)(*(int *)ptr);
			return &conv_d;
		}
		if ( value->flag == HSPVAR_FLAG_INT64 ) {
			conv_d = (double)(*(int64_t *)ptr);
			return &conv_d;
		}
		if ( value->flag == HSPVAR_FLAG_STR ) {
			conv_d = atof( (char *)ptr );
			return &conv_d;
		}
		conv_d = 0.0;
		return &conv_d;
	case HSPVAR_FLAG_INT64:
		if ( value->flag == HSPVAR_FLAG_INT64 ) return ptr;
		if ( value->flag == HSPVAR_FLAG_INT ) {
			conv_l = (int64_t)(*(int *)ptr);
			return &conv_l;
		}
		if ( value->flag == HSPVAR_FLAG_DOUBLE ) {
			conv_l = (int64_t)(*(double *)ptr);
			return &conv_l;
		}
		if ( value->flag == HSPVAR_FLAG_STR ) {
			conv_l = (int64_t)strtoll( (char *)ptr, NULL, 10 );
			return &conv_l;
		}
		conv_l = 0;
		return &conv_l;
	default:
		throw HSPVAR_ERROR_TYPEMISS;
	}
}

static int HspVarAny_CountElems( const PVal *pval )
{
	int count = pval->len[1];
	if ( pval->len[2] ) count *= pval->len[2];
	if ( pval->len[3] ) count *= pval->len[3];
	if ( pval->len[4] ) count *= pval->len[4];
	return count;
}

static int HspVarAny_GetCountFromSize( const PVal *pval )
{
	return pval->size / (int)sizeof(AnyValue);
}

static void HspVarAny_Free( PVal *pval )
{
	if ( pval->mode == HSPVAR_MODE_MALLOC ) {
		AnyValue *value = (AnyValue *)pval->pt;
		int count = HspVarAny_GetCountFromSize( pval );
		for( int i = 0; i < count; i++ ) {
			AnyValue_Clear( &value[i] );
		}
		any_free( pval->pt );
	}
	pval->pt = NULL;
	pval->mode = HSPVAR_MODE_NONE;
}

static void HspVarAny_Alloc( PVal *pval, const PVal *pval2 )
{
	if ( pval->len[1] < 1 ) pval->len[1] = 1;

	int count = HspVarAny_CountElems( pval );
	int size = count * (int)sizeof(AnyValue);

	if ( pval == pval2 ) {
		int old_count = HspVarAny_GetCountFromSize( pval );
		AnyValue *value = (AnyValue *)pval->pt;
		if ( count < old_count ) {
			for( int i = count; i < old_count; i++ ) {
				AnyValue_Clear( &value[i] );
			}
		}
		if ( size > pval->size ) {
			value = (AnyValue *)any_expand( pval->pt, size );
			memset( value + old_count, 0, size - pval->size );
			pval->pt = (char *)value;
		}
		pval->size = size;
		pval->mode = HSPVAR_MODE_MALLOC;
		return;
	}

	AnyValue *value = (AnyValue *)any_alloc( size );
	memset( value, 0, size );
	pval->pt = (char *)value;
	pval->size = size;
	pval->mode = HSPVAR_MODE_MALLOC;

	if ( pval2 != NULL ) {
		AnyValue *src = (AnyValue *)pval2->pt;
		int old_count = pval2->size / (int)sizeof(AnyValue);
		if ( old_count > count ) old_count = count;
		for( int i = 0; i < old_count; i++ ) {
			AnyValue_SetTyped( &value[i], src[i].flag, HspVarAny_GetBlockPtr( &src[i], NULL ) );
		}
	}
}

static int HspVarAny_GetSize( const PDAT *pdat )
{
	int size;
	HspVarAny_GetBlockPtr( (const AnyValue *)pdat, &size );
	return size;
}

static int HspVarAny_GetUsing( const PDAT *pdat )
{
	return ((const AnyValue *)pdat)->flag != HSPVAR_FLAG_NONE;
}

static void HspVarAny_Set( PVal *pval, PDAT *pdat, const void *in )
{
	(void)pval;
	AnyValue_SetTyped( (AnyValue *)pdat, g_any_flag, in );
}

static void HspVarAny_ObjectWrite( PVal *pval, void *data, int type )
{
	void *any = HspVarAny_Cnv( data, type );
	HspVarAny_Set( pval, HspVarAny_GetPtr( pval ), any );
}

static void HspVarAny_Array( PVal *pval, int offset )
{
	if ( pval->arraycnt >= 5 ) throw HSPVAR_ERROR_ARRAYOVER;
	if ( pval->arraycnt == 0 ) {
		pval->arraymul = 1;
	} else {
		pval->arraymul *= pval->len[pval->arraycnt];
	}
	pval->arraycnt++;
	if ( offset < 0 ) throw HSPVAR_ERROR_ARRAYOVER;
	if ( offset >= pval->len[pval->arraycnt] ) throw HSPVAR_ERROR_ARRAYOVER;
	pval->offset += offset * pval->arraymul;
}

static int HspVarAny_GetElement( PVal *pval )
{
	PVal pval_temp;
	int chk, idx;
	pval->offset = 0;
	pval->arraycnt = 0;
	while( 1 ) {
		pval_temp.arraycnt = pval->arraycnt;
		pval_temp.offset = pval->offset;
		pval_temp.arraymul = pval->arraymul;
		chk = g_exinfo->HspFunc_prm_get();
		pval->arraycnt = pval_temp.arraycnt;
		pval->offset = pval_temp.offset;
		pval->arraymul = pval_temp.arraymul;
		if ( chk == PARAM_ENDSPLIT ) {
			if ( pval->arraycnt == 0 ) throw HSPERR_BAD_ARRAY_EXPRESSION;
			break;
		}
		if ( chk != PARAM_OK && chk != PARAM_SPLIT ) throw HSPERR_ARRAY_OVERFLOW;
		if ( (*g_exinfo->mpval)->flag != HSPVAR_FLAG_INT ) throw HSPERR_TYPE_MISMATCH;
		idx = *(int *)((*g_exinfo->mpval)->pt);
		HspVarAny_Array( pval, idx );
	}
	return chk;
}

static void HspVarAny_ArrayObject( PVal *pval )
{
	int chk = HspVarAny_GetElement( pval );
	if ( chk != PARAM_ENDSPLIT ) throw HSPERR_BAD_ARRAY_EXPRESSION;
}

static void *HspVarAny_ArrayObjectRead( PVal *pval, int *mptype )
{
	int chk = HspVarAny_GetElement( pval );
	if ( chk != PARAM_ENDSPLIT ) throw HSPERR_BAD_ARRAY_EXPRESSION;

	AnyValue *value = (AnyValue *)HspVarAny_GetPtr( pval );
	*mptype = HspVarAny_GetValueType( value );
	return HspVarAny_GetBlockPtr( value, NULL );
}

static void *HspVarAny_GetBlockSize( PVal *pval, PDAT *pdat, int *size )
{
	(void)pval;
	return HspVarAny_GetBlockPtr( (const AnyValue *)pdat, size );
}

static void HspVarAny_AllocBlock( PVal *pval, PDAT *pdat, int size )
{
	(void)pval;
	(void)pdat;
	(void)size;
}

static void HspVarAny_Init( HspVarProc *p )
{
	g_any_flag = p->flag;

	p->Set = HspVarAny_Set;
	p->Cnv = HspVarAny_Cnv;
	p->CnvCustom = HspVarAny_CnvCustom;
	p->GetPtr = HspVarAny_GetPtr;
	p->GetSize = HspVarAny_GetSize;
	p->GetUsing = HspVarAny_GetUsing;
	p->GetBlockSize = HspVarAny_GetBlockSize;
	p->AllocBlock = HspVarAny_AllocBlock;

	p->ArrayObject = HspVarAny_ArrayObject;
	p->ArrayObjectRead = HspVarAny_ArrayObjectRead;
	p->ObjectWrite = HspVarAny_ObjectWrite;

	p->Alloc = HspVarAny_Alloc;
	p->Free = HspVarAny_Free;

	p->vartype_name = (char *)HSPANY_TYPE_NAME;
	p->version = 0x001;
	p->support = HSPVAR_SUPPORT_STORAGE | HSPVAR_SUPPORT_FLEXARRAY | HSPVAR_SUPPORT_ARRAYOBJ | HSPVAR_SUPPORT_NOCONVERT | HSPVAR_SUPPORT_VARUSE;
	p->basesize = sizeof(AnyValue);
}

HSPANY_API hsp3cmdinit( HSP3TYPEINFO *info )
{
	g_exinfo = info->hspexinfo;
	g_any_flag = 0;
	g_exinfo->HspFunc_registvar( -1, HspVarAny_Init );
}
