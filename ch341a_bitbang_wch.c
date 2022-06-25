/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 *
 * avrdude support for CH341
 * Copyright (C) 2016  Alexey Sadkov
 * Copyright (C) 2017, 2022  maxxproff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Interface to the CH341A programmer.
 *
 */
 
#if defined(WIN32NATIVE)
#include "ac_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "avrdude.h"
#include "libavrdude.h"

#include "ch341a_bitbang_wch.h"
#include "usbdevs.h"

#include    "CH341DLL.H" 

#define LIBUSB_ENDPOINT_OUT 0x02
#define LIBUSB_ENDPOINT_IN 0x82


/*
 * Private data for this programmer.
 */
struct pdata
{
  HANDLE      *usbhandle;
  int sckfreq_hz;
};

#define PDATA(pgm) ((struct pdata *)(pgm->cookie))

// ----------------------------------------------------------------------
/* Prototypes */
static void ch341a_bitbang_wch_setup(PROGRAMMER * pgm);
static void ch341a_bitbang_wch_teardown(PROGRAMMER * pgm);
static int  ch341a_bitbang_wch_open(PROGRAMMER * pgm, char * port);
static void ch341a_bitbang_wch_close(PROGRAMMER * pgm);
static int ch341a_bitbang_wch_initialize(PROGRAMMER * pgm, AVRPART * p);
static int ch341a_bitbang_wch_spi_cmd(PROGRAMMER * pgm, const unsigned char *cmd, unsigned char *res);
static int ch341a_bitbang_wch_spi(PROGRAMMER * pgm, const unsigned char *in, unsigned char *out, int size);
static int ch341a_bitbang_wch_spi_program_enable(PROGRAMMER * pgm, AVRPART * p);
static int ch341a_bitbang_wch_spi_chip_erase(PROGRAMMER * pgm, AVRPART * p);

BOOL	MegaSpiOutInData_wch(PROGRAMMER * pgm, ULONG OutLen, PVOID DataBuf, PVOID DataBufOut );
BOOL	MegaSpiEnable_wch( PROGRAMMER * pgm );

// dummy functions
static void ch341a_bitbang_wch_disable(PROGRAMMER * pgm);
static void ch341a_bitbang_wch_enable(PROGRAMMER * pgm);
static void ch341a_bitbang_wch_display(PROGRAMMER * pgm, const char * p);

#define		mUSBI0_MAX_NUMBER		16
#define		mMAX_BUFFER_LENGTH		0x1000

ULONG Ch341index;
BOOL        Ch341State;
HANDLE      Ch341Handle;

static int CH341USBTransfer_WCHPart(PROGRAMMER * pgm, int dir, unsigned char *buff, unsigned int bytestransferred) {
    int ret;
    if ( dir == LIBUSB_ENDPOINT_OUT )
        ret = CH341WriteData( Ch341index, buff, (PULONG)&bytestransferred );
    if ( dir == LIBUSB_ENDPOINT_IN )
        ret = CH341ReadData( Ch341index, buff, (PULONG)&bytestransferred );
    
    if ( ret == FALSE )
    {
        avrdude_message(MSG_INFO, "Error: CH341USBTransfer_WCHPart returns FALSE\n");
        return -1;
    }
    avrdude_message(MSG_DEBUG, "CH341USBTransfer_WCHPart: bytestransferred = %d\n", (int)bytestransferred);
    return bytestransferred;
}

static bool CH341USBTransfer_WCH(PROGRAMMER * pgm, int dir, unsigned char *buff, unsigned int size)
{
    int pos = 0, bytestransferred;
    while (size) {
        bytestransferred = CH341USBTransfer_WCHPart(pgm, dir, buff + pos, size);
        if (bytestransferred <= 0)
            return false;
        pos += bytestransferred;
        size -= bytestransferred;
    }
    return true;
}
/* The assumed map between UIO command bits, pins on CH341A chip and pins on SPI chip:
 * UIO  CH341A  SPI CH341A SPI name
 * 0    D0/15   CS/1    (CS0)
 * 1    D1/16   unused  (CS1)
 * 2    D2/17   unused  (CS2)
 * 3    D3/18   SCK/6   (DCK)
 * 4    D4/19   unused  (DOUT2)
 * 5    D5/20   SI/5    (DOUT)
 * - The UIO stream commands seem to only have 6 bits of output, and D6/D7 are the SPI inputs,
 *  mapped as follows:
 *  D6/21   unused  (DIN2)
 *  D7/22   SO/2    (DIN)
 */
