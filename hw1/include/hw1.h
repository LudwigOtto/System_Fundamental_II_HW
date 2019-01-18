#ifndef HW_H
#define HW_H

#include "const.h"

#define HELP_CODE 0x8000
#define FAULT_CODE 0x0000
#define FMC_CODE 0x4000
#define DCRPT_CODE 0x2000
#define ROW_CODE 0x00F0
#define COL_CODE 0x000F
#define DFLTRC_CODE 0x00AA
#define INIT_ROW_BIT 4
#define INIT_COL_BIT 0
#define LENGTH_POLY_ALPHABET 94 // 0x21 - 0x7e
#define LENGTH_FM_ALPHABET 26
int activateCrypto(unsigned short mode);

#endif
