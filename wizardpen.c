/* 
 * Copyright (c) 2005 Jan Horak, Nickolay Kondrashov
 *
 * Copyright (c) 2001 Edouard TISSERANT <edouard.tisserant@wanadoo.fr>
 * Parts inspired from Shane Watts <shane@bofh.asn.au> XFree86 3 AceCat Driver
 * Thanks to Emily, from AceCat, For giving me documents.
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

#define _WIZARDPEN_C_
/*****************************************************************************
 *	Standard Headers
 ****************************************************************************/

#ifdef LINUX_INPUT
#include <asm/types.h>
#include <linux/input.h>
#ifdef BUS_PCI
#undef BUS_PCI
#endif
#ifdef BUS_ISA
#undef BUS_ISA
#endif
#endif

#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xisb.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <mipointer.h>
#include <xf86Module.h>

/*****************************************************************************
 *	Local Headers
 ****************************************************************************/
#include "wizardpen.h"

/******************************************************************************
 * debugging macro
 *****************************************************************************/
#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

static int debug_level = 0;

#define DEBUG 1
#if DEBUG
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}
#else
#define DBG(lvl, f)
#endif

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables
 ****************************************************************************/

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))
#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))

/* max number of input events to read in one read call */
#define MAX_EVENTS 64

/* This holds the input driver entry and module information. */
InputDriverRec WIZARDPEN = {
    4,				       // driver version
    "wizardpen",		       // driver name
    NULL,			       // (*Identify)(int flags)
    WizardPenPreInit,		       // preinit
    NULL,			       // uninit 
    NULL,			       // module;
    0				       // refCount;
};

#ifdef XFree86LOADER
/* This structure is expected to be returned by the initfunc */
static XF86ModuleVersionInfo VersionRec = {
    "wizardpen",		       /* name of module, e.g. "foo" */
    MODULEVENDORSTRING,		       /* vendor specific string */
    MODINFOSTRING1,		       /* constant MODINFOSTRING1/2 to find */
    MODINFOSTRING2,		       /* infoarea with a binary editor or sign tool */
    XORG_VERSION_CURRENT,	       /* contains XF86_VERSION_CURRENT */
    0, 4, 1,			       /* module-specific major, minor, patch version */
    ABI_CLASS_XINPUT,		       /* ABI class that the module uses */
    ABI_XINPUT_VERSION,		       /* ABI version */
    MOD_CLASS_XINPUT,		       /* module class description */
    {0, 0, 0, 0}		       /* contains a digital signature of the */
    /* version info structure */
};

/* XF86ModuleVersionInfo, 
 * func ModuleSetupProc run after sucessfully loaded ,
 * func ModuleTearDownProc run after unloaded*/
XF86ModuleData wizardpenModuleData = { &VersionRec, SetupProc, TearDownProc };

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static pointer
SetupProc(pointer module, pointer options, int *errmaj, int *errmin)
{
    DBG(1, ErrorF("WizardPen SetupProc\n"));
    /*
     * The input driver's Setup function calls xf86AddInputDriver() to register the
     * driver's InputDriverRec, which contains a small set of essential details and
     * driver entry points required during the early phase of InitInput().
     * xf86AddInputDriver() adds it to the global xf86InputDriverList[] array. For
     * a static server, the xf86InputDriverList[] array is initialised at build
     * time.
     */
    xf86AddInputDriver(&WIZARDPEN, module, 0);
    return module;
}

static void
TearDownProc(pointer p)
{
    DBG(1, ErrorF("WizardPen TearDownProc\n"));
#if 0
    LocalDevicePtr local = (LocalDevicePtr) p;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) local->private;

    DeviceOff(local->dev);

    xf86CloseSerial(local->fd);
    XisbFree(priv->buffer);
    xfree(priv);
    xfree(local->name);
    xfree(local);
#endif
}
#endif

