//
//	token.cpp structures
//
#pragma once

// token type
#define TK_NONE 0
#define TK_OBJ 1
#define TK_STRING 2
#define TK_DNUM 3
#define TK_NUM 4
#define TK_INT64 5
#define TK_CODE 6
#define TK_LABEL 7
#define TK_VOID 0x1000
#define TK_SEPARATE 0x1001
#define TK_EOL 0x1002
#define TK_EOF 0x1003
#define TK_ERROR ( -1 )
#define TK_CALCERROR ( -2 )
#define TK_CALCSTOP ( -3 )

#define CMPMODE_PPOUT 1
#define CMPMODE_OPTCODE 2
#define CMPMODE_CASE 4
#define CMPMODE_OPTINFO 8
#define CMPMODE_PUTVARS 16
#define CMPMODE_VARINIT 32
#define CMPMODE_OPTPRM 64
#define CMPMODE_SKIPJPSPC 128
#define CMPMODE_UTF8OUT 256

// module related define
#define OBJNAME_MAX 60

#define COMP_MODE_DEBUG 1
#define COMP_MODE_DEBUGWIN 2
#define COMP_MODE_UTF8 4
#define COMP_MODE_STRMAP 8
#define COMP_MODE_LABOUT 16
#define COMP_MODE_SKIPERROR 64
