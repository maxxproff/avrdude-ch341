/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 *
 * avrdude support for CH341
 * Copyright (C) 2016  Alexey Sadkov
 * Copyright (C) 2017  maxxproff
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

//HANDLE      Ch341Handle;

#define LIBUSB_ENDPOINT_OUT 0x02
#define LIBUSB_ENDPOINT_IN 0x82

/*#if defined(HAVE_LIBUSB_1_0)

#ifdef HAVE_LIBUSB_1_0
# define USE_LIBUSB_1_0
# if defined(HAVE_LIBUSB_1_0_LIBUSB_H)
#  include <libusb-1.0/libusb.h>
# else
#  include <libusb.h>
# endif
#endif

#ifdef USE_LIBUSB_1_0

static libusb_context *ctx = NULL;

static int libusb_to_errno(int result)
{
    switch (result) {
    case LIBUSB_SUCCESS:
        return 0;
    case LIBUSB_ERROR_IO:
        return EIO;
    case LIBUSB_ERROR_INVALID_PARAM:
        return EINVAL;
    case LIBUSB_ERROR_ACCESS:
        return EACCES;
    case LIBUSB_ERROR_NO_DEVICE:
        return ENXIO;
    case LIBUSB_ERROR_NOT_FOUND:
        return ENOENT;
    case LIBUSB_ERROR_BUSY:
        return EBUSY;
#ifdef ETIMEDOUT
    case LIBUSB_ERROR_TIMEOUT:
        return ETIMEDOUT;
#endif
#ifdef EOVERFLOW
    case LIBUSB_ERROR_OVERFLOW:
        return EOVERFLOW;
#endif
    case LIBUSB_ERROR_PIPE:
        return EPIPE;
    case LIBUSB_ERROR_INTERRUPTED:
        return EINTR;
    case LIBUSB_ERROR_NO_MEM:
        return ENOMEM;
    case LIBUSB_ERROR_NOT_SUPPORTED:
        return ENOSYS;
    default:
        return ERANGE;
    }
}

#endif
*/


/*
 * Private data for this programmer.
 */
struct pdata
{
  //libusb_device_handle *usbhandle;
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
//static int ch341a_bitbang_wch_spi_transfer(PROGRAMMER * pgm, const unsigned char *cmd, unsigned char *res);
// dummy functions
static void ch341a_bitbang_wch_disable(PROGRAMMER * pgm);
static void ch341a_bitbang_wch_enable(PROGRAMMER * pgm);
static void ch341a_bitbang_wch_display(PROGRAMMER * pgm, const char * p);

BOOL	MegaSpiOutInData_wch(PROGRAMMER * pgm, ULONG OutLen, PVOID DataBuf, PVOID DataBufOut );
//BOOL	CH341WriteRead(PROGRAMMER * pgm, ULONG iWriteLength, PVOID iWriteBuffer, ULONG iReadStep, ULONG iReadTimes, PULONG oReadLength, PVOID oReadBuffer);
BOOL	MegaSpiEnable_wch( PROGRAMMER * pgm );
//BOOL     SpiResetMega( PROGRAMMER * pgm );

//BOOL CH341SetStream_bitbang( PROGRAMMER * pgm, ULONG Mode );
//static int CH341USBTransfer_bitbangPart2(PROGRAMMER * pgm, enum libusb_endpoint_direction dir, unsigned char *buff, unsigned int size);

#define		mUSBI0_MAX_NUMBER		16
#define		mMAX_BUFFER_LENGTH		0x1000

ULONG Ch341index;
BOOL        Ch341State;
HANDLE      Ch341Handle;

//#include <usb.h>

//int CH341_write_data( PROGRAMMER * pgm, unsigned char *buf, unsigned long size );
//bool CH341_write_read ( PROGRAMMER * pgm, unsigned long write_length, unsigned char * write_buffer, unsigned long read_step,	 unsigned long read_times, unsigned long *read_length, unsigned char *read_buffer );
//bool CH341_write_read_new ( PROGRAMMER * pgm, unsigned long write_length, unsigned char * write_buffer, unsigned long read_step,	 unsigned long read_times, unsigned long *read_length, unsigned char *read_buffer );

//static int usb_bulk_io(PROGRAMMER * pgm, int ep, char *bytes, int size, int timeout);
//static int usb_bulk_io(libusb_device_handle * dev, int ep, char *bytes, int size, int timeout);

/*struct sUSBIO_context
{
	struct usb_device *dev;
	//struct  PROGRAMMER * pgm; 
	int write_timeout; 
	int read_timeout; 
	int out_ep; 
	int in_ep; 
	int index;	 	
	unsigned char	USBIO_ver_ic;	
	unsigned char	USBIO_stream_mode;	
};*/

//struct sUSBIO_context sUSBIO[ mUSBI0_MAX_NUMBER ];
//usb_dev_handle *usb_dev[ mUSBI0_MAX_NUMBER ];

/* ch341 requires LSB first, swap the bit order before send and after receive */
/* static unsigned char swap_byte(unsigned char byte) {
    byte = ((byte >> 1) & 0x55) | ((byte << 1) & 0xaa);
    byte = ((byte >> 2) & 0x33) | ((byte << 2) & 0xcc);
    byte = ((byte >> 4) & 0x0f) | ((byte << 4) & 0xf0);
    return byte;
} */

//static int CH341USBTransfer_WCHPart(PROGRAMMER * pgm, enum libusb_endpoint_direction dir, unsigned char *buff, unsigned int size) {
static int CH341USBTransfer_WCHPart(PROGRAMMER * pgm, int dir, unsigned char *buff, unsigned int bytestransferred) {
    int ret;
    //int bytestransferred;

    //if (!PDATA(pgm)->usbhandle)
        //return 0;

    //if ((ret = libusb_bulk_transfer(PDATA(pgm)->usbhandle, CH341A_USB_BULK_ENDPOINT | dir, buff, size, &bytestransferred, CH341A_USB_TIMEOUT))) {
    if ( dir == LIBUSB_ENDPOINT_OUT )
        ret = CH341WriteData( Ch341index, buff, (PULONG)&bytestransferred );
    if ( dir == LIBUSB_ENDPOINT_IN )
        ret = CH341ReadData( Ch341index, buff, (PULONG)&bytestransferred );
    
    if ( ret == FALSE )
    {
        //avrdude_message(MSG_INFO, "%s: Error: libusb_bulk_transfer for IN_EP failed: %d (%s)\n", progname, ret, libusb_error_name(ret));
        avrdude_message(MSG_INFO, "Error: CH341USBTransfer_WCHPart returns FALSE\n");
        return -1;
    }
    avrdude_message(MSG_DEBUG, "CH341USBTransfer_WCHPart: bytestransferred = %d\n", (int)bytestransferred);
    return bytestransferred;
}

//static bool CH341USBTransfer_WCH(PROGRAMMER * pgm, enum libusb_endpoint_direction dir, unsigned char *buff, unsigned int size)
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
    //int bytestransferred = 3;
    memset(cmd, 0, sizeof(cmd));
    memset(res, 0, sizeof(res));
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_ChipSelect()\n", progname);
    if (cs > 2) {
        avrdude_message(MSG_INFO, "%s: Error: invalid CS pin %d, 0~2 are available\n", progname, cs);
        return false;
    }
    cmd[0] = CH341A_CMD_UIO_STREAM;
    if (enable)
        //cmd[1] = CH341A_CMD_UIO_STM_OUT | ( 0x37 & ~(1<<cs));
        cmd[1] = CH341A_CMD_UIO_STM_OUT | 0x00;
    else
        //cmd[1] = CH341A_CMD_UIO_STM_OUT | 0x37;
        cmd[1] = CH341A_CMD_UIO_STM_OUT | 0x01;
    //cmd[2] = CH341A_CMD_UIO_STM_DIR | 0x3F;
    //cmd[3] = CH341A_CMD_UIO_STM_END;
    cmd[2] = CH341A_CMD_UIO_STM_END;
    //return CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, cmd, 4);
    return CH341USBTransfer_WCH(pgm, LIBUSB_ENDPOINT_OUT, cmd, 3);
    //return CH341WriteData( 0, cmd, (PULONG)&bytestransferred);
}