/* WizardPen buttons scheme as is */
static int wizardpenButtonMap[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

/* Maps to standard mouse scheme (at least for a1212) */
static int standardButtonMap[8] = { 0, 1, 4, 3, 2, 5, 6, 7 };

static const char *default_options[] = {
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
	DBG(1, ErrorF("Kernel Input driver version is %d.%d.%d\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff));
	return 1;
    } else {
	return 0;
    }
}
#endif

static InputInfoPtr
WizardPenPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    LocalDevicePtr local = xf86AllocateInput(drv, 0);
    WizardPenPrivatePtr priv = xcalloc(1, sizeof(WizardPenPrivateRec));
    int speed;
    char *s;

    DBG(1, ErrorF("WizardPen PreInit\n"));

    if ((!local) || (!priv)) {
	goto SetupProc_fail;
    }

    memset(priv, 0, sizeof(WizardPenPrivateRec));

    local->name = dev->identifier;
    local->type_name = "WizardPen Tablet";
    local->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
    local->motion_history_proc = xf86GetMotionEvents;
    local->control_proc = NULL;
    local->close_proc = CloseProc;
    local->switch_mode = SwitchMode;
    local->conversion_proc = ConvertProc;
    local->reverse_conversion_proc = ReverseConvertProc;
    local->dev = NULL;
    local->private = priv;
    local->private_flags = 0;
    local->conf_idev = dev;
    local->device_control = DeviceControl;
    /*local->always_core_feedback = 0; */

    /* xf86CollectInputOptions collects the options for an InputDevice. */
    xf86CollectInputOptions(local, default_options, NULL);

    xf86OptionListReport(local->options);

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

    // port setting
    local->fd = xf86OpenSerial(local->options);
    if (local->fd == -1) {
	xf86Msg(X_ERROR,
	    "%s: WizardPen driver unable to open device\n", local->name);
	goto SetupProc_fail;
    }
    xf86Msg(X_CONFIG, "%s: tty port opened successfully\n", local->name);

    // model setting
    s = xf86FindOptionValue(local->options, "Model");
    if (s && (xf86NameCmp(s, "Flair") == 0)) {
	priv->packet_size = FLAIR_PACKET_SIZE;
    } else {
	if (s && (xf86NameCmp(s, "A-Series") == 0)) {
	    priv->packet_size = A_SERIES_PACKET_SIZE;
	} else {
	    if (s) {
		xf86Msg(X_ERROR,
		    "%s: invalid WizardPen model "
		    "(should be Flair or A-Series).\n", local->name);
		goto SetupProc_fail;
	    }
	    /* this should work anyway */
	    priv->packet_size = FLAIR_PACKET_SIZE;
	}
    }
    xf86Msg(X_CONFIG,
	(s ? "%s: WizardPen model is %s\n" :
	    "%s: WizardPen model is unspecified, defaults to %s\n"),
	local->name, (s ? s : "Flair")
	);
    // end model setting

    // mode setting
    s = xf86FindOptionValue(local->options, "Mode");
    if (s && (xf86NameCmp(s, "Relative") == 0)) {
	priv->flags = priv->flags & ~ABSOLUTE_FLAG;
    } else {
	priv->flags = priv->flags | ABSOLUTE_FLAG;
    }
    xf86Msg(X_CONFIG,
	"%s: WizardPen is in %s mode\n",
	local->name, (priv->flags & ABSOLUTE_FLAG) ? "absolute" : "relative");
    // end mode setting

    // query hardware
#ifdef LINUX_INPUT
    if (IsUSBLine(local->fd)) {
	priv->wizardpenUSB = 1;

	local->read_input = USBReadInput;

	if (USBQueryHardware(local) != Success) {
	    ErrorF("%s: Unable to query/initialize "
		"WizardPen hardware.\n", local->name);
	    goto SetupProc_fail;
	}
    } else
#endif
    {
	priv->wizardpenUSB = 0;

	local->read_input = ReadInput;

	speed = xf86SetIntOption(local->options, "ReportSpeed", 85);

	switch (speed) {
	case 120:
	    priv->wizardpenReportSpeed = 'Q';
	    break;
	case 85:
	    priv->wizardpenReportSpeed = 'R';
	    break;
	case 10:
	    priv->wizardpenReportSpeed = 'S';
	    break;
	case 2:
	    priv->wizardpenReportSpeed = 'T';
	    break;
	default:
	    priv->wizardpenReportSpeed = 'R';
	    speed = 85;
	    xf86Msg(X_CONFIG,
		"%s: WizardPen ReportSpeed possible "
		"values: 120, 85, 10, 2\n", local->name);
	}

	xf86Msg(X_CONFIG,
	    "%s: WizardPen reports %d points/s\n", local->name, speed);

	priv->buffer = XisbNew(local->fd, 200);

	/* 
	 * Verify that hardware is attached and functional
	 */
	if (QueryHardware(priv) != Success) {
	    xf86Msg(X_ERROR,
		"%s: Unable to query/initialize "
		"WizardPen hardware.\n", local->name);
	    goto SetupProc_fail;
	}
    }

    // Max[XYZ] settings

    /* override wizardpenMaxX, wizardpenMaxY, wizardpenMaxZ */
    s = xf86FindOptionValue(local->options, "MaxX");
    if (s && strlen(s) > 0) {
	priv->wizardpenMaxX = atoi(s);

	/* deprecated options for X axis */
	s = xf86FindOptionValue(local->options, "BorderLeft");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderLeft\" "
		"is DEPRECATED, please use \"TopX\". "
		"Assuming MaxX was given old-style.\n", local->name);
	    priv->topX = atoi(s);
	    /* assuming MaxX was given old-style */
	    priv->wizardpenMaxX = priv->wizardpenMaxX + priv->topX;
	    xf86Msg(X_CONFIG, "%s: TopX = %d\n", local->name, priv->topX);
	}

	s = xf86FindOptionValue(local->options, "BorderRight");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderRight\" "
		"is DEPRECATED, please use \"BottomX\". IGNORED.\n",
		local->name);
	    /* ignoring, because MaxX is set */
	}
	/* end of deprecated options for X axis */

	xf86Msg(X_CONFIG,
	    "%s: MaxX is overriden by user to %d\n",
	    local->name, priv->wizardpenMaxX);

    } else {

	/* deprecated options for X axis */
	s = xf86FindOptionValue(local->options, "BorderLeft");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderLeft\" "
		"is DEPRECATED, please use \"TopX\"\n", local->name);
	    priv->topX = atoi(s);
	    xf86Msg(X_CONFIG, "%s: TopX = %d\n", local->name, priv->topX);
	}

	s = xf86FindOptionValue(local->options, "BorderRight");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderRight\" "
		"is DEPRECATED, please use \"BottomX\"\n", local->name);
	    priv->bottomX = priv->wizardpenMaxX - atoi(s);
	    xf86Msg(X_CONFIG, "%s: BottomX = %d\n", local->name,
		priv->bottomX);
	}
	/* end of deprecated options for X axis */

    }

    s = xf86FindOptionValue(local->options, "MaxY");
    if (s && strlen(s) > 0) {
	priv->wizardpenMaxY = atoi(s);

	/* deprecated options for Y axis */
	s = xf86FindOptionValue(local->options, "BorderTop");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderTop\" "
		"is DEPRECATED, please use \"TopY\". "
		"Assuming MaxY was given old-style.\n", local->name);
	    priv->topY = atoi(s);
	    /* assuming MaxY was given old-style */
	    priv->wizardpenMaxY = priv->wizardpenMaxY + priv->topY;
	    xf86Msg(X_CONFIG, "%s: TopY = %d\n", local->name, priv->topY);
	}

	s = xf86FindOptionValue(local->options, "BorderBottom");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderBottom\" "
		"is DEPRECATED, please use \"BottomY\". IGNORED.\n",
		local->name);
	    /* ignoring, because MaxY is set */
	}
	/* end of deprecated options for X axis */

	xf86Msg(X_CONFIG,
	    "%s: MaxY is overriden by user to %d\n",
	    local->name, priv->wizardpenMaxY);
    } else {
	/* deprecated options for Y axis */
	s = xf86FindOptionValue(local->options, "BorderTop");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderTop\" "
		"is DEPRECATED, please use \"TopY\"\n", local->name);
	    priv->topY = atoi(s);
	    xf86Msg(X_CONFIG, "%s: TopY = %d\n", local->name, priv->topY);
	}

	s = xf86FindOptionValue(local->options, "BorderBottom");
	if (s && strlen(s) > 0) {
	    xf86Msg(X_WARNING, "%s: Option \"BorderBottom\" "
		"is DEPRECATED, please use \"BottomY\"\n", local->name);
	    priv->bottomY = priv->wizardpenMaxY - atoi(s);
	    xf86Msg(X_CONFIG, "%s: BottomY = %d\n", local->name,
		priv->bottomY);
	}
	/* end of deprecated options for X axis */

    }

    s = xf86FindOptionValue(local->options, "MaxZ");
    if (s && strlen(s) > 0) {
	priv->wizardpenMaxZ = atoi(s);
	xf86Msg(X_CONFIG,
	    "%s: MaxZ is overriden by user to %d\n",
	    local->name, priv->wizardpenMaxZ);
    }
    // end of Max[XYZ] settings

    /* deprecated box settings, bottom values */
    s = xf86FindOptionValue(local->options, "BorderRight");
    if (s && strlen(s) > 0) {
	xf86Msg(X_CONFIG, "%s: WARNING! Option \"BorderRight\" "
	    "is DEPRECATED, please use \"BottomX\"\n");
	priv->wizardpenMaxX = priv->wizardpenMaxX - atoi(s);
	priv->bottomX = priv->wizardpenMaxX;

	xf86Msg(X_CONFIG, "%s: BottomX, MaxX = %d\n", local->name,
	    priv->bottomX);

    }

    s = xf86FindOptionValue(local->options, "BorderTop");
    if (s && strlen(s) > 0) {
	xf86Msg(X_CONFIG, "%s: WARNING! Option \"BorderTop\" "
	    "is DEPRECATED, please use \"TopY\"\n", local->name);
	priv->topY = atoi(s);
	xf86Msg(X_CONFIG, "%s: TopY = %d\n", local->name, priv->topY);
    }

    /* end of deprecated box settings, bottom values */

    // set default bottomX, bottomY and bottomZ if not set by user
    if (priv->bottomX == 0) {
	priv->bottomX = priv->wizardpenMaxX;
    }

    if (priv->bottomY == 0) {
	priv->bottomY = priv->wizardpenMaxY;
    }

    if (priv->bottomZ == 0) {
	priv->bottomZ = priv->wizardpenMaxZ;
    }
    // verify box validity
    if (priv->topX > priv->wizardpenMaxX || priv->topX < 0) {
	ErrorF("%s: invalid TopX (%d) resetting to 0\n",
	    local->name, priv->topX);
	priv->topX = 0;
    }

    if (priv->topY > priv->wizardpenMaxY || priv->topY < 0) {
	ErrorF("%s: invalid TopY (%d) resetting to 0\n",
	    local->name, priv->topY);
	priv->topY = 0;
    }

    if (priv->topZ > priv->wizardpenMaxZ || priv->topZ < 0) {
	ErrorF("%s: invalid TopZ (%d) resetting to 0\n",
	    local->name, priv->topZ);
	priv->topZ = 0;
    }

    if (priv->bottomX > priv->wizardpenMaxX || priv->bottomX < priv->topX) {
	ErrorF("%s: invalid BottomX (%d) resetting to %d\n",
	    local->name, priv->bottomX, priv->wizardpenMaxX);
	priv->bottomX = priv->wizardpenMaxX;
    }

    if (priv->bottomY > priv->wizardpenMaxY || priv->bottomY < priv->topY) {
	ErrorF("%s: invalid BottomY (%d) resetting to %d\n",
	    local->name, priv->bottomY, priv->wizardpenMaxY);
	priv->bottomY = priv->wizardpenMaxY;
    }

    if (priv->bottomZ > priv->wizardpenMaxZ || priv->bottomZ < priv->topZ) {
	ErrorF("%s: invalid BottomZ (%d) reseting to %d\n",
	    local->name, priv->bottomZ, priv->wizardpenMaxZ);
	priv->bottomZ = priv->wizardpenMaxZ;
    }
    // end of box validity verification

    // button map setting
    s = xf86FindOptionValue(local->options, "ButtonMap");
    if (s && (xf86NameCmp(s, "WizardPen") == 0)) {
	priv->buttonMap = wizardpenButtonMap;
    } else {
	if (s && (xf86NameCmp(s, "Standard") == 0)) {
	    priv->buttonMap = standardButtonMap;
	} else {
	    if (s) {
		xf86Msg(X_ERROR,
		    "%s: invalid ButtonMap "
		    "(should be WizardPen or Standard), "
		    "defaults to Standard.\n", local->name);
	    }
	    priv->buttonMap = standardButtonMap;
	}
    }
    xf86Msg(X_CONFIG,
	"%s: using %s button map\n",
	local->name,
	(xf86NameCmp(s, "WizardPen") == 0) ? "WizardPen" : "Standard");
    // end of button map settings

    // set no history
    local->history_size = xf86SetIntOption(local->options, "HistorySize", 0);

    // process common options
    xf86ProcessCommonOptions(local, local->options);

    // ok, we are configured now
    local->flags |= XI86_CONFIGURED;

    if (local->fd != -1) {
	RemoveEnabledDevice(local->fd);
	if (priv->buffer) {
	    XisbFree(priv->buffer);
	    priv->buffer = NULL;
	}
	xf86CloseSerial(local->fd);
    }
    RemoveEnabledDevice(local->fd);
    local->fd = -1;
    return (local);

    /* 
     * If something went wrong, cleanup and return NULL
     */
  SetupProc_fail:
    if ((local) && (local->fd))
	xf86CloseSerial(local->fd);
    if (local)
	xf86DeleteInput(local, 0);

    if ((priv) && (priv->buffer))
	XisbFree(priv->buffer);
    if (priv)
	xfree(priv);
    return (NULL);
}

