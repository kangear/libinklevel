/* bjnp.h
 *
 * (c) 2008, 2009 Louis Lagendijk
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

/*
 * Include standard headers 
 */

#  include <stdlib.h>
#  include <errno.h>
#  include <signal.h>
#  include <stdint.h>
#  include <string.h>
#  include <wchar.h>
#  include <unistd.h>
#  include <netinet/in.h>

/*
 * BJNP protocol related definitions
 */

/* port numbers */
typedef enum bjnp_port_e
{
  BJNP_PORT_PRINT = 8611,
  BJNP_PORT_SCAN = 8612,
  BJNP_PORT_3 = 8613,
  BJNP_PORT_4 = 8614
} bjnp_port_t;

#define BJNP_STRING "BJNP"

/* commands */
typedef enum bjnp_cmd_e
{
  CMD_UDP_DISCOVER = 0x01,	/* discover if service type is listening at this port */
  CMD_UDP_PRINT_JOB_DET = 0x10,	/* send print job owner details */
  CMD_UDP_CLOSE = 0x11,		/* request connection closure */
  CMD_TCP_PRINT = 0x21,		/* print */
  CMD_UDP_GET_STATUS = 0x20,	/* get printer status  */
  CMD_UDP_GET_ID = 0x30,	/* get printer identity */
  CMD_UDP_SCAN_JOB = 0x32	/* send scan job owner details */
} bjnp_cmd_t;

/* command type */

typedef enum uint8_t
{
  BJNP_CMD_PRINT = 0x1,		/* printer command */
  BJNP_CMD_SCAN = 0x2,		/* scanner command */
  BJNP_RES_PRINT = 0x81,	/* printer response */
  BJNP_RES_SCAN = 0x82		/* scanner response */
} bjnp_cmd_type_t;


struct BJNP_command
{
  char BJNP_id[4];		/* string: BJNP */
  uint8_t dev_type;		/* 1 = printer, 2 = scanner */
  uint8_t cmd_code;		/* command code/response code */
  uint32_t seq_no;		/* sequence number */
  uint16_t session_id;		/* session id for printing */
  uint32_t payload_len;		/* length of command buffer */
} __attribute__ ((__packed__));

/* Layout of the init response buffer */

struct INIT_RESPONSE
{
  struct BJNP_command response;	/* reponse header */
  char unknown1[6];		/* 00 01 08 00 06 04 */
  char mac_addr[6];		/* printers mac address */
  unsigned char ip_addr[4];	/* printers IP-address */
} __attribute__ ((__packed__));

/* layout of payload for the JOB_DETAILS command */

struct JOB_DETAILS
{
  struct BJNP_command cmd;	/* command header */
  char unknown[8];		/* don't know what these are for */
  char hostname[64];		/* hostname of sender */
  char username[64];		/* username */
  char jobtitle[256];		/* job title */
} __attribute__ ((__packed__));

/* Layout of ID and status responses */

struct IDENTITY
{
  struct BJNP_command cmd;
  uint16_t id_len;		/* length of identity */
  char id[2048];		/* identity */
} __attribute__ ((__packed__));


/* response to TCP print command */

struct PRINT_RESP
{
  struct BJNP_command cmd;
  uint32_t num_printed;		/* number of print bytes received */
} __attribute__ ((__packed__));

typedef enum bjnp_paper_status_e
{
  BJNP_PAPER_UNKNOWN = -1,
  BJNP_PAPER_OK = 0,
  BJNP_PAPER_OUT = 1
} bjnp_paper_status_t;


/* 
 *  BJNP definitions 
 */

#define BJNP_PRINTBUF_MAX 4096	/* size of printbuffer */
#define BJNP_CMD_MAX 2048	/* size of BJNP response buffer */
#define BJNP_RESP_MAX 2048	/* size of BJNP response buffer */
#define BJNP_SOCK_MAX 256	/* maximum number of open sockets */
#define BJNP_MODEL_MAX 64	/* max allowed size for make&model */
#define BJNP_IEEE1284_MAX 1024	/* max. allowed size of IEEE1284 id */
#define KEEP_ALIVE_SECONDS 3	/* max interval/2 seconds before we */
				/* send an empty data packet to the */
				/* printer */
#define HOSTNAME_MAX 128

/*
 * structure that stores information on found printers
 */

struct printer_list
{
  char ip_address[16];
  char hostname[256];		/* hostame, if found, else ip-address */
  int port;			/* udp/tcp port */
  struct sockaddr_in addr;	/* address/port of printer */
  char model[BJNP_MODEL_MAX];	/* printer make and model */
};

typedef enum bjnp_loglevel_e
{
  LOG_NONE,
  LOG_EMERG,
  LOG_ALERT,
  LOG_CRIT,
  LOG_ERROR,
  LOG_WARN,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
  LOG_DEBUG2,
  LOG_END			/* not a real loglevel, but indicates end */
                                /* of list */
} bjnp_loglevel_t;

#define LOGFILE "bjnp_log"
/*
 * debug related functions
 */

void bjnp_set_debug_level (char *level);
void bjnp_debug (bjnp_loglevel_t, const char *, ...);
void bjnp_hexdump (bjnp_loglevel_t level, char *header, const void *d_,
		   unsigned len);

int bjnp_get_id_from_named_printer (const int port_number, const char *device_file, char *device_id);
int bjnp_get_id_from_printer_port (const int port_number, char *device_id);
int bjnp_get_printer_status (const int port_type, const char *device_uri, const int portnumber, char *status);