static int ch341a_bitbang_wch_open(PROGRAMMER * pgm, char * port) {
    /*LNODEID usbpid = lfirst(pgm->usbpid);
    int                   pid, vid, j, r;
    int                   errorCode = USB_ERROR_NOTFOUND;
    libusb_device_handle *handle = NULL;
    static int            didUsbInit = 0;

    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_open(\"%s\")\n", progname, port);

    if(!didUsbInit) {
        didUsbInit = 1;
        libusb_init(&ctx);
    }

    if (usbpid) {
        pid = *(int *)(ldata(usbpid));
        if (lnext(usbpid))
            avrdude_message(MSG_INFO, "%s: Warning: using PID 0x%04x, ignoring remaining PIDs in list\n",
                            progname, pid);
    } else {
        pid = CH341A_PID;
    }
    vid = pgm->usbvid? pgm->usbvid: CH341A_VID;

    libusb_device **dev_list;
    int dev_list_len = libusb_get_device_list(ctx, &dev_list);

    for (j=0; j<dev_list_len; ++j) {
        libusb_device *dev = dev_list[j];
        struct libusb_device_descriptor descriptor;
        libusb_get_device_descriptor(dev, &descriptor);
        if (descriptor.idVendor == vid && descriptor.idProduct == pid) {
            r = libusb_open(dev, &handle);
            if (!handle) {
                errorCode = USB_ERROR_ACCESS;
                avrdude_message(MSG_INFO, "%s: Warning: cannot open USB device: %s\n",
                                progname, strerror(libusb_to_errno(r)));
                continue;
            }
        }
    }
    libusb_free_device_list(dev_list,1);
    if (handle != NULL) {
        errorCode = 0;
        PDATA(pgm)->usbhandle = handle;
    }

    if (errorCode!= 0) {
        avrdude_message(MSG_INFO, "%s: error: could not find USB device with vid=0x%x pid=0x%x",
                        progname, vid, pid);
        avrdude_message(MSG_INFO, "\n");
        return -1;
    }
    if ((r = libusb_claim_interface(PDATA(pgm)->usbhandle, 0))) {
        fprintf(stderr, "%s: error: libusb_claim_interface failed: %d (%s)\n", progname, r, libusb_error_name(r));
        libusb_close(PDATA(pgm)->usbhandle);
        libusb_exit(ctx);
    } */
    int pid, vid;
    //int ret;
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
					//CH341SetExclusive( Ch341index, TRUE );
					Ch341State = TRUE;
					break;
				}
			}

	if( Ch341State == FALSE )
    //ret = (int)CH341OpenDevice(0);
    //if ( ret < 0 ) 
        {
			avrdude_message(MSG_INFO, "%s: error: could not open USB device with vid=0x%x pid=0x%x", progname, vid, pid);
            avrdude_message(MSG_INFO, "\n");
			return -1;
		}
    else return 0;
}