static Bool
DeviceControl(DeviceIntPtr dev, int mode)
{
    Bool RetValue;

    DBG(1, ErrorF("WizardPen DeviceControl\n"));

    switch (mode) {
    case DEVICE_INIT:
	RetValue = DeviceInit(dev);
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

    return (RetValue);
}

static Bool
DeviceOn(DeviceIntPtr dev)
{
    char buffer[256];
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    xf86Msg(X_CONFIG, "WizardPen Device On\n");

    local->fd = xf86OpenSerial(local->options);
    if (local->fd == -1) {
	xf86Msg(X_WARNING, "%s: cannot open input device\n", local->name);
	return (!Success);
    }

    if (priv->wizardpenUSB == 0) {
	priv->buffer = XisbNew(local->fd, 200);
	if (!priv->buffer) {
	    xf86CloseSerial(local->fd);
	    local->fd = -1;
	    return (!Success);
	}

	/*Rets qu'a l'envoyer a la tablette */
	sprintf(buffer,
	    "%s%c%c%c%c",
	    wizardpen_initstr,
	    priv->wizardpenReportSpeed,
	    WIZARDPEN_INCREMENT,
	    32 + priv->wizardpenInc,
	    (priv->flags & ABSOLUTE_FLAG) ?
	    WIZARDPEN_ABSOLUTE : WIZARDPEN_RELATIVE);
	XisbWrite(priv->buffer, (unsigned char *)buffer, strlen(buffer));
    }

    xf86FlushInput(local->fd);
    xf86AddEnabledDevice(local);
    dev->public.on = TRUE;
    return (Success);
}

static Bool
DeviceOff(DeviceIntPtr dev)
{
    DBG(1, ErrorF("WizardPen DeviceOff\n"));

    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    if (local->fd != -1) {
	RemoveEnabledDevice(local->fd);
	if (priv->buffer) {
	    XisbFree(priv->buffer);
	    priv->buffer = NULL;
	}
	xf86CloseSerial(local->fd);
    }

    xf86RemoveEnabledDevice(local);
    dev->public.on = FALSE;
    return (Success);
}

static Bool
DeviceClose(DeviceIntPtr dev)
{
    DBG(1, ErrorF("WizardPen DeviceClose\n"));
    return (Success);
}

static void
ControlProc(DeviceIntPtr device, PtrCtrl * ctrl)
{
    DBG(1, ErrorF("WizardPen ControlProc\n"));
}

static Bool
DeviceInit(DeviceIntPtr dev)
{
    int rx, ry;
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
    unsigned char map[] = { 0, 1, 2, 3, 4, 5 };

    DBG(1, ErrorF("WizardPen DeviceInit\n"));

    /* 5 buttons */
    if (InitButtonClassDeviceStruct(dev, 5, map) == FALSE) {
	ErrorF("Unable to allocate WizardPen ButtonClassDeviceStruct\n");
	return !Success;
    }

    if (InitFocusClassDeviceStruct(dev) == FALSE) {
	ErrorF("Unable to allocate WizardPen FocusClassDeviceStruct\n");
	return !Success;
    }

    if (InitPtrFeedbackClassDeviceStruct(dev, ControlProc) == FALSE) {
	ErrorF("Unable to init ptr feedback\n");
	return !Success;
    }

    /* 3 axes */
    if (InitValuatorClassDeviceStruct(dev,
	    3,
	    xf86GetMotionEvents,
	    local->history_size,
	    (((priv->flags & ABSOLUTE_FLAG) ?
		    Absolute : Relative) | OutOfProximity)
	) == FALSE) {
	ErrorF("Unable to allocate WizardPen ValuatorClassDeviceStruct\n");
	return !Success;
    } else {

	InitValuatorAxisStruct(dev, 0, 0,	/* min val */
	    priv->bottomX - priv->topX,	/* max val */
	    1000,		       /* resolution */
	    0,			       /* min_res */
	    1000		       /* max_res */
	    );

	InitValuatorAxisStruct(dev, 1, 0,	/* min val */
	    priv->bottomY - priv->topY,	/* max val */
	    1000,		       /* resolution */
	    0,			       /* min_res */
	    1000		       /* max_res */
	    );

	InitValuatorAxisStruct(dev, 2, 0,	/* min val */
	    priv->bottomZ - priv->topZ,	/* max val */
	    1000,		       /* resolution */
	    0,			       /* min_res */
	    1000		       /* max_res */
	    );

    }

    if (InitProximityClassDeviceStruct(dev) == FALSE) {
	ErrorF("Unable to allocate ProximityClassDeviceStruct\n");
	return !Success;
    }

    xf86MotionHistoryAllocate(local);

    /* On ne peut pas calculer l'increment avant, faute d'ecran pour
     * connaitre la taille... */

    if (priv->wizardpenInc > 95) {
	priv->wizardpenInc = 95;
    }

    if (priv->wizardpenInc < 1) {
	/* guess the best increment value given video mode */
	rx = (priv->bottomX - priv->topX) / screenInfo.screens[0]->width;
	ry = (priv->bottomY - priv->topY) / screenInfo.screens[0]->height;

	if (rx < ry) {
	    priv->wizardpenInc = rx;
	} else {
	    priv->wizardpenInc = ry;
	}

	if (priv->wizardpenInc < 1) {
	    priv->wizardpenInc = 1;
	}
    }

    xf86Msg(X_CONFIG, "WizardPen Increment: %d\n", priv->wizardpenInc);

    return (Success);
}

static int
SwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    DBG(1, ErrorF("WizardPen SwitchMode dev=0x%x mode=%d\n", dev, mode));

    if (mode == Absolute) {
	priv->flags = priv->flags | ABSOLUTE_FLAG;
    } else {
	if (mode == Relative) {
	    priv->flags = priv->flags & ~ABSOLUTE_FLAG;
	} else {
	    ErrorF("WizardPen SwitchMode dev=0x%x invalid mode=%d\n", dev,
		mode);
	    return BadMatch;
	}
    }
    return Success;
}

