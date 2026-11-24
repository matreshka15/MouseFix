#ifndef MOUSEFIX_VERSION_DEFS_H
#define MOUSEFIX_VERSION_DEFS_H

/* Single source of truth for the version and product name. version.h includes
   this for C code; MouseFix.rc includes it directly so the executable's
   version resource cannot drift from the About menu. */
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 9
#define APP_VERSION_STRING "1.0.9"

#define APP_NAME "MouseFix"
#define APP_AUTHOR "CodingTech"

#endif