static void ch341a_bitbang_wch_close(PROGRAMMER * pgm) {
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_close()\n", progname);
    CH341ChipSelect_bitbang_wch(pgm, 0,false);

    /* if (PDATA(pgm)->usbhandle!=NULL) {
        libusb_release_interface(PDATA(pgm)->usbhandle, 0);
        libusb_close(PDATA(pgm)->usbhandle);
    }
    libusb_exit(ctx); */
    CH341CloseDevice(Ch341index);
}

static int ch341a_bitbang_wch_initialize(PROGRAMMER * pgm, AVRPART * p) {
    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_initialize()\n", progname);
    //CH341SetStream_bitbang( pgm, 0x81 );
    //CH341OpenDevice(0);
    CH341SetStream( Ch341index, 0x81 );
    MegaSpiEnable_wch( pgm );
    CH341ChipSelect_bitbang_wch(pgm, 0,false);
    usleep(20 * 1000);
    CH341ChipSelect_bitbang_wch(pgm, 0,true);
    return pgm->program_enable(pgm, p);
}

/* static int ch341a_bitbang_wch_spi_transfer(PROGRAMMER * pgm, const unsigned char *cmd, unsigned char *res) {
    unsigned char pkt[CH341A_PACKET_LENGTH];
    unsigned int i;
    //int ret;
    //int bytestransferred;
    int size=sizeof(cmd);

    avrdude_message(MSG_DEBUG, "%s: ch341a_bitbang_wch_spi_transfer(0x%02x, 0x%02x, 0x%02x, 0x%02x)%s",
                    progname, cmd[0], cmd[1], cmd[2], cmd[3], verbose > 3? "...\n": "\n");

    if (size > CH341A_PACKET_LENGTH - 1)
        size = CH341A_PACKET_LENGTH - 1;

    for (i = 0; i < size; i++) {
        pkt[i] = cmd[i];
    }
	
    if (CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, pkt, size)<=0 || CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_IN, pkt, size)<=0) {
        avrdude_message(MSG_INFO, "%s: failed to transfer data to/from CH341\n", progname);
        return -1;
    }

    for (i = 0; i < size; i++) {
        res[i] = pkt[i];
    }
    return size;
} */

static int ch341a_bitbang_wch_spi_cmd(PROGRAMMER * pgm, const unsigned char *cmd, unsigned char *res) {
    return pgm->spi(pgm, cmd, res, 4);
}

