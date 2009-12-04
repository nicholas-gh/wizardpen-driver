/*
 * Copyright (c) 2001 Edouard TISSERANT <tissered@esstin.u-nancy.fr>
 * Parts inspired from Shane Watts <shane@bofh.asn.au> XFree86 3 Acecad Driver
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/acecad/acecad.c,v 1.4 2003/10/30 00:40:45 dawes Exp $ */

#include "config.h"

#define _WIZARDPEN_C_
/*****************************************************************************
 *	Standard Headers
 ****************************************************************************/

#ifdef LINUX_INPUT
#include <asm/types.h>
#include <linux/input.h>
#ifndef EV_SYN
#define EV_SYN EV_RST
#define SYN_REPORT 0
#endif
#ifdef BUS_PCI
#undef BUS_PCI
#endif
#ifdef BUS_ISA
#undef BUS_ISA
#endif
#endif

#include <misc.h>
#include <xf86.h>
#ifndef NEED_XF86_TYPES
#define NEED_XF86_TYPES
#endif
#include <xf86_OSproc.h>
#include <xisb.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <xf86Module.h>

#include <string.h>
#include <stdio.h>

#ifdef LINUX_INPUT
#include <errno.h>
#include <fcntl.h>
#ifdef LINUX_SYSFS
#include <sysfs/libsysfs.h>
#include <dlfcn.h>
#endif
#endif

/*****************************************************************************
 *	Local Headers
 ****************************************************************************/
#include "wizardpen.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/

#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

_X_EXPORT InputDriverRec WIZARDPEN =
{
	6,
	"wizardpen",
	NULL,
	WizardPenPreInit,
	NULL,
	NULL,
	0
};

#ifdef XFree86LOADER
static XF86ModuleVersionInfo VersionRec =
{
	"wizardpen",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}
};


_X_EXPORT XF86ModuleData wizardpenModuleData = {
	&VersionRec,
	SetupProc,
	TearDownProc
};

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static pointer
SetupProc(	pointer module,
		pointer options,
		int *errmaj,
		int *errmin )
{
	xf86AddInputDriver(&WIZARDPEN, module, 0);
	return module;
}

static void
TearDownProc( pointer p )
{
#if 0
	LocalDevicePtr local = (LocalDevicePtr) p;
	AceCadPrivatePtr priv = (AceCadPrivatePtr) local->private;

	DeviceOff (local->dev);

	xf86CloseSerial (local->fd);
	XisbFree (priv->buffer);
	xfree (priv);
	xfree (local->name);
	xfree (local);
#endif
}
#endif

static const char *default_options[] =
{
	"BaudRate", "9600",
	"StopBits", "1",
	"DataBits", "8",
	"Parity", "Odd",
	"Vmin", "1",
	"Vtime", "10",
	"FlowControl", "Xoff",
	NULL
};

#ifdef LINUX_INPUT
static int
IsUSBLine(int fd)
{
    int version;
    int err;

    SYSCALL(err = ioctl(fd, EVIOCGVERSION, &version));

    if (!err) {
        xf86MsgVerb(X_PROBED, 4, "Kernel Input driver version is %d.%d.%d\n",
                version >> 16, (version >> 8) & 0xff, version & 0xff);
        return 1;
    } else {
        xf86MsgVerb(X_PROBED, 4, "No Kernel Input driver found\n");
        return 0;
    }
}

/* Heavily inspired by synpatics/eventcomm.c */

#define DEV_INPUT_EVENT "/dev/input/event"
#define EV_DEV_NAME_MAXLEN 64
#define SET_EVENT_NUM(str, num) \
    snprintf(str, EV_DEV_NAME_MAXLEN, "%s%d", DEV_INPUT_EVENT, num)

static Bool
fd_query_wizardpen(int fd, char *wizardpen_name) {
    char name[256] = "Unknown";
    int cmp_at = strlen(wizardpen_name);
    if (cmp_at > 256)
        cmp_at = 256;
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    name[cmp_at] = '\0';
	 if (xf86NameCmp(name, wizardpen_name) == 0)
        return TRUE;
    return FALSE;
}

static char wizardpen_name_default[10] = "  TABL";

#ifdef LINUX_SYSFS
static char usb_bus_name[4] = "usb";
static char wizardpen_driver_name[11] = "evdev";
#endif

