/* util.h
 *
 * (c) 2009 Markus Heinz, Louis Lagendijk
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "internal.h"
#include "inklevel.h"

int read_from_printer(int fd, void *buf, size_t bufsize, int nonblocking);
int my_axtoi(char *t);
int my_atoi(char *t);
void tokenize_device_id(const char *string, char tags[NR_TAGS][BUFLEN]);
char *get_tag_value(char tags[NR_TAGS][BUFLEN], char *tag);
int get_tag_index(char tags[NR_TAGS][BUFLEN], char *tag);
