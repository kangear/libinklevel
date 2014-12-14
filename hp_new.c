/* hp.c
 *
 * (c) 2003, 2004, 2005, 2006, 2007, 2008 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <stdlib.h>

#include "inklevel.h"
#include "util.h"
#include "hp_new.h"

/* This function parses the device id of a new HP printer
 * for example HP Deskjet 5550 
 */

int parse_device_id_new_hp(char tags[NR_TAGS][BUFLEN], int n, 
                           struct ink_level *level) {
  char *s = tags[n];
  int length = 0;
  int colors = 0;
  char colorsAscii[2];
  int i = 0;
  int j = 0;
  int k = 0;
  int colorType = 0;
  char colorTypeAscii[3];
  int colorValue = 0;
  char colorValueAscii[3];
  int colorTypeIndex;
  int offset;
  char black[3];
  char cyan[3];
  char magenta[3];
  char yellow[3];

  /* Determine the length of the string */

  while (*s != '\0') {
    length++;
    s++;
  }

  s = tags[n];

  if (length > 3 && s[2] == '0' && s[3] == '3') {
    
#ifdef DEBUG
    printf("Version 3 detected\n");
#endif
    
    offset = 20;

  } else if (length > 3 && s[2] == '0' && (s[3] == '0' || s[3] == '1')) {

#ifdef DEBUG
    printf("Version 0 or 1 detected\n");
#endif

      offset = 18;

  } else if (length > 3 && s[2] == '0' && s[3] == '4') {

#ifdef DEBUG
    printf("Version 0 or 1 detected\n");
#endif

      offset = 24;

  } else if (length > 3 && s[2] == '0' && s[3] == '2') {

#ifdef DEBUG
    printf("Version 2 detected\n");
#endif

    // Do not know if this is alsways correct
    // Worked at least in the old version

    yellow[0] = s[length - 2];
    yellow[1] = s[length - 1];
    yellow[2] = '\0';

    magenta[0] = s[length - 6];
    magenta[1] = s[length - 5];
    magenta[2] = '\0';

    cyan[0] = s[length - 10];
    cyan[1] = s[length - 9];
    cyan[2] = '\0';

    black[0] = s[length - 14];
    black[1] = s[length - 13];
    black[2] = '\0';

    level->status = RESPONSE_VALID;
    level->levels[0][INDEX_TYPE] = CARTRIDGE_BLACK;
    level->levels[0][INDEX_LEVEL] = my_axtoi(black);
    level->levels[1][INDEX_TYPE] = CARTRIDGE_CYAN;
    level->levels[1][INDEX_LEVEL] = my_axtoi(cyan);
    level->levels[2][INDEX_TYPE] = CARTRIDGE_MAGENTA;
    level->levels[2][INDEX_LEVEL] = my_axtoi(magenta);
    level->levels[3][INDEX_TYPE] = CARTRIDGE_YELLOW;
    level->levels[3][INDEX_LEVEL] = my_axtoi(yellow);

#ifdef DEBUG
    printf("Yellow: %d%%, Magenta: %d%%, Cyan: %d%%, Black: %d%%\n",
           my_axtoi(yellow), my_axtoi(magenta), my_axtoi(cyan), 
           my_axtoi(black));
#endif

    return OK;

  } else {

#ifdef DEBUG
    printf("Printer not supported\n");
#endif

    return PRINTER_NOT_SUPPORTED;
  }

  colorsAscii[0] = s[offset];
  colorsAscii[1] = '\0';

  colors = atoi(colorsAscii);

#ifdef DEBUG
  printf("Number of colors: %d\n", colors);
#endif

  i = 0; /* current color */
  j = offset; /* index in device id */
  k = 0; /* index in struct ink_level->levels */

  while ((j+8 < length) && (i < colors)) {

#ifdef DEBUG
    printf("Processing color number %d\n", i);
#endif
      
    colorTypeAscii[0] = s[j+1];
    colorTypeAscii[1] = s[j+2];
    colorTypeAscii[2] = '\0';
    colorType = my_axtoi(colorTypeAscii) & 0x3f ;

#ifdef DEBUG
    printf("Raw Color Type: %d\n", my_axtoi(colorTypeAscii));
#endif

    /* Only show inks with their own head */
    /* Not sure if this is the right approach */
    /* Is needed for Photosmart 8250 to get rid of bogus entry */
      
    if (my_axtoi(colorTypeAscii) & 0x40) { 
      
#ifdef DEBUG
      printf("Color type: %d\n", colorType);
#endif

      colorValueAscii[0] = s[j+7];
      colorValueAscii[1] = s[j+8];
      colorValueAscii[2] = '\0';
      colorValue = my_axtoi(colorValueAscii);

#ifdef DEBUG
      printf("Color value: %d\n", colorValue);
#endif

      switch (colorType) {
      case 0:
        colorTypeIndex = CARTRIDGE_NOT_PRESENT;
        break;

      case 1:
        colorTypeIndex = CARTRIDGE_BLACK;
        break;

      case 2:
        colorTypeIndex = CARTRIDGE_COLOR;
        break;

      case 3:
        colorTypeIndex = CARTRIDGE_KCM;
        break;

      case 4:
        colorTypeIndex = CARTRIDGE_CYAN;
        break;

      case 5:
        colorTypeIndex = CARTRIDGE_MAGENTA;
        break;

      case 6:
        colorTypeIndex = CARTRIDGE_YELLOW;
        break;

      case 7:
        colorTypeIndex = CARTRIDGE_PHOTOCYAN;
        break;

      case 8:
        colorTypeIndex = CARTRIDGE_PHOTOMAGENTA;
        break;

      case 9:
        colorTypeIndex = CARTRIDGE_PHOTOYELLOW;
        break;

      case 10:
        colorTypeIndex = CARTRIDGE_GGK;
        break;

      case 11:
        colorTypeIndex = CARTRIDGE_BLUE;
        break;

      case 12:
        colorTypeIndex = CARTRIDGE_KCMY;
        break;

      case 13:
        colorTypeIndex = CARTRIDGE_LCLM;
        break;

      case 14:
        colorTypeIndex = CARTRIDGE_YM;
        break;

      case 15:
        colorTypeIndex = CARTRIDGE_CK;
        break;

      case 16:
        colorTypeIndex = CARTRIDGE_LGPK;
        break;

      case 17:
        colorTypeIndex = CARTRIDGE_LG;
        break;

      case 18:
        colorTypeIndex = CARTRIDGE_G;
        break;

      case 19:
        colorTypeIndex = CARTRIDGE_PG;
        break;

      case 32:
        colorTypeIndex = CARTRIDGE_WHITE;
        break;
        
      case 33:
        colorTypeIndex = CARTRIDGE_RED;
        break;

      default:
        colorTypeIndex = CARTRIDGE_UNKNOWN;
        break;
      }

      if (colorTypeIndex != CARTRIDGE_NOT_PRESENT) {
        level->status = RESPONSE_VALID;
        level->levels[k][INDEX_TYPE] = colorTypeIndex;
        level->levels[k][INDEX_LEVEL] = colorValue;
        k++;

#ifdef DEBUG
        printf("Added to output array\n");
#endif

      }
    }

    j += 8;
    i++;
  }

  return OK;
}

