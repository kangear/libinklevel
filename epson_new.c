/* epson_new.c
 *
 * (c) 2006 Markus Heinz
 *
 * Code taken from escputil (gutenprint 5.0.0).
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdarg.h>

#include "inklevel.h"
#include "platform_specific.h"
#include "epson_new.h"
#include "d4lib.h"

static int do_status_command_internal(void);
static int initialize_printer();
static void exit_packet_mode_old(int do_init);
static int open_raw_device(void);
static int init_packet(int fd, int force);
static void do_remote_cmd(const char *cmd, int nargs, ...);
static void add_resets(int count);
static void do_new_status(char *buf, int bytes);
static void do_old_status(const char *buf);
static void print_old_ink_levels(const char *ind);
static void start_remote_sequence(void);
static void end_remote_sequence(void);
static const char *looking_at_command(const char *buf, const char *cmd);
static const char *find_group(const char *buf);
static int get_digit(char digit);

extern int open_printer_device(const int port, const char* device_file, 
                               const int portnumber);
extern int read_from_printer(int fd, char *buf, int bufsize, int nonblocking);

static char printer_cmd[1025];
static int bufpos = 0;
static int isnew = 0;
static volatile int alarm_interrupt;
static int send_size = 0x0200;
static int receive_size = 0x0200;
static int socket_id = -1;
static char *printer_model = NULL;
static int my_port;
static char my_device[256];
static int my_portnumber;
static struct ink_level *my_level;

int get_ink_level_epson(const int port, const char *device_file, 
                        const int portnumber, struct ink_level *level) {
  int result;

  my_port = port;
  strncpy(my_device, device_file, 255);
  my_device[255] = '\0';
  my_portnumber = portnumber;
  my_level = level;

  result = initialize_printer();
  if (result == OK) {
    result = do_status_command_internal();
    level = my_level;
  }

  return result;
}

static void alarm_handler(int sig) {
  alarm_interrupt = 1;
}

static void exit_packet_mode_old(int do_init) {
  static char hdr[] = "\000\000\000\033\001@EJL 1284.4\n@EJL     \n\033@";
  memcpy(printer_cmd + bufpos, hdr, sizeof(hdr) - 1); /* DON'T include null! */

#ifdef DEBUG
  printf("Exit packet mode (%d)\n", do_init);
#endif

  bufpos += sizeof(hdr) - 1;
  if (!do_init) {
    bufpos -= 2;
  }
}

static void initialize_print_cmd(int do_init) {
  bufpos = 0;

#ifdef DEBUG
  printf("Initialize print command\n");
#endif

  if (isnew) {
    exit_packet_mode_old(do_init);
  }
}

static int do_status_command_internal() {
  int fd;
  int status;
  int credit;
  int retry = 4;
  char buf[1024];

#ifdef DEBUG
  printf("ink levels...\n");
#endif

  
  fd = open_raw_device();
  if (fd <0) {
    return fd;
  }

  if (isnew) {
    credit = askForCredit(fd, socket_id, &send_size, &receive_size);
    if (credit < 0) {

#ifdef DEBUG
      printf("\nCannot get credit\n");
#endif

      return COULD_NOT_GET_CREDIT;
    }

    /* request status command */
    status = writeData(fd, socket_id, (const unsigned char*)"st\1\0\1", 5, 1);
    if (status <= 0) {

#ifdef DEBUG
      printf("\nCannot write to printer: %s\n", strerror(errno));
#endif
      return COULD_NOT_WRITE_TO_PRINTER;
    }

    do {
      status = readData(fd, socket_id, (unsigned char*) buf, 1023);
      if (status < 0) {
        return COULD_NOT_READ_FROM_PRINTER;
      }

#ifdef DEBUG
      printf("readData try %d status %d\n", retry, status);
#endif

    } while ((retry-- != 0) && strncmp("st", buf, 2) && 
             strncmp("@BDC ST", buf, 7));

    /* "@BCD ST ST"  found */

    if (!retry)	{
      return COULD_NOT_PARSE_RESPONSE_FROM_PRINTER;
    }

    buf[status] = '\0';

    if (buf[7] == '2') {
      do_new_status(buf + 12, status - 12);
    } else {
      do_old_status(buf + 9);
    }
    
    CloseChannel(fd, socket_id);
  } else {
    do {
      add_resets(2);
      initialize_print_cmd(1);
      do_remote_cmd("ST", 2, 0, 1);
      add_resets(2);
      if (SafeWrite(fd, printer_cmd, bufpos) < bufpos) {

#ifdef DEBUG
        printf("Cannot write to printer: %s\n", strerror(errno));
#endif

        return COULD_NOT_WRITE_TO_PRINTER;
      }
      
      status = read_from_printer(fd, buf, 1024, 1);
      if (status < 0) {
        return COULD_NOT_READ_FROM_PRINTER;
      }
    } while (--retry != 0 && !status);
    
    buf[status] = '\0';
    
    if (status > 9) {
      do_old_status(buf + 9);
    } else {
      return NO_INK_LEVEL_FOUND;
    }
  }

  close(fd);

  return OK;
}

