/* ink.c
 *
 * (c) 2010 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <inklevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void usage(void) {
  printf("ink -p \"usb\"|\"parport\" [-n <portnumber>] [-t <threshold>] | -d <device_file>\n");
  printf("ink -p \"bjnp\" | -b \"bjnp://<printer.my.domain>\" | -v\n\n");

  printf("'ink -p parport' Query first parallel port printer\n");
  printf("'ink -p parport -n 1' Query second parallel port printer\n");
  printf("'ink -p usb' Query first usb port printer\n");
  printf("'ink -p usb -n 1' Query second usb port printer\n");
  printf("'ink -p bjnp' Query first bjnp network printer\n");
  printf("'ink -d /dev/usblp0' Query usb printer on device /dev/usblp0\n");
  printf("'ink -b bjnp://printer.my.domain' Query bjnp network printer on printer.my.domain\n");
  printf("'ink -b bjnp://111.222.111.222' Query bjnp network printer on ip-address 111.222.111.222\n");
  printf("'ink -p usb -t 20' Only print ink levels less than or equal to 20%%\n");
  printf("'ink -v' Show version information\n");
}

void print_version_information(void) {
  char *libinklevel_version_string;

  printf(PACKAGE_STRING);
  printf("\n");
  libinklevel_version_string = get_version_string();
  printf("%s", libinklevel_version_string);
  printf("\n");
}

int main(int argc, char *argv[]) {
  struct ink_level *level = NULL;
  int result = 0;
  int port = 0;
  int portnumber = 0;
  int c;
  int i;
  int threshold = -1;
  int headerNeeded = 1;
  char headerline[80] = "";
  char *devicefile = ""; 

  char *strCartridges[MAX_CARTRIDGE_TYPES] = {
    "Not present:",
    "Black:",
    "Color:",
    "Photo:",
    "Cyan:",
    "Magenta:",
    "Yellow:",
    "Photoblack:",
    "Photocyan:",
    "Photomagenta:",
    "Photoyellow:",
    "Red:",
    "Green:",
    "Blue:",
    "Light Black:",
    "Light Cyan:",
    "Light Magenta:",
    "Light Light Black:",
    "Matte Black:",
    "Gloss Optimizer:",
    "Unknown:",
    "Black, Cyan, Magenta:",
    "2x Grey and Black:",
    "Black, Cyan, Magenta, Yellow:",
    "Photocyan and Photomagenta:",
    "Yellow and Magenta:",
    "Cyan and Black:",
    "Light Grey and Photoblack:",
    "Light Grey:",
    "Medium Grey:",
    "Photogrey:",
    "White:"
  };

  strcat(headerline, PACKAGE_STRING);
  strcat(headerline, " (c) 2010 Markus Heinz\n\n");

  if (argc == 1) {
    usage();
    return 1;
  }

  while ((c = getopt(argc, argv, "b:p:n:t:d:v")) != -1) {
    switch (c) {
    case 'p':
      if (strcmp(optarg, "parport") == 0) {
	port = PARPORT;
      } else if (strcmp(optarg, "usb") == 0) {
	port = USB;
       } else if (strcmp(optarg, "bjnp") == 0) {
        port = BJNP;
     } else {
	usage();
	return 1;
      }
      break;
    case 't':
      if (optarg){
	threshold = atoi(optarg);
	if (threshold < 0 || threshold > 100) {
	  usage();
	  return 1;
	}
      }
      break;
    case 'd':
      if (optarg) {
        devicefile = optarg;
        port = CUSTOM_USB;
      } else {
        usage();
        return 1;
      }
      break;
    case 'b':
      if (optarg) {
        devicefile = optarg;
        port = CUSTOM_BJNP;
      } else {
        usage();
        return 1;
      }
      break;
    case 'n':
      if (optarg) {
	portnumber = atoi(optarg);
      } else {
	usage();
	return 1;
      }
      break;
    case 'v':
      print_version_information();
      return 0;
      break;
    default:
      usage();
      return 1;
    }
  }

  level = (struct ink_level *) malloc(sizeof(struct ink_level));

  if (level == NULL) {
    printf("Not enough memory available.\n");
    return 1;
  }
  
  result = get_ink_level(port, devicefile, portnumber, level);

  if (result != OK) {
    switch (result) {
    case ERROR:
      printf("An unknown error occured.\n");
      break;
    case DEV_PARPORT_INACCESSIBLE:
      printf("Could not access '/dev/parport%d'.\n", portnumber);
      break;
    case DEV_LP_INACCESSIBLE:
      printf("Could not access '/dev/lp%d'.\n", portnumber);
      break;
    case COULD_NOT_GET_DEVICE_ID:
      printf("Could not get device id.\n");
      break;
    case DEV_USB_LP_INACCESSIBLE:
      printf("Could not access '/dev/usb/lp%d' or '/dev/usblp%d'.\n", 
             portnumber, portnumber);
      break;
    case UNKNOWN_PORT_SPECIFIED:
      printf("Unknown port specified.\n");
      break;
    case NO_PRINTER_FOUND:
      printf("No printer found.\n");
      break;
    case NO_DEVICE_CLASS_FOUND:
      printf("No device class found.\n");
      break;
    case NO_CMD_TAG_FOUND:
      printf("No cmd tag found.\n");
      break;
    case PRINTER_NOT_SUPPORTED:
      printf("Printer not supported.\n");
      break;
    case NO_INK_LEVEL_FOUND:
      printf("No ink level found.\n");
      break;
    case COULD_NOT_WRITE_TO_PRINTER:
      printf("Could not write to printer.\n");
      break;
    case COULD_NOT_READ_FROM_PRINTER:
      printf("Could not read from printer.\n");
      break;
    case COULD_NOT_PARSE_RESPONSE_FROM_PRINTER:
      printf("Could not parse response from printer.\n");
      break;
    case COULD_NOT_GET_CREDIT:
      printf("Could not get credit.\n");
      break;
    case DEV_CUSTOM_USB_INACCESSIBLE:
      printf("Could not access custom usb device '%s'.\n", devicefile);
      break;
    case BJNP_URI_INVALID:
      printf("Printer URI is invalid: '%s'.\n", devicefile);
      break;
    case BJNP_INVALID_HOSTNAME:
      printf("Could not open hostname: '%s'.\n", devicefile);
      break;
   }
    printf("Could not get ink level.\n");
    free(level);
    return 1;
  }

  switch (level->status) {
  case RESPONSE_INVALID:
    printf("No ink level found\n");
    break;

  case RESPONSE_VALID:
    for(i = 0; i < MAX_CARTRIDGE_TYPES; i++) {
      if (threshold == -1 || level->levels[i][INDEX_LEVEL] <= threshold) {
	if (headerNeeded) {
	  printf("%s", headerline);
	  printf("%s\n\n", level->model);
	  headerNeeded = 0;
	}
	if(level->levels[i][INDEX_TYPE] != CARTRIDGE_NOT_PRESENT) {

	  printf("%-29s %3d%%\n", strCartridges[level->levels[i][INDEX_TYPE]],
		 level->levels[i][INDEX_LEVEL]);
	} else {
	  break;
	}
      }
    }
    break;

  default:
    printf("Printer returned unknown status\n");
    break;
  }
  
  free(level);

  return 0;
}