static Bool
WizardPenAutoDevProbe(LocalDevicePtr local, int verb)
{
    /* We are trying to find the right eventX device */
    int i = 0;
    Bool have_evdev = FALSE;
    int noent_cnt = 0;
    const int max_skip = 4;
    char *wizardpen_name = xf86FindOptionValue(local->options, "Name");
    char fname[EV_DEV_NAME_MAXLEN];
    int np;

#ifdef LINUX_SYSFS
    struct sysfs_bus *usb_bus = NULL;
    struct sysfs_driver *wizardpen_driver = NULL;
    struct sysfs_device *candidate = NULL;
    char *link = NULL;
    struct dlist *devs = NULL;
    struct dlist *links = NULL;
    unsigned int major = 0, minor = 0;
    void *libsysfs = NULL;

    if (libsysfs = dlopen("libsysfs.so", RTLD_NOW | RTLD_GLOBAL)) {
        xf86MsgVerb(X_INFO, verb, "%s: querying sysfs for wizardpen tablets\n", local->name);
        usb_bus = sysfs_open_bus(usb_bus_name);
        if (usb_bus) {
            xf86MsgVerb(X_PROBED, 4, "%s: usb bus opened\n", local->name);
            wizardpen_driver = sysfs_get_bus_driver(usb_bus, wizardpen_driver_name);
            if (wizardpen_driver) {
                xf86MsgVerb(X_PROBED, 4, "%s: usb_wizardpen driver opened\n", local->name);
                devs = sysfs_get_driver_devices(wizardpen_driver);
                if (devs) {
                    xf86MsgVerb(X_PROBED, 4, "%s: usb_wizardpen devices retrieved\n", local->name);
                    dlist_for_each_data(devs, candidate, struct sysfs_device) {
                        xf86MsgVerb(X_PROBED, 4, "%s: device %s at %s\n", local->name, candidate->name, candidate->path);
                        links = sysfs_open_link_list(candidate->path);
                        dlist_for_each_data(links, link, char) {
                            if (sscanf(link, "input:event%d", &i) == 1) {
                                xf86MsgVerb(X_PROBED, 4, "%s: device %s at %s: %s\n", local->name, candidate->name, candidate->path, link);
                                break;
                            }
                        }
                        sysfs_close_list(links);
                        if (i > 0) // We found something 
                            break;
                    }
                } else
                    xf86MsgVerb(X_WARNING, 4, "%s: no wizardpen devices found\n", local->name);
            } else
                xf86MsgVerb(X_WARNING, 4, "%s: evdev driver not found\n", local->name);
        } else
            xf86MsgVerb(X_WARNING, 4, "%s: usb bus not found\n", local->name);
        sysfs_close_bus(usb_bus);
        dlclose(libsysfs);

        if (i > 0) {
            // We found something 
            np = SET_EVENT_NUM(fname, i);
            if (np < 0 || np >= EV_DEV_NAME_MAXLEN) {
                xf86MsgVerb(X_WARNING, verb, "%s: unable to manage event device %d\n", local->name, i);
            } else {
                goto ProbeFound;
            }
        } else
            xf86MsgVerb(X_WARNING, verb, "%s: no Wizardpen devices found via sysfs\n", local->name);
    } else
        xf86MsgVerb(X_WARNING, 4, "%s: libsysfs not found\n", local->name);

#endif

    if (!wizardpen_name)
        wizardpen_name = wizardpen_name_default;

    xf86MsgVerb(X_INFO, verb, "%s: probing event devices for WizardPen tablets\n", local->name);
    for (i = 0; i<50; i++) {
        int fd = -1;
	      Bool is_wizardpen;

        np = SET_EVENT_NUM(fname, i);
        if (np < 0 || np >= EV_DEV_NAME_MAXLEN) {
            xf86MsgVerb(X_WARNING, verb, "%s: too many devices, giving up %d\n", local->name, i);
            break;
        }
        SYSCALL(fd = open(fname, O_RDONLY));
        if (fd < 0) {
            if (errno == ENOENT) {
                if (++noent_cnt >= max_skip)
                    break;
                else
                    continue;
            } else {
                continue;
            }
        }
        have_evdev = TRUE;
        is_wizardpen = fd_query_wizardpen(fd, wizardpen_name);
        SYSCALL(close(fd));
        if (is_wizardpen) {
            goto ProbeFound;
        }
    }
    xf86MsgVerb(X_WARNING, verb, "%s: no WizardPen event device found (checked %d nodes, no device name started with '%s')\n",
            local->name, i + 1, wizardpen_name);
    if (i <= max_skip)
        xf86MsgVerb(X_WARNING, verb, "%s: The /dev/input/event* device nodes seem to be missing\n",
                local->name);
    if (i > max_skip && !have_evdev)
        xf86MsgVerb(X_WARNING, verb, "%s: The evdev kernel module seems to be missing\n", local->name);
    return FALSE;

ProbeFound:
    xf86Msg(X_PROBED, "%s auto-dev sets device to %s\n",
            local->name, fname);
    xf86ReplaceStrOption(local->options, "Device", fname);
    return TRUE;
}

