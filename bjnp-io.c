/* bjnp-io.c
 *
 * (c) 2008, 2009 Louis Lagendijk
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <netdb.h>
#include <net/if.h>

#include "bjnp.h"
#include "inklevel.h"

#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

/* Local functions */
static int charTo2byte (char d[], char s[], int len);
static int set_cmd (struct BJNP_command *cmd, char cmd_code, int my_session_id,
         int payload_len);
static int udp_command (const struct sockaddr_in *addr, char *command, 
                      int cmd_len, char *response, int resp_len);
static int bjnp_get_printer_id (struct sockaddr_in *addr, char *IEEE1284_id);
static void get_printer_address (char *resp_buf, char *address, 
					char *name);
static int bjnp_send_broadcast (struct in_addr local_addr, 
			struct in_addr broadcast_addr,
                     	struct BJNP_command cmd, int size);
static int bjnp_discover_printers (struct printer_list *list);
static int bjnp_send_job_details (struct sockaddr_in *addr, char *user, 
			char *title);
static int bjnp_get_address_for_named_printer (const char *device_uri, 
				struct sockaddr_in *addr);

/* static data */

static int serial = 0;
static int session_id;
struct printer_list list[16];
static int num_printers = 0;

static int
charTo2byte (char d[], char s[], int len)
{
  /*
   * copy ASCII string to 2 byte unicode string
   * Returns: number of characters copied
   */

  int done = 0;
  int copied = 0;
  int i;

  for (i = 0; i < len; i++)
    {
      d[2 * i] = '\0';
      if (s[i] == '\0')
	{
	  done = 1;
	}
      if (done == 0)
	{
	  d[2 * i + 1] = s[i];
	  copied++;
	}
      else
	d[2 * i + 1] = '\0';
    }
  return copied;
}


static int
set_cmd (struct BJNP_command *cmd, char cmd_code, int my_session_id,
	 int payload_len)
{
  /*
   * Set command buffer with command code, session_id and lenght of payload
   * Returns: sequence number of command
   */
  strncpy (cmd->BJNP_id, BJNP_STRING, sizeof (cmd->BJNP_id));
  cmd->dev_type = BJNP_CMD_PRINT;
  cmd->cmd_code = cmd_code;
  cmd->seq_no = htonl (++serial);
  cmd->session_id = htons (my_session_id);

  cmd->payload_len = htonl (payload_len);

  return serial;
}



static int
udp_command (const struct sockaddr_in *addr, char *command, int cmd_len,
	     char *response, int resp_len)
{
  /*
   * Send UDP command and retrieve response
   * Returns: length of response or -1 in case of error
   */

  int sockfd;
  int numbytes;
  fd_set fdset;
  struct timeval timeout;
  int try;

  bjnp_debug (LOG_DEBUG, "Sending UDP command to %s:%d\n",
	      inet_ntoa (addr->sin_addr), ntohs (addr->sin_port));

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      bjnp_debug (LOG_CRIT, "udp_command: sockfd - %s\n", strerror (errno));
      return -1;
    }

  if (connect (sockfd, (struct sockaddr *) addr,
	       (socklen_t) sizeof (struct sockaddr_in)) != 0)
    {
      bjnp_debug (LOG_CRIT, "udp_command: connect - %s\n", strerror (errno));
      return -1;
    }

  for (try = 0; try < 3; try++)
    {
      if ((numbytes = send (sockfd, command, cmd_len, 0)) != cmd_len)
	{
	  bjnp_debug (LOG_CRIT, "udp_command: Sent only %d bytes of packet",
		      numbytes);
	}


      FD_ZERO (&fdset);
      FD_SET (sockfd, &fdset);
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      if (select (sockfd + 1, &fdset, NULL, NULL, &timeout) == 0)
	{
	  bjnp_debug (LOG_CRIT, "udpcommand: No data received (select)...\n");
	  continue;
	}

      if ((numbytes = recv (sockfd, response, resp_len, MSG_WAITALL)) == -1)
	{
	  bjnp_debug (LOG_CRIT, "udp_command: no data received (recv)");
	  continue;
	}
      close (sockfd);
      return numbytes;
    }
  /* max tries reached, return failure */
  close (sockfd);
  return -1;
}