static int ch341a_bitbang_wch_spi(PROGRAMMER * pgm, const unsigned char *in, unsigned char *out, int size) {
    //unsigned char DataBuf[CH341A_PACKET_LENGTH];
    //unsigned int i;

    if (!size)
        return 0;

    if (size > CH341A_PACKET_LENGTH)
        size = CH341A_PACKET_LENGTH;

    //pkt[0] = CH341A_CMD_SPI_STREAM;

    //for (i = 0; i < size; i++)
        //DataBuf[i] = swap_byte(in[i]);
         //DataBuf[i] = in[i];       
        
    //MegaSpiOutInData_wch(pgm, OutLen, DataBuf );

    //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, pkt, size +1))
    //{
    //    fprintf(stderr, "Error: failed to transfer data to CH341\n");
    //    return -1;
    //}

    //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_IN, pkt, size))
    //{
    //    fprintf(stderr, "Error: failed to transfer data from CH341\n");
    //    return -1;
    //}

    //if (!MegaSpiOutInData_wch(pgm, (ULONG)size, (PVOID)DataBuf))
    if (!MegaSpiOutInData_wch(pgm, (ULONG)size, (PVOID)in, out))
    {
        fprintf(stderr, "Error: failed to transfer data to or from CH341\n");
        return -1;
    }
    
    //for (i = 0; i < size; i++)
        //out[i] = swap_byte(DataBuf[i]);
        //out[i] = in[i];

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


// ----------------------------------------------------------------------
//#else /* HAVE_LIBUSB */

/* static int ch341a_bitbang_wch_nousb_open (struct programmer_t *pgm, char * name) {
    avrdude_message(MSG_INFO, "%s: error: no usb support. please compile again with libusb installed.\n",
                    progname);
    return -1;
}

void ch341a_bitbang_wch_initpgm(PROGRAMMER * pgm) {
    strcpy(pgm->type, "ch341a_bitbang_wch");
    pgm->open = ch341a_bitbang_wch_nousb_open;
}*/

//#endif  /* HAVE_LIBUSB */
const char ch341a_bitbang_wch_desc[] = "Driver for \"ch341a_bitbang_wch\"-type programmers";

//#define     DELAY_US        4
#define     DELAY_US        10

BOOL    MegaSpiOutInData_wch(
	PROGRAMMER * pgm,
    ULONG   OutLen,         // Output data length
    PVOID   DataBufIn,
    PVOID   DataBufOut )        // The output data buffer SPI data returned
{
PUCHAR p;
ULONG i, j, k;
//ULONG zz;
ULONG zzz;
//ULONG zzzz;
UCHAR c, tem;
UCHAR  mBuffer[ 512 ];
//UCHAR  zzzzBuffer[ 4 ];

        //if( MegaSpiEnable_wch( pgm ) == FALSE)   // ?Ê¼»¯CH341µÄSPI
            //return FALSE;
        //if( SpiResetMega( pgm ) == FALSE )
            //return FALSE;                  // ¸´Î»MEGA8
        //Sleep( 200 );
        
        for( zzz = 0; zzz != 512; zzz++ )
        {
			mBuffer[ zzz ] = 0xff;
		}
        
    // 202334066 on? + 6.95us - 5.85us
        p = (PUCHAR)DataBufIn;
        k = 0;
        //zzzz = 0;
        //avrdude_message(MSG_DEBUG, "OutLen = %x\n", (int)OutLen);
        
        for( i = 0; i != OutLen; i++ )
        
        {
        
            //if (zzzz > 1)
                //zzzz = 0;
        
            tem = *p;
            //tem = 0xac;
            //if (tem == 0x00)
                //tem = 0xff;
                
            // FOR transfer in cycle
            //k = 0; // FOR transfer in cycle
            
            //avrdude_message(MSG_DEBUG, "i = %x\n", (int)i);
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: tem = %x\n", (int)tem);
            mBuffer[ k++ ] = CH341A_CMD_UIO_STREAM;            // Command code
            //mBuffer[ k ] = CH341A_CMD_UIO_STREAM;            // Command code
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)k-1, (int)mBuffer[k-1]);

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
            //  mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | 0x20;
            //  mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | 0x00; 
            mBuffer[ k++ ] = CH341A_CMD_UIO_STM_END;                   // The current command pack ends prematurely
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k-1 = %x\n", (int)k-1);
            k += CH341A_PACKET_LENGTH - 1;
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k = %x\n", (int)k);
            k &= ~ ( CH341A_PACKET_LENGTH - 1 );
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k-1 = %x\n", (int)k-1);
            //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k-1))
            //if (!CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 23))
            
            //for( zz = 0; zz != k; zz++ )
            //{
				//avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)zz, (int)mBuffer[zz]);
			//}
			
            //if (CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k)<=0)
               //{
                 //fprintf(stderr, "Error: failed to transfer data to CH341\n");
                 //return FALSE;
               //}
               
               //k = 0;

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
              //mBuffer[ k++ ] = CH341A_CMD_UIO_STM_US | DELAY_US;
              //mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | 0x20;
              //mBuffer[ k++ ] = CH341A_CMD_UIO_STM_US | DELAY_US;
              //mBuffer[ k++ ] = CH341A_CMD_UIO_STM_OUT | 0x00; 
            mBuffer[ k++ ] = CH341A_CMD_UIO_STM_END;                   // The current command pack ends prematurely
            //mBuffer[ k ] = CH341A_CMD_UIO_STM_END;              
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k = %x\n", (int)k);
            k += CH341A_PACKET_LENGTH - 1;
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k = %x\n", (int)k);
            k &= ~ ( CH341A_PACKET_LENGTH - 1 );
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k = %x\n", (int)k);
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)k-1, (int)mBuffer[k-1]);
            //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k-1))
            //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k))
            
            //for( zz = 0; zz != k; zz++ )
            //{
				//avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)zz, (int)mBuffer[zz]);
			//}
            //if (!CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 23))
            //if (!CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k/2))
            //if (CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k)<=0)
            //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k))
               //{
                 //fprintf(stderr, "Error: failed to transfer data to CH341\n");
                //return FALSE;
               //}
               
            //if (zzzz == 1)
              //{
				  //zzzzBuffer[ 0 ] = 0xFE;
				  //zzzzBuffer[ 1 ] = 0xDE;
				  //zzzzBuffer[ 2 ] = 0xFE;
				  //zzzzBuffer[ 3 ] = 0xDE;
				  //if (CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, zzzzBuffer, 4)<=0)
                      //return FALSE;
                  //if (CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k)<=0)
                      //return FALSE;
			  //}
            
            //zzzz++;
                              
               //MegaSpiEnable_wch( pgm );
               
               //CH341ChipSelect_bitbang_wch(pgm, 0,false);
               //usleep(20);
               //CH341ChipSelect_bitbang_wch(pgm, 0,true);

            p++;
        }

        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: k = %x\n", (int)k);
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: sizeof(mBuffer) = %x\n", (int)sizeof(mBuffer));
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[0] = %x\n", (int)mBuffer[0]);
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)k-1, (int)mBuffer[k-1]);
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)246, (int)mBuffer[246]);
        //mBuffer[ k++ ] = CH341A_CMD_UIO_STM_END;
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x\n", (int)k-1, (int)mBuffer[k-1]);
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: sizeof(mBuffer) = %x\n", (int)sizeof(mBuffer));
        
        //for( zz = 0; zz != k; zz++ )
            //{
				//avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x \n", (int)zz, (int)mBuffer[zz]);
			//}
			
        //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k))
        //if (CH341USBTransfer_bitbangPart2(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, (unsigned int)k)<=0)
        //if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 246))
               //{
                 //fprintf(stderr, "Error: failed to transfer data to CH341\n");
                 //return FALSE;
               //}
                
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: before CH341_write_read OutLen = %d \n", (int)OutLen);
        
        //i = CH341_write_read( pgm, k, mBuffer, 4, OutLen*2, &j, mBuffer );
        //i = CH341_write_read( pgm, k, mBuffer, 4, OutLen, &j, mBuffer );
        //i = CH341_write_read( pgm, k, mBuffer, 8, OutLen, &j, mBuffer );
        
        //i = CH341_write_read_new( pgm, k, mBuffer, 4, OutLen*2, &j, mBuffer );
        
        i = CH341WriteRead( Ch341index, k, mBuffer, 4, OutLen*2, &j, mBuffer );
          
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: after CH341WritRead i = %d \n", (int)i);
        //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: after CH341_write_read OutLen = %d \n", (int)OutLen);
                
        //for( zz = 0; zz != 128; zz++ )
            //{
				//avrdude_message(MSG_DEBUG, "mBuffer[%x] = %x ", (int)zz, (int)mBuffer[zz]);
			//}
        if( i == FALSE )
            return( FALSE );

        if( j != OutLen * 8 )
        //if( j != OutLen * 4 )
            return( FALSE );

        k = 0;
        for ( i = 0; i != OutLen; i++ )
        {
            c = 0;
            for ( j = 0; j < 8; j ++ )
            //for ( j = 0; j < 4; j ++ )
            {
                c <<= 1;
                if ( mBuffer[ k++ ] & 0x80 )        // input 8 bit
                    c ++;
            }

            *(PUCHAR)DataBufOut = c;
            //((PUCHAR)DataBuf)++;
            //avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: DataBufOut[%x] = %x \n", (int)i, (int)c);
            DataBufOut++;
        } 
        //for( zz = 0; zz != j; zz++ )
            //{
				//avrdude_message(MSG_DEBUG, "MegaSpiOutInData_wch: mBuffer[%x] = %x \n", (int)zz, (int)mBuffer[zz]);
			//}
        return TRUE;
}