static int initialize_printer() {
  int packet_initialized = 0;
  int fd;
  int credit;
  int retry = 4;
  int tries = 0;
  int status;
  int forced_packet_mode = 0;
  char* pos;
  char* spos;
  unsigned char buf[1024];
  const char init_str[] = "\033\1@EJL ID\r\n";

#ifdef DEBUG
  int found = 0;
#endif

  fd = open_raw_device();
  if (fd < 0) {
    return fd;
  }

  do {
    alarm_interrupt = 0;
    signal(SIGALRM, alarm_handler);
    alarm(5);
    status = SafeWrite(fd, init_str, sizeof(init_str) - 1);
    alarm(0);
    signal(SIGALRM, SIG_DFL);

#ifdef DEBUG
    printf("status %d alarm %d\n", status, alarm_interrupt);
#endif

    if (status != sizeof(init_str) - 1 && (status != -1 || !alarm_interrupt)) {

#ifdef DEBUG
      printf("Cannot write to printer: %s\n", strerror(errno));
#endif
      return COULD_NOT_WRITE_TO_PRINTER;
    }

#ifdef DEBUG
    printf("Try old command %d alarm %d\n", tries, alarm_interrupt);
#endif

    status = read_from_printer(fd, (char*)buf, 1024, 1);

    if (status <= 0 && tries > 0) {
      forced_packet_mode = !init_packet(fd, 1);
      status = 1;
    }

    if (!forced_packet_mode && status > 0 && 
        !strstr((char *) buf, "@EJL ID") && tries < 3) {

#ifdef DEBUG
      printf("Found bad data: %s\n", buf);
#endif
      
      /*
       * We know the printer's not dead.  Try to turn off status and try again.
       */
      initialize_print_cmd(1);
      do_remote_cmd("ST", 2, 0, 0);
      add_resets(2);
      SafeWrite(fd, printer_cmd, bufpos);
      status = 0;
    }
    
    tries++;
  } while (status <= 0);

  if (forced_packet_mode || ((buf[3] == status) && (buf[6] == 0x7f))) {

#ifdef DEBUG 
    printf("Printer in packet mode....\n");
#endif
  
    packet_initialized = 1;
    isnew = 1;
    
    credit = askForCredit(fd, socket_id, &send_size, &receive_size);
    if (credit < 0) {
      
#ifdef DEBUG
      printf("Cannot get credit\n");
#endif
      
      return COULD_NOT_GET_CREDIT;
    }

    /* request status command */
    status = writeData(fd, socket_id, (const unsigned char*)"di\1\0\1", 5, 1);
    if (status <= 0) {
      
#ifdef DEBUG
      printf("\nCannot write to printer: %s\n", strerror(errno));
#endif

      return COULD_NOT_WRITE_TO_PRINTER;
    }
    
    do {
      status = readData(fd, socket_id, (unsigned char*)buf, 1023);
      if (status <= -1 ) {
        return COULD_NOT_READ_FROM_PRINTER;
      }

#ifdef DEBUG
      printf("readData try %d status %d\n", retry, status);
#endif
      
    } while ((retry-- != 0) && strncmp("di", (char*)buf, 2) &&
             strncmp("@EJL ID", (char*)buf, 7));

    if (!retry) {
      return COULD_NOT_PARSE_RESPONSE_FROM_PRINTER;
    }
  }

#ifdef DEBUG
  printf("status: %i\n", status);
  printf("Buf: %s\n", buf);
#endif

  if (status > 0) {
    pos = strstr((char*)buf, "@EJL ID");

#ifdef DEBUG
    printf("pos: %s\n", pos);
#endif

    if (pos) {
      pos = strchr(pos, (int) ';');
    }
     
#ifdef DEBUG
    printf("pos: %s\n", pos);
#endif

    if (pos) {
      pos = strchr(pos + 1, (int) ';');
    }

#ifdef DEBUG
    printf("pos: %s\n", pos);
#endif
    
    if (pos) {
      pos = strchr(pos, (int) ':');
    }
     
#ifdef DEBUG
    printf("pos: %s\n", pos);
#endif
    
    if (pos) {
      spos = strchr(pos, (int) ';');
    }
    
    if (!pos) {
      /*
       * Some printers seem to return status, but don't respond
       * usefully to @EJL ID.  The Stylus Pro 7500 seems to be
       * one of these.
       */
      if (status > 0 && strstr((char *) buf, "@BDC ST")) {
        
        /*
         * Set the printer model to something rational so that
         * attempts to describe parameters will succeed.
         * However, make it clear that this is a dummy,
         * so we don't actually try to print it out.
         */
         
#ifdef DEBUG
        printf("Can't find printer name, assuming Stylus Photo\n");
#endif

        printer_model = strdup("escp2-photo");
      } else {
        return ERROR;
      }
    } else {
      if (spos) {
        *spos = '\000';
      }

      printer_model = pos + 1;

#ifdef DEBUG
      printf("printer model: %s\n", printer_model);
#endif
      
    }
  }
  
  if (isnew && !packet_initialized) {
    isnew = !init_packet(fd, 0);
  }

  close(fd);

#ifdef DEBUG
  printf("new? %s found? %s\n", isnew ? "yes" : "no", found ? "yes" : "no");
#endif
  return OK;
}