static void
ReadInput(LocalDevicePtr local)
{
    int x, y, z;
    int prox, buttons;
    int is_core_pointer, is_absolute;
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    DBG(8, ErrorF("WizardPen ReadInput\n"));

    is_absolute = (priv->flags & ABSOLUTE_FLAG);
    is_core_pointer = xf86IsCorePointer(local->dev);

    /* 
     * set blocking to -1 on the first call because we know there is data to
     * read. Xisb automatically clears it after one successful read so that
     * succeeding reads are preceeded buy a select with a 0 timeout to prevent
     * read from blocking indefinately.
     */
    XisbBlockDuration(priv->buffer, -1);

    while (WizardPenGetPacket(priv) == Success) {
	x = (int)priv->packet[1] | ((int)priv->packet[2] << 7);
	y = (int)priv->packet[3] | ((int)priv->packet[4] << 7);

	if (!(priv->flags & ABSOLUTE_FLAG)) {
	    x = priv->packet[0] & XSIGN_BIT ? x : -x;
	    y = priv->packet[0] & YSIGN_BIT ? y : -y;
	} else {
	    y = priv->wizardpenMaxY - y;
	}

	z = ((int)priv->packet[5] << 2) |
	    (((int)priv->packet[6] & 0x01) << 1) |
	    (((int)priv->packet[6] & 0x10) >> 4);

	buttons = ((int)priv->packet[0] & 0x07) |
	    ((int)priv->packet[6] & 0x02 << 2);

	prox = (priv->packet[0] & PROXIMITY_BIT) ? 0 : 1;

	if (prox) {
	    if (!(priv->wizardpenOldProximity)) {
		if (!is_core_pointer) {
		    DBG(10, ErrorF("WizardPen ProxIN %d %d %d\n", x, y, z));
		    xf86PostProximityEvent(local->dev, 1, 0, 3, x, y, z);
		}
	    }

	    if ((is_absolute && (
			(priv->wizardpenOldX != x) ||
			(priv->wizardpenOldY != y) ||
			(priv->wizardpenOldZ != z)
		    )
		) || (!is_absolute && (x || y))
		) {
		if (is_absolute || priv->wizardpenOldProximity) {
		    DBG(10, ErrorF("WizardPen Motion %d %d %d\n", x, y, z));
		    xf86PostMotionEvent(local->dev, is_absolute, 0, 3, x, y,
			z);
		}
	    }

	    if (priv->wizardpenOldButtons != buttons) {
		int delta;

		delta = buttons ^ priv->wizardpenOldButtons;
		while (delta) {
		    int id;

		    id = ffs(delta);
		    delta &= ~(1 << (id - 1));

		    DBG(10, ErrorF("WizardPen Button %d 0x%x\n", id,
			    (buttons & (1 << (id - 1)))));
		    xf86PostButtonEvent(local->dev, is_absolute, id,
			(buttons & (1 << (id - 1))), 0, 3, x, y, z);
		}
	    }

	    priv->wizardpenOldButtons = buttons;
	    priv->wizardpenOldX = x;
	    priv->wizardpenOldY = y;
	    priv->wizardpenOldZ = z;
	    priv->wizardpenOldProximity = prox;
	} else {
	    if (!is_core_pointer)
		if (priv->wizardpenOldProximity) {
		    DBG(10, ErrorF("WizardPen ProxOUT %d %d %d\n", x, y, z));
		    xf86PostProximityEvent(local->dev, 0, 0, 3, x, y, z);
		}
	    priv->wizardpenOldProximity = 0;
	}
    }
}