#endif

static InputInfoPtr
WizardPenPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    LocalDevicePtr local = xf86AllocateInput(drv, 0);
    WizardPenPrivatePtr priv = xcalloc (1, sizeof(WizardPenPrivateRec));
    int speed;
    int msgtype;
    char *s;

    if ((!local) || (!priv))
        goto SetupProc_fail;

    memset(priv, 0, sizeof(WizardPenPrivateRec));

    local->name = dev->identifier;
    local->type_name = "WizardPen Tablet";
    local->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
    local->control_proc = NULL;
    local->close_proc = CloseProc;
    local->switch_mode = NULL;
    local->conversion_proc = ConvertProc;
    local->reverse_conversion_proc = ReverseConvertProc;
    local->dev = NULL;
    local->private = priv;
    local->private_flags = 0;
    local->conf_idev = dev;
    local->device_control = DeviceControl;
    /*local->always_core_feedback = 0;*/
    xf86CollectInputOptions(local, default_options, NULL);

    xf86OptionListReport(local->options);

    priv->wizardpenInc = xf86SetIntOption(local->options, "Increment", 0 );

    /* deprecated box settings, Z axis */
    s = xf86FindOptionValue(local->options, "ClickPressureLevel");
    if (s && strlen(s) > 0) {
	xf86Msg(X_WARNING, "%s: Option \"ClickPressureLevel\" "
	    "is DEPRECATED, please use \"TopZ\"\n", local->name);
	priv->topZ = atoi(s);
	xf86Msg(X_CONFIG, "%s: TopZ = %d\n", local->name, priv->topZ);
    }
    /* end of deprecated box settings, Z axis */

    /* box settings */
    s = xf86FindOptionValue(local->options, "TopX");
    if (s && strlen(s) > 0) {
	priv->topX = atoi(s);
	xf86Msg(X_CONFIG, "%s: TopX = %d\n", local->name, priv->topX);
    }

    s = xf86FindOptionValue(local->options, "TopY");
    if (s && strlen(s) > 0) {
	priv->topY = atoi(s);
	xf86Msg(X_CONFIG, "%s: TopY = %d\n", local->name, priv->topY);
    }

    s = xf86FindOptionValue(local->options, "TopZ");
    if (s && strlen(s) > 0) {
	priv->topZ = atoi(s);
	xf86Msg(X_CONFIG, "%s: TopZ = %d\n", local->name, priv->topZ);
    }

    s = xf86FindOptionValue(local->options, "BottomX");
    if (s && strlen(s) > 0) {
	priv->bottomX = atoi(s);
	xf86Msg(X_CONFIG, "%s: BottomX = %d\n", local->name, priv->bottomX);
    }

    s = xf86FindOptionValue(local->options, "BottomY");
    if (s && strlen(s) > 0) {
	priv->bottomY = atoi(s);
	xf86Msg(X_CONFIG, "%s: BottomY = %d\n", local->name, priv->bottomY);
    }

    s = xf86FindOptionValue(local->options, "BottomZ");
    if (s && strlen(s) > 0) {
	priv->bottomZ = atoi(s);
	xf86Msg(X_CONFIG, "%s: BottomZ = %d\n", local->name, priv->bottomZ);
    }
    /* end of box settings */

    // increment setting
    priv->wizardpenInc = xf86SetIntOption(local->options, "Increment", 0);
    s = xf86FindOptionValue(local->options, "Device");
    if (!s || (s && (xf86NameCmp(s, "auto-dev") == 0))) {
#ifdef LINUX_INPUT
        priv->flags |= AUTODEV_FLAG;
        if (!WizardPenAutoDevProbe(local, 0))
        {
            xf86Msg(X_ERROR, "%s: unable to find device\n", local->name);
            goto SetupProc_fail;
        }
#else
        xf86Msg(X_NOT_IMPLEMENTED, "%s: device autodetection not implemented, sorry\n", local->name);
        goto SetupProc_fail;
#endif
    }

    local->fd = xf86OpenSerial (local->options);
    if (local->fd == -1)
    {
        xf86Msg(X_ERROR, "%s: unable to open device\n", local->name);
        goto SetupProc_fail;
    }
    xf86ErrorFVerb( 6, "tty port opened successfully\n" );

