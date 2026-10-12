/* tools/mkbootimg/samsung_dtbh.h
**
** Copyright 2017, The LineageOS Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
 * The DTBH values of the Galaxy J5 2017, which dtbimg.c needs at compile time
 * (see the dummy in dtbhtool/samsung_dtbh.h: "You need to extract the values
 * for your target manually"). Same file, same values as the LineageOS device
 * trees of the Exynos7870 platform carry as include/samsung_dtbh.h
 * (a5y17lte), only DTBH_MODEL is J5Y17:
 *
 *   DTBH_MODEL    has to appear in the "model" property of every dtb that is
 *                 packed, "Samsung J5Y17 LTE OPEN rev00 board based on
 *                 Exynos7870" for this device. A dtb of another device in the
 *                 same kernel (a3y17lte, a5y17lte, ...) is skipped with a
 *                 warning instead of ending up as a table entry.
 *   DTBH_PLATFORM / DTBH_SUBTYPE
 *                 have to match the model_info-platform / model_info-subtype
 *                 of the dtbs ("android" / "samsung").
 *   ..._CODE      are the two values the entries of the DTBH table carry in
 *                 their platform and subtype fields: 0x50a6 and 0x217584da,
 *                 the same for every Exynos device of this generation. They
 *                 are *not* read from the dtb - the tool writes them into
 *                 every entry - so they have to be taken from a stock image:
 *                 they are what the bootloader compares against before it
 *                 picks an entry by chip / hw_rev.
 */

#ifndef _SAMSUNG_DTBH_H_
#define _SAMSUNG_DTBH_H_

#define DTBH_MAGIC         "DTBH"
#define DTBH_VERSION       2
#define DTBH_MODEL         "J5Y17"
#define DTBH_PLATFORM      "android"
#define DTBH_SUBTYPE       "samsung"
 /* Hardcoded entry */
#define DTBH_PLATFORM_CODE 0x000050a6
#define DTBH_SUBTYPE_CODE  0x217584da

/* DTBH_MAGIC + DTBH_VERSION + DTB counts */
#define DT_HEADER_PHYS_SIZE 12

/*
 * keep the eight uint32_t entries first in this struct so we can memcpy them to the file
 */
#define DT_ENTRY_PHYS_SIZE (sizeof(uint32_t) * 8)

#endif // _SAMSUNG_DTBH_H_