bool CH341ChipSelect_bitbang_wch(PROGRAMMER * pgm, unsigned int cs, bool enable) {
    unsigned char res[4];
    unsigned char cmd[4];
    memset(cmd, 0, sizeof(cmd));
    memset(res, 0, sizeof(res));
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_ChipSelect()\n", progname);
    if (cs > 2) {
        avrdude_message(MSG_INFO, "%s: Error: invalid CS pin %d, 0~2 are available\n", progname, cs);
        return false;
    }
    cmd[0] = CH341A_CMD_UIO_STREAM;
    if (enable)
        cmd[1] = CH341A_CMD_UIO_STM_OUT | 0x00;
    else
        cmd[1] = CH341A_CMD_UIO_STM_OUT | 0x01;
    cmd[2] = CH341A_CMD_UIO_STM_END;
    return CH341USBTransfer_WCH(pgm, LIBUSB_ENDPOINT_OUT, cmd, 3);
}

static int ch341a_bitbang_wch_open(PROGRAMMER * pgm, char * port) {
    int pid, vid;
    ULONG i;
    pid = CH341A_PID;
    vid = CH341A_VID;

    for( i = 0; i != mCH341_MAX_NUMBER; i++)
            {
                Ch341index = i;
                Ch341Handle = CH341OpenDevice( Ch341index );
                avrdude_message(MSG_DEBUG, "CH341OpenDevice(%d) = %d \n", Ch341index, (int)Ch341Handle);
                if ( Ch341Handle  == INVALID_HANDLE_VALUE )
                {
					Ch341State = FALSE;
				}
				else
				{
					Ch341State = TRUE;
					break;
				}
			}

	if( Ch341State == FALSE )
        {
			avrdude_message(MSG_INFO, "%s: error: could not open USB device with vid=0x%04x pid=0x%04x", progname, vid, pid);
            avrdude_message(MSG_INFO, "\n");
			return -1;
		}
    else return 0;
}

static void ch341a_bitbang_wch_close(PROGRAMMER * pgm) {
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_close()\n", progname);
    CH341ChipSelect_bitbang_wch(pgm, 0,false);
    CH341CloseDevice(Ch341index);
}

static int ch341a_bitbang_wch_initialize(PROGRAMMER * pgm, AVRPART * p) {
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_initialize()\n", progname);
    CH341SetStream( Ch341index, 0x81 );
    MegaSpiEnable_wch( pgm );
    CH341ChipSelect_bitbang_wch(pgm, 0,false);
    usleep(20 * 1000);
    CH341ChipSelect_bitbang_wch(pgm, 0,true);
    return pgm->program_enable(pgm, p);
}

static int ch341a_bitbang_wch_spi_cmd(PROGRAMMER * pgm, const unsigned char *cmd, unsigned char *res) {
    return pgm->spi(pgm, cmd, res, 4);
}

static int ch341a_bitbang_wch_spi(PROGRAMMER * pgm, const unsigned char *in, unsigned char *out, int size) {

    if (!size)
        return 0;

    if (size > CH341A_PACKET_LENGTH)
        size = CH341A_PACKET_LENGTH;

    if (!MegaSpiOutInData_wch(pgm, (ULONG)size, (PVOID)in, out))
    {
        fprintf(stderr, "Error: failed to transfer data to or from CH341\n");
        return -1;
    }
    return size;
}

