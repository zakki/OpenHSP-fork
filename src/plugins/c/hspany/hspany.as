#ifndef __hspany__
#define __hspany__

#ifdef _hsplinux
#regcmd "hsp3cmdinit","./hspany.so",1
#else
#regcmd "hsp3cmdinit","hspany.dll",1
#endif

#endif