static int
bjnp_get_printer_id (struct sockaddr_in *addr, char *IEEE1284_id)
{
  /*
   * get printer identity
   * Sets IEEE1284_id
   */

  struct BJNP_command cmd;
  struct IDENTITY *id;
  char printer_id[BJNP_IEEE1284_MAX];
  int resp_len;
  int id_len;
  char resp_buf[BJNP_RESP_MAX];

  /* set defaults */

  strcpy (IEEE1284_id, "");

  set_cmd (&cmd, CMD_UDP_GET_ID, 0, 0);

  bjnp_hexdump (LOG_DEBUG2, "Get printer identity", (char *) &cmd,
		sizeof (struct BJNP_command));

  resp_len =
    udp_command (addr, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX);

  if (resp_len <= 0)
    return COULD_NOT_READ_FROM_PRINTER;

  bjnp_hexdump (LOG_DEBUG2, "Printer identity:", resp_buf, resp_len);

  id = (struct IDENTITY *) resp_buf;

  id_len = ntohs (id->id_len) - sizeof (id->id_len);

  /* set IEEE1284_id */

  strncpy (printer_id, id->id, id_len);
  printer_id[id_len] = '\0';

  bjnp_debug (LOG_INFO, "Identity = %s\n", printer_id);

  if (IEEE1284_id != NULL)
    strcpy (IEEE1284_id, printer_id);
  return OK;
}


static void
get_printer_address (char *resp_buf, char *address, char *name)
{
  /*
   * Parse identify responses to ip-address
   * and lookup hostname
   */

  struct in_addr ip_addr;
  struct hostent *myhost;

  struct INIT_RESPONSE *init_resp;

  init_resp = (struct INIT_RESPONSE *) resp_buf;
  sprintf (address, "%u.%u.%u.%u",
	   init_resp->ip_addr[0],
	   init_resp->ip_addr[1],
	   init_resp->ip_addr[2], init_resp->ip_addr[3]);

  bjnp_debug (LOG_INFO, "Found printer at ip address: %s\n", address);

  /* do reverse name lookup, if hostname can not be fouund return ip-address */

  inet_aton (address, &ip_addr);
  myhost = gethostbyaddr (&ip_addr, sizeof (ip_addr), AF_INET);

  /* some buggy routers return noname if reverse lookup fails */

  if ((myhost == NULL) || (myhost->h_name == NULL) ||
      (strncmp (myhost->h_name, "noname", 6) == 0))

    /* no valid name found, so we will use the ip-address */

    strcpy (name, address);
  else
    /* we received a name, so we will use it */

    strcpy (name, myhost->h_name);
}

static int
bjnp_send_broadcast (struct in_addr local_addr, struct in_addr broadcast_addr,
		     struct BJNP_command cmd, int size)
{
  /*
   * send command to interface and return open socket
   */

  struct sockaddr_in locaddr;
  struct sockaddr_in sendaddr;
  int sockfd;
  int broadcast = 1;
  int numbytes;


