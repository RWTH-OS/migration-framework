#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "conf.hpp"

#ifdef LOG_USE_SYSTEMD
	#include <systemd/sd-journal.h>
	#define LOG_PRINT(PRIORITY, STRING) sd_journal_print(PRIORITY, STRING)
#endif
#ifdef LOG_USE_SYSLOG
	#include <syslog.h>
	#define LOG_PRINT(PRIORITY, STRING) syslog(PRIORITY, STRING)
#endif
#ifdef LOG_USE_STDOUT
	#include <cstdio>
	#define LOG_PRINT(PRIORITY, STRING) printf("%s%s\n", PRIORITY, STRING)
	// Define log priorities with same symbols like syslog.h.
	#define LOG_EMERG	"<0>"       /* system is unusable */
	#define LOG_ALERT	"<1>"       /* action must be taken immediately */
	#define LOG_CRIT	"<2>"       /* critical conditions */
	#define LOG_ERR         "<3>"       /* error conditions */
	#define LOG_WARNING     "<4>"       /* warning conditions */
	#define LOG_NOTICE      "<5>"       /* normal but significant condition */
	#define LOG_INFO        "<6>"       /* informational */
	#define LOG_DEBUG       "<7>"       /* debug-level messages */
#endif
#ifdef LOG_USE_NONE
	#define LOG_PRINT(PRIORITY, STRING) 
#endif

#endif