#ifdef LINUX_INPUT
    if (IsUSBLine(local->fd)) {
        priv->flags |= USB_FLAG;

        local->read_input = USBReadInput;

        if (USBQueryHardware(local) != Success)
        {
            xf86Msg(X_ERROR, "%s: unable to query/initialize hardware (not an %s?).\n", local->name, local->type_name);
            goto SetupProc_fail;
        }
    } else
#endif

    s = xf86FindOptionValue(local->options, "Mode");
    msgtype = s ? X_CONFIG : X_DEFAULT;
    if (!(s && (xf86NameCmp(s, "relative") == 0)))
    {
        priv->flags |= ABSOLUTE_FLAG;
    }

    xf86Msg(msgtype, "%s is in %s mode\n", local->name, (priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");
   // DBG (9, XisbTrace (priv->buffer, 1));

    local->history_size = xf86SetIntOption(local->options , "HistorySize", 0);

    xf86ProcessCommonOptions(local, local->options);

    local->flags |= XI86_CONFIGURED;

    if (local->fd != -1)
    {
        RemoveEnabledDevice (local->fd);
        if (priv->buffer)
        {
            XisbFree(priv->buffer);
            priv->buffer = NULL;
        }
        xf86CloseSerial(local->fd);
    }
    RemoveEnabledDevice (local->fd);
    local->fd = -1;
    return local;

    /*
     * If something went wrong, cleanup and return NULL
     */
SetupProc_fail:
    if ((local) && (local->fd))
        xf86CloseSerial (local->fd);
    if ((priv) && (priv->buffer))
        XisbFree (priv->buffer);
    if (priv)
        xfree (priv);
    return NULL;
}

static Bool
DeviceControl (DeviceIntPtr dev, int mode)
{
    Bool RetValue;

    switch (mode)
    {
        case DEVICE_INIT:
            DeviceInit(dev);
            RetValue = Success;
            break;
        case DEVICE_ON:
            RetValue = DeviceOn(dev);
            break;
        case DEVICE_OFF:
            RetValue = DeviceOff(dev);
            break;
        case DEVICE_CLOSE:
            RetValue = DeviceClose(dev);
            break;
        default:
            RetValue = BadValue;
    }

    return RetValue;
}

static Bool
DeviceOn (DeviceIntPtr dev)
{
    char buffer[256];
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
	int err =0;

    xf86MsgVerb(X_INFO, 4, "%s Device On\n", local->name);

    local->fd = xf86OpenSerial(local->options);
    if (local->fd == -1)
    {
        xf86Msg(X_WARNING, "%s: cannot open input device %s: %s\n", local->name, xf86FindOptionValue(local->options, "Device"), strerror(errno));
        priv->flags &= ~AVAIL_FLAG;
#ifdef LINUX_INPUT
        if ((priv->flags & AUTODEV_FLAG) && WizardPenAutoDevProbe(local, 4))
            local->fd = xf86OpenSerial(local->options);
        if (local->fd == -1)
#endif
            return !Success;
    }
    priv->flags |= AVAIL_FLAG;


    if (!(priv->flags & USB_FLAG)) {
        priv->buffer = XisbNew(local->fd, 200);
#ifdef EVIOCGRAB
	/* Try to grab the event device so that data don't leak to /dev/input/mice */
	SYSCALL(err = ioctl(local->fd, EVIOCGRAB, (pointer)1));

	if (err < 0) 
		ErrorF("%s WizardPen driver can't grab event device, errno=%d\n",
				local->name, errno);
	else 
		ErrorF("%s WizardPen driver grabbed event device\n", local->name);
#endif
        if (!priv->buffer)
        {
            xf86CloseSerial(local->fd);
            local->fd = -1;
            return !Success;
        }

        /* Rets qu'a l'envoyer a la tablette */
        sprintf(buffer, "%s%c%c%c%c", wizardpen_initstr, priv->wizardpenReportSpeed, WIZARDPEN_INCREMENT, 32 + priv->wizardpenInc, (priv->flags & ABSOLUTE_FLAG)? WIZARDPEN_ABSOLUTE: WIZARDPEN_RELATIVE);
        XisbWrite (priv->buffer, (unsigned char *)buffer, strlen(buffer));
    }

    xf86FlushInput(local->fd);
    xf86AddEnabledDevice (local);
    dev->public.on = TRUE;
    return Success;
}

static Bool
DeviceOff (DeviceIntPtr dev)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    xf86MsgVerb(X_INFO, 4, "%s Device Off\n", local->name);

    if (local->fd != -1)
    {
        RemoveEnabledDevice (local->fd);
        if (priv->buffer)
        {
            XisbFree(priv->buffer);
            priv->buffer = NULL;
        }
        xf86CloseSerial(local->fd);
    }


    xf86RemoveEnabledDevice (local);
    dev->public.on = FALSE;
    return Success;
}