/*BOOL CH341WriteRead(PROGRAMMER * pgm, ULONG iWriteLength, PVOID iWriteBuffer,
			ULONG iReadStep, ULONG iReadTimes,
			PULONG oReadLength, PVOID oReadBuffer)
{
	//int retval = 0;
	//ULONG mLength;
	ULONG mReadlen;
	//ULONG iReadStep,iReadTimes;
	//struct{
	//	ULONG oReadlen;
	//	PUCHAR iBuf;	
	//	PUCHAR oBuffer;
	//	ULONG oReturnlen;
	//	}Read;
	//iReadStep =*(PUCHAR)(iWriteBuffer+iWriteLength-8);
	//iReadTimes = *(PUCHAR)(iWriteBuffer+iWriteLength-4);
	mReadlen = iReadStep * iReadTimes;
	if( mReadlen == 0 )
		return FALSE;
	//mLength = max( iWriteLength, mReadlen );
//#if 0
	//printf("iWriteLength : %d\n",iWriteLength);
	//printf("iReadTimes : %d\n",iReadTimes);
	//printf("iReadStep : %d\n",iReadStep);
//#endif
	//Read.iBuf = (PUCHAR)iWriteBuffer;
	//Read.oBuffer = (PUCHAR)oReadBuffer;
	//Read.oReturnlen = oReadLength;
//	printf("iBuffer Addr is ------>:%p\n",Read.iBuf);
	//Read.oReadlen = iWriteLength;
	//retval = ioctl( dev_fd, CH34x_PIPE_WRITE_READ, (unsigned long)&Read );
	
	if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, iWriteBuffer, (unsigned int)iWriteLength))
	//if (!CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, iWriteBuffer, iWriteLength))
	//if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, iWriteBuffer, sizeof(iWriteBuffer)))
	//if (!CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, iWriteBuffer, sizeof(iWriteBuffer)))
	//if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, iWriteBuffer, 100))
	//if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, iWriteBuffer, 200))
    {
        fprintf(stderr, "Error: failed to transfer data to CH341\n");
        //return -1;
        return FALSE;
    }

    if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_IN, oReadBuffer, oReadLength))
    {
        fprintf(stderr, "Error: failed to transfer data from CH341\n");
        //return -1;
        return FALSE;
    }
	//if( retval == -1 )
	//{
		//printf("Error in pipe write/read\n");
		//return FALSE;
	//}	
	return TRUE;
}*/