  if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      bjnp_debug (LOG_CRIT, "discover_printer: sockfd - %s",
		  strerror (errno));
      return -1;
    }

  /* Set broadcast flag on socket */

  if (setsockopt
      (sockfd, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast,
       sizeof (broadcast)) != 0)
    {
      bjnp_debug (LOG_CRIT, "discover_printer: setsockopts - %s",
		  strerror (errno));
      close (sockfd);
      return -1;
    };

  /* Bind to local address of interface, use BJNP printer port */

  locaddr.sin_port = htons (BJNP_PORT_PRINT);
  locaddr.sin_addr = local_addr;
  memset (locaddr.sin_zero, '\0', sizeof locaddr.sin_zero);

  if (bind
      (sockfd, (struct sockaddr *) &locaddr,
       (socklen_t) sizeof (locaddr)) != 0)
    {
      bjnp_debug (LOG_CRIT, "discover_printer: bind - %s\n",
		  strerror (errno));
      close (sockfd);
      return -1;
    }

  /* set address to send packet to */

  sendaddr.sin_family = AF_INET;
  sendaddr.sin_port = htons (BJNP_PORT_PRINT);

  /* usebroadcast address of interface */
  sendaddr.sin_addr = broadcast_addr;
  memset (sendaddr.sin_zero, '\0', sizeof sendaddr.sin_zero);


  if ((numbytes = sendto (sockfd, &cmd, sizeof (struct BJNP_command), 0,
			  (struct sockaddr *) &sendaddr, size)) != size)
    {
      bjnp_debug (LOG_DEBUG,
		  "discover_printers: Sent only %d bytes of packet, error = %s\n",
		  numbytes, strerror (errno));
      /* not allowed, skip this interface */

      close (sockfd);
      return -1;
    }
  return sockfd;
}

static int
bjnp_discover_printers (struct printer_list *list)
{
  int numbytes = 0;
  struct BJNP_command cmd;
  char resp_buf[2048];
#ifdef HAVE_GETIFADDRS
  struct ifaddrs *interfaces;
  struct ifaddrs *interface;
#else
  struct in_addr broadcast;
  struct in_addr local;
#endif
  int socket_fd[BJNP_SOCK_MAX];
  int no_sockets;
  int i;
  int last_socketfd = 0;
  fd_set fdset;
  fd_set active_fdset;
  struct timeval timeout;

  FD_ZERO (&fdset);

  set_cmd (&cmd, CMD_UDP_DISCOVER, 0, 0);

#ifdef HAVE_GETIFADDRS

  /*
   * Send UDP broadcast to discover printers and return the list of printers found
   * Returns: number of printers found
   */

  num_printers = 0;
  getifaddrs (&interfaces);
  interface = interfaces;

  for (no_sockets = 0; (no_sockets < BJNP_SOCK_MAX) && (interface != NULL);)
    {
      /* send broadcast packet to each suitable  interface */

      if ((interface->ifa_addr == NULL) || (interface->ifa_broadaddr == NULL) ||

          (interface->ifa_addr->sa_family != AF_INET) ||
	  (((struct sockaddr_in *) interface->ifa_addr)->sin_addr.s_addr ==
	   htonl (INADDR_LOOPBACK)))
	{
	  /* not an IPv4 capable interface */

	  bjnp_debug (LOG_DEBUG,
		      "%s is not a valid IPv4 interface, skipping...\n",
		      interface->ifa_name);
	}
      else
	{
	  bjnp_debug (LOG_DEBUG, "%s is IPv4 capable, sending broadcast..\n",
		      interface->ifa_name);

	  if ((socket_fd[no_sockets] =
	       bjnp_send_broadcast (((struct sockaddr_in *)
				     interface->ifa_addr)->sin_addr,
				    ((struct sockaddr_in *)
				     interface->ifa_broadaddr)->sin_addr, cmd,
				    sizeof (cmd))) != -1)

	    {
	      if (socket_fd[no_sockets] > last_socketfd)
		{
		  /* track highest used socket for use in select */

		  last_socketfd = socket_fd[no_sockets];
		}
	      FD_SET (socket_fd[no_sockets], &fdset);
	      no_sockets++;
	    }
	}
      interface = interface->ifa_next;
    }
  freeifaddrs (interfaces);
#else
  /* 
   * we do not have getifaddrs(), so there is no easy way to find all interfaces
   * with teir broadcast addresses. We use a single global broadcast instead
   */
  no_sockets = 0;
  broadcast.s_addr = htonl (INADDR_BROADCAST);
  local.s_addr = htonl (INADDR_ANY);

  if ((socket_fd[no_sockets] =
       bjnp_send_broadcast (local, broadcast, cmd, sizeof (cmd))) != -1)
    {
      if (socket_fd[no_sockets] > last_socketfd)
	{
	  /* track highest used socket for use in select */

	  last_socketfd = socket_fd[no_sockets];
	}
      FD_SET (socket_fd[no_sockets], &fdset);
      no_sockets++;
    }
#endif

  /* wait for up to 1 second for a UDP response */

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  active_fdset = fdset;

  while (select (last_socketfd + 1, &active_fdset, NULL, NULL, &timeout) > 0)
    {
      bjnp_debug (LOG_DEBUG, "Select returned, time left %d.%d....\n",
		  timeout.tv_sec, timeout.tv_usec);

      for (i = 0; i < no_sockets; i++)
	{
	  if (FD_ISSET (socket_fd[i], &active_fdset))
	    {
	      if ((numbytes =
		   recv (socket_fd[i], resp_buf, sizeof (resp_buf),
			 MSG_WAITALL)) == -1)
		{
		  bjnp_debug (LOG_CRIT,
			      "discover_printers: no data received");
		  break;
		}
	      else
		{

		  bjnp_hexdump (LOG_DEBUG2, "Discover response:", &resp_buf,
				numbytes);


		  /* check if ip-address of printer is returned */

		  if ((numbytes != sizeof (struct INIT_RESPONSE))
		      || (strncmp ("BJNP", resp_buf, 4) != 0))
		    {
		      /* printer not found */
		      break;
		    }
		};


	      /* printer found, get IP-address and hostname */
	      get_printer_address (resp_buf,
				   list[num_printers].ip_address,
				   list[num_printers].hostname);
	      list[num_printers].port = BJNP_PORT_PRINT;
	      list[num_printers].addr.sin_family = AF_INET;
	      list[num_printers].addr.sin_port = htons (BJNP_PORT_PRINT);
	      list[num_printers].addr.sin_addr.s_addr =
		inet_addr (list[num_printers].ip_address);

	      num_printers++;
	    }
	}
      /* wait for 300 ms for next response */
      active_fdset = fdset;
      timeout.tv_sec = 0;
      timeout.tv_usec = 300 * 1000;
    }
  bjnp_debug (LOG_DEBUG, "printer discovery finished...\n");

  for (i = 0; i < no_sockets; i++)
    close (socket_fd[i]);

  return num_printers;
}


