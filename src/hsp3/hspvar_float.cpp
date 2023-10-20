
//
//	HSPVAR core module
//	onion software/onitama 2003/4
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hspvar_core.h"
#include "hsp3debug.h"
#include "strbuf.h"

/*------------------------------------------------------------*/
/*
		HSPVAR core interface (float)
*/
/*------------------------------------------------------------*/

#define GetPtr(pval) ((float *)pval)

static float conv;
static short *aftertype;

// Core
static PDAT *HspVarFloat_GetPtr( PVal *pval )
{
	return (PDAT *)(( (float *)(pval->pt))+pval->offset);
}

static void *HspVarFloat_Cnv( const void *buffer, int flag )
{
	//		リクエストされた型 -> 自分の型への変換を行なう
	//		(組み込み型にのみ対応でOK)
	//		(参照元のデータを破壊しないこと)
	//
	switch( flag ) {
	case HSPVAR_FLAG_STR:
		conv = (float)atof( (char *)buffer );
		return &conv;
	case HSPVAR_FLAG_INT:
		conv = (float)( *(int *)buffer );
		return &conv;
	case HSPVAR_FLAG_DOUBLE:
		conv = (float)( *(double *)buffer );
		break;
	case HSPVAR_FLAG_INT64:
		conv = (float)( *(int64_t *)buffer );
		return &conv;
	case HSPVAR_FLAG_FLOAT:
		break;
	default:
		throw HSPVAR_ERROR_TYPEMISS;
	}
	return (void *)buffer;
}

/*
static void *HspVarFloat_CnvCustom( const void *buffer, int flag )
{
	//		(カスタムタイプのみ)
	//		自分の型 -> リクエストされた型 への変換を行なう
	//		(組み込み型に対応させる)
	//		(参照元のデータを破壊しないこと)
	//
	return buffer;
}
*/

static int GetVarSize( PVal *pval )
{
	//		PVALポインタの変数が必要とするサイズを取得する
	//		(sizeフィールドに設定される)
	//
	return HspVarCoreCountElems(pval) * sizeof(float);
}


static void HspVarFloat_Free( PVal *pval )
{
	//		PVALポインタの変数メモリを解放する
	//
	if ( pval->mode == HSPVAR_MODE_MALLOC ) { sbFree( pval->pt ); }
	pval->pt = NULL;
	pval->mode = HSPVAR_MODE_NONE;
}


static void HspVarFloat_Alloc( PVal *pval, const PVal *pval2 )
{
	HspVarCoreAllocPODArray(pval, pval2, sizeof(float));
}

/*
static void *HspVarFloat_ArrayObject( PVal *pval, int *mptype )
{
	//		配列要素の指定 (文字列/連想配列用)
	//
	throw HSPERR_UNSUPPORTED_FUNCTION;
	return NULL;
}
*/

// Size
static int HspVarFloat_GetSize( const PDAT *pval )
{
	return sizeof(float);
}

// Set
static void HspVarFloat_Set( PVal *pval, PDAT *pdat, const void *in )
{
	//*GetPtr(pdat) = *((float *)(in));
	memcpy(pdat, in, sizeof(float));
}

// Add
static void HspVarFloat_AddI( PDAT *pval, const void *val )
{
	*GetPtr(pval) += *((float *)(val));
	*aftertype = HSPVAR_FLAG_FLOAT;
}

// Sub
static void HspVarFloat_SubI( PDAT *pval, const void *val )
{
	*GetPtr(pval) -= *((float *)(val));
	*aftertype = HSPVAR_FLAG_FLOAT;
}

// Mul
static void HspVarFloat_MulI( PDAT *pval, const void *val )
{
	*GetPtr(pval) *= *((float *)(val));
	*aftertype = HSPVAR_FLAG_FLOAT;
}

// Div
static void HspVarFloat_DivI( PDAT *pval, const void *val )
{
	float p = *((float *)(val));
	if ( p == 0.0 ) throw( HSPVAR_ERROR_DIVZERO );
	*GetPtr(pval) /= p;
	*aftertype = HSPVAR_FLAG_FLOAT;
}