#ifdef LINUX_INPUT

#define set_bit(byte,nb,bit)	(bit ? byte | (1<<nb) : byte & (~(1<<nb)))

/*
 * What events to post when:
 *
 * ---------|---------------------|
 * DRIVER   | DEVICE EVENT TYPE   |
 *          |---------------------|
 * MODE     | RELATIVE | ABSOLUTE |
 * ---------|----------|----------|
 * RELATIVE | relative | relative |
 * ---------|----------|----------|
 * ABSOLUTE | relative | absolute |
 * ---------|----------|----------|
 */
static void
TranslateEventCoords(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    DBG(8, ErrorF("WizardPen TranslateEventCoords( "
	    "priv->event: ( is_proximity: %s, mask: 0x%X, "
	    "x: %d, y: %d, z: %d, dx: %d, dy: %d, dz: %d, "
	    "buttons: 0x%.4X, wheel: %d ) )\n",
	    (priv->event.is_proximity ? "TRUE" : "FALSE"),
	    priv->event.mask,
	    priv->event.x,
	    priv->event.y,
	    priv->event.z,
	    priv->event.dx,
	    priv->event.dy,
	    priv->event.dz, priv->event.buttons, priv->event.wheel));

    // Translate coordinates according to Top and Bottom points.

    if (priv->event.mask & WIZARDPEN_EVENT_MASK_X) {

	if (priv->event.x > priv->bottomX) {
	    priv->event.x = priv->bottomX;
	}

	priv->event.x = priv->event.x - priv->topX;

	if (priv->event.x < 0) {
	    priv->event.x = 0;
	}
    }

    if (priv->event.mask & WIZARDPEN_EVENT_MASK_Y) {

	if (priv->event.y > priv->bottomY) {
	    priv->event.y = priv->bottomY;
	}

	priv->event.y = priv->event.y - priv->topY;

	if (priv->event.y < 0) {
	    priv->event.y = 0;
	}
    }

    if (priv->event.mask & WIZARDPEN_EVENT_MASK_Z) {
	if (priv->event.z > priv->bottomZ) {
	    priv->event.z = priv->bottomZ;
	}

	priv->event.z = priv->event.z - priv->topZ;

	if (priv->event.z < 0) {
	    priv->event.z = 0;
	}
	// override first button state with pressure level
	priv->event.buttons = set_bit(priv->event.buttons,
	    0, priv->event.z > 0);

    }

    /* Multiply relative coords by relFactor */
    priv->event.dx = priv->event.dx * 24;
    priv->event.dy = priv->event.dy * 24;

    DBG(8, ErrorF("WizardPen TranslateEventCoords END: "
	    "priv->event: ( is_proximity: %s, mask: 0x%X, "
	    "x: %d, y: %d, z: %d, dx: %d, dy: %d, dz: %d, "
	    "buttons: 0x%.4X, wheel: %d )\n",
	    (priv->event.is_proximity ? "TRUE" : "FALSE"),
	    priv->event.mask,
	    priv->event.x,
	    priv->event.y,
	    priv->event.z,
	    priv->event.dx,
	    priv->event.dy,
	    priv->event.dz, priv->event.buttons, priv->event.wheel));
}				       /* TranslateEventCoords */

static void
RegisterButtons(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
    int is_proximity;
    int buttons;

    DBG(8, ErrorF("WizardPen RegisterButtons( "
	    "priv->event: ( is_proximity: %s, mask: 0x%X, "
	    "x: %d, y: %d, z: %d, dx: %d, dy: %d, dz: %d, "
	    "buttons: 0x%.4X, wheel: %d ) )\n",
	    (priv->event.is_proximity ? "TRUE" : "FALSE"),
	    priv->event.mask,
	    priv->event.x,
	    priv->event.y,
	    priv->event.z,
	    priv->event.dx,
	    priv->event.dy,
	    priv->event.dz, priv->event.buttons, priv->event.wheel));

    is_proximity = priv->wizardpenOldProximity;
    buttons = priv->event.buttons;

    if (is_proximity) {
	int button;

	for (button = 1; button <= 16; button++) {

	    int mask = 1 << (button - 1);

	    if ((mask & priv->wizardpenOldButtons) != (mask & buttons)) {

		DBG(4, ErrorF("WizardPen Posting button "
			"event: %d, %d\n", button, (buttons & mask) != 0));

		xf86PostButtonEvent(local->dev, 0, button, (buttons & mask) != 0, 0,	// first valuator
		    3,		       // number of valuators
		    priv->wizardpenOldX,
		    priv->wizardpenOldY, priv->wizardpenOldZ);

	    }
	}
    }
    // store new buttons state
    priv->wizardpenOldButtons = buttons;
}				       /* RegisterButtons */

static void
RegisterWheel(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
    int is_proximity;
    int count;
    int button;

    DBG(8, ErrorF("WizardPen RegisterWheel( "
	    "priv->event: ( is_proximity: %s, mask: 0x%X, "
	    "x: %d, y: %d, z: %d, dx: %d, dy: %d, dz: %d, "
	    "buttons: 0x%.4X, wheel: %d ) )\n",
	    (priv->event.is_proximity ? "TRUE" : "FALSE"),
	    priv->event.mask,
	    priv->event.x,
	    priv->event.y,
	    priv->event.z,
	    priv->event.dx,
	    priv->event.dy,
	    priv->event.dz, priv->event.buttons, priv->event.wheel));

    is_proximity = priv->wizardpenOldProximity;

    if (is_proximity) {

	if (priv->event.wheel > 0) {
	    button = 4;
	    count = priv->event.wheel;
	} else {
	    button = 5;
	    count = -priv->event.wheel;
	}

	for (; count > 0; count--) {

	    DBG(4, ErrorF("WizardPen Posting button event: %d, %d\n",
		    button, 1));

	    xf86PostButtonEvent(local->dev, 0, button, 1,	// button down
		0,		       // first valuator
		3,		       // number of valuators
		priv->wizardpenOldX,
		priv->wizardpenOldY, priv->wizardpenOldZ);

	    DBG(4, ErrorF("WizardPen Posting button event: %d, %d\n",
		    button, 0));

	    xf86PostButtonEvent(local->dev, 0, button, 0,	// button up
		0,		       // first valuator
		3,		       // number of valuators
		priv->wizardpenOldX,
		priv->wizardpenOldY, priv->wizardpenOldZ);
	}
    }
    // clear event
    priv->event.wheel = 0;
}				       /* RegisterWheel */