static int
bjnp_send_job_details (struct sockaddr_in *addr, char *user, char *title)
{
/* 
 * send details of printjob to printer
 * Returns: addrlist set to address details of used printer
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  char hostname[256];
  int resp_len;
  struct JOB_DETAILS *job;
  struct BJNP_command *resp;

  /* send job details command */

  set_cmd ((struct BJNP_command *) cmd_buf, CMD_UDP_PRINT_JOB_DET, 0,
	   sizeof (*job));

  /* create payload */

  gethostname (hostname, 255);

  job = (struct JOB_DETAILS *) (cmd_buf);
  charTo2byte (job->unknown, "", sizeof (job->unknown));
  charTo2byte (job->hostname, hostname, sizeof (job->hostname));
  charTo2byte (job->username, user, sizeof (job->username));
  charTo2byte (job->jobtitle, title, sizeof (job->jobtitle));

  bjnp_hexdump (LOG_DEBUG2, "Job details", cmd_buf,
		(sizeof (struct BJNP_command) + sizeof (*job)));

  bjnp_debug (LOG_DEBUG, "Connecting to %s:%d\n",
	      inet_ntoa (addr->sin_addr), ntohs (list->addr.sin_port));
  resp_len =
    udp_command (&list->addr, cmd_buf,
		 sizeof (struct BJNP_command) +
		 sizeof (struct JOB_DETAILS), resp_buf, BJNP_RESP_MAX);

  if (resp_len > 0)
    {
      bjnp_hexdump (LOG_DEBUG2, "Job details response:", resp_buf, resp_len);
      resp = (struct BJNP_command *) resp_buf;
      session_id = ntohs (resp->session_id);

      return OK;
    }
  return COULD_NOT_READ_FROM_PRINTER;
}