static int open_raw_device(void) {
  int fd;

  fd = open_printer_device(my_port, my_device, my_portnumber);
  return fd;
}

static int init_packet(int fd, int force) {
  int status;

  if (!force) {

#ifdef DEBUG
    printf("Flushing data...\n");
#endif

    flushData(fd, (unsigned char) -1);
  }

#ifdef DEBUG
  printf("EnterIEEE...\n");
#endif

  if (!EnterIEEE(fd)) {
    return ERROR;
  }

#ifdef DEBUG
  printf("Init...\n");
#endif

  if (!Init(fd)) {
    return ERROR;
  }

#ifdef DEBUG
  printf("GetSocket...\n");
#endif

  socket_id = GetSocketID(fd, "EPSON-CTRL");
  if (!socket_id) {
    return ERROR;
  }

#ifdef DEBUG
  printf("OpenChannel...\n");
#endif

  switch (OpenChannel(fd, socket_id, &send_size, &receive_size)) {
  case -1:

#ifdef DEBUG
    printf("Fatal Error return 1\n");
#endif

    return ERROR; /* unrecoverable error */
    break;

  case  0:

#ifdef DEBUG  
    printf("Error\n"); /* recoverable error ? */
#endif

    return ERROR;
    break;
  }

  status = 1;

#ifdef DEBUG
  printf("Flushing data...\n");
#endif

  flushData(fd, socket_id);
  return OK;
}

static void do_remote_cmd(const char *cmd, int nargs, ...) {
  int i;
  va_list args;

  va_start(args, nargs);

  start_remote_sequence();
  memcpy(printer_cmd + bufpos, cmd, 2);

#ifdef DEBUG
  printf("Remote command: %s", cmd);
#endif

  bufpos += 2;
  printer_cmd[bufpos] = nargs % 256;
  printer_cmd[bufpos + 1] = (nargs >> 8) % 256;
  

#ifdef DEBUG
  printf(" %02x %02x", (unsigned) printer_cmd[bufpos], 
         (unsigned) printer_cmd[bufpos + 1]);
#endif

  if (nargs > 0) {
    for (i = 0; i < nargs; i++) {
      printer_cmd[bufpos + 2 + i] = va_arg(args, int);

#ifdef DEBUG
      printf(" %02x", (unsigned) printer_cmd[bufpos + 2 + i]);
#endif
    }
  }

#ifdef DEBUG
  printf("\n");
#endif

  bufpos += 2 + nargs;
  end_remote_sequence();
}

