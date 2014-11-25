#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "conf.hpp"

/**
 * \def LOG_PRINT(PRIORITY, STRING)
 * Write \a STRING with \a PRIORITY to log.
 * \example LOG_PRINT(LOG_NOTICE, "Hello World!");
 */

/**
 * \def LOG_STREAM(PRIORITY, STREAM)
 * Write \a STREAM with \a PRIORITY to log.
 * Uses a stringstream internally so most types like int or std::string can be logged without cast.
 * \example LOG_STREAM(LOG_NOTICE, "Hello World!" << std::string("string") << 42);
 */

/**
 * \def LOG_USE_SYSTEMD
 * Log to systemd journal system.
 * Defined in CMakeLists.txt.
 * \see LOG_USE_SYSLOG, LOG_USE_STDOUT, LOG_USE_NONE
 */

/**
 * \def LOG_USE_SYSLOG
 * Log to syslog.
 * Defined in CMakeLists.txt.
 * \see LOG_USE_SYSTEMD, LOG_USE_STDOUT, LOG_USE_NONE
 */

/**
 * \def LOG_USE_STDOUT
 * Log to stdout.
 * Defined in CMakeLists.txt.
 * \see LOG_USE_SYSTEMD, LOG_USE_SYSLOG, LOG_USE_NONE
 */

/**
 * \def LOG_USE_NONE
 * Disable logging.
 * Defined in CMakeLists.txt.
 * \see LOG_USE_SYSTEMD, LOG_USE_SYSLOG, LOG_USE_STDOUT
 */

#ifdef LOG_USE_SYSTEMD
	#include <systemd/sd-journal.h>
	#include <sstream>
	#define LOG_PRINT(PRIORITY, STRING) sd_journal_print(PRIORITY, STRING)
	#define LOG_STREAM(PRIORITY, STREAM) {std::ostringstream ss;ss<< STREAM ;sd_journal_print(PRIORITY, ss.str().c_str());}
#endif
#ifdef LOG_USE_SYSLOG
	#include <syslog.h>
	#include <sstream>
	#define LOG_PRINT(PRIORITY, STRING) syslog(PRIORITY, STRING)
	#define LOG_STREAM(PRIORITY, STREAM) {std::ostringstream ss;ss<< STREAM ;syslog(PRIORITY, ss.str().c_str());}
#endif
#ifdef LOG_USE_STDOUT
	#include <cstdio>
	#include <sstream>
	#define LOG_PRINT(PRIORITY, STRING) printf("%s%s\n", PRIORITY, STRING)
	#define LOG_STREAM(PRIORITY, STREAM) {std::ostringstream ss;ss<< STREAM ;printf("%s%s\n", PRIORITY, ss.str().c_str());}
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
	#define LOG_STREAM(PRIORITY, STREAM) 
#endif

#endif