BOOL   MegaSpiEnable_wch( PROGRAMMER * pgm )
{
UCHAR   mBuffer[ CH341A_PACKET_LENGTH ];
ULONG   i;

    i = 0;
    mBuffer[ i++ ] = CH341A_CMD_UIO_STREAM;            // ÃüÁîÂë
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_OUT | 0x00;    // default status: all 0
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_DIR | 0x29;    // D0 & D3 & D5 output, other input
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_US | 32;       // ÑÓ?32Î¢Ãë
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_END;           // ??ÃüÁî?üÌáÇ?½áÊø
    //return( CH341WriteData( pgm, mBuffer, &i ) );     // Ö´ÐÐÊý¾Ý?ÃüÁî
    if (!CH341USBTransfer_WCH(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 5))
    {
        fprintf(stderr, "Error: MegaSpiEnable_wch returns FALSE\n");
        //return -1;
        return FALSE;
    }
    return TRUE;
}

/*BOOL     SpiResetMega( PROGRAMMER * pgm )
{
UCHAR  mBuffer[ CH341A_PACKET_LENGTH];
ULONG i;

    i = 0;
    mBuffer[ i++ ] = CH341A_CMD_UIO_STREAM;            // ÃüÁîÂë
    //mBuffer[ i++ ] = mCH341A_CMD_UIO_STM_OUT |0x02;     // D1=1
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_OUT |0x01;     // D0=1
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_END;
    //if(CH341WriteData( index, mBuffer, &i ) == FALSE )
        //return FALSE;
    if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 4))
    {
        fprintf(stderr, "Error: SpiResetMega_1\n");
        //return -1;
        return FALSE;
    }
    i = 0;
    mBuffer[ i++ ] = CH341A_CMD_UIO_STREAM;            // ÃüÁîÂë
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_OUT |0x00;     // D1=0
    mBuffer[ i++ ] = CH341A_CMD_UIO_STM_END;
    //return (CH341WriteData( index, mBuffer, &i ) );
    if (!CH341USBTransfer_bitbang(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 4))
    {
        fprintf(stderr, "Error: SpiResetMega_2\n");
        //return -1;
        return FALSE;
    }
    return TRUE;
}*/

/* BOOL CH341SetStream_bitbang( PROGRAMMER * pgm, ULONG Mode )
{
	UCHAR mBuffer[CH341A_PACKET_LENGTH];
	UCHAR StreamMode;
	//ULONG mLength;
	StreamMode = (UCHAR)( Mode & 0x8F );
	mBuffer[0] = CH341A_CMD_I2C_STREAM;
	mBuffer[1] = CH341A_CMD_I2C_STM_SET | (StreamMode & 0x0F);
	mBuffer[2] = CH341A_CMD_I2C_STM_END;
	//mLength = 3;
	//if( CH34xWriteData( mBuffer, &mLength ))
	//{
		//if( mLength >= 2 )
			//return true;
	//}
	//return false;
    return CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, mBuffer, 3);
} */


/*static int CH341USBTransfer_bitbangPart2(PROGRAMMER * pgm, enum libusb_endpoint_direction dir, unsigned char *buff, unsigned int size) {
    int ret, bytestransferred;

    if (!PDATA(pgm)->usbhandle)
        return 0;

    if ((ret = libusb_interrupt_transfer(PDATA(pgm)->usbhandle, CH341A_USB_BULK_ENDPOINT | dir, buff, size, &bytestransferred, CH341A_USB_TIMEOUT))) {
        avrdude_message(MSG_INFO, "%s: Error: libusb_interrupt_transfer for IN_EP failed: %d (%s)\n", progname, ret, libusb_error_name(ret));
        return -1;
    }
    return bytestransferred;
}*/


/* bool CH341_write_read ( PROGRAMMER * pgm, unsigned long write_length, unsigned char * write_buffer, unsigned long read_step, unsigned long read_times, unsigned long *read_length, unsigned char *read_buffer )	
{
	unsigned char read[CH341A_PACKET_LENGTH];
	//unsigned long j=0,k=0,s=0;
	long j=0,k=0,s=0;
	unsigned long 	m_length, m_rdlen;
    int bytestransferred2;
    int lwr_err;
	m_rdlen = read_step * read_times;
	
	avrdude_message(MSG_DEBUG, "CH341_write_read: read_step = %x \n", (int)read_step);
	avrdude_message(MSG_DEBUG, "CH341_write_read: read_times = %x \n", (int)read_times);
	avrdude_message(MSG_DEBUG, "CH341_write_read: m_rdlen = %x \n", (int)m_rdlen);
	
	//ULONG iReadStep,iReadTimes;
	//iReadStep = (write_buffer+write_length-8);
	//iReadTimes = (write_buffer+write_length-4);
	//avrdude_message(MSG_DEBUG, "CH341_write_read: iReadStep = %x \n", (int)iReadStep);
	//avrdude_message(MSG_DEBUG, "CH341_write_read: iReadTimes = %x \n", (int)iReadTimes);
	
	if ( m_rdlen == 0 ) return( FALSE );
	m_length = max( write_length, m_rdlen );
	j = CH341_write_data( pgm, write_buffer, write_length );
	//j = CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, write_buffer, write_length);
	do
	{
		//s = usb_bulk_read( usb_dev[ pgm ], sUSBIO[ pgm ].in_ep, read, read_step, sUSBIO[ pgm ].read_timeout );
		//s = usb_bulk_io( PDATA(pgm)->usbhandle, 0x82, read, read_step, 5000 );
        
        if ((lwr_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, 0x82, read, read_step, &bytestransferred2, 5000)))
        //if (lwr_err = libusb_interrupt_transfer(PDATA(pgm)->usbhandle, 0x82, read, read_step, &bytestransferred2, 5000))
            {
			avrdude_message(MSG_INFO, "CH341_write_read. Error: libusb_bulk_transfer for IN_EP failed: %d (%s)\n", lwr_err, libusb_error_name(lwr_err));
			return FALSE;
			}
        s = bytestransferred2;
        
        avrdude_message(MSG_INFO, "s = %d\n", (int)s);
        
        if( s < 0 )
	       {
		     //printf("Error in pipe write/read\n");
		     avrdude_message(MSG_INFO, "Error in pipe write/read. s = %d\n", (int)s);
             return false;
	       }	

		memcpy( read_buffer, read, s );
		read_buffer += s;
		k += s;
	}
	while
	(--read_times);
	*read_length = k;
	//return k;	
	
    avrdude_message(MSG_INFO, "k = %d\n", (int)k);
    
	if( k == -1 )
	{
		//printf("Error in pipe write/read\n");
		avrdude_message(MSG_INFO, "Error in pipe write/read. k = %d\n", (int)k);
        return false;
	}	
	return true;
} */