// Mod
static void HspVarFloat_ModI( PDAT *pval, const void *val )
{
	float p = *((float *)(val));
	float dval;
	if ( p == 0.0 ) throw( HSPVAR_ERROR_DIVZERO );
	dval = *GetPtr(pval);
	*GetPtr(pval) = fmod( dval, p );
	*aftertype = HSPVAR_FLAG_FLOAT;
}


// Eq
static void HspVarFloat_EqI( PDAT *pval, const void *val )
{
	*((int *)pval) = ( *GetPtr(pval) == *((float *)(val)) );
	*aftertype = HSPVAR_FLAG_INT;
}

// Ne
static void HspVarFloat_NeI( PDAT *pval, const void *val )
{
	*((int *)pval) = ( *GetPtr(pval) != *((float *)(val)) );
	*aftertype = HSPVAR_FLAG_INT;
}

// Gt
static void HspVarFloat_GtI( PDAT *pval, const void *val )
{
	*((int *)pval) = ( *GetPtr(pval) > *((float *)(val)) );
	*aftertype = HSPVAR_FLAG_INT;
}

// Lt
static void HspVarFloat_LtI( PDAT *pval, const void *val )
{
	*((int *)pval) = ( *GetPtr(pval) < *((float *)(val)) );
	*aftertype = HSPVAR_FLAG_INT;
}

// GtEq
static void HspVarFloat_GtEqI( PDAT *pval, const void *val )
{
	*((int *)pval) = ( *GetPtr(pval) >= *((float *)(val)) );
	*aftertype = HSPVAR_FLAG_INT;
}

// LtEq
static void HspVarFloat_LtEqI( PDAT *pval, const void *val )
{
	*((int *)pval) = ( *GetPtr(pval) <= *((float *)(val)) );
	*aftertype = HSPVAR_FLAG_INT;
}

/*
// INVALID
static void HspVarFloat_Invalid( PDAT *pval, const void *val )
{
	throw( HSPVAR_ERROR_INVALID );
}
*/

static void *GetBlockSize( PVal *pval, PDAT *pdat, int *size )
{
	*size = pval->size - ( ((char *)pdat) - pval->pt );
	return (pdat);
}

static void AllocBlock( PVal *pval, PDAT *pdat, int size )
{
}


/*------------------------------------------------------------*/

void HspVarFloat_Init( HspVarProc *p )
{
	aftertype = &p->aftertype;

	p->Set = HspVarFloat_Set;
	p->Cnv = HspVarFloat_Cnv;
	p->GetPtr = HspVarFloat_GetPtr;
//	p->CnvCustom = HspVarFloat_CnvCustom;
	p->GetSize = HspVarFloat_GetSize;
	p->GetBlockSize = GetBlockSize;
	p->AllocBlock = AllocBlock;

//	p->ArrayObject = HspVarFloat_ArrayObject;
	p->Alloc = HspVarFloat_Alloc;
	p->Free = HspVarFloat_Free;

	p->AddI = HspVarFloat_AddI;
	p->SubI = HspVarFloat_SubI;
	p->MulI = HspVarFloat_MulI;
	p->DivI = HspVarFloat_DivI;
	p->ModI = HspVarFloat_ModI;

//	p->AndI = HspVarFloat_Invalid;
//	p->OrI  = HspVarFloat_Invalid;
//	p->XorI = HspVarFloat_Invalid;

	p->EqI = HspVarFloat_EqI;
	p->NeI = HspVarFloat_NeI;
	p->GtI = HspVarFloat_GtI;
	p->LtI = HspVarFloat_LtI;
	p->GtEqI = HspVarFloat_GtEqI;
	p->LtEqI = HspVarFloat_LtEqI;

//	p->RrI = HspVarFloat_Invalid;
//	p->LrI = HspVarFloat_Invalid;

	p->vartype_name = "float";				// タイプ名
	p->version = 0x001;					// 型タイプランタイムバージョン(0x100 = 1.0)
	p->support = HSPVAR_SUPPORT_STORAGE|HSPVAR_SUPPORT_FLEXARRAY;
										// サポート状況フラグ(HSPVAR_SUPPORT_*)
	p->basesize = sizeof(float);		// １つのデータが使用するサイズ(byte) / 可変長の時は-1
}

/*------------------------------------------------------------*/