static Bool
DeviceClose (DeviceIntPtr dev)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;

    xf86MsgVerb(X_INFO, 4, "%s Device Close\n", local->name);

    return Success;
}

static void
ControlProc(DeviceIntPtr dev, PtrCtrl *ctrl)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;

    xf86MsgVerb(X_INFO, 4, "%s Control Proc\n", local->name);
}

static Bool
DeviceInit (DeviceIntPtr dev)
{
    int rx, ry;
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
    priv->wizardpenOldX = 0;
	priv->wizardpenOldY = 0;
	priv->wizardpenOldZ = 0;
	unsigned char map[] =
    {0, 1, 2, 3};
    xf86MsgVerb(X_INFO, 4, "%s Init\n", local->name);

    /* 3 buttons */
    if (InitButtonClassDeviceStruct (dev, 3, map) == FALSE)
    {
        xf86Msg(X_ERROR, "%s: unable to allocate ButtonClassDeviceStruct\n", local->name);
        return !Success;
    }

    if (InitFocusClassDeviceStruct (dev) == FALSE)
    {
        xf86Msg(X_ERROR, "%s: unable to allocate FocusClassDeviceStruct\n", local->name);
        return !Success;
    }

    if (InitPtrFeedbackClassDeviceStruct(dev, ControlProc) == FALSE) {
        xf86Msg(X_ERROR, "%s: unable to init ptr feedback\n", local->name);
        return !Success;
    }


    /* 3 axes */
    if (InitValuatorClassDeviceStruct (dev, 3, xf86GetMotionEvents,
                local->history_size,
                ((priv->flags & ABSOLUTE_FLAG)? Absolute: Relative)|OutOfProximity)
            == FALSE)
    {
        xf86Msg(X_ERROR, "%s: unable to allocate ValuatorClassDeviceStruct\n", local->name);
        return !Success;
    }
    else
    {

        InitValuatorAxisStruct(dev,
                0,
                0,			/* min val */
                screenInfo.screens[0]->width,	/* max val */
                1000,			/* resolution */
                0,			/* min_res */
                1000);			/* max_res */
        InitValuatorAxisStruct(dev,
                1,
                0,			/* min val */
                screenInfo.screens[0]->height,	/* max val */
                1000,			/* resolution */
                0,			/* min_res */
                1000);			/* max_res */
        InitValuatorAxisStruct(dev,
                2,
                0,			/* min val */
                priv->bottomZ,	/* max val */
                1000,			/* resolution */
                0,			/* min_res */
                1000);		/* max_res */

    }

    if (InitProximityClassDeviceStruct (dev) == FALSE)
    {
        xf86Msg(X_ERROR, "%s: unable to allocate ProximityClassDeviceStruct\n", local->name);
        return !Success;
    }

    xf86MotionHistoryAllocate (local);


    /* On ne peut pas calculer l'increment avant, faute d'ecran pour
       connaitre la taille... */

    if (priv->wizardpenInc > 95)
        priv->wizardpenInc = 95;
    if (priv->wizardpenInc < 1)
    {
        /* guess the best increment value given video mode */
        rx = priv->wizardpenMaxX / screenInfo.screens[0]->width;
        ry = priv->wizardpenMaxY / screenInfo.screens[0]->height;
        if (rx < ry)
            priv->wizardpenInc = rx;
        else
            priv->wizardpenInc = ry;
        if (priv->wizardpenInc < 1)
            priv->wizardpenInc = 1;
    }

    xf86Msg(X_INFO, "%s Increment: %d\n", local->name, priv->wizardpenInc);

    return Success;
}

