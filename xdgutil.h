#ifndef XDGUTIL_H
#define XDGUTIL_H
#include "stdbool.h"

extern char *HOME;
extern char *TEMPDIR;
extern char *XDG_DATA_HOME;
extern char *XDG_CONFIG_HOME;
extern char *XDG_CACHE_HOME;

bool xdgutil_init(void);

#endif