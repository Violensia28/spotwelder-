#ifndef BUILDVER_FIX_AUTOSHIM_H
#define BUILDVER_FIX_AUTOSHIM_H
// Build Version -> String Autoshim (aman walau -D BUILD_VERSION tanpa tanda kutip)

#ifndef BUILD_VERSION
#define BUILD_VERSION Build_6_2_3_SpotPattern
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define BUILD_VERSION_STR STR(BUILD_VERSION)

// Pastikan semua pemakaian BUILD_VERSION menjadi string literal
#undef BUILD_VERSION
#define BUILD_VERSION BUILD_VERSION_STR

#endif
