/* 
 * Copyright (c) 2001 Edouard TISSERANT <tissered@esstin.u-nancy.fr>
 * Parts inspired from Shane Watts <shane@bofh.asn.au> Xfree 3 Acecad Driver
 * Thanks to Emily, from AceCad, For giving me documents.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/acecad/acecad.h,v 1.2tsi Exp $ */

#ifndef _WIZARDPEN_H_
#define _WIZARDPEN_H_

/******************************************************************************
 *		Definitions
 *		structs, typedefs, #defines, enums
 *****************************************************************************/
#define WIZARDPEN_PACKET_SIZE	7

#define WIZARDPEN_CONFIG		"a"		/* Send configuration (max coords) */

#define WIZARDPEN_ABSOLUTE		'F'		/* Absolute mode */
#define WIZARDPEN_RELATIVE		'E'		/* Relative mode */

#define WIZARDPEN_UPPER_ORIGIN	"b"		/* Origin upper left */

#define WIZARDPEN_PROMPT_MODE	"B"		/* Prompt mode */
#define WIZARDPEN_STREAM_MODE	"@"		/* Stream mode */
#define WIZARDPEN_INCREMENT	'I'		/* Set increment */
#define WIZARDPEN_BINARY_FMT	"zb"		/* Binary reporting */

#define WIZARDPEN_PROMPT		"P"		/* Prompt for current position */

#define PHASING_BIT		0x80
#define PROXIMITY_BIT		0x40
#define TABID_BIT		0x20
#define XSIGN_BIT		0x10
#define YSIGN_BIT		0x08
#define BUTTON_BITS		0x07
#define COORD_BITS		0x7f

#define ABSOLUTE_FLAG		1
#define USB_FLAG		2
#define AUTODEV_FLAG		4
#define AVAIL_FLAG		8

#define NOTAVAIL ((errno == ENODEV) || (errno == ENXIO) || (errno == ENOENT))

#define milisleep(ms) xf86usleep (ms * 1000)

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

static const char * wizardpen_initstr = WIZARDPEN_BINARY_FMT WIZARDPEN_STREAM_MODE;

typedef struct 
{
    XISBuffer	*buffer;
    int		wizardpenInc;		/* increment between transmits */
    int		wizardpenOldX;		/* previous X position */
    int		wizardpenOldY;		/* previous Y position */
    int		wizardpenOldZ;		/* previous Z position */
    int		wizardpenOldProximity;	/* previous proximity */
    int		wizardpenOldButtons;	/* previous buttons state */
    int		wizardpenMaxX;		/* max X value */
    int		wizardpenMaxY;		/* max Y value */
    int		wizardpenMaxZ;		/* max Y value */
    char	wizardpenReportSpeed;	/* report speed */
    int		flags;			/* various flags */
    int		packeti;		/* number of bytes read */
    int		PacketSize;		/* number of bytes read */
    unsigned char packet[WIZARDPEN_PACKET_SIZE];	/* data read on the device */
	 int topZ;
	 int bottomZ;
	 int topX;
	 int topY;
	 int bottomX;
	 int bottomY;
} WizardPenPrivateRec, *WizardPenPrivatePtr;


/******************************************************************************
 *		Declarations
 *****************************************************************************/
#ifdef XFree86LOADER
static MODULESETUPPROTO( SetupProc );
static void TearDownProc (void *);
#endif
static Bool DeviceControl (DeviceIntPtr, int);
static Bool DeviceOn (DeviceIntPtr);
static Bool DeviceOff (DeviceIntPtr);
static Bool DeviceClose (DeviceIntPtr);
static Bool DeviceInit (DeviceIntPtr);
static void CloseProc (LocalDevicePtr);
static Bool ConvertProc (LocalDevicePtr, int, int, int, int, int, int, int, int, int *, int *);
static Bool ReverseConvertProc(LocalDevicePtr , int , int , int*);
static Bool QueryHardware (WizardPenPrivatePtr);
static void NewPacket (WizardPenPrivatePtr priv);
static Bool WizardPenGetPacket (WizardPenPrivatePtr);
static InputInfoPtr WizardPenPreInit(InputDriverPtr, IDevPtr , int);
#ifdef LINUX_INPUT
static void USBReadInput (LocalDevicePtr);
static Bool USBQueryHardware (LocalDevicePtr);
static int IsUSBLine(int);
static Bool WizardPenAutoDevProbe(LocalDevicePtr, int);
#endif


#endif
