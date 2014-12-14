/* epson_new.h
 *
 * (c) 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

int get_ink_level_epson(const int port, const char *device_file,
			const int portnumber, struct ink_level *level);