#ifdef LINUX_INPUT
#define set_bit(byte,nb,bit)	(bit ? byte | (1<<nb) : byte & (~(1<<nb)))
static void
USBReadInput (LocalDevicePtr local)
{
    int len;
    struct input_event * event;
    char eventbuf[sizeof(struct input_event) * MAX_EVENTS];
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
    int x = priv->wizardpenOldX;
    int y = priv->wizardpenOldY;
    int z = priv->wizardpenOldZ;
	int report_x,report_y;
	int prox = priv->wizardpenOldProximity;
    int buttons = priv->wizardpenOldButtons & 7;
    int is_core_pointer = 0;
    /* Is autodev active? */
    int autodev = priv->flags & AUTODEV_FLAG;
    /* Was the device available last time we checked? */
    int avail = priv->flags & AVAIL_FLAG;

    SYSCALL(len = read(local->fd, eventbuf, sizeof(eventbuf)));

    if (len <= 0) {
        if (avail) {
			xf86Msg(X_ERROR, "%s: error reading device %s: %s\n", local->name, xf86FindOptionValue(local->options, "Device"), strerror(errno));
        }
        if (NOTAVAIL) {
            priv->flags &= ~AVAIL_FLAG;
            if(autodev) {
                WizardPenAutoDevProbe(local, 4);{
                    DeviceOff(local->dev);
                    DeviceOn(local->dev);
				}
              
            }
        }
        return;
    } else {
        if (!avail) {
           // If the device wasn't available last time we checked 
            xf86Msg(X_INFO, "%s: device %s is available again\n", local->name, xf86FindOptionValue(local->options, "Device"));
            priv->flags |= AVAIL_FLAG;
        }
    }

    for (event = (struct input_event *)eventbuf;
            event < (struct input_event *)(eventbuf+len); event++) 
				{
	         switch (event->type) {
            case EV_SYN:// 2.6.x 
                if (event->code != SYN_REPORT && event->code != 0)
                    xf86Msg(X_ERROR, "%s: unknown EV_SYN code %d\n", local->name, event->code);
                break;
            case EV_ABS:
                switch (event->code) {
                    case ABS_X:
						x = event->value;
                    break;	
					case ABS_Z:    
						x = event->value;
                    break;
					case ABS_RX:
                        y = event->value;
                    break;
                    case ABS_Y:
                        y = event->value;
                    break;

                    case ABS_PRESSURE:
                    	z = event->value;
                    break;

                    case ABS_MISC:
                        break;

                }
                break; // EV_ABS 
			case EV_REL:
				switch(event->code){
					case REL_X:
						x = x + event->value*(priv->bottomX - priv->topX)/screenInfo.screens[0]->width;
						break;
					case REL_Y:
						y = y + event->value*(priv->bottomY - priv->topY)/screenInfo.screens[0]->width;
						break;
					case REL_WHEEL:
						if (event->value>0)
							buttons = set_bit(buttons,3,1);
						if (event->value<0)
							buttons = set_bit(buttons,4,1);
						break;
				}
					
            case EV_KEY:
                switch (event->code) {
                    case BTN_TOOL_PEN:
                        prox = event->value;
                        break;

                    case BTN_LEFT:
					case BTN_SIDE:
                        buttons = set_bit(buttons,0,event->value);
                        break;

					case BTN_EXTRA:
					case BTN_MIDDLE:
                    	buttons = set_bit(buttons,1,event->value);
                    	break;

					case BTN_FORWARD:
                    case BTN_RIGHT:
                        buttons = set_bit(buttons,2,event->value);
                        break;
                }
                break; // EV_KEY 
				case EV_MSC:
					 break;
				default:
                xf86Msg(X_ERROR, "%s: unknown event type/code %d/%d\n", local->name, event->type, event->code);
        } /* switch event->type */

        /* Linux Kernel 2.6.x sends EV_SYN/SYN_REPORT as an event terminator,
         * whereas 2.4.x sends EV_ABS/ABS_MISC. We have to support both.
         */
        if (!(  (event->type == EV_SYN && event->code == SYN_REPORT) ||
                    (event->type == EV_ABS && event->code == ABS_MISC)
             )) {
            continue;
        }
	if(x>priv->topX && x<priv->bottomX)
    	report_x = (x-priv->topX) * screenInfo.screens[0]->width / (priv->bottomX-priv->topX);
	else{
		if(x<priv->topX)
			report_x = 0;
		else
			report_x = screenInfo.screens[0]->width;
	}
	if(y>priv->topY && y<priv->bottomY)
    	report_y = (y-priv->topY) * screenInfo.screens[0]->height / (priv->bottomY-priv->topY);
	else{
		if(y<priv->topY)
			report_y = 0;
		else
			report_y = screenInfo.screens[0]->height;
	}
		if (1)
        {
            if (!(priv->wizardpenOldProximity))
                if (!is_core_pointer)
                {
                    xf86PostProximityEvent(local->dev, 1, 0, 3 , report_x, report_y, z);
                }


            xf86PostMotionEvent(local->dev, 1, 0, 3, report_x, report_y, z);
            if (priv->wizardpenOldButtons != buttons)
            {
                int delta = buttons ^ priv->wizardpenOldButtons;
                while (delta)
                {
                    int id = ffs(delta);
                    delta &= ~(1 << (id-1));
                    xf86PostButtonEvent(local->dev, 1, id, (buttons&(1<<(id-1))), 0, 3, report_x, report_y,z);
                }
            }
        }
        else
        {
            if (!is_core_pointer)
                if (priv->wizardpenOldProximity)
                {
                    xf86PostProximityEvent(local->dev, 0, 0, 3, report_x,report_y,z);
                }
            priv->wizardpenOldProximity = 0;
        }

        priv->wizardpenOldButtons = buttons;
        priv->wizardpenOldX = x;
        priv->wizardpenOldY = y;
        priv->wizardpenOldZ = z;
        priv->wizardpenOldProximity = prox;
    }
    /*xf86Msg(X_INFO, "ACECAD Tablet Sortie Read Input\n");*/
}
#endif