static void
RegisterProximity(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    int is_core_pointer;
    int is_proximity;

    int oldProximity, oldButtons;

    DBG(8, ErrorF("WizardPen RegisterProximity( "
	    "priv->event: ( is_proximity: %s, mask: 0x%X, "
	    "x: %d, y: %d, z: %d, dx: %d, dy: %d, dz: %d, "
	    "buttons: 0x%.4X, wheel: %d ) )\n",
	    (priv->event.is_proximity ? "TRUE" : "FALSE"),
	    priv->event.mask,
	    priv->event.x,
	    priv->event.y,
	    priv->event.z,
	    priv->event.dx,
	    priv->event.dy,
	    priv->event.dz, priv->event.buttons, priv->event.wheel));

    is_core_pointer = xf86IsCorePointer(local->dev);
    is_proximity = priv->event.is_proximity;

    oldProximity = priv->wizardpenOldProximity;
    oldButtons = priv->wizardpenOldButtons;

    if (is_proximity) {

	if (!oldProximity) {
	    DBG(4, ErrorF("WizardPen Posting proximity " "event: In\n"));
	    xf86PostProximityEvent(local->dev, TRUE,	// proximity in
		0,		       // first valuator
		0		       // number of valuators
		);
	}

    } else {
	/* reports buttons up if any button has been
	 * down and device becomes out of proximity */
	if (oldButtons) {
	    priv->event.buttons = 0;
	    RegisterButtons(local);
	}

	if (!is_core_pointer) {
	    if (oldProximity) {
		DBG(4, ErrorF("WizardPen Posting proximity " "event: Out\n"));
		xf86PostProximityEvent(local->dev, FALSE,	// proximity out
		    0,		       // first valuator
		    0		       // number of valuators
		    );
	    }
	}
    }

    // save new proximity
    priv->wizardpenOldProximity = is_proximity;
}				       /* RegisterProximity */

static void
RegisterMotion(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);
    int is_absolute;
    int is_proximity;

    DBG(8, ErrorF("WizardPen RegisterMotion( "
	    "priv->event: ( is_proximity: %s, mask: 0x%X, "
	    "x: %d, y: %d, z: %d, dx: %d, dy: %d, dz: %d, "
	    "buttons: 0x%.4X, wheel: %d ) )\n",
	    (priv->event.is_proximity ? "TRUE" : "FALSE"),
	    priv->event.mask,
	    priv->event.x,
	    priv->event.y,
	    priv->event.z,
	    priv->event.dx,
	    priv->event.dy,
	    priv->event.dz, priv->event.buttons, priv->event.wheel));

    is_absolute = (priv->flags & ABSOLUTE_FLAG);
    is_proximity = priv->wizardpenOldProximity;

    if (is_absolute && (priv->event.mask & WIZARDPEN_EVENT_MASK_ALL)
	) {
	if (is_proximity) {
	    // event is absolute, absolute mode
	    DBG(4, ErrorF("WizardPen Posting absolute motion "
		    "event: %d, %d, %d\n",
		    priv->event.x, priv->event.y, priv->event.z));

	    xf86PostMotionEvent(local->dev, 1,	// absolute
		0,		       // first valuator
		3,		       // num valuators
		priv->event.x, priv->event.y, priv->event.z);
	}

    } else {
	if (priv->event.mask & WIZARDPEN_EVENT_MASK_ALL) {
	    // Convert absolute coordinates to relative
	    // for message posting

	    if (priv->event.mask & WIZARDPEN_EVENT_MASK_X) {
		priv->event.dx = priv->event.x - priv->wizardpenOldX;
	    }
	    if (priv->event.mask & WIZARDPEN_EVENT_MASK_Y) {
		priv->event.dy = priv->event.y - priv->wizardpenOldY;
	    }
	    if (priv->event.mask & WIZARDPEN_EVENT_MASK_Z) {
		priv->event.dz = priv->event.z - priv->wizardpenOldZ;
	    }
	} else {
	    // Convert relative coordinates to absolute
	    // for position storage

	    priv->event.x = priv->wizardpenOldX + priv->event.dx;

	    if (priv->event.x > (priv->bottomX - priv->topX)) {
		priv->event.x = (priv->bottomX - priv->topX);
	    } else {
		if (priv->event.x < 0) {
		    priv->event.x = 0;
		}
	    }

	    priv->event.y = priv->wizardpenOldY + priv->event.dy;

	    if (priv->event.y > (priv->bottomY - priv->topY)) {
		priv->event.y = (priv->bottomY - priv->topY);
	    } else {
		if (priv->event.y < 0) {
		    priv->event.y = 0;
		}
	    }

	    // XXX priv->event.z = priv->wizardpenOldZ + priv->event.dz;

	    // XXX if( priv->event.z > ( priv->bottomZ - priv->topZ ) ) {
	    // XXX  priv->event.z = ( priv->bottomZ - priv->topZ );
	    // XXX } else {
	    // XXX  if( priv->event.z < 0 ) {
	    // XXX          priv->event.z = 0;
	    // XXX  }
	    // XXX }

	}

	if (is_proximity &&
	    (priv->event.dx || priv->event.dy || priv->event.dz)
	    ) {
	    DBG(4, ErrorF("WizardPen Posting relative motion "
		    "event: %d, %d, %d\n",
		    priv->event.dx, priv->event.dy, priv->event.dz));

	    xf86PostMotionEvent(local->dev, 0,	// relative
		0,		       // first valuator
		3,		       // num valuators
		priv->event.dx, priv->event.dy, priv->event.dz);
	}
    }

    // save new coordinates
    priv->wizardpenOldX = priv->event.x;
    priv->wizardpenOldY = priv->event.y;
    priv->wizardpenOldZ = priv->event.z;

    // clear event
    priv->event.mask = WIZARDPEN_EVENT_MASK_NONE;
    priv->event.dx = 0;
    priv->event.dy = 0;
    priv->event.dz = 0;
}				       /* RegisterMotion */