static int ch341a_bitbang_wch_spi_program_enable(PROGRAMMER * pgm, AVRPART * p) {
    unsigned char res[4];
    unsigned char cmd[4];
    memset(cmd, 0, sizeof(cmd));
    memset(res, 0, sizeof(res));

    cmd[0] = 0;
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_program_enable() %s\n",
                    progname, p->op[AVR_OP_PGM_ENABLE]);

    if (p->op[AVR_OP_PGM_ENABLE] == NULL) {
        avrdude_message(MSG_INFO, "program enable instruction not defined for part \"%s\"\n", p->desc);
        return -1;
    }
    avr_set_bits(p->op[AVR_OP_PGM_ENABLE], cmd);
    avrdude_message(MSG_DEBUG, "program_enable(): sending command. Cmd = %x %x %x %x \n", (int)cmd[0], (int)cmd[1], (int)cmd[2], (int)cmd[3]);
    pgm->cmd(pgm, cmd, res);

    avrdude_message(MSG_DEBUG, "program_enable(): receiving. Resp = %x %x %x %x \n", (int)res[0], (int)res[1], (int)res[2], (int)res[3]);
    // check for sync character
    if (res[2] != cmd[1])
        return -2;
    return 0;
}
static int  ch341a_bitbang_wch_spi_chip_erase(struct programmer_t * pgm, AVRPART * p) {
    unsigned char cmd[4];
    unsigned char res[4];
    if (p->op[AVR_OP_CHIP_ERASE] == NULL) {
        avrdude_message(MSG_INFO, "chip erase instruction not defined for part \"%s\"\n", p->desc);
        return -1;
    }
    memset(cmd, 0, sizeof(cmd));
    avr_set_bits(p->op[AVR_OP_CHIP_ERASE], cmd);
    pgm->cmd(pgm, cmd, res);
    usleep(p->chip_erase_delay);
    pgm->initialize(pgm, p);
    return 0;
}


void ch341a_bitbang_wch_initpgm(PROGRAMMER * pgm) {
    strcpy(pgm->type, "ch341a_bitbang_wch");
    /*
    * mandatory functions
    */
    pgm->initialize     = ch341a_bitbang_wch_initialize;
    pgm->display        = ch341a_bitbang_wch_display;
    pgm->enable         = ch341a_bitbang_wch_enable;
    pgm->disable        = ch341a_bitbang_wch_disable;
    pgm->program_enable = ch341a_bitbang_wch_spi_program_enable;
    pgm->chip_erase     = ch341a_bitbang_wch_spi_chip_erase;
    pgm->cmd            = ch341a_bitbang_wch_spi_cmd;
    pgm->spi            = ch341a_bitbang_wch_spi;
    pgm->open           = ch341a_bitbang_wch_open;
    pgm->close          = ch341a_bitbang_wch_close;
    pgm->read_byte      = avr_read_byte_default;
    pgm->write_byte     = avr_write_byte_default;

    /*
    * optional functions
    */

    //pgm->paged_write    = ch341a_bitbang_wch_spi_paged_write;
    //pgm->paged_load     = ch341a_bitbang_wch_spi_paged_load;
    pgm->setup          = ch341a_bitbang_wch_setup;
    pgm->teardown       = ch341a_bitbang_wch_teardown;

}

/* Interface - management */
static void ch341a_bitbang_wch_setup(PROGRAMMER * pgm) {
    if ((pgm->cookie = malloc(sizeof(struct pdata))) == 0) {
        avrdude_message(MSG_INFO, "%s: ch341a_bitbang_wch_setup(): Out of memory allocating private data\n",
                        progname);
        exit(1);
    }
    memset(pgm->cookie, 0, sizeof(struct pdata));
}

static void ch341a_bitbang_wch_teardown(PROGRAMMER * pgm) {
    free(pgm->cookie);
}
/* Dummy functions */
static void ch341a_bitbang_wch_disable(PROGRAMMER * pgm) {
    /* Do nothing. */
    return;
}

static void ch341a_bitbang_wch_enable(PROGRAMMER * pgm) {
    /* Do nothing. */
    return;
}

static void ch341a_bitbang_wch_display(PROGRAMMER * pgm, const char * p) {
    return;
}


const char ch341a_bitbang_wch_desc[] = "Driver for \"ch341a_bitbang_wch\"-type programmers";

//#define     DELAY_US        4
#define     DELAY_US        10  //Edit DELAY_US for tune bitbang speed. 