/* int CH341_write_data( PROGRAMMER * pgm, unsigned char *buf, unsigned long size )
{
    int ret;
    int offset = 0;
    int total_written = 0;
    int bytestransferred3;
    int lw_err;
	while (offset < size) 
	{
        int write_size = mMAX_BUFFER_LENGTH;
		if (offset+write_size > size)
            write_size = size-offset;
            
        avrdude_message(MSG_INFO, "write_size = %d\n", (int)write_size);
        
		//ret = usb_bulk_write( usb_dev[ pgm ], sUSBIO[ pgm ].out_ep, (char *)buf+offset, write_size, sUSBIO[ pgm ].write_timeout);
		//ret = usb_bulk_io( PDATA(pgm)->usbhandle, 0x02, (char *)buf+offset, write_size, 5000);
		
        if ((lw_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, 0x02, (char *)buf+offset, write_size, &bytestransferred3, 5000)))
        //if (lw_err = libusb_interrupt_transfer(PDATA(pgm)->usbhandle, 0x02, (char *)buf+offset, write_size, &bytestransferred3, 5000))
        //if (lw_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, CH341A_USB_BULK_ENDPOINT | dir, buff, size, &bytestransferred, CH341A_USB_TIMEOUT))
            {
			avrdude_message(MSG_INFO, "CH341_write_data. Error: libusb_bulk_transfer for IN_EP failed: %d (%s)\n", lw_err, libusb_error_name(lw_err));
		    //fprintf(stderr, "CH341_write_data. Error: failed to transfer data to CH341\n");
			return FALSE;
			}
        ret = bytestransferred3;
        		
	    avrdude_message(MSG_INFO, "bytestransferred3 = %d\n", (int)bytestransferred3);
        //avrdude_message(MSG_INFO, "ret = %d\n", (int)ret);
    
		if (ret < 0)
			return FALSE;
        total_written += ret;
        offset += write_size;
        
	    avrdude_message(MSG_INFO, "offset = %d\n", (int)offset);
	    avrdude_message(MSG_INFO, "total_written = %d\n", (int)total_written);
    }
	return total_written;
} */

//#define compat_err(e) -(errno=libusb_to_errno(e))

/* static int usb_bulk_io(libusb_device_handle * dev, int ep, char *bytes, int size, int timeout)
{
	int actual_length;
	int r;
	//usbi_dbg("endpoint %x size %d timeout %d", ep, size, timeout);
	r = libusb_bulk_transfer(dev, ep & 0xff, bytes, size, &actual_length, timeout);
	
	// if we timed out but did transfer some data, report as successful short
	// read. FIXME: is this how libusb-0.1 works?
	if (r == 0 || (r == LIBUSB_ERROR_TIMEOUT && actual_length > 0))
		return actual_length;

	return compat_err(r);
} */


