/*  This file is part of retrobox
    Copyright (C) 2010  Jamse A. Shackleford

    retrobox is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _romreader_h_
#define _romreader_h_

#include "6502_types.h"

typedef struct NES_ROM_struct nes_rom;
struct NES_ROM_struct {
    /* Bytes 0 - 3 */
    char type[4];

    /* Byte 4 */
    byte prg_rom_size;      // # of 16KB blocks

    /* Byte 5 */
    byte chr_rom_size;      // # of  8KB blocks (0 = CHR_RAM, not ROM)

    /* From Flag 6 */
    byte flg_mirroring;     // 0 = horizontal, 1 = vertical, 2 = 4-way)
    byte flg_sram;          // 0 = None, 1 = SRAM     (@ $6000-$7FFF)
    byte flg_trainer;       // 0 = None, 1 = Trainter (@ $7000-$71FF)

    /* From Flag 6 & 7 */
    byte mapper;            // MMC Number

    /* From Flag 7 */
    byte flg_vsunisystem;   // VS Unisystem
    byte flg_playchoice;    // PlayChoice-10 (8KB after CHR data)
    byte flg_nes20;         // 0 = iNES, 1 = iNES 2.0

    /* Byte 8 */
    byte prg_ram_size;      // # of 8KB blocks (0 = one 8KB block)

    /* From Flag 9 */
    byte flg_tv;            // 0 = NTSC, 1 = PAL

    byte* trainer;
    byte* prg_rom;
    byte* chr_rom;
    byte* hint_scr;
};


#if defined __cplusplus
extern "C" {
#endif

char* read_file (char *filename);
nes_rom* read_rom  (char *filename);
    

#if defined __cplusplus
}
#endif

#endif
