#include <Arduino.h>
#include "ssr_logger.h"
#ifndef SSR_LOGGER_STUB
#define SSR_LOGGER_STUB 1
#endif
#if SSR_LOGGER_STUB
String SSRLog::dumpCSV(){
  return F("ts_ms,preset,mode,pre_ms,pause_ms,main_ms,i_start,i_post,v_start,v_post\n");
}
#endif