BOOL    MegaSpiOutInData_wch(
	PROGRAMMER * pgm,
    ULONG   OutLen,         // Output data length
    PVOID   DataBufIn,
    PVOID   DataBufOut )        // The output data buffer SPI data returned
{
PUCHAR p;
ULONG i, j, k;
ULONG zzz;
UCHAR c, tem;
UCHAR  mBuffer[ 512 ];
        
        for( zzz = 0; zzz != 512; zzz++ )
        {
			mBuffer[ zzz ] = 0xff;
		}
        
    // 202334066 on? + 6.95us - 5.85us
        p = (PUCHAR)DataBufIn;
        k = 0;
        
        for( i = 0; i != OutLen; i++ )
        
        {
        
            tem = *p;
            mBuffer[ k++ ] = CH341A_CMD_UIO_STREAM;            // Command code
            for( j = 0; j != 4; j++ )
            {
                c = tem & 0x80 ? 0x20 : 0;                             // D5 Data output
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | c;           // D3=0, I/O_CLOCK=LOW 
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_US | DELAY_US;       
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | c | 0x08;    // D3=1, I/O_CLOCK=HIGH
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_IN;                // input from D7
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_US | DELAY_US;
                tem <<= 1;
            }
            mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | c | 0x00;        // D3=0, I/O_CLOCK=LOW
            mBuffer[ k++ ] = CH341A_CMD_UIO_STM_END;                   // The current command pack ends prematurely
            k += CH341A_PACKET_LENGTH - 1;
            k &= ~ ( CH341A_PACKET_LENGTH - 1 );

            mBuffer[ k++ ] = CH341A_CMD_UIO_STREAM;            // Command code
            for( j = 0; j != 4; j++ )
            {
                c = tem & 0x80 ? 0x20 : 0;                              // D5 Data output
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | c;           // D3=0, I/O_CLOCK=LOW        
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_US | DELAY_US; 
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | c | 0x08;    // D3=1, I/O_CLOCK=HIGH
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_IN;                // input from D7
                mBuffer[ k++ ] = CH341A_CMD_UIO_STM_US | DELAY_US;
                tem <<= 1;
            }
            mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | c | 0x00;        // D3=0, I/O_CLOCK=LOW 
            mBuffer[ k++ ] = CH341A_CMD_UIO_STM_END;                   // The current command pack ends prematurely
            k += CH341A_PACKET_LENGTH - 1;
            k &= ~ ( CH341A_PACKET_LENGTH - 1 );
            p++;
        }

        i = CH341WriteRead( Ch341index, k, mBuffer, 4, OutLen*2, &j, mBuffer );
        
        if( i == FALSE )
            return( FALSE );

        if( j != OutLen * 8 )
            return( FALSE );

        k = 0;
        for ( i = 0; i != OutLen; i++ )
        {
            c = 0;
            for ( j = 0; j < 8; j ++ )
            {
                c <<= 1;
                if ( mBuffer[ k++ ] & 0x80 )        // input 8 bit
                    c ++;
            }

            *(PUCHAR)DataBufOut = c;
            DataBufOut++;
        } 
        return TRUE;
}


BOOL   MegaSpiEnable_wch( PROGRAMMER * pgm )
{
UCHAR   mBuffer[ CH341A_PACKET_LENGTH ];
ULONG   i;

    i = 0;
    mBuffer[ i++ ] = CH341A_CMD_UIO_STREAM;            // ÃüÁîÂë
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_OUT | 0x00;    // default status: all 0
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_DIR | 0x29;    // D0 & D3 & D5 output, other input
    //mBuffer[ i++ ] = CH341A_CMD_UIO_STM_DIR | 0x2B;    //TODO: D0 & D1 & D3 & D5 output, other input
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_US | 32;       // ÑÓ?32Î¢Ãë
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_END;           // ??ÃüÁî?üÌáÇ?½áÊø
    if (!CH341USBTransfer_WCH(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 5))
    {
        fprintf(stderr, "Error: MegaSpiEnable_wch returns FALSE\n");
        return FALSE;
    }
    return TRUE;
}

#endif
