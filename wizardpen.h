/* 
 * Copyright (c) 2005 Jan Horak, Nickolay Kondrashov
 *
 * Copyright (c) 2001 Edouard TISSERANT <edouard.tisserant@wanadoo.fr>
 * Parts inspired from Shane Watts <shane@bofh.asn.au> XFree86 3 WizardPen Driver
 * Thanks to Emily, from WizardPen, For giving me documents.
 *
 * Modified by Carlo Vittoli <carlo@abila.it> for compatibility with
 * the A-Series tablet.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/wizardpen/wizardpen.h,v 1.2tsi Exp $ */

#ifndef	_WIZARDPEN_H_
#define _WIZARDPEN_H_

/* define X_ORG_VERSION_CURRENT when not defined */
#ifndef XORG_VERSION_CURRENT
#define XORG_VERSION_CURRENT (((6) * 10000000) + ((8) * 100000) + ((2) * 1000) + 0)
#endif

/******************************************************************************
 *		Definitions
 *		structs, typedefs, #defines, enums
 *****************************************************************************/
#define WIZARDPEN_PACKET_SIZE	7
#define FLAIR_PACKET_SIZE	7
#define A_SERIES_PACKET_SIZE	5

#define WIZARDPEN_CONFIG	"a"    /* Send configuration (max coords) */

#define WIZARDPEN_ABSOLUTE	'F'    /* Absolute mode */
#define WIZARDPEN_RELATIVE	'E'    /* Relative mode */

#define WIZARDPEN_UPPER_ORIGIN	"b"    /* Origin upper left */

#define WIZARDPEN_PROMPT_MODE	"B"    /* Prompt mode */
#define WIZARDPEN_STREAM_MODE	"@"    /* Stream mode */
#define WIZARDPEN_INCREMENT	'I'    /* Set increment */
#define WIZARDPEN_BINARY_FMT	"zb"   /* Binary reporting */

#define WIZARDPEN_PROMPT	"P"    /* Prompt for current position */

#define PHASING_BIT		0x80
#define PROXIMITY_BIT		0x40
#define TABID_BIT		0x20
#define XSIGN_BIT		0x10
#define YSIGN_BIT		0x08
#define BUTTON_BITS		0x07
#define COORD_BITS		0x7f

#define ABSOLUTE_FLAG		1

#define milisleep(ms) xf86usleep (ms * 1000)

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

static const char *wizardpen_initstr =
    WIZARDPEN_BINARY_FMT WIZARDPEN_STREAM_MODE;

typedef struct
{
    int is_proximity;
    char mask;
    int x;
    int y;
    int z;
    int dx;
    int dy;
    int dz;
    int buttons;
    int wheel;
} WizardPenEventRec, *WizardPenEventRecPtr;

#define WIZARDPEN_EVENT_MASK_NONE	0x00
#define WIZARDPEN_EVENT_MASK_X	0x01
#define WIZARDPEN_EVENT_MASK_Y	0x02
#define WIZARDPEN_EVENT_MASK_Z	0x04
#define WIZARDPEN_EVENT_MASK_ALL	0x07

typedef struct
{
    XISBuffer *buffer;
    int wizardpenInc;		       /* increment between transmits */
    int wizardpenMaxX;		       /* MaxX */
    int wizardpenMaxY;		       /* MaxY */
    int wizardpenMaxZ;		       /* MaxZ */
    int wizardpenOldX;		       /* previous X position */
    int wizardpenOldY;		       /* previous Y position */
    int wizardpenOldZ;		       /* previous Z position */
    int wizardpenOldProximity;	       /* previous proximity */
    int wizardpenOldButtons;	       /* previous buttons state */
    char wizardpenReportSpeed;	       /* report speed */
    int wizardpenUSB;		       /*USB flag */
    int flags;			       /* various flags */
    int packeti;		       /* number of bytes read */
    int PacketSize;		       /* number of bytes read */
    int *buttonMap;		       /* defines button mapping */
    int packet_size;		       /* model-specific */
    unsigned char packet[WIZARDPEN_PACKET_SIZE];	/* data read on the device */
    /* pressure */
    int topZ;
    int bottomZ;
    /* border settings */
    int topX;
    int topY;
    int bottomX;
    int bottomY;
    /* event */
    WizardPenEventRec event;
} WizardPenPrivateRec, *WizardPenPrivatePtr;

/******************************************************************************
 *		Declarations
 *****************************************************************************/
#ifdef XFree86LOADER
static MODULESETUPPROTO(SetupProc);
static void TearDownProc(void *);
#endif
static Bool DeviceControl(DeviceIntPtr, int);
static Bool DeviceOn(DeviceIntPtr);
static Bool DeviceOff(DeviceIntPtr);
static Bool DeviceClose(DeviceIntPtr);
static Bool DeviceInit(DeviceIntPtr);
static void ReadInput(LocalDevicePtr);
static void CloseProc(LocalDevicePtr);
static Bool ConvertProc(LocalDevicePtr, int, int, int, int, int, int, int,
    int, int *, int *);
static Bool ReverseConvertProc(LocalDevicePtr, int, int, int *);
static Bool QueryHardware(WizardPenPrivatePtr);
static void NewPacket(WizardPenPrivatePtr priv);
static Bool WizardPenGetPacket(WizardPenPrivatePtr);
static InputInfoPtr WizardPenPreInit(InputDriverPtr, IDevPtr, int);
static int SwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);

#ifdef LINUX_INPUT
static void TranslateEventCoords(LocalDevicePtr local);
static void RegisterButtons(LocalDevicePtr local);
static void RegisterWheel(LocalDevicePtr local);
static void RegisterProximity(LocalDevicePtr local);
static void RegisterMotion(LocalDevicePtr local);
static void ResetEvent(LocalDevicePtr local);
static void FlushEvent(LocalDevicePtr local);
static void USBInputABS(LocalDevicePtr local, struct input_event *event);
static void USBInputREL(LocalDevicePtr local, struct input_event *event);
static void USBInputKEY(LocalDevicePtr local, struct input_event *event);
static void USBInputSYN(LocalDevicePtr local, struct input_event *event);
static void USBReadInput(LocalDevicePtr);
static Bool USBQueryHardware(LocalDevicePtr);
static int IsUSBLine(int);
#endif

#endif
