/*--------------------------------------------------------
	Optional GPIO provider using libgpiod
  --------------------------------------------------------*/

#include <gpiod.h>

#define GPIO_MAX 32

static const char *gpiod_appname = "hsp3dish";

static gpiod_chip *gchip;
static gpiod_line *gline;
static int gpio_type[GPIO_MAX];

static int get_gpio_line( int port )
{
	if ( port < 0 || port >= GPIO_MAX ) return -1;
	gline = gpiod_chip_get_line(gchip, port);
	if ( gline == NULL ) return -1;
	return 0;
}

extern "C" int hsp3_gpio_init( void )
{
	gchip = gpiod_chip_open_lookup("");
	if ( gchip == NULL ) return -1;

	for (int i = 0; i < GPIO_MAX; i++) {
		gpio_type[i] = 0;
	}
	return 0;
}

extern "C" void hsp3_gpio_bye( void )
{
	if ( gchip != NULL ) {
		gpiod_chip_close(gchip);
		gchip = NULL;
	}
}

extern "C" int hsp3_gpio_out( int port, int value )
{
	int i = get_gpio_line(port);
	if ( i < 0 ) return -1;

	if ( gpio_type[port] != GPIOD_LINE_DIRECTION_OUTPUT ) {
		if (gpiod_line_request_output(gline, gpiod_appname, value) != 0) {
			return -1;
		}
		gpio_type[port]=GPIOD_LINE_DIRECTION_OUTPUT;
		return 0;
	}

	i = gpiod_line_set_value( gline, value );
	if  (i < 0 ) return -1;
	return 0;
}

extern "C" int hsp3_gpio_in( int port, int *value )
{
	int i = get_gpio_line(port);
	if ( i < 0 ) return -2;

	if ( gpio_type[port] != GPIOD_LINE_DIRECTION_INPUT ) {
		if (gpiod_line_request_input(gline, gpiod_appname) != 0) {
			return -3;
		}
		gpio_type[port]=GPIOD_LINE_DIRECTION_INPUT;
	}

	i = gpiod_line_get_value(gline);
	if  (i < 0 ) return -4;
	*value = i;
	return 0;
}

extern "C" int hsp3_gpio_dir( int port, int *value )
{
	int i = get_gpio_line(port);
	if ( i < 0 ) return -1;

	i = gpiod_line_direction(gline);
	*value = i;
	return 0;
}
