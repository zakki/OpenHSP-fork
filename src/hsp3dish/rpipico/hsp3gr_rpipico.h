
//
//	hsp3gr_linux.cpp header
//
#ifndef __hsp3gr_linux_h
#define __hsp3gr_linux_h

#include "../../hsp3/hsp3struct.h"
#include "../hsp3dev.h"


void hsp3typeinit_cl_extcmd( HSP3TYPEINFO *info );
void hsp3typeinit_cl_extfunc( HSP3TYPEINFO *info );

void hsp3gr_dbg_gui( void );

HSP3DEVINFO *hsp3extcmd_getdevinfo( void );

#endif