/* bool CH341_write_read_new ( PROGRAMMER * pgm, unsigned long write_length, unsigned char * write_buffer, unsigned long read_step, unsigned long read_times, unsigned long *read_length, unsigned char *read_buffer )	
{
	unsigned char read[CH341A_PACKET_LENGTH];
	//unsigned long j=0,k=0,s=0;
	//long j=0;
	long k=0,s=0;
	//unsigned long 	m_length;
	unsigned long m_rdlen;
    int bytestransferred2;
    int bytestransferred3 = 0;
    int sum_bytestransferred3 = 0;
    int lw_err;
    int lwr_err;
	m_rdlen = read_step * read_times;
	
	//avrdude_message(MSG_DEBUG, "CH341_write_read: read_step = %x \n", (int)read_step);
	//avrdude_message(MSG_DEBUG, "CH341_write_read: read_times = %x \n", (int)read_times);
	//avrdude_message(MSG_DEBUG, "CH341_write_read: m_rdlen = %x \n", (int)m_rdlen);
	
	//ULONG iReadStep,iReadTimes;
	//iReadStep = (write_buffer+write_length-8);
	//iReadTimes = (write_buffer+write_length-4);
	//avrdude_message(MSG_DEBUG, "CH341_write_read: iReadStep = %x \n", (int)iReadStep);
	//avrdude_message(MSG_DEBUG, "CH341_write_read: iReadTimes = %x \n", (int)iReadTimes);
	
	if ( m_rdlen == 0 ) return( FALSE );
	//m_length = max( write_length, m_rdlen );
	//j = CH341_write_data( pgm, write_buffer, write_length );
	//j = CH341USBTransfer_bitbangPart(pgm, LIBUSB_ENDPOINT_OUT, write_buffer, write_length);
	do
	{
	    //avrdude_message(MSG_INFO, "bytestransferred3 = %d\n", (int)bytestransferred3);
	    
		if (write_length > sum_bytestransferred3)
			{
			//if (lw_err = libusb_interrupt_transfer(PDATA(pgm)->usbhandle, 0x02, (char *)buf+offset, write_size, &bytestransferred3, 5000))
			//if (lw_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, CH341A_USB_BULK_ENDPOINT | dir, buff, size, &bytestransferred, CH341A_USB_TIMEOUT))
			//if ((lw_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, 0x02, write_buffer, write_length, &bytestransferred3, 5000)))
			if ((lw_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, 0x02, (char *)write_buffer+sum_bytestransferred3, 32, &bytestransferred3, 5000)))
				{
				avrdude_message(MSG_INFO, "CH341_write_data. Error: libusb_bulk_transfer for IN_EP failed: %d (%s)\n", lw_err, libusb_error_name(lw_err));
				//fprintf(stderr, "CH341_write_data. Error: failed to transfer data to CH341\n");
				return FALSE;
				}
            sum_bytestransferred3 += bytestransferred3;			
			}
		//s = usb_bulk_read( usb_dev[ pgm ], sUSBIO[ pgm ].in_ep, read, read_step, sUSBIO[ pgm ].read_timeout );
		//s = usb_bulk_io( PDATA(pgm)->usbhandle, 0x82, read, read_step, 5000 );
        
        if ((lwr_err = libusb_bulk_transfer(PDATA(pgm)->usbhandle, 0x82, read, read_step, &bytestransferred2, 5000)))
        //if (lwr_err = libusb_interrupt_transfer(PDATA(pgm)->usbhandle, 0x82, read, read_step, &bytestransferred2, 5000))
            {
			avrdude_message(MSG_INFO, "CH341_write_read. Error: libusb_bulk_transfer for IN_EP failed: %d (%s)\n", lwr_err, libusb_error_name(lwr_err));
			return FALSE;
			}
        s = bytestransferred2;
        
        //avrdude_message(MSG_INFO, "s = %d\n", (int)s);
        
        //if( s < 0 )
	       //{
		     //printf("Error in pipe write/read\n");
		     //avrdude_message(MSG_INFO, "Error in pipe write/read. s = %d\n", (int)s);
             //return false;
	       //}	

		memcpy( read_buffer, read, s );
		read_buffer += s;
		k += s;
	}
	while
	(--read_times);
	*read_length = k;
	//return k;	
	
    //avrdude_message(MSG_INFO, "k = %d\n", (int)k);
	return true;
} */


/* void IniCh341Device( void )
{
    Ch341Handle = CH341OpenDevice( Ch341index );
    if ( Ch341Handle  == INVALID_HANDLE_VALUE )
    {
        EnableWindow( GetDlgItem( MainDialog, IDC_BUTTON_DOWN ), 0 );
        EnableWindow( GetDlgItem( MainDialog, IDC_SET_CONFIG), 0);
        EnableWindow( GetDlgItem( MainDialog, IDC_RUNING ), 0 );
        EnableWindow( GetDlgItem( MainDialog, IDC_BUTTON_BROWER ), 0 );
        SetDlgItemText( MainDialog, IDC_STATIC_PROMPT, _T("Lost connection with CH341 device") );
        Ch341State = FALSE;
    }
    else
    {
        CH341SetExclusive( Ch341index, TRUE );
        CH341ChipVer = CH341GetVerIC( Ch341index );
        CH341SPIBit = FALSE;
        if( CH341ChipVer >= 0x30 )
        {
            CH341SPIBit = TRUE;
            // ÏÂÃæÉèÖÃ´®¿ÚÁ÷Ä£Ê½
            CH341SetStream( Ch341index, 0x81 );
        }

        // ²»Ö§³Ö Ôò½ûÖ¹
        SetSpiSysMenu( );
        // Èç¹ûÖ§³ÖSPI Ôò¶ÁÅäÖÃ²ÎÊý
        if( CH341SPIBit )
            ReadSpiRegConfig( );

        EnableWindow( GetDlgItem( MainDialog, IDC_SET_CONFIG), 1 );
        EnableWindow( GetDlgItem( MainDialog, IDC_RUNING ), 1 );
        EnableWindow( GetDlgItem( MainDialog, IDC_BUTTON_BROWER ), 1 );
        EnableWindow( GetDlgItem( MainDialog, IDC_BUTTON_DOWN ), FileOpenBit );
        SetDlgItemText( MainDialog, IDC_STATIC_PROMPT, _T("Successfully open CH341 device ") );
        Ch341State = TRUE;
    }
}
*/
