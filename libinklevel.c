/* libinklevel.c
 *
 * (c) 2003, 2004, 2005, 2006, 2007, 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <string.h>

#include "internal.h"
#include "inklevel.h"
#include "platform_specific.h"
#include "hp_new.h"
#include "epson_new.h"
#include "canon.h"
#include "util.h"

/* local functions */

static int parse_device_id(const int port, const char *device_file, 
                           const int portnumber, 
                           const char *device_id, struct ink_level *level);

int get_ink_level(const int port, const char *device_file, 
                  const int portnumber, struct ink_level *level) {
  char device_id[BUFLEN];
  int ret;

#ifdef DEBUG
  setvbuf (stdout, NULL, _IONBF, 0);
  setvbuf (stderr, NULL, _IONBF, 0);
#endif

  memset(level->model, 0, MODEL_NAME_LENGTH);
  memset(level->levels, 0, MAX_CARTRIDGE_TYPES * sizeof(unsigned short) * 2);
  level->status = RESPONSE_INVALID;

  if ((ret = get_device_id(port, device_file, portnumber, device_id)) == OK) {
    if ((ret = parse_device_id(port, device_file, portnumber, device_id, 
                               level)) == OK) {
      return OK;
    }
  }

  return ret;
}

/* This function parses the device id and calls the appropiate function */

static int parse_device_id(int port, const char *device_file, int portnumber, 
                           const char *device_id, struct ink_level *level) {
  const char *tag_mfg = NULL;
  const char *c;
  char tags[NR_TAGS][BUFLEN];  
  int i;

  tokenize_device_id(device_id, tags);

  /* Check if we deal with a printer */
  /* First try the "CLS:" tag */

  if ((c = get_tag_value(tags, "CLS:")) != NULL){
    if (strncasecmp(c, "PRINTER", 7) != 0){
#ifdef DEBUG
      printf("Device is not a printer\n");
#endif
      return NO_PRINTER_FOUND;
    }
  } else { 
    /* "CLS:" tag not found, try the "CLASS:" tag */
    if ((c = get_tag_value(tags, "CLASS:")) != NULL){
      if (strncasecmp(c, "PRINTER", 7) != 0){
#ifdef DEBUG
	printf("Device is not a printer\n");
#endif
	return NO_PRINTER_FOUND;
      }
    } else {
#ifdef DEBUG 
      printf("No device class found\n");
#endif

      return NO_DEVICE_CLASS_FOUND;
    }
  }

  /* Insert the name of the printer */

  level->model[0] = '\0';

  /* first insert Manufacturer */

  if ((c = get_tag_value(tags, "MFG:")) != NULL) {
    strncpy(level->model, c, MODEL_NAME_LENGTH-1);
    level->model[MODEL_NAME_LENGTH-1] = '\0';
    tag_mfg = c;
  }

  /* append a space character after manufacturer */

  if (strlen(level->model) < MODEL_NAME_LENGTH-1){
    strcat(level->model, " ");  
  }

  /* now append the model */ 
  if ((c = get_tag_value(tags, "MDL:")) != NULL) {
    strncat(level->model, c, MODEL_NAME_LENGTH -1 - strlen(level->model));
    level->model[MODEL_NAME_LENGTH-1] = '\0';
  } 
  
  /* Check for a new HP printer (has S: tag) */

  if ((i = get_tag_index(tags, "S:")) != -1)
    return parse_device_id_new_hp(tags, i, level);
  
  /* Check for an old HP printer (has VSTATUS: tag) */

  if ((i = get_tag_index(tags, "VSTATUS:")) != -1) {
    return parse_device_id_old_hp(tags, i, level);
  }

  /* Check for manufacturer */

  if (tag_mfg != NULL) {
    /* Check if it is "EPSON" */
   
    if (strncmp(tag_mfg, "EPSON", 5) == 0){
      return get_ink_level_epson(port, device_file, portnumber, level);
    } 

    /* check for Canon */
    if (strncmp(tag_mfg, "Canon", 5) == 0)
      return get_ink_level_canon(port, device_file, portnumber, level);

    /* Insert code to check for other printers here */
    
  }
  
  return PRINTER_NOT_SUPPORTED; /* No matching printer was found */
}

char *get_version_string(void) {
  return PACKAGE_STRING;
}
