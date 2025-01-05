//
//		Draw lib (RaspberryPi Pico)
//

#include <stdio.h>

#include <pico/time.h>
#include <ff.h>

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

//
//		FILE I/O Service
//

// File system
static FATFS filesystem;

/*
 * `fatfs_flash_driver.c:disk_initialize()` is called to test the file
 * system and initialize it if necessary.
 */
void test_and_init_filesystem(void) {
    f_mount(&filesystem, "/", 1);
    f_unmount("/");
}


int hgio_file_exist( char *fname )
{
    int size = -1;
    FRESULT res = f_mount(&filesystem, "/", 1);
    if (res != FR_OK) {
        printf("f_mount fail rc=%d\n", res);
        return size;
    }

    bool result = false;
    FIL fp;
    res = f_open(&fp, fname, FA_READ);
    if (res == FR_OK) {
        size = f_size(&fp);
        f_close(&fp);
    }
    printf("file size %s %d\n", fname, size);
    f_unmount("/");
    return size;
}


int hgio_file_read( char *fname, void *ptr, int size, int offset )
{
	int readsize = -1;
    printf("read %s\n", fname);

    FRESULT res = f_mount(&filesystem, "/", 1);
    if (res != FR_OK) {
        printf("f_mount fail rc=%d\n", res);
        return readsize;
    }

    bool result = false;
    FIL fp;
    res = f_open(&fp, fname, FA_READ);
    if (res == FR_OK) {
        uint8_t* buffer = (uint8_t*)ptr;
        UINT length;
        //memset(buffer, 0, sizeof(buffer));
        f_read(&fp, buffer, size, &length);
        f_close(&fp);
        readsize = length;
    } else {
        printf("can't open %s: %d\n", fname, res);
        readsize = -1;
    }
    f_unmount("/");
    return readsize;
}
