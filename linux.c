/* linux.c
 *
 * (c) 2003, 2004, 2005, 2006, 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */
#include "config.h"

#if (HOST_OS == LINUX)
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

/* ieee1284.h uses HAVE_IEEE1284_H, so we undefine it */
#undef HAVE_IEEE1284_H
#include <ieee1284.h>

#include "internal.h"
#include "inklevel.h"
#include "platform_specific.h"
#include "bjnp.h"

#define IOCNR_GET_DEVICE_ID 1
#define LPIOC_GET_DEVICE_ID _IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, BUFLEN)

/* This function retrieves the device id of the specified port */

int get_device_id(const int port, const char *device_file, 
                  const int portnumber, char *device_id) {
  struct parport_list parports;
  char tmp[BUFLEN];
  char device_file1[256];
  char device_file2[256];
  int size;
  int fd;
  char *c;
  int realsize;

  if (port == PARPORT ) {
    /* check if we have appropiate permissions */

    sprintf(device_file1, "/dev/parport%d", portnumber);

    if ((fd = open(device_file1, O_RDWR)) < 0) {
      return DEV_PARPORT_INACCESSIBLE;
    }

    close(fd);

    sprintf(device_file1, "/dev/lp%d", portnumber);
    
    if ((fd = open(device_file1, O_RDWR)) < 0) {
      return DEV_LP_INACCESSIBLE;
    }

    close(fd);

    if (ieee1284_find_ports(&parports, 0) == E1284_OK) {
      if (portnumber < parports.portc) {
	size = ieee1284_get_deviceid(parports.portv[portnumber], -1, 
                                     F1284_FRESH, tmp, BUFLEN);
	if (size >= 2) {
	  strncpy(device_id, tmp + 2, size - 2);
	  return OK;
	}
      }
    }

    return COULD_NOT_GET_DEVICE_ID;

  } else if (port == USB || port == CUSTOM_USB) {

    if (port == USB) {
      sprintf(device_file1, "/dev/usb/lp%d", portnumber);
      sprintf(device_file2, "/dev/usblp%d", portnumber);
      fd = open(device_file1, O_RDONLY);
      if (fd == -1) {
        fd = open(device_file2, O_RDONLY);
      }
      if (fd == -1) {
        return DEV_USB_LP_INACCESSIBLE;
      }
    } else {
      fd = open(device_file, O_RDONLY);
      if (fd == -1) {
        return DEV_CUSTOM_USB_INACCESSIBLE;
      }
    }

    if (ioctl(fd, LPIOC_GET_DEVICE_ID, tmp) < 0) {
      close(fd);
      return COULD_NOT_GET_DEVICE_ID;
    }
    close(fd);
    size = ((unsigned char) tmp[0] << 8) | ((unsigned char) tmp[1]);

    /* Some printers report the size of the device id incorrectly. */

    realsize = 2;
    c = &tmp[2];
    while (*c++ != '\0') {
      realsize++;
    }

#ifdef DEBUG
    printf("Printer reported size %d, real size is %d\n", size, realsize);
#endif

    size = (realsize < size) ? realsize : size;
    size = (size < BUFLEN - 1) ? size : BUFLEN - 1;
    tmp[size] = '\0';
    if (size >= 2) {
      strncpy(device_id, tmp + 2, size - 2);
      return OK;
    } else {
      return COULD_NOT_GET_DEVICE_ID;
    }
  } else if (port == CUSTOM_BJNP)  {
    return bjnp_get_id_from_named_printer(portnumber, device_file, device_id);
  } else if (port == BJNP) {
    return bjnp_get_id_from_printer_port(portnumber, device_id);
  } else {
    return UNKNOWN_PORT_SPECIFIED;
  }
}

int open_printer_device(const int port, const char *device_file,
                        const int portnumber) {
  char device_file1[256];
  char device_file2[256];
  int fd;

  if (port == USB) {
    sprintf(device_file1, "/dev/usb/lp%d", portnumber);
    sprintf(device_file2, "/dev/usblp%d", portnumber);
  } else if (port == PARPORT) {
    sprintf(device_file1, "/dev/lp%d", portnumber);
  } else if (port == CUSTOM_USB) {
    strncpy(device_file1, device_file, 255);
    device_file1[255] = '\0';
  } else {
    return UNKNOWN_PORT_SPECIFIED;
  }

#ifdef DEBUG
  printf("Device file: %s\n", device_file1);
#endif

  fd = open(device_file1, O_RDWR);

  if (fd == -1 && port == USB) {

#ifdef DEBUG
    printf("Could not open %s\n", device_file1);
    printf("Device file: %s\n", device_file2);
#endif

    fd = open(device_file2, O_RDWR);
    
    if (fd == -1) {
#ifdef DEBUG
      printf("Could not open %s\n", device_file2);
#endif
    }
  } else if (fd == -1 && (port == PARPORT || port == CUSTOM_USB)) {

#ifdef DEBUG
    printf("Could not open %s\n", device_file1);
#endif

  }
  
  if (fd == -1) {
    if (port == USB) {
      return DEV_USB_LP_INACCESSIBLE;
    } else if (port == CUSTOM_USB) { 
      return DEV_CUSTOM_USB_INACCESSIBLE;
    } else {
      return DEV_LP_INACCESSIBLE;
    }
  } else {
    return fd;
  }
}
#endif
