/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

struct zip_struct;
typedef struct zip_struct zip;

char *zip_error (int err);

zip *zip_open (const char *fname, int *err);
void zip_close (zip *z);

iks *zip_load_xml (zip *z, const char *name, int *err);

unsigned long zip_get_size (zip *z, const char *name);
int zip_load (zip *z, const char *name, char *buf);
