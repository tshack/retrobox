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
