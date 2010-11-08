/*  This file is part of retrobox
    Copyright (C) 2010  James A. Shackleford

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "romreader.h"

#define _16KB 16384
#define  _8KB 8192

// Reads specified binary file and returns
// populated buffer of file contents
char*
read_file (char *filename)
{
    FILE *fp;
    char *buffer;
    unsigned int flen;

    // open the file
    fp = fopen (filename, "rb");
    if (!fp)
    {
        fprintf (stderr, "Unable to open %s\n\n", filename);
        exit (0);
    }
    
    // check file length
    fseek (fp, 0, SEEK_END);
    flen = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    printf ("Opened %s (%u bytes)\n", filename, flen);

    // allocate buffer for file contents
    buffer = (char *) malloc (flen + 1);
    if (!buffer)
    {
        fprintf (stderr, "Memory error!");
        fclose (fp);
        return;
    }

    // read file contents into buffer
    fread (buffer, flen, 1, fp);
    fclose (fp);

    return buffer;
}

void
read_ines (nes_rom* rom, char* buffer)
{
    char* tmp = buffer;

    // Read Byte 4
    rom->prg_rom_size = buffer[4];

    // Read Byte 5
    rom->chr_rom_size = buffer[5];

    // Read Byte 6 (Flags)
    rom->flg_mirroring = ((buffer[6] & BIT3) >> 2) | (buffer[6] & BIT0);
    rom->flg_sram = (buffer[6] & BIT1) >> 1;
    rom->flg_trainer = (buffer[6] & BIT2) >> 2;
    rom->mapper = (buffer[6] & 0xF0) >> 4;

    // Read Byte 7 (Flags)
    rom->mapper |= buffer[7] & 0xF0;
    rom->flg_vsunisystem = buffer[7] & BIT0;
    rom->flg_playchoice = (buffer[7] & BIT1) >> 1;
    rom->flg_nes20 = (buffer[7] & (BIT2 | BIT3)) >> 2;

    // Read Byte 8
    rom->prg_ram_size = buffer[8];

    // Read Byte 9
    rom->flg_tv = buffer[9] & BIT0;

    // Skip past the header
    tmp = buffer + 16*sizeof(byte);

    // Read Trainer
    if (rom->flg_trainer) {
        rom->trainer = (byte*) malloc (512*sizeof(byte));
        memcpy (rom->trainer, tmp, 512*sizeof(byte));

        // Move to start of PRG-ROM
        tmp += 512*sizeof(byte);
    }

    // Read PRG ROM
    if (rom->prg_rom_size) {
        rom->prg_rom = (byte*) malloc (_16KB * rom->prg_rom_size * sizeof(byte));
        memcpy (rom->prg_rom, tmp, _16KB * rom->prg_rom_size * sizeof(byte));
    } else {
        printf ("Bad ROM! (No PRG-ROM)\n");
        exit(0);
    }

    // Skip to start of CHR ROM
    tmp += _16KB * rom->prg_rom_size * sizeof(byte);

    // Read CHR ROM
    if (rom->chr_rom_size) {
        rom->chr_rom = (byte*) malloc (_8KB * rom->chr_rom_size * sizeof(byte));
        memcpy (rom->chr_rom, tmp, _8KB * rom->chr_rom_size * sizeof(byte));
    } else {
        // No CHR-ROM??
        // Well... I suppose we /should/ just allocate 8KB anyway
        // and memset to zeros??
        rom->chr_rom = (byte*) malloc (_8KB * sizeof(byte));
        memset (rom->chr_rom, 0, _8KB * sizeof(byte));
    }


    // Read PlayChoice Hint Screen
    if (rom->flg_playchoice) {
        tmp += _8KB * rom->chr_rom_size * sizeof(byte);
        rom->hint_scr = (byte*) malloc (_8KB*sizeof(byte));
        memcpy (rom->hint_scr, tmp, _8KB*sizeof(byte));
    }

}

nes_rom*
read_rom (char *filename)
{
    char *buffer;
    nes_rom *rom;
    char tmp1[4];

    rom = (nes_rom*) malloc (sizeof(nes_rom));

    buffer = read_file (filename);

    // Check for iNES file header
    memcpy (rom->type, buffer, 3*sizeof(char));
    if (!(strcmp(rom->type, "NES"))) {
        read_ines (rom, buffer);
    }

    return rom;
}
