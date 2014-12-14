/* util.c
 *
 * (c) 2003, 2004, 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "inklevel.h"
#include "util.h"

/* This function reads from the printer nonblockingly */
int read_from_printer(int fd, void *buf, size_t bufsize, int nonblocking) {
  int status;
  int retry = 10;
  struct pollfd ufds;

  memset(buf, 0, bufsize);

  if (nonblocking) {
    fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL));
  }

  do {
    ufds.fd = fd;
    ufds.events = POLLIN;
    ufds.revents = 0;
    if ((status = poll(&ufds, 1, 1000)) < 0) {
      break;
    }
    status = read(fd, buf, bufsize - 1);
    if ((status == 0) || ((status < 0) && (errno == EAGAIN))) {
      usleep(2000);
      status = 0;
    }
  } while ((status == 0) && (--retry != 0));

#ifdef DEBUG
  if ((status == 0) && (retry == 0)) {
    printf("Read from printer timed out\n");
  } else if (status < 0) {
    printf("Could not read from printer\n");
  }
#endif

  return status;
}


/* This function converts a string containing a two digit hexadecimal number
 * to an int 
 */

int my_axtoi(char* t) {
  int r = 0;

  switch (t[0]) {
  case '0':
    r = 0;
    break;
  case '1':
    r = 16;
    break;
  case '2':
    r = 32;
    break;
  case '3':
    r = 48;
    break;
  case '4':
    r = 64;
    break;
  case '5':
    r = 80;
    break;
  case '6':
    r = 96;
    break;
  case '7':
    r = 112;
    break;
  case '8':
    r = 128;
    break;
  case '9':
    r = 144;
    break;
  case 'a':
  case 'A':
    r = 160;
    break;
  case 'b':
  case 'B':
    r = 176;
    break;
  case 'c':
  case 'C':
    r = 192;
    break;
  case 'd':
  case 'D':
    r = 208;
    break;
  case 'e':
  case 'E':
    r = 224;
    break;
  case 'f':
  case 'F':
    r = 240;
    break;
  }

  switch (t[1]) {
  case '0':
    r += 0;
    break;
  case '1':
    r += 1;
    break;
  case '2':
    r += 2;
    break;
  case '3':
    r += 3;
    break;
  case '4':
    r += 4;
    break;
  case '5':
    r += 5;
    break;
  case '6':
    r += 6;
    break;
  case '7':
    r += 7;
    break;
  case '8':
    r += 8;
    break;
  case '9':
    r += 9;
    break;
  case 'a':
  case 'A':
    r += 10;
    break;
  case 'b':
  case 'B':
    r += 11;
    break;
  case 'c':
  case 'C':
    r += 12;
    break;
  case 'd':
  case 'D':
    r += 13;
    break;
  case 'e':
  case 'E':
    r += 14;
    break;
  case 'f':
  case 'F':
    r += 15;
    break;
  }

  return r;
}

/* This function converts a string containig a three digit decimal number
 * to an int 
 */

int my_atoi(char* t) {
  int r = 0;

  switch (t[0]) {
  case '0':
    r = 0;
    break;
  case '1':
    r = 100;
    break;
  case '2':
    r = 200;
    break;
  case '3':
    r = 300;
    break;
  case '4':
    r = 400;
    break;
  case '5':
    r = 500;
    break;
  case '6':
    r = 600;
    break;
  case '7':
    r = 700;
    break;
  case '8':
    r = 800;
    break;
  case '9':
    r = 900;
    break;
  }
  
  switch (t[1]) {
  case '0':
    r += 0;
    break;
  case '1':
    r += 10;
    break;
  case '2':
    r += 20;
    break;
  case '3':
    r += 30;
    break;
  case '4':
    r += 40;
    break;
  case '5':
    r += 50;
    break;
  case '6':
    r += 60;
    break;
  case '7':
    r += 70;
    break;
  case '8':
    r += 80;
    break;
  case '9':
    r += 90;
    break;
  }

  switch (t[2]) {
  case '0':
    r += 0;
    break;
  case '1':
    r += 1;
    break;
  case '2':
    r += 2;
    break;
  case '3':
    r += 3;
    break;
  case '4':
    r += 4;
    break;
  case '5':
    r += 5;
    break;
  case '6':
    r += 6;
    break;
  case '7':
    r += 7;
    break;
  case '8':
    r += 8;
    break;
  case '9':
    r += 9;
    break;
  }
  
  return r;
}

/*
 * Parse device id string to tag value pair table
 */
void tokenize_device_id(const char *string, char tags[NR_TAGS][BUFLEN]) {
  int i;
  int j;
  const char *c;

  memset(tags, 0, NR_TAGS*BUFLEN);

  /* Tokenize the device id */

  i = 0;
  j = 0;
  c = string;

  while ((*c != '\0') && (i < NR_TAGS)) {
    j = 0;
    while ((*c != '\0') && (*c != ';') && (j < BUFLEN -1)) {
      tags[i][j] = *c;
      c++;
      j++;
    }

    /* terminate string */

    tags[i][j]='\0';
    if (*c == ';') { /* Some printers do not terminate the last tag with ';' */
      c++; /* Skip the ';' */
    }
    i++;
  }

#ifdef DEBUG
  /* print all tags */

  for (i = 0; i < NR_TAGS; i++) {
    printf("%d: %s\n", i, tags[i]);
  }
#endif

}

/*
 * Retrieve value for given tag
 * An empty tag will return a string of length 0
 * If tag is not found, NULL is returned
 */
char *get_tag_value(char tags[NR_TAGS][BUFLEN], char *tag)
{
  int i = 0;
  while (i < NR_TAGS){
    if (strncmp(tags[i], tag, strlen(tag)) == 0){

#ifdef DEBUG
      printf("The %s tag has number %d\n", tag, i);
#endif

      return tags[i] + strlen(tag);
    }
    i++;
  }

#ifdef DEBUG
  printf ("Tag %s not found\n", tag);
#endif

  return NULL;
}

/*
 * Retrieve the index for a given tag. If the tag is not
 * found -1 will be returned.
 */
int get_tag_index(char tags[NR_TAGS][BUFLEN], char *tag) {
  int i = 0;
  while (i < NR_TAGS){
    if (strncmp(tags[i], tag, strlen(tag)) == 0){

#ifdef DEBUG
      printf("The %s tag has number %d\n", tag, i);
#endif

      return i;
    }
    i++;
  }

#ifdef DEBUG
  printf ("Tag %s not found\n", tag);
#endif

  return -1;
}
