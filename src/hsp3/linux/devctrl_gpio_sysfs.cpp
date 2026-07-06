/*--------------------------------------------------------
	Optional GPIO provider using /sys/class/gpio
  --------------------------------------------------------*/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GPIO_TYPE_NONE 0
#define GPIO_TYPE_OUT 1
#define GPIO_TYPE_IN 2
#define GPIO_MAX 32

#define GPIO_CLASS "/sys/class/gpio/"

static int gpio_type[GPIO_MAX];
static int gpio_value[GPIO_MAX];

static int echo_file( const char *name, const char *value )
{
	int fd = open( name, O_WRONLY );
	if (fd < 0) {
		return -1;
	}
	write( fd, value, strlen(value)+1 );
	close(fd);
	return 0;
}

static int echo_file2( const char *name, int value )
{
	char vstr[64];
	sprintf( vstr, "%d", value );
	return echo_file( name, vstr );
}

static int gpio_delport( int port )
{
	if ((port<0)||(port>=GPIO_MAX)) return -1;

	if ( gpio_type[port]==GPIO_TYPE_NONE ) return 0;
	//echo_file2( GPIO_CLASS "unexport", port );
	//usleep(100000);		//0.1秒待つ(念のため)
	gpio_type[port]=GPIO_TYPE_NONE;
	return 0;
}

static int gpio_setport( int port, int type )
{
	if ((port<0)||(port>=GPIO_MAX)) return -1;

	if ( gpio_type[port]==GPIO_TYPE_NONE ) {
		echo_file2( GPIO_CLASS "export", port );
		usleep(100000);		//0.1秒待つ(念のため)
	}

	if ( gpio_type[port] == type ) return 0;

	int res = 0;
	char vstr[256];
	sprintf( vstr, GPIO_CLASS "gpio%d/direction", port );

	switch( type ) {
	case GPIO_TYPE_OUT:
		res = echo_file( vstr, "out" );
		break;
	case GPIO_TYPE_IN:
		res = echo_file( vstr, "in" );
		break;
	}

	if ( res ) {
		gpio_type[port] = GPIO_TYPE_NONE;
		return res;
	}

	gpio_type[port] = type;
	gpio_value[port] = 0;
	return 0;
}

extern "C" int hsp3_gpio_init( void )
{
	for(int i=0;i<GPIO_MAX;i++) {
		gpio_type[i] = GPIO_TYPE_NONE;
	}
	return 0;
}

extern "C" void hsp3_gpio_bye( void )
{
	for(int i=0;i<GPIO_MAX;i++) {
		gpio_delport(i);
	}
}

extern "C" int hsp3_gpio_out( int port, int value )
{
	if ((port<0)||(port>=GPIO_MAX)) return -1;
	if ( gpio_type[port]!=GPIO_TYPE_OUT ) {
		int res = gpio_setport( port, GPIO_TYPE_OUT );
		if ( res ) return res;
	}

	char vstr[256];
	sprintf( vstr, GPIO_CLASS "gpio%d/value", port );
	if ( value == 0 ) {
		gpio_value[port] = 0;
		return echo_file( vstr, "0" );
	}
	gpio_value[port] = 1;
	return echo_file( vstr, "1" );
}

extern "C" int hsp3_gpio_in( int port, int *value )
{
	if ((port<0)||(port>=GPIO_MAX)) return -1;
	if ( gpio_type[port]!=GPIO_TYPE_IN ) {
		int res = gpio_setport( port, GPIO_TYPE_IN );
		if ( res ) return res;
	}

	int fd,rd,i;
	char vstr[256];
	char ev[256];
	char a1;
	sprintf( vstr, GPIO_CLASS "gpio%d/value", port );

	fd = open( vstr, O_RDONLY | O_NONBLOCK );
	if (fd < 0) {
		return -1;
	}
	rd = read(fd,ev,255);
	if(rd > 0) {
		i = 0;
		while(1) {
			if ( i >= rd ) break;
			a1 = ev[i++];
			if ( a1 == '0' ) gpio_value[port] = 0;
			if ( a1 == '1' ) gpio_value[port] = 1;
		}
	}
	close(fd);

	*value = gpio_value[port];
	return 0;
}

extern "C" int hsp3_gpio_dir( int port, int *value )
{
	*value = 0;
	return 0;
}
