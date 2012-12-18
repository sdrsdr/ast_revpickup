#ifndef AST_SYSLOG_MAGICK_H
#define AST_SYSLOG_MAGICK_H

#define SYSLOG_EMERG       0       /* system is unusable */
#define SYSLOG_ALERT       1       /* action must be taken immediately */
#define SYSLOG_CRIT        2       /* critical conditions */
#define SYSLOG_ERR         3       /* error conditions */
#define SYSLOG_WARNING     4       /* warning conditions */
#define SYSLOG_NOTICE      5       /* normal but significant condition */
#define SYSLOG_INFO        6       /* informational */
#define SYSLOG_DEBUG       7       /* debug-level messages */
extern void syslog (int __pri, __const char *__fmt, ...) __attribute__ ((__format__ (__printf__, 2, 3)));

#endif