static void add_resets(int count) {
  int i;

#ifdef DEBUG
  printf("Add %d resets\n", count);
#endif

  for (i = 0; i < count; i++) {
    printer_cmd[bufpos++] = '\033';
    printer_cmd[bufpos++] = '\000';
  }
}

static void do_new_status(char *buf, int bytes) {
  /* static const char *colors_new[] = { */
  /*   "Black",		/\* 0 *\/ */
  /*   "Photo Black",	/\* 1 *\/ */
  /*   "Unknown",		/\* 2 *\/ */
  /*   "Cyan",		/\* 3 *\/ */
  /*   "Magenta",		/\* 4 *\/ */
  /*   "Yellow",		/\* 5 *\/ */
  /*   "Light Cyan",	        /\* 6 *\/ */
  /*   "Light Magenta",	/\* 7 *\/ */
  /*   "Unknown",		/\* 8 *\/ */
  /*   "Unknown",		/\* 9 *\/ */
  /*   "Light Black",	/\* a *\/ */
  /*   "Matte Black",	/\* b *\/ */
  /*   "Red",		/\* c *\/ */
  /*   "Blue",		/\* d *\/ */
  /*   "Gloss Optimizer",	/\* e *\/ */
  /*   "Light Light Black"	/\* f *\/ */
  /* }; */

  /* static int color_count = sizeof(colors_new) / sizeof(const char *); */

  static const int colors_new[] = {
    CARTRIDGE_BLACK,
    CARTRIDGE_PHOTOBLACK,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_CYAN,
    CARTRIDGE_MAGENTA,
    CARTRIDGE_YELLOW,
    CARTRIDGE_LIGHTCYAN,
    CARTRIDGE_LIGHTMAGENTA,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_LIGHTBLACK,
    CARTRIDGE_MATTEBLACK,
    CARTRIDGE_RED,
    CARTRIDGE_BLUE,
    CARTRIDGE_GLOSSOPTIMIZER,
    CARTRIDGE_LIGHTLIGHTBLACK
  };

  static int color_count = sizeof(colors_new) / sizeof(int);

  /* static const char *aux_colors[] = { */
  /*   "Black",		/\* 0 *\/ */
  /*   "Cyan",		/\* 1 *\/ */
  /*   "Magenta",		/\* 2 *\/ */
  /*   "Yellow",		/\* 3 *\/ */
  /*   "Unknown",		/\* 4 *\/ */
  /*   "Unknown",		/\* 5 *\/ */
  /*   "Unknown",		/\* 6 *\/ */
  /*   "Unknown",		/\* 7 *\/ */
  /*   "Unknown",		/\* 8 *\/ */
  /*   "Red",		/\* 9 *\/ */
  /*   "Blue",		/\* a *\/ */
  /*   "Unknown",		/\* b *\/ */
  /*   "Unknown",		/\* c *\/ */
  /*   "Unknown",		/\* d *\/ */
  /*   "Unknown",		/\* e *\/ */
  /*   "Unknown"		/\* f *\/ */
  /* }; */

  /* static int aux_color_count = sizeof(aux_colors) / sizeof(const char *); */

  static const int aux_colors[] = {
    CARTRIDGE_BLACK,
    CARTRIDGE_CYAN,
    CARTRIDGE_MAGENTA,
    CARTRIDGE_YELLOW,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_RED,
    CARTRIDGE_BLUE,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
    CARTRIDGE_UNKNOWN,
  };

  static int aux_color_count = sizeof(aux_colors) / sizeof(int);

  int i = 0;
  int j;
  int c = 0;
  const char *ind;

#ifdef DEBUG
  printf("New format bytes: %d bytes\n", bytes);
#endif

  while (i < bytes) {
    unsigned hdr = buf[i];
    unsigned total_param_count = buf[i + 1];
    unsigned param = buf[i + 2];

#ifdef DEBUG
    printf("Header: %x param count: %d\n", hdr, total_param_count);
#endif

    if (hdr == 0x0f) {	/* Always report ink */
      size_t count = (total_param_count - 1) / param;
      ind = buf + i + 3;

#ifdef DEBUG
      printf("%18s    %20s\n", "Ink color", "Percent remaining");
#endif

      for (j = 0; j < count; j++) {
        if (ind[0] < color_count) {

#ifdef DEBUG
          printf("%18d    %20d\n",colors_new[(int) ind[0]], ind[2]);
#endif
          
          my_level->status = RESPONSE_VALID;
          my_level->levels[c][INDEX_TYPE] = colors_new[(int) ind[0]];
          my_level->levels[c][INDEX_LEVEL] = (short) ind[2];
          c++;
        } else if (ind[j] == 0x40 && ind[1] < aux_color_count) {

#ifdef DEBUG
          printf("%18d    %20d\n", aux_colors[(int) ind[1]], ind[2]);
#endif

          my_level->status = RESPONSE_VALID;
          my_level->levels[c][INDEX_TYPE] = aux_colors[(int) ind[1]];
          my_level->levels[c][INDEX_LEVEL] = (short) ind[2];
          c++;
        } else {

#ifdef DEBUG
          printf("%8s 0x%2x 0x%2x    %20d\n",
                 "Unknown", (unsigned char) ind[0],
                 (unsigned char) ind[1], ind[2]);
#endif
        }

        ind += param;
      }
    }
    i += total_param_count + 2;
  }
}