static int
bjnp_get_address_for_named_printer (const char *device_uri, 
				struct sockaddr_in *addr)
{
  char hostname[HOSTNAME_MAX];
  const char *c;
  int ipport = 0;
  int i;
  struct hostent *result;
  struct in_addr *addr_list;

  /* sanity check on input */

  if ((device_uri == NULL) || (strlen(device_uri) == 0))
    return ERROR;

  c = device_uri;
  if (strncasecmp (c, "bjnp://", 7) != 0)
    return BJNP_URI_INVALID;
  c = c + 7;

  i = 0;
  while ((*c != '\0') && (*c != '/') && (*c != ':') && (i < HOSTNAME_MAX - 1))
    {
      hostname[i] = *c;
      c++;
      i++;
    }
  hostname[i] = '\0';

  if (*c == ':')
    {
      while ((*c != '\0') && (*c != '/'))
	{
	  if ((*c < '0') && (*c > '9'))
	    return BJNP_URI_INVALID;
	  else
	    ipport = ipport * 10 + *c - '0';
	}
    }
  else
    ipport = BJNP_PORT_PRINT;

  if (*c == '/')
    c++;

  if (*c != '\0')
    return BJNP_URI_INVALID;

  result = gethostbyname (hostname);

  if ((result == NULL) || result->h_addrtype != AF_INET)
    {
      bjnp_debug (LOG_CRIT, "Cannot resolve hostname: %s\n", hostname);
      return BJNP_INVALID_HOSTNAME;
    }

  addr_list = (struct in_addr *) *result->h_addr_list;
  addr->sin_family = AF_INET;
  addr->sin_port = htons (ipport);
  addr->sin_addr = addr_list[0];

  return OK;
}

/* exported functions */

int
bjnp_get_printer_status (const int port_type, const char *device_uri, 
			const int port_number, char *status)
{
  /*
   * get printer status
   */

  struct BJNP_command cmd;
  struct IDENTITY *id;
  int resp_len;
  int id_len;
  char resp_buf[BJNP_RESP_MAX];
  struct sockaddr_in addr;

  if (port_type == BJNP)
    {
      if (port_number > num_printers)
        return NO_PRINTER_FOUND;
      else
        memcpy(&addr, &list[port_number].addr, sizeof(struct sockaddr_in));
    }
  else
    if(bjnp_get_address_for_named_printer(device_uri, &addr) != OK)
      return NO_PRINTER_FOUND;

  /* set defaults */

  strcpy (status, "");

  set_cmd (&cmd, CMD_UDP_GET_STATUS, 0, 0);

  bjnp_hexdump (10, "Get printer status", (char *) &cmd,
		sizeof (struct BJNP_command));

  resp_len =
    udp_command (&addr, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX);

  if (resp_len <= sizeof (struct BJNP_command))
    return -1;

  bjnp_hexdump (10, "Printer status:", resp_buf, resp_len);

  id = (struct IDENTITY *) resp_buf;

  id_len = ntohs (id->id_len) - sizeof (id->id_len);

  /* set status */

  strncpy (status, id->id, id_len);
  status[id_len] = '\0';

  bjnp_debug (7, "Status = %s\n", status);
  return 0;
}

int
bjnp_get_id_from_named_printer (const int port, const char *device_uri, char *device_id)
{
  struct sockaddr_in addr;
  int retval;
  if ((retval = bjnp_get_address_for_named_printer(device_uri, &addr)) != OK)
    return retval;
  return bjnp_get_printer_id (&addr, device_id);
}

int
bjnp_get_id_from_printer_port (const int port, char *device_id)
{
  /* 
   * As we are using responses from broadcasts, there is no ordering 
   * of the printers, so we prefer that the application provides the URI
   * This function may behave unpredictable if more than one printer
   * is found, as the ordering may change from one call to the next.
   */

  /* check if a successfull scan was done before, 
     we will then not scan again so we do not mess up the ordering */

  if (num_printers == 0)
   bjnp_discover_printers (list);

  if (port < num_printers)
    return bjnp_get_printer_id (&list[port].addr, device_id);
  return NO_PRINTER_FOUND;
}
