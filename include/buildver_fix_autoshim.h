
#ifndef BUILDVER_FIX_AUTOSHIM_H
#define BUILDVER_FIX_AUTOSHIM_H
// ---- Build Version -> String Autoshim ----
// Makes BUILD_VERSION safe even if defined without quotes in platformio.ini
// Example: -D BUILD_VERSION=Build_6_2_3_SpotPattern
// After this header, BUILD_VERSION becomes a string literal everywhere.

#ifndef BUILD_VERSION
// Fallback if not provided by platformio.ini
#define BUILD_VERSION Build_6_2_3_SpotPattern
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define BUILD_VERSION_STR STR(BUILD_VERSION)

#undef BUILD_VERSION
#define BUILD_VERSION BUILD_VERSION_STR

#endif /* BUILDVER_FIX_AUTOSHIM_H */