static void do_old_status(const char *buf) {
  do {
    const char *ind;
    
    if ((ind = looking_at_command(buf, "IQ")) != 0)  {
	  
#ifdef DEBUG
      printf("%18s    %20s\n", "Ink color", "Percent remaining");
#endif

      print_old_ink_levels(ind);
    }

#ifdef DEBUG
    printf("looking at %s\n", buf);
#endif

  } while ((buf = find_group(buf)) != NULL);
}  

static void print_old_ink_levels(const char *ind) {
  /* static const char *old_colors[] = { */
  /*   "Black",        /\* 0 *\/ */
  /*   "Cyan",         /\* 1 *\/ */
  /*   "Magenta",      /\* 2 *\/ */
  /*   "Yellow",       /\* 3 *\/ */
  /*   "LightCyan",    /\* 4 *\/ */
  /*   "Light Magenta" /\* 5 *\/ */
  /* }; */

  /* static int old_color_count = sizeof(old_colors) / sizeof(const char *); */

  static const int old_colors[] = {
    CARTRIDGE_BLACK,
    CARTRIDGE_CYAN,
    CARTRIDGE_MAGENTA,
    CARTRIDGE_YELLOW,
    CARTRIDGE_LIGHTCYAN,
    CARTRIDGE_LIGHTMAGENTA
  };
  
  static int old_color_count = sizeof(old_colors) / sizeof(int);

  int i;
  int c = 0;

  for (i = 0; i < old_color_count; i++) {
    int val;
    
    if (!ind[0] || ind[0] == ';')
      return;
    val = (get_digit(ind[0]) << 4) + get_digit(ind[1]);

#ifdef DEBUG
    printf("%18d    %20d\n", old_colors[i], val);
#endif

    my_level->status = RESPONSE_VALID;
    my_level->levels[c][INDEX_TYPE] = old_colors[i];
    my_level->levels[c][INDEX_LEVEL] = val;
    c++;

    ind += 2;
  }
}

static void start_remote_sequence(void) {
  static char remote_hdr[] = "\033@\033(R\010\000\000REMOTE1";
  memcpy(printer_cmd + bufpos, remote_hdr, sizeof(remote_hdr) - 1);
  bufpos += sizeof(remote_hdr) - 1;

#ifdef DEBUG
  printf("Start remote sequence\n");
#endif
}

static void end_remote_sequence(void) {  
  static char remote_trailer[] = "\033\000\000\000\033\000";
  memcpy(printer_cmd + bufpos, remote_trailer, sizeof(remote_trailer) - 1);
  bufpos += sizeof(remote_trailer) - 1;

#ifdef DEBUG
  printf("End remote sequence\n");
#endif
}

static const char *looking_at_command(const char *buf, const char *cmd) {
  if (!strncmp(buf, cmd, strlen(cmd))) {
    const char *answer = buf + strlen(cmd);
    if (answer[0] == ':') {
      return (answer + 1);
    }
  }
  return NULL;
}

static const char *find_group(const char *buf) {
  while (buf[0] == ';') {
    buf++;
  }

  buf = strchr(buf, ';');
  if (buf && buf[1]) {
    return buf + 1;
  }  else {
    return NULL;
  }
}

static int get_digit(char digit) {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  } else if (digit >= 'A' && digit <= 'F') {
    return digit - 'A' + 10;
  } else if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + 10;
  } else {
    return 0;
  }
}