static void
ResetEvent(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    priv->event.is_proximity = priv->wizardpenOldProximity;
    priv->event.mask = WIZARDPEN_EVENT_MASK_NONE;
    priv->event.dx = 0;
    priv->event.dy = 0;
    priv->event.dz = 0;
    priv->event.buttons = priv->wizardpenOldButtons;
    priv->event.wheel = 0;
}				       /* ResetEvent */

static void
FlushEvent(LocalDevicePtr local)
{
    TranslateEventCoords(local);
    RegisterProximity(local);
    RegisterMotion(local);
    RegisterButtons(local);
    RegisterWheel(local);
}				       /* FlushEvent */

static void
USBInputABS(LocalDevicePtr local, struct input_event *event)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    switch (event->code) {

    case ABS_X:
    case ABS_Z:		       /* some tablets send ABS_Z (ABS_X+2) instead of ABS_X */
	DBG(10, ErrorF("WizardPen ABS event %s: %d\n",
		(event->code == ABS_X ? "ABS_X" : "ABS_Z(ABS_X)"),
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.mask = priv->event.mask | WIZARDPEN_EVENT_MASK_X;
	priv->event.x = event->value;
	break;

    case ABS_Y:
    case ABS_RX:		       /* some tablets send ABS_RX (ABS_Y+2) instead of ABS_Y */
	DBG(10, ErrorF("WizardPen ABS event %s: %d\n",
		(event->code == ABS_Y ? "ABS_Y" : "ABS_RX(ABS_Y)"),
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.mask = priv->event.mask | WIZARDPEN_EVENT_MASK_Y;
	priv->event.y = event->value;
	break;

    case ABS_PRESSURE:
	DBG(10, ErrorF("WizardPen ABS event PRESSURE: %d\n", event->value));

	priv->event.is_proximity = TRUE;
	priv->event.mask = priv->event.mask | WIZARDPEN_EVENT_MASK_Z;
	priv->event.z = event->value;
	break;

    default:
	DBG(3, ErrorF("WizardPen UNKNOWN ABS event 0x%X: 0x%X\n", event->code,
		event->value));
	break;
    }
}				       /* USBInputABS */

static void
USBInputREL(LocalDevicePtr local, struct input_event *event)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    switch (event->code) {
    case REL_X:
	DBG(10, ErrorF("WizardPen REL event REL_DX: %d\n", event->value));
	priv->event.is_proximity = TRUE;
	priv->event.dx = priv->event.dx + event->value;
	break;

    case REL_Y:
	DBG(10, ErrorF("WizardPen REL event REL_DY: %d\n", event->value));
	priv->event.is_proximity = TRUE;
	priv->event.dy = priv->event.dy + event->value;
	break;

    case REL_Z:
	DBG(5, ErrorF("WizardPen IGNORED REL event REL_DZ: %d\n",
		event->value));
	// IGNORED, don't know how to process
	break;

    case REL_WHEEL:
	DBG(10, ErrorF("WizardPen REL event REL_WHEEL: 0x%X\n",
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.wheel = priv->event.wheel + event->value;
	break;

    default:
	DBG(3, ErrorF("WizardPen UNKNOWN REL event 0x%X: 0x%X\n", event->code,
		event->value));
	break;
    }
}				       /* USBInputREL */

static void
USBInputKEY(LocalDevicePtr local, struct input_event *event)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    switch (event->code) {

    case BTN_TOUCH:
    case BTN_SIDE:		       /* Some tablets send BTN_SIDE instead of BTN_TOUCH */
	DBG(10, ErrorF("WizardPen KEY event %s: 0x%X\n",
		(event->code ==
		    BTN_TOUCH ? "BTN_TOUCH" : "BTN_SIDE(BTN_TOUCH)"),
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.buttons = set_bit(priv->event.buttons, 0, event->value);
	break;

    case BTN_STYLUS:
    case BTN_EXTRA:		       /* Some tablets send BTN_EXTRA instead of BTN_STYLUS */
	DBG(10, ErrorF("WizardPen KEY event %s: 0x%X\n",
		(event->code ==
		    BTN_STYLUS ? "BTN_STYLUS" : "BTN_EXTRA(BTN_STYLUS)"),
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.buttons = set_bit(priv->event.buttons, 1, event->value);
	break;

    case BTN_STYLUS2:
    case BTN_FORWARD:		       /* Some tablets send BTN_FORWARD instead of BTN_STYLUS2 */
	DBG(10, ErrorF("WizardPen KEY event %s: 0x%X\n",
		(event->code ==
		    BTN_STYLUS2 ? "BTN_STYLUS2" : "BTN_FORWARD(BTN_STYLUS2)"),
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.buttons = set_bit(priv->event.buttons, 2, event->value);
	break;

    case BTN_LEFT:
	DBG(10, ErrorF("WizardPen KEY event BTN_LEFT: 0x%X\n", event->value));
	priv->event.is_proximity = TRUE;
	priv->event.buttons = set_bit(priv->event.buttons, 0, event->value);
	break;

    case BTN_MIDDLE:
	DBG(10, ErrorF("WizardPen KEY event BTN_MIDDLE: 0x%X\n",
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.buttons = set_bit(priv->event.buttons, 1, event->value);
	break;

    case BTN_RIGHT:
	DBG(10, ErrorF("WizardPen KEY event BTN_RIGHT: 0x%X\n",
		event->value));
	priv->event.is_proximity = TRUE;
	priv->event.buttons = set_bit(priv->event.buttons, 2, event->value);
	break;

    default:
	DBG(3, ErrorF("WizardPen UNKNOWN KEY event 0x%X: 0x%X\n", event->code,
		event->value));
	break;
    }				       /* switch( event->code ) */
}				       /* USBInputKEY */

static void
USBInputSYN(LocalDevicePtr local, struct input_event *event)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    switch (event->code) {
    case SYN_REPORT:
	DBG(10, ErrorF("WizardPen SYN event SYN_REPORT: 0x%X\n",
		event->value));
	FlushEvent(local);
	break;
    default:
	DBG(3, ErrorF("WizardPen UNKNOWN SYN event 0x%X: 0x%X\n", event->code,
		event->value));
	break;
    }
}				       /* USBInputSYN */

static void
USBReadInput(LocalDevicePtr local)
{
    int len;
    struct input_event *event;
    char eventbuf[sizeof(struct input_event) * MAX_EVENTS];

    DBG(8, ErrorF("WizardPen USBReadInput\n"));

    SYSCALL(len = read(local->fd, eventbuf, sizeof(eventbuf)));

    if (len <= 0) {
	ErrorF("Error reading WizardPen device : %s\n", strerror(errno));
	return;
    }

    for (event = (struct input_event *)eventbuf;
	event < (struct input_event *)(eventbuf + len); event++) {
	switch (event->type) {
	case EV_ABS:
	    USBInputABS(local, event);
	    break;

	case EV_REL:
	    USBInputREL(local, event);
	    break;

	case EV_KEY:
	    USBInputKEY(local, event);
	    break;

	case EV_SYN:
	    USBInputSYN(local, event);
	    break;

	default:
	    DBG(3, ErrorF("WizardPen UNKNOWN 0x%X event 0x%X: 0x%X\n",
		    event->type, event->code, event->value));
	    break;
	}			       /* switch event->type */
    }				       /* for event */
}				       /* USBReadInput */
#endif

static void
CloseProc(LocalDevicePtr local)
{
    DBG(1, ErrorF("WizardPen CloseProc\n"));
}

/* 
 * The ConvertProc function may need to be tailored for your device.
 * This function converts the device's valuator outputs to x and y coordinates
 * to simulate mouse events.
 */
static Bool
ConvertProc(LocalDevicePtr local,
    int first,
    int num, int v0, int v1, int v2, int v3, int v4, int v5, int *x, int *y)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    // calculate in double to avoid integer overflows when moving mouse
    // at low speeds, due to its low resolution (???)
    *x = v0 * ((double)miPointerCurrentScreen()->width) /
	(priv->bottomX - priv->topX) + 0.5;
    *y = v1 * ((double)miPointerCurrentScreen()->height) /
	(priv->bottomY - priv->topY) + 0.5;

    return TRUE;
}

static Bool
ReverseConvertProc(LocalDevicePtr local, int x, int y, int *valuators)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) (local->private);

    // calculate in double to avoid integer overflows when moving mouse
    // at low speeds, due to its low resolution (???)
    valuators[0] = (double)(x) * (priv->bottomX - priv->topX) /
	((double)miPointerCurrentScreen()->width) + 0.5;
    valuators[1] = (double)(y) * (priv->bottomY - priv->topY) /
	((double)miPointerCurrentScreen()->height) + 0.5;

    return TRUE;
}

#define WriteString(str)\
XisbWrite (priv->buffer, (unsigned char *)(str), strlen(str))

static Bool
QueryHardware(WizardPenPrivatePtr priv)
{
    DBG(1, ErrorF("WizardPen QueryHardware\n"));

    /* Reset */
    WriteString("z0");

    /* Wait */
    milisleep(250);

    /* Prompt Mode in order to not be disturbed */
    WriteString(WIZARDPEN_PROMPT_MODE);

    /* Flush */
    while (XisbRead(priv->buffer) >= 0) {
    };

    /* Ask for Config packet */
    WriteString(WIZARDPEN_CONFIG);

    /* Read the packet */
    XisbBlockDuration(priv->buffer, 1000000);
    NewPacket(priv);

    if (WizardPenGetPacket(priv) == Success) {
	priv->wizardpenMaxX = (int)priv->packet[1] +
	    ((int)priv->packet[2] << 7);
	priv->wizardpenMaxY = (int)priv->packet[3] +
	    ((int)priv->packet[4] << 7);
	priv->wizardpenMaxZ = 512;
	xf86Msg(X_CONFIG,
	    "WizardPen MaxX: %d MaxY: %d\n",
	    priv->wizardpenMaxX, priv->wizardpenMaxY);
    } else {
	return (!Success);
    }

    return (Success);
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)

#ifdef LINUX_INPUT
static Bool
USBQueryHardware(LocalDevicePtr local)
{
    WizardPenPrivatePtr priv = (WizardPenPrivatePtr) local->private;
    char name[256] = "Unknown";
    struct input_absinfo abs_features;

    DBG(1, ErrorF("WizardPen USBQueryHardware\n"));

    ioctl(local->fd, EVIOCGNAME(sizeof(name)), name);
    xf86Msg(X_CONFIG, "WizardPen device name: \"%s\"\n", name);

    ioctl(local->fd, EVIOCGABS(ABS_X), &abs_features);
    DBG(3, ErrorF("%s: X(min: %d, max: %d, flatness: %d, fuzz: %d)\n",
	    local->name,
	    abs_features.minimum,
	    abs_features.maximum, abs_features.flat, abs_features.fuzz));

    priv->wizardpenMaxX = abs_features.maximum;

    ioctl(local->fd, EVIOCGABS(ABS_Y), &abs_features);
    DBG(3, ErrorF("%s: Y(min: %d, max: %d, flatness: %d, fuzz: %d)\n",
	    local->name,
	    abs_features.minimum,
	    abs_features.maximum, abs_features.flat, abs_features.fuzz));

    priv->wizardpenMaxY = abs_features.maximum;

    ioctl(local->fd, EVIOCGABS(ABS_PRESSURE), &abs_features);
    DBG(3, ErrorF("%s: PRESSURE(min: %d, max: %d, flatness: %d, fuzz: %d)\n",
	    local->name,
	    abs_features.minimum,
	    abs_features.maximum, abs_features.flat, abs_features.fuzz));

    priv->wizardpenMaxZ = abs_features.maximum;

    xf86Msg(X_CONFIG,
	"%s: hardware reports: MaxX: %d MaxY: %d MaxZ: %d\n",
	local->name,
	priv->wizardpenMaxX, priv->wizardpenMaxY, priv->wizardpenMaxZ);

    return (Success);
}
#endif

static void
NewPacket(WizardPenPrivatePtr priv)
{
    priv->packeti = 0;
}

static Bool
WizardPenGetPacket(WizardPenPrivatePtr priv)
{
    int count = 0;
    int c = 0;

    while ((c = XisbRead(priv->buffer)) >= 0) {

	/* 
	 * fail after 500 bytes so the server doesn't hang forever if a
	 * device sends bad data.
	 */
	if (count > 500) {
	    NewPacket(priv);
	    return (!Success);
	}

	count++;

	if (c & PHASING_BIT) {
	    NewPacket(priv);

	    /*xf86Msg(X_CONFIG, "Push %2.2x\n",(char) c); */
	    XisbBlockDuration(priv->buffer, 10000);
	    priv->packet[priv->packeti++] = c;
	    count = WIZARDPEN_PACKET_SIZE - 1;
	    while (count-- && (c = XisbRead(priv->buffer)) >= 0) {
		/*xf86Msg(X_CONFIG, "Push %2.2x\n",(char) c); */
		priv->packet[priv->packeti++] = c;
	    }
	    XisbBlockDuration(priv->buffer, 0);
	    if (c > 0) {
		return (Success);
	    }
	}
    }
    return (!Success);
}
