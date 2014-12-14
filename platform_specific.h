/* platform_specific.h
 *
 * (c) 2009 Markus Heinz
 *
 * This software is licensed under the terms of the GPL.
 * For details see file COPYING.
 */

int get_device_id(const int port, const char *device_file, 
                  const int portnumber, char *device_id);
int open_printer_device(const int port, const char* device_file, 
                        const int portnumber);
