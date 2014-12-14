/* canon.h
 *
 * (c) 2009 Thierry Merle, Louis Lagendijk
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

int get_ink_level_canon(const int port, const char* device_file,
			const int portnumber, struct ink_level *level);
