
/* opensolaris.c
 *
 * (c) 2003, 2004, 2005, 2006, 2008, 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#if (HOST_OS == OPENSOLARIS)
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prnio.h>

#include "internal.h"
#include "inklevel.h"
#include "platform_specific.h"
#include "bjnp.h"

/* This function retrieves the device id of the specified port */

int get_device_id(const int port, const char *device_file, 
                  const int portnumber, char *device_id) {
  char tmp[BUFLEN];
  char my_device_file[256];
  int size;
  int fd;
  struct prn_1284_device_id id;
  int rc;
 
  if (port == CUSTOM_BJNP) {
    return bjnp_get_id_from_named_printer(portnumber, device_file, device_id);
  } else if (port == BJNP) {
    return bjnp_get_id_from_printer_port(portnumber, device_id);
  } else if (port == USB) {
    sprintf(my_device_file, "/dev/usb/printer%d", portnumber);
  } else if (port == PARPORT) {
    sprintf(my_device_file, "/dev/lp%d", portnumber);
  } else if (port == CUSTOM_USB) {
    strcpy(my_device_file, device_file);
  } else {
    return UNKNOWN_PORT_SPECIFIED;
  }

  fd = open(my_device_file, O_RDONLY);

  if (fd == -1) {
    switch (port) {
    case USB:
      return DEV_USB_LP_INACCESSIBLE;
    case PARPORT:
      return DEV_LP_INACCESSIBLE;
    case CUSTOM_USB:
      return DEV_CUSTOM_USB_INACCESSIBLE;
    }
  }
  
  /* get the IEEE 1284 device id */
  memset(&id, 0, sizeof (id));
  memset(&tmp, 0, sizeof (tmp));
  id.id_len = sizeof (tmp);
  id.id_data = tmp;
  
  rc = ioctl(fd, PRNIOC_GET_1284_DEVID, &id);
  
  if (rc < 0) {
    close(fd);
    return COULD_NOT_GET_DEVICE_ID;
  }
  close(fd);

  size = id.id_rlen;
  tmp[size] = '\0';
  
  if (size > 0) {
    strncpy(device_id, tmp, size + 1);
    return OK;
  } else {
    return COULD_NOT_GET_DEVICE_ID;
  }
}

int open_printer_device(const int port, const char *device_file,
                        const int portnumber) {
  char my_device_file[256];
  int fd;

  if (port == USB) {
    sprintf(my_device_file, "/dev/usb/printer%d", portnumber);
  } else if (port == PARPORT) {
    sprintf(my_device_file, "/dev/lp%d", portnumber);
  } else if (port == CUSTOM_USB) {
    strncpy(my_device_file, device_file, 255);
    my_device_file[255] = '\0';
  } else {
    return UNKNOWN_PORT_SPECIFIED;
  }

#ifdef DEBUG
  printf("Device file: %s\n", my_device_file);
#endif

  fd = open(my_device_file, O_RDWR);

  if (fd == -1) {

#ifdef DEBUG
    printf("Could not open %s\n", my_device_file);
#endif

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
