#include <stdio.h>
#include "logger.h"
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

//bit flags representing what levels to log
uint8_t logging_level_flags = 0;

int logger_has_level(enum logger_level l) {
  return (logging_level_flags & l) != 0;
}

void logger_add_log_level(enum logger_level l) {
  logging_level_flags |= l; 
}

void logger_remove_log_level(enum logger_level l) {
  logging_level_flags &= ~l;
  //0b1010 & ~(0b0010) = 0b1010 & 0b1101 = 0b1000
  //

}

void logger_log_all_levels(void) {
  logging_level_flags = LOGGER_LEVEL_DEBUG | LOGGER_LEVEL_INFO | LOGGER_LEVEL_WARNING | LOGGER_LEVEL_ERROR | LOGGER_LEVEL_VERBOSE;
}

void logger_set_log_level(uint32_t level_flags) {
  logging_level_flags = level_flags;
}

void logger_log_no_levels(void) {
  logging_level_flags = 0;
}


void logger_log(enum logger_level l, const char *str, ...) {
  if(!logger_has_level(l)) {
    return;
  }

  va_list args;
  va_start(args, str);

  //use vprintf when using va_list as a parameter
  vprintf(str, args);

  //perror will print a textual, OS-specific error based on the errno global.
  if(errno != 0) {
    perror("Error from Operating System: ");
  }

  va_end(args);
}