/* This function parses the device id of an old HP printer
 * for example HP Photosmart 1000 
 */

int parse_device_id_old_hp(char tags[NR_TAGS][BUFLEN], int n, 
                           struct ink_level *level) {
  char *s = tags[n];
  int length = 0;
  char b[4]; /* level of black ink as decimal string */
  char c[4]; /* level of color ink as decimal string */
  int i;
  int j;

  /* Determine the length of the string */

  while (*s != '\0') {
    length++;
    s++;
  }

  s = tags[n];

  i = 0;
  j = 0;

  while (i < (length - 3)) {
  
    if (s[i] == ',' && s[i+1] == 'K' && (s[i+2] == '0' || s[i+2] == '3')
        && s[i+3] == ',') {

      if ((s[length - 11] == 'K') && (s[length - 10] == 'P')) {
        b[0] = s[length - 9];
        b[1] = s[length - 8];
        b[2] = s[length - 7];
        b[3] = '\0';
        
        level->status = RESPONSE_VALID;
        level->levels[j][INDEX_TYPE] = CARTRIDGE_BLACK;
        level->levels[j][INDEX_LEVEL] = my_atoi(b);

#ifdef DEBUG
        printf("Black: %d\n", my_atoi(b));
#endif
        
        j++;
      }
    }
    
    i++;
  }

  i = 0;

  while (i < (length - 3)) {
  
    if (s[i] == ',' && s[i+1] == 'C' && (s[i+2] == '0' || s[i+2] == '3')
        && s[i+3] == ',') {

      if ((s[length - 5] == 'C') && (s[length - 4] == 'P')) {
        c[0] = s[length - 3];
        c[1] = s[length - 2];
        c[2] = s[length - 1];
        c[3] = '\0';
        
        level->status = RESPONSE_VALID;
        level->levels[j][INDEX_TYPE] = CARTRIDGE_COLOR;
        level->levels[j][INDEX_LEVEL] = my_atoi(c);
        
#ifdef DEBUG
        printf("Color: %d\n", my_atoi(c));
#endif
        j++;
      }
    }

    i++;
  }

  if (j > 0) {
    return OK;
  } else {

#ifdef DEBUG
    printf("No ink level found\n");
#endif

    return NO_INK_LEVEL_FOUND;
  }
}
