//
//		Draw lib (RaspberryPi Pico)
//

#include <stdio.h>

#include <pico/time.h>
#include "../../hsp3/hsp3config.h"

#include "../supio.h"
#include "../sysreq.h"
#include "../hgio.h"
#include "../hsp3ext.h"

int hgio_gettick( void )
{
    // 経過時間の計測
	return to_ms_since_boot(get_absolute_time());
}
