/* hp_new.h
 *
 * (c) 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

#include "inklevel.h"

int parse_device_id_old_hp(char tags[NR_TAGS][BUFLEN], int n,
			   struct ink_level *level);

int parse_device_id_new_hp(char tags[NR_TAGS][BUFLEN], int n,
			   struct ink_level *level);
