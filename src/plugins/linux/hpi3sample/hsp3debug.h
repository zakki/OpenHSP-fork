
//
//	hsp3debug.cpp header
//
#ifndef __hsp3debug_h
#define __hsp3debug_h

// Error codes
typedef enum {

HSPERR_NONE = 0,				// Script terminated
HSPERR_UNKNOWN_CODE,
HSPERR_SYNTAX,
HSPERR_ILLEGAL_FUNCTION,
HSPERR_WRONG_EXPRESSION,
HSPERR_NO_DEFAULT,
HSPERR_TYPE_MISMATCH,
HSPERR_ARRAY_OVERFLOW,
HSPERR_LABEL_REQUIRED,
HSPERR_TOO_MANY_NEST,
HSPERR_RETURN_WITHOUT_GOSUB,
HSPERR_LOOP_WITHOUT_REPEAT,
HSPERR_FILE_IO,
HSPERR_PICTURE_MISSING,
HSPERR_EXTERNAL_EXECUTE,
HSPERR_PRIORITY,
HSPERR_TOO_MANY_PARAMETERS,
HSPERR_TEMP_BUFFER_OVERFLOW,
HSPERR_WRONG_NAME,
HSPERR_DIVIDED_BY_ZERO,
HSPERR_BUFFER_OVERFLOW,
HSPERR_UNSUPPORTED_FUNCTION,
HSPERR_EXPRESSION_COMPLEX,
HSPERR_VARIABLE_REQUIRED,
HSPERR_INTEGER_REQUIRED,
HSPERR_BAD_ARRAY_EXPRESSION,
HSPERR_OUT_OF_MEMORY,
HSPERR_TYPE_INITALIZATION_FAILED,
HSPERR_NO_FUNCTION_PARAMETERS,
HSPERR_STACK_OVERFLOW,
HSPERR_INVALID_PARAMETER,
HSPERR_INVALID_ARRAYSTORE,
HSPERR_INVALID_FUNCPARAM,
HSPERR_WINDOW_OBJECT_FULL,
HSPERR_INVALID_ARRAY,
HSPERR_STRUCT_REQUIRED,
HSPERR_INVALID_STRUCT_SOURCE,
HSPERR_INVALID_TYPE,
HSPERR_DLL_ERROR,
HSPERR_COMDLL_ERROR,
HSPERR_NORETVAL,
HSPERR_FUNCTION_SYNTAX,
HSPERR_INVALID_CALLBACK,
HSPERR_FIXED_VARTYPE,
HSPERR_FIXED_VARVALUE,

HSPERR_INTJUMP,					// Interrupt jump
HSPERR_EXITRUN,					// External file execution
HSPERR_MAX

} HSPERROR;

char *hspd_geterror( HSPERROR error );


// Debug Info ID
enum
{
DEBUGINFO_GENERAL = 0,
DEBUGINFO_VARNAME,
DEBUGINFO_INTINFO,
DEBUGINFO_GRINFO,
DEBUGINFO_MMINFO,
DEBUGINFO_MAX
};

// Debug Flag ID
enum
{
HSPDEBUG_NONE = 0,
HSPDEBUG_RUN,
HSPDEBUG_STOP,
HSPDEBUG_STEPIN,
HSPDEBUG_STEPOVER,
HSPDEBUG_MAX
};

typedef struct HSP3DEBUG
{
	//	[in/out] tranfer value
	//	(for communication with the host system)
	//
	int	flag;				// Flag ID
	int	line;				// Line number info
	char *fname;			// File name info
	void *dbgwin;			// Debug window handle
	char *dbgval;			// Debug info buffer

	//	[in] system value
	//	(set after initialization)
	//
	struct HSPCTX 	*hspctx;
	//
	char *	(* get_value) (int);			// Debug info callback
	char *	(* get_varinf) (char *,int);	// Variable info callback
	void	(* dbg_close) (char *);			// Finish debug info retrieval
	void	(* dbg_curinf)( void );			// Get current line and file name
	int		(* dbg_set) (int);				// Set debug mode
	char *  (* dbg_callstack) ( void );		// Get call stack

} HSP3DEBUG;

// Debug Module

#define HSP3DEBUG_MODULE "hsp3debug"
#define HSP3DEBUG_INIT "debugini"
#define HSP3DEBUG_NOTICE "debug_notice"


#endif