static void
CloseProc (LocalDevicePtr local)
{
}

/*
 * The ConvertProc function may need to be tailored for your device.
 * This function converts the device's valuator outputs to x and y coordinates
 * to simulate mouse events.
 */
static Bool
ConvertProc (LocalDevicePtr local, int first, int num,
        int v0, int v1, int v2, int v3, int v4, int v5, int *x, int *y)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr)(local->private);
	if(v0>priv->topX && v0<priv->bottomX)
    	*x = (v0-priv->topX) * screenInfo.screens[0]->width / (priv->bottomX-priv->topX);
	else{
		if(v0<priv->topX)
			*x = priv->topX;
		else
			*x = priv->bottomX;
	}
	if(v1>priv->topY && v1<priv->bottomY)
    	*y = (v1-priv->topY) * screenInfo.screens[0]->width / (priv->bottomY-priv->topY);
	else{
		if(v1<priv->topY)
			*y = priv->topY;
		else
			*y = priv->bottomY;
	}
    return TRUE;
}


static Bool
ReverseConvertProc (LocalDevicePtr local,
        int x, int  y,
        int *valuators)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr)(local->private);

    valuators[0] = (x * (priv->bottomX-priv->topX) / screenInfo.screens[0]->width) + priv->topX;
    valuators[1] = (y * (priv->bottomY-priv->topY) / screenInfo.screens[0]->height) + priv->topY;

    return TRUE;
}


#define WriteString(str)\
    XisbWrite (priv->buffer, (unsigned char *)(str), strlen(str))


