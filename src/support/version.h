/**
 * @file version.h
 * @brief ??????? ????? ???????.
 */

#ifndef BAA_VERSION_H
#define BAA_VERSION_H

#define BAA_VERSION "0.5.3"
#ifdef BAA_BUILD_DATE_OVERRIDE
#define BAA_BUILD_DATE BAA_BUILD_DATE_OVERRIDE
#else
#define BAA_BUILD_DATE __DATE__
#endif

#endif
