/* canon.c
 *
 * (c) 2006 Thierry MERLE <thierry.merle@free.fr>
 * (c) 2009 Louis Lagendijk
 *
 * Code taken from epson.c. Commands taken from CanonUtil 0.07 
 * (http://xwtools.automatix.de/)
 *
 * Commands come from  cnijfilter-common-2.60 from CANON INC.
 * (ftp://download.canon.jp/pub/driver/bj/linux/)
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "inklevel.h"
#include "platform_specific.h"
#include "util.h"
#include "bjnp.h"
#include "canon.h"

#ifdef __ANDROID__
#include <cutils/log.h>
#define LOG_TAG "libinklevel"
#define printf(fmt,args...)  LOGD (fmt ,##args)
#endif

#ifdef DEBUG
/* for isprint(), for debug prints */
#include <ctype.h>
#endif

/* shortcut to compute string length */
#define GET_STR_LENGTH(var) (sizeof((var))-1)

/* from an input string, this function encodes the datagram to send */
static int makeCommand_canon(const char *cmd, int cmdLen, char *data);

/* Some taken from CanonUtil::CanonUtilStatus.c */
typedef unsigned short levelTab[MAX_CARTRIDGE_TYPES];

/* decode "CHD" pattern. This gives the cartridge type that helps deducing the
   type and number of cartridges */

static void decodeCHD(char *s, struct ink_level *level);

/* decode "DOC" pattern. Operator Call tells which cartridge the operator must
   change. */

static void decodeDOC(char *s, levelTab lt);

/* decode "DWS" pattern. Warning State tells 
   which cartridge has low ink level */

static void decodeDWS(char *s, levelTab lt);

/* decode "CIR" pattern. Refined Ink levels, 
   tells the exact value of ink levels,
   but this function seems implemented on newer printers only */
static void decodeCIR(char *s, struct ink_level *level);

/* 
 * IMPORTANT NOTE: the number of cartridges is to be confirmed for each type 
 * of printer, EXCEPT for those printers that report the CIR-tag
 */