static Bool
QueryHardware (WizardPenPrivatePtr priv)
{

    /* Reset */
    WriteString("z0");

    /* Wait */
    milisleep (250);

    /* Prompt Mode in order to not be disturbed */
    WriteString(WIZARDPEN_PROMPT_MODE);

    /* Flush */
    while (XisbRead(priv->buffer) >= 0);

    /* Ask for Config packet*/
    WriteString(WIZARDPEN_CONFIG);

    /* Read the packet */
    XisbBlockDuration (priv->buffer, 1000000);
    NewPacket (priv);

    /*xf86Msg(X_CONFIG, "ACECAD Tablet init envoyé \n");*/

    if ((WizardPenGetPacket (priv) == Success))
    {
        priv->wizardpenMaxX = (int)priv->packet[1] + ((int)priv->packet[2] << 7);
        priv->wizardpenMaxY = (int)priv->packet[3] + ((int)priv->packet[4] << 7);
        priv->wizardpenMaxZ = 512;
        xf86Msg(X_PROBED, "Wizardpen Tablet MaxX:%d MaxY:%d\n", priv->wizardpenMaxX, priv->wizardpenMaxY);
    }
    else
        return !Success;

    /*xf86Msg(X_INFO, "ACECAD Tablet query hardware fini \n");*/
    return Success;
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)

#ifdef LINUX_INPUT
static Bool
USBQueryHardware (LocalDevicePtr local)
{
    WizardPenPrivatePtr	priv = (WizardPenPrivatePtr) local->private;
    unsigned long	bit[EV_MAX][NBITS(KEY_MAX)];
    int			i, j;
    int			abs[5];
    char		name[256] = "Unknown";

    ioctl(local->fd, EVIOCGNAME(sizeof(name)), name);
    xf86MsgVerb(X_PROBED, 4, "Kernel Input device name: \"%s\"\n", name);

    memset(bit, 0, sizeof(bit));
    ioctl(local->fd, EVIOCGBIT(0, EV_MAX), bit[0]);

    for (i = 0; i < EV_MAX; i++)
        if (test_bit(i, bit[0])) {
            ioctl(local->fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
            for (j = 0; j < KEY_MAX; j++)
                if (test_bit(j, bit[i])) {
                    if (i == EV_ABS) {
                        ioctl(local->fd, EVIOCGABS(j), abs);
                        switch (j) {
                            case ABS_X:
                                if(!priv->bottomX)
											  priv->bottomX = abs[2];
                                break;

                            case ABS_Y:
										  if(!priv->bottomY)
                                		priv->bottomY = abs[2];
                                break;

                            case ABS_PRESSURE:
										  if(!priv->bottomZ)
                                		priv->bottomZ = abs[2];
                                break;
                        }
                    }
                }
        }

    xf86Msg(X_PROBED, "Wizardpen Tablet MaxX:%d MaxY:%d MaxZ:%d\n", priv->wizardpenMaxX, priv->wizardpenMaxY, priv->wizardpenMaxZ);
    return Success;
}
#endif

static void
NewPacket (WizardPenPrivatePtr priv)
{
    priv->packeti = 0;
}

static Bool
WizardPenGetPacket (WizardPenPrivatePtr priv)
{
    int count = 0;
    int c = 0;

    while((c = XisbRead(priv->buffer)) >= 0 )
    {

        /*
         * fail after 500 bytes so the server doesn't hang forever if a
         * device sends bad data.
         */
        if (count++ > 500)
        {
            NewPacket (priv);
            return !Success;
        }

        if (c & PHASING_BIT)
        {
            NewPacket(priv);

            /*xf86Msg(X_CONFIG, "Push %2.2x\n",(char) c);*/
            XisbBlockDuration (priv->buffer, 10000);
            priv->packet[priv->packeti++] = c;
            count = WIZARDPEN_PACKET_SIZE - 1;
            while (count-- && (c = XisbRead(priv->buffer)) >= 0)
            {
                /*xf86Msg(X_INFO, "Push %2.2x\n",(char) c);*/
                priv->packet[priv->packeti++] = c;
            }
            XisbBlockDuration (priv->buffer, 0);
            if(c > 0)
                return Success;
        }
    }
    return !Success;
}
