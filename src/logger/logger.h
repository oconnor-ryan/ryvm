#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>


enum logger_level {
  LOGGER_LEVEL_DEBUG =    1,             //0b1, 
  LOGGER_LEVEL_INFO =     2,             //0b10,
  LOGGER_LEVEL_WARNING =  4,             //0b100,
  LOGGER_LEVEL_ERROR =    8,             //0b1000,
  LOGGER_LEVEL_VERBOSE =  16             //0b10000
};

int logger_has_level(enum logger_level l);
void logger_add_log_level(enum logger_level l);
void logger_remove_log_level(enum logger_level l);
void logger_set_log_level(uint32_t level_flags);
void logger_log_all_levels(void);
void logger_log_no_levels(void);


void logger_log(enum logger_level l, const char *str, ...);


#endif //LOGGER_H
