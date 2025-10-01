#pragma once
#include <Arduino.h>
struct SSRPulseRec{
  uint32_t t0_ms; int preset_idx; bool dual; uint16_t pre_ms; uint16_t pause_ms; uint16_t main_ms;
  float i_start_mA; float i_post_mA; float v_start_V; float v_post_V;
};
struct SSRLog{ static String dumpCSV(); };