struct
{ char *model; /* The model identifier */
  struct {
    char *chd; /* The 'CHD' pattern value. You can get it by USB snooping */
    unsigned char numberOfColors;
    char cartridgeTypes[10];
  } chdTypes[4];
} existingPrinters [] =
  {
    {"iP1800",
     {{"BK,CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {"BK",1,{CARTRIDGE_BLACK}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP1000",
     {{"BK",1,{CARTRIDGE_BLACK}},
      {"CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP2200",
     {{"BK,CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {"BK",1,{CARTRIDGE_BLACK}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP1600",
     {{"BK,CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {"BK",1,{CARTRIDGE_BLACK}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"S300",
     {{"VC",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {"CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"S500",
     {{"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"S520",
     {{"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i550",
     {{"VC",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i560",
     {{"VC",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i850",
     {{"VC",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i860",
     {{"VC",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {"CL",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i865",
     {{"VC",5,{CARTRIDGE_PHOTOBLACK,CARTRIDGE_BLACK,
               CARTRIDGE_YELLOW,CARTRIDGE_MAGENTA,
               CARTRIDGE_CYAN}},
      {"CL",5,{CARTRIDGE_PHOTOBLACK,CARTRIDGE_BLACK,
               CARTRIDGE_YELLOW,CARTRIDGE_MAGENTA,
               CARTRIDGE_CYAN}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i950",
     {{"LS",6,{CARTRIDGE_CYAN,CARTRIDGE_LIGHTCYAN,
               CARTRIDGE_BLACK,CARTRIDGE_YELLOW,
               CARTRIDGE_MAGENTA,CARTRIDGE_LIGHTMAGENTA}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i965",
     {{"LS",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i960",
     {{"LS",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i990",
     {{"LS",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP1500",
     {{"CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP2000",
     {{"CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP4100",
     {{"VC",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {"CL",6,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,
               CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP4200",
     {{"VC",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK, 
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {"CL",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP4300",
     {{"VC",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK, 
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {"CL",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP4500",
     {{"VC",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK, 
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {"CL",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP3000",
     {{"VC",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP3100",
     {{"VC",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP3300",
     {{"VC",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"CL",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
               CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"BJC-6200",
     {{"VC,BK",4,{CARTRIDGE_BLACK,CARTRIDGE_CYAN,
                  CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP5000",
     {{"CL",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP5200",
     {{"CL",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"MP160",
     {{"BK,CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"MP360",
     {{"CL",2,{CARTRIDGE_BLACK,CARTRIDGE_COLOR}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"MP530",
     {{"VC",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {"CL",5,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOBLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW,}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"iP4000",
     {{"VC",5,{CARTRIDGE_PHOTOBLACK,CARTRIDGE_BLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,
               CARTRIDGE_YELLOW}}, 
      {"CL",5,{CARTRIDGE_PHOTOBLACK,CARTRIDGE_BLACK,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,
               CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"i9100",
     {{"DS",6,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {"LS",6,{CARTRIDGE_BLACK,CARTRIDGE_PHOTOCYAN,CARTRIDGE_PHOTOMAGENTA,
               CARTRIDGE_CYAN,CARTRIDGE_MAGENTA,CARTRIDGE_YELLOW}},
      {NULL,RESPONSE_INVALID,{}}}},
    {"860i",
     {{"VC",5,{CARTRIDGE_PHOTOBLACK,CARTRIDGE_BLACK,
               CARTRIDGE_YELLOW,CARTRIDGE_MAGENTA,
               CARTRIDGE_CYAN}},
      {"CL",5,{CARTRIDGE_PHOTOBLACK,CARTRIDGE_BLACK,
               CARTRIDGE_YELLOW,CARTRIDGE_MAGENTA,
               CARTRIDGE_CYAN}},
      {NULL,RESPONSE_INVALID,{}}}},
  };

const static int MAX_NUMBER_OF_MODELS = 
  sizeof(existingPrinters) / sizeof(existingPrinters[0]);

/* Some Canon printers give a binary ink level indicator :
   LOW (attributed to 20% remaining) or not LOW, 100% remaining.
   Some other printers give more precise indicator, 40 or 70% remaining. */
#define LEVEL_LOW 20 /* 20% at which there is a warning */
#define LEVEL_LOW_70 70
#define LEVEL_LOW_40 40
#define LEVEL_OUT 0

/* This funtion retrieves the ink level of an Canon printer conncected to
 * the specifies port and portnumber
 * Ink levels are binary: Normal or Low
 * An arbitrary percentage is estimated from these indicators.
 * The printer is accessed in command mode.
 * BYTE FORMAT OF A COMMAND :
 * b0-b2 : command header
 * b3-b4 : command length that follows (2 bytes, little-endian)
 * b5-b6 : command separator \x00\x1e
 * b7-b8 : data length including this parameter (2 bytes, big-endian)
 * b9-... : data (SSR=...)
 */

int get_ink_level_canon(const int port, const char* device_file, 
                        const int portnumber, struct ink_level *level) {
  int fd;
  char cmdGetColors[] = 
    "SSR=BST,SFA,CHD,CIL,CIR,HRI,DBS,DWS,DOC,DSC,DJS,CTK,HCF;";
  char command[256];
  int length;
  int i = 0;
  char buffer[BUFLEN];
  char *indexDOC = NULL, *indexDWS = NULL, *indexCHD = NULL, *indexCIR = NULL;
  int retry = 6; /* You can change this, but keep same parity */
  levelTab lt;

  if ((port == BJNP) || (port == CUSTOM_BJNP)) {
    bjnp_get_printer_status(port, device_file,  portnumber, buffer);

    /* The command response is a list of semicolons-separated TOKEN:VALUE.
       example : DOC:4,00,NO;DWS:1512,1513;CHD:CL;
       First 2 bytes are response length. Ignoring them. */

    indexDOC = strstr(buffer+2, "DOC:");
    indexDWS = strstr(buffer+2, "DWS:");
    indexCHD = strstr(buffer+2, "CHD:");
    indexCIR = strstr(buffer+2, "CIR:");
  } else {
    do {
      fd = open_printer_device(port, device_file, portnumber);
      if (fd < 0) {
	return fd;
      }

      /* Get colors command */
      length = makeCommand_canon(cmdGetColors,
				 GET_STR_LENGTH(cmdGetColors),
				 command);

      i = write(fd, command, length);
      if (i < length) {

#ifdef DEBUG
	printf("Could not send command to printer\n");
#endif

	close(fd);
	return COULD_NOT_WRITE_TO_PRINTER;
      }

      length = read_from_printer(fd, buffer, BUFLEN, 0);
      if (length <= 0) {

#ifdef DEBUG    
	printf("Could not read from printer\n");
#endif
	close(fd);
	return COULD_NOT_READ_FROM_PRINTER;
      }
      /* Insert a terminator so that whe can do string operations */
      buffer[length] = '\0';

#ifdef DEBUG
      printf("Command Response: \n");
      for (i = 0; i<length; i++) {
	if (isprint(buffer[i])) 
	  printf("%c", (unsigned char) buffer[i]);
	else 
	  printf("\\x%02x", (unsigned char) buffer[i]);
      }
      printf("\n");
#endif

      /* The command response is a list of semicolons-separated TOKEN:VALUE.
	 example : DOC:4,00,NO;DWS:1512,1513;CHD:CL;
	 First 2 bytes are response length. Ignoring them. */
      indexDOC = strstr(buffer+2, "DOC:");
      indexDWS = strstr(buffer+2, "DWS:");
      indexCHD = strstr(buffer+2, "CHD:");
      indexCIR = strstr(buffer+2, "CIR:");
    
      close(fd);

    } while (!indexDOC && !indexDWS && !indexCHD && !indexCHD && --retry);
  }

  if (!indexDOC && !indexDWS && !indexCHD && !indexCHD) {
	  
#ifdef DEBUG
    printf("Could not parse output from printer\n");
#endif

    return COULD_NOT_PARSE_RESPONSE_FROM_PRINTER;
  }
  /* Check CIR ->Ink Fill Detail<- exact ink level */
  if(indexCIR) 
    decodeCIR(indexCIR,level);
  else {
    /* decodeCHD -> Cartridge type <-
       this fuction will update the level structure with default values depending
       of the printer definition */
    if(indexCHD) decodeCHD(indexCHD,level);
    /* At this state, we must have a valid status.
       otherwise that means the printer is not supported */
    if(level->status == RESPONSE_INVALID)	{
      return PRINTER_NOT_SUPPORTED;
    }

    /* before processing, set to 100% all ink levels. */
    for(i=0;i<MAX_CARTRIDGE_TYPES;i++) {
      lt[i] = 100;
    }
    /* Check DWS ->Warning State<- */
    if(indexDWS) decodeDWS(indexDWS,lt);

    /* Check DOC ->Operator Call<- */
    if(indexDOC) decodeDOC(indexDOC,lt);

    /* Need to put the level tab computed by the decodexxx functions
       into the struct inklevel before return */
    for(i=0;i<MAX_CARTRIDGE_TYPES;i++) {
      level->levels[i][INDEX_LEVEL] = lt[level->levels[i][INDEX_TYPE]];
    }
  }
#ifdef DEBUG
  printf("Ink levels : \n");
  for(i=0;i<MAX_CARTRIDGE_TYPES;i++) {
    if(level->levels[i][INDEX_TYPE] != CARTRIDGE_NOT_PRESENT) {
      printf("Level %d = %d\n",
	     level->levels[i][INDEX_TYPE],
	     level->levels[i][INDEX_LEVEL]);
    }
  }
#endif

  return OK;
}

int get_ink_level_canon_simple(const int mfd, const int port,
      const char* device_file, const int portnumber, struct ink_level *level) {
  int fd = mfd;
  char cmdGetColors[] =
    "SSR=BST,SFA,CHD,CIL,CIR,HRI,DBS,DWS,DOC,DSC,DJS,CTK,HCF;";
  char command[256];
  int length;
  int i = 0;
  char buffer[BUFLEN];
  char *indexDOC = NULL, *indexDWS = NULL, *indexCHD = NULL, *indexCIR = NULL;
  int retry = 6; /* You can change this, but keep same parity */
  levelTab lt;

  if ((port == BJNP) || (port == CUSTOM_BJNP)) {
    bjnp_get_printer_status(port, device_file,  portnumber, buffer);

    /* The command response is a list of semicolons-separated TOKEN:VALUE.
       example : DOC:4,00,NO;DWS:1512,1513;CHD:CL;
       First 2 bytes are response length. Ignoring them. */

    indexDOC = strstr(buffer+2, "DOC:");
    indexDWS = strstr(buffer+2, "DWS:");
    indexCHD = strstr(buffer+2, "CHD:");
    indexCIR = strstr(buffer+2, "CIR:");
  } else {
    do {
      //fd = open_printer_device(port, device_file, portnumber);
      if (fd < 0) {
        printf("fd < 0");
        return fd;
      }

      /* Get colors command */
      length = makeCommand_canon(cmdGetColors,
				 GET_STR_LENGTH(cmdGetColors),
				 command);

      i = write(fd, command, length);
      if (i < length) {

        #ifdef DEBUG
        printf("Could not send command to printer\n");
        #endif

        close(fd);
        return COULD_NOT_WRITE_TO_PRINTER;
      }

      length = read_from_printer(fd, buffer, BUFLEN, 0);
      if (length <= 0) {

        #ifdef DEBUG
        printf("Could not read from printer\n");
        #endif
        close(fd);
        return COULD_NOT_READ_FROM_PRINTER;
      }
      /* Insert a terminator so that whe can do string operations */
      buffer[length] = '\0';

      #ifdef DEBUG
      printf("Command Response: \n");
      for (i = 0; i<length; i++) {
      if (isprint(buffer[i]))
        printf("%c", (unsigned char) buffer[i]);
      else
        printf("\\x%02x", (unsigned char) buffer[i]);
      }
      printf("\n");
      #endif

      /* The command response is a list of semicolons-separated TOKEN:VALUE.
          example : DOC:4,00,NO;DWS:1512,1513;CHD:CL;
          First 2 bytes are response length. Ignoring them. */
      indexDOC = strstr(buffer+2, "DOC:");
      indexDWS = strstr(buffer+2, "DWS:");
      indexCHD = strstr(buffer+2, "CHD:");
      indexCIR = strstr(buffer+2, "CIR:");

      /* close(fd); */

    } while (!indexDOC && !indexDWS && !indexCHD && !indexCHD && --retry);
  }

  if (!indexDOC && !indexDWS && !indexCHD && !indexCHD) {

    #ifdef DEBUG
    printf("Could not parse output from printer\n");
    #endif

    return COULD_NOT_PARSE_RESPONSE_FROM_PRINTER;
  }
  /* Check CIR ->Ink Fill Detail<- exact ink level */
  if(indexCIR)
    decodeCIR(indexCIR,level);
  else {
    /* decodeCHD -> Cartridge type <-
       this fuction will update the level structure with default values depending
       of the printer definition */
    if(indexCHD) decodeCHD(indexCHD,level);
    /* At this state, we must have a valid status.
       otherwise that means the printer is not supported */
    if(level->status == RESPONSE_INVALID)	{
      return PRINTER_NOT_SUPPORTED;
    }

    /* before processing, set to 100% all ink levels. */
    for(i=0;i<MAX_CARTRIDGE_TYPES;i++) {
      lt[i] = 100;
    }
    /* Check DWS ->Warning State<- */
    if(indexDWS) decodeDWS(indexDWS,lt);

    /* Check DOC ->Operator Call<- */
    if(indexDOC) decodeDOC(indexDOC,lt);

    /* Need to put the level tab computed by the decodexxx functions
       into the struct inklevel before return */
    for(i=0;i<MAX_CARTRIDGE_TYPES;i++) {
      level->levels[i][INDEX_LEVEL] = lt[level->levels[i][INDEX_TYPE]];
    }
  }
  #ifdef DEBUG
  printf("Ink levels : \n");
  for(i=0;i<MAX_CARTRIDGE_TYPES;i++) {
    if(level->levels[i][INDEX_TYPE] != CARTRIDGE_NOT_PRESENT) {
      printf("Level %d = %d\n",
	     level->levels[i][INDEX_TYPE],
	     level->levels[i][INDEX_LEVEL]);
    }
  }
  #endif

  return OK;
}

/* Taken from CanonUtil::CanonUtilStatus.c */
static void decodeDWS(char *s, levelTab lt) {
  while ( *s && *s != ';' ) {
    if ( strncmp(s, "1900", 4) == 0 ) {
      s += 4;
    } else if ( strncmp(s, "N0", 2) == 0 ) {
      s += 2; 
    } else if ( ( strncmp(s, "1501", 4) == 0 ) ||
                ( strncmp(s, "1541", 4) == 0 ) ||
                ( strncmp(s, "1561", 4) == 0 ) ) {
      s += 4;
      lt[CARTRIDGE_BLACK] = LEVEL_LOW;
    } else if ( strncmp(s, "1502", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOBLACK] = LEVEL_LOW;
    } else if ( ( strncmp(s, "1510", 4) == 0 ) ||
                ( strncmp(s, "1542", 4) == 0 ) ||
                ( strncmp(s, "1562", 4) == 0 ) ) {
      s += 4;
      lt[CARTRIDGE_COLOR] = LEVEL_LOW;
    } else if ( strncmp(s, "1511", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_YELLOW] = LEVEL_LOW;
    } else if ( strncmp(s, "1512", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_MAGENTA] = LEVEL_LOW;
    } else if ( strncmp(s, "1513", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_CYAN] = LEVEL_LOW;
    } else if ( strncmp(s, "1534", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOMAGENTA] = LEVEL_LOW;
    } else if ( strncmp(s, "1535", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOCYAN] = LEVEL_LOW;
    } else if ( strncmp(s, "1507", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_BLACK] = LEVEL_LOW_70;
    } else if ( strncmp(s, "1571", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_YELLOW] = LEVEL_LOW_70;
    } else if ( strncmp(s, "1572", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_MAGENTA] = LEVEL_LOW_70;
    } else if ( strncmp(s, "1573", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_CYAN] = LEVEL_LOW_70;
    } else if ( strncmp(s, "1574", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOMAGENTA] = LEVEL_LOW_70;
    } else if ( strncmp(s, "1575", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOCYAN] = LEVEL_LOW_70;
    }  else if ( strncmp(s, "1508", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_BLACK] = LEVEL_LOW_40;
    } else if ( strncmp(s, "1581", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_YELLOW] = LEVEL_LOW_40;
    } else if ( strncmp(s, "1582", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_MAGENTA] = LEVEL_LOW_40;
    } else if ( strncmp(s, "1583", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_CYAN] = LEVEL_LOW_40;
    } else if ( strncmp(s, "1584", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOMAGENTA] = LEVEL_LOW_40;
    } else if ( strncmp(s, "1585", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOCYAN] = LEVEL_LOW_40;
    }

    if ( *s && *s != ';' )
      s++;
  }
}

/* Taken from CanonUtil::CanonUtilStatus.c */
static void decodeDOC(char *s, levelTab lt) {
  while ( *s && *s != ';' ) {
    if ( strncmp(s, "1000", 4) == 0 ) {
      s += 4;
    } else if ( strncmp(s, "1300", 4) == 0 ) {
      s += 4;
    } else if ( strncmp(s, "NO", 2) == 0 ) {
      s += 2;
    } else if ( strncmp(s, "1601", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_BLACK] = LEVEL_OUT;
    } else if ( strncmp(s, "1611", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_YELLOW] = LEVEL_OUT;
    } else if ((strncmp(s, "1612", 4) == 0) || (strncmp(s, "1660", 4) == 0)){
      s += 4;
      lt[CARTRIDGE_MAGENTA] = LEVEL_OUT;
    } else if ((strncmp(s, "1613", 4) == 0) || (strncmp(s, "1681", 4) == 0)){
      s += 4;
      lt[CARTRIDGE_CYAN] = LEVEL_OUT;
    } else if ( strncmp(s, "1634", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOMAGENTA] = LEVEL_OUT;
    } else if ( strncmp(s, "1635", 4) == 0 ) {
      s += 4;
      lt[CARTRIDGE_PHOTOCYAN] = LEVEL_OUT;
    }
      
    if ( *s && *s != ';' )
      s++;
  }
}

/* Partially taken from CanonUtil::CanonUtilStatus.c */
static void decodeCHD(char *s, struct ink_level *level) {
  int mdl,i,ic;

  i=0;
  level->status = RESPONSE_INVALID;
  /* First, try to find a substring matching a model in our database */
  mdl=0;
  while( (mdl<MAX_NUMBER_OF_MODELS) &&
         (!strstr(level->model,existingPrinters[mdl].model))
	 )
    {
      mdl++;
    }
  /* No model found, return and let the invalidated status */
  if(mdl == MAX_NUMBER_OF_MODELS) return;

  /* At this point, mdl is valid or is the index of the last model */
  /* Now trying to deduce the number of cartridges
     from the CHD response pattern*/
  while ( *s && *s != ';' && (level->status == RESPONSE_INVALID)) {
    i=0;
    while(existingPrinters[mdl].chdTypes[i].chd) {
      if(!strncmp(s,existingPrinters[mdl].chdTypes[i].chd,
                  strlen(existingPrinters[mdl].chdTypes[i].chd))) {
        /* Affect all cartridges to the level structure */
        for(ic=0;ic<existingPrinters[mdl].chdTypes[i].numberOfColors;ic++) {
	  level->levels[ic][INDEX_TYPE] =
	    existingPrinters[mdl].chdTypes[i].cartridgeTypes[ic];
	  /* by default ink level is 100. further indicators will give more
	     information */
	  level->levels[ic][INDEX_LEVEL] = 100;
	}

        level->status = RESPONSE_VALID;
        break;
      }
      i++;
    }
    s += 2;
  }
#ifdef DEBUG
  printf("%d colors found\n",existingPrinters[mdl].chdTypes[i].numberOfColors);
#endif
}

static void decodeCIR(char *s, struct ink_level *level) {
  /*   char *keys[9]={",K=",",BK=",",PBK=",",LC=",",LM=",",Y=",",M=",",C=",",CL="}; */
  /*  int values[9]={0,0,0,0,0,0,0,0,0}, i; */

  struct {
    short type;
    char *key;
  } cartridges[] = 
      {
	{CARTRIDGE_BLACK, ",K="},
	{CARTRIDGE_BLACK, ",BK="},
	{CARTRIDGE_PHOTOBLACK , ",PBK="}, 
	{CARTRIDGE_PHOTOCYAN, ",LC="}, 
	{CARTRIDGE_PHOTOMAGENTA, ",LM="}, 
	{CARTRIDGE_YELLOW, ",Y="}, 
	{CARTRIDGE_MAGENTA, ",M="}, 
	{CARTRIDGE_CYAN, ",C="}, 
	{CARTRIDGE_COLOR, ",CL="}
      };
#define NO_CARTRIDGES 9

  char *cart_level;	/* inklevel reported by printer */
  int i;		/* loop variable for cartridges table */
  int level_index =0;	/* next avaiable position in level table */

  /* skip CIR tag */
  s+=3;
  
  /* replace ":" by "," to ensure that all subtags start with "," */
  s[0]=',';

  for(i = 0; i < NO_CARTRIDGES; i++) { 
    if ((cart_level=strstr(s,cartridges[i].key)) != 0) {
      level->levels[level_index][INDEX_TYPE] = cartridges[i].type;
      sscanf(cart_level+strlen(cartridges[i].key),
             "%hd",&level->levels[level_index][INDEX_LEVEL]);
      level_index++;
    }
  }
  level->status = RESPONSE_VALID;
}

static int makeCommand_canon(const char * /* in */ cmd, int cmdLen,
			     char * /* out */ data) {
  int dataLen = 0;
  int length;
  char cmdHdr[]= "\x1b[K"; /* All commands begin with that, 
                              followed by the request length (2 bytes) */
  char cmdSeparator[] = "\x00\x1e";

#ifdef DEBUG
  int i;
#endif

  /* b0-b2 : First, command header */
  memcpy(data, cmdHdr, GET_STR_LENGTH(cmdHdr));
  dataLen = GET_STR_LENGTH(cmdHdr);

  /* b3-b4 : command length.
     4 bytes are command separator + command data length */
  length = cmdLen + 4;
  memcpy(data + dataLen, &length, 1); /* command length, 
					 may be a 2-byte little 
					 endian data */
  memset(data + dataLen + 1, 0, 1); /* this is not the clean way to 
				       encode a little-endian 
				       data... */
  dataLen += 2;

  /* b5-b6 : command separator */
  memcpy(data + dataLen, cmdSeparator,
	 GET_STR_LENGTH(cmdSeparator));
  dataLen += GET_STR_LENGTH(cmdSeparator);

  /* b7-b8 : command data length (including this parameter) */
  length = cmdLen + 2;
  memset(data + dataLen, 0, 1); /* this is not the clean way to 
				   encode a little-endian 
				   data... */
  memcpy(data + dataLen + 1, &length, 1); /* command length, 
					     may be a 2-byte big endian data */
  dataLen += 2;
   
  /* b9+ : the command data finally */
  memcpy(data + dataLen, cmd, cmdLen);
  dataLen += cmdLen;

#ifdef DEBUG
  printf("Command: \n");
  for (i = 0; i < dataLen; i++) {
    if (isprint(data[i])) 
      printf("%c", (unsigned char) data[i]);
    else 
      printf("\\x%02x", (unsigned char) data[i]);
  }
  printf("\n");
#endif

  return dataLen;
}
