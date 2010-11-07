/*******************************************************
 *    Filename: 6502_test.c                            *
 *      Author: James A. Shackleford (tshack@ieee.org) *
 *        Date: Sept 20th, 2010                        *
 *     License: GPL                                    *
 *******************************************************/

// TODO: Rewrite command input for debugger prompt.  Single
//       character commands is... sad.  Also, need watch
//       and breakpoint lists...

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "6502.h"
#include "6502_types.h"
#include "2C02.h"
#include "timer.h"
#include "disasm.h"
#include "romreader.h"
#include "display.h"

static void
print_patterns (disp_inst* displayx, nes_rom* romx)
{
    int tile;
    int i,j;

    printf ("Dumping Pattern Tables... ");

    for (tile=0; tile<256; tile++) {
        for (j=0; j<8; j++) {
            for (i=0; i<8; i++) {
                displayx->pixels[j*tile+i] = i*j;
            }
        }
    }

    update_display (displayx);

    printf ("done.\n");
}

// Print stack (starting at top of stack)
static void
print_stack (cpu_inst *cpux)
{
    int i, j;
    byte SPtmp = cpux->SP;

    printf ("Starting from 0x%.2X:\n", SPtmp++);
    for (j=0; j < 16; j++) {
        for (i=0; i < 16; i++) {
            printf ("%.2X ", *cpux->mmap[0x0100+SPtmp++]);
        }
        printf ("\n");
    }
}

// Print registers
static void
print_regs (cpu_inst *cpux)
{
    printf ("PC: 0x%.4X  SP: 0x%.2X   A: 0x%.2X   X: 0x%.2X   Y: 0x%.2X\n", cpux->PC, cpux->SP, cpux->A, cpux->X, cpux->Y);
    printf ("P0: 0x%.4X  D0: 0x%.2X   ", cpux->P0, cpux->D0);
    printf ("S: [ %c%c%c%c%c%c%c%c ]  ",
            (cpux->S & FLAG_SIGN)  ? 'S' : '.',
            (cpux->S & FLAG_OVR)   ? 'V' : '.',
            (cpux->S & FLAG_5)     ? 'R' : '.',
            (cpux->S & FLAG_SWI)   ? 'B' : '.',
            (cpux->S & FLAG_BCD)   ? 'D' : '.',
            (cpux->S & FLAG_IRQE)  ? 'I' : '.',
            (cpux->S & FLAG_ZERO ) ? 'Z' : '.',
            (cpux->S & FLAG_CARRY) ? 'C' : '.');
    printf ("0x%.2X", cpux->S);
    printf ("\n");
}

// Print ROM information
static void
print_rom_info (nes_rom *romx)
{
    printf ("PRG-ROM size: %2i x 16KB (%i bytes)\n", romx->prg_rom_size, romx->prg_rom_size * 16384);
    printf ("CHR-ROM size: %2i x  8KB (%i bytes)\n", romx->chr_rom_size, romx->chr_rom_size * 8192);
    printf ("PRG-RAM size: %2i x  8KB (%i bytes)\n", romx->prg_ram_size, romx->prg_ram_size * 8192);
    switch (romx->flg_mirroring)
    {
        case 0x00:
            printf ("   Mirroring: Horizontal\n");
            break;
        case 0x01:
            printf ("   Mirroring: Vertical\n");
            break;
        case 0x02:
            printf ("   Mirroring: 4-way\n");
            break;
    }
    printf ("      Mapper: %i ", romx->mapper);
    switch (romx->mapper)
    {
        case 0:
            printf ("(NROM)\n");
            break;
        case 1:
            printf ("(Nintendo MMC1)\n");
            break;
        case 2:
            printf ("(UNROM switch)\n");
            break;
        case 3:
            printf ("(CNROM switch)\n");
            break;
        case 4:
            printf ("(Nintendo MMC3)\n");
            break;
        case 5:
            printf ("(Nintendo MMC5)\n");
            break;
        case 6:
            printf ("(FFE F4xxx)\n");
            break;
        case 7:
            printf ("(AOROM switch)\n");
            break;
        case 8:
            printf ("(FFE F3xxx)\n");
            break;
        case 9:
            printf ("(Nintendo MMC2)\n");
            break;
        case 10:
            printf ("(Nintendo MMC4)\n");
            break;
        case 11:
            printf ("(ColorDreams chip)\n");
            break;
        case 12:
            printf ("(FFE F6xxx)\n");
            break;
        case 15:
            printf ("(100-in-1 switch)\n");
            break;
        case 16:
            printf ("(Bandai chip)\n");
            break;
        case 17:
            printf ("(FFE F8xxx)\n");
            break;
        case 18:
            printf ("(Jaleco SS8806)\n");
            break;
        case 19:
            printf ("(Namcot 106 chip)\n");
            break;
        case 20:
            printf ("(Nintendo DiskSystem)\n");
            break;
        case 21:
            printf ("(Konami VRC4a)\n");
            break;
        case 22:
            printf ("(Konami VRC2a)\n");
            break;
        case 23:
            printf ("(Konami VRC2a)\n");
            break;
        case 24:
            printf ("(Konami VRC6)\n");
            break;
        case 25:
            printf ("(Konami VRC4b)\n");
            break;
        case 32:
            printf ("(Irem G-101 chip)\n");
            break;
        case 33:
            printf ("(Taito TC0190/TC0350)\n");
            break;
        case 34:
            printf ("(32KB ROM switch)\n");
            break;
        case 64:
            printf ("(Tengen RAMBO-1 chip)\n");
            break;
        case 65:
            printf ("(Irem H-3001 chip)\n");
            break;
        case 66:
            printf ("(GRROM switch)\n");
            break;
        case 67:
            printf ("(SunSoft3 chip)\n");
            break;
        case 68:
            printf ("(SunSoft4 chip)\n");
            break;
        case 69:
            printf ("(SunSoft5 FME-7 chip)\n");
            break;
        case 71:
            printf ("(Camerica chip)\n");
            break;
        case 78:
            printf ("(Irem 74HC161/32-based)\n");
            break;
        case 91:
            printf ("(Pirate HK-SF3 chip)\n");
            break;
        default:
            printf ("(Unknown)\n");
            break;
    }
    switch (romx->flg_tv)
    {
        case 0x00:
            printf ("   TV Format: NTSC\n");
            break;
        case 0x01:
            printf ("   TV Format: PAL\n");
            break;
    }
}


int
main (int argc, char* argv[])
{
    int i, j, cyc;
    char cmd = '\0';

    cpu_luts *cluts;        /* CPU Engine LUTs */
    cpu_inst *cpu0;         /* CPU Instance 0  */

    ppu_inst *ppu0;         /* PPU Instance 0 */
    disp_inst* display0;    /* NTSC Display */
    nes_rom* rom0;          /* Nintendo ROM Dump  */

    disasm_luts *dluts;     /* DisAsm Engine LUTs */
    disasm_inst *da0;       /* Disasm Instance 0  */



    /* Open rom from command line */
    if (argc > 1) {
        rom0 = read_rom (argv[1]);
    } else {
        printf ("No input ROM specified.\n\n");
        exit (0);
    }

    /* Bring up some Video */
    init_display ();
    display0 = make_display (
                  256,        // width
                  240,        // height
                  32,         // bit depth
                  0,          // 0 = windowed, 1 = fullscreen
                  0           // interpolation mode
               );

    /* Get 6502 running & all memory mapped up */
    cluts = init_6502_engine ();
    cpu0 = make_cpu (cluts);
    ppu0 = make_ppu ();
    init_nes_memorymap (cpu0, ppu0);
    ppu0->displayx = display0;

    /* Setup Mapper and run its init routine */
    cpu0->mapper_id = rom0->mapper;
    cpu0->rom0 = rom0;
    cpu0->mapper[cpu0->mapper_id](cpu0, 1);
    reset_cpu (cpu0);


    dluts = init_disasm_engine ();

    /* Attach dissassembler to cpu0 */
    da0 = make_disasm (dluts, cpu0);

    for (;;) {
        printf ("retrodbg] ");
        scanf ("%s", &cmd);
        
        switch (cmd)
        {
            // Render pattern table
            case 'm':
                print_patterns (display0, rom0);
                break;

            // Test
            case 't':
                for (i=0; i<4000000; i++) {
                    run_cpu (cpu0, 1);
                }
                printf ("\nResult:\n%s\n", cpu0->mmap[0x6004]);
                break;

            // eXecute
            case 'x':
                for (i=0; i<7900; i++) {
                    da0->PC = cpu0->PC;
                    disasm (cpu0, da0);
                    run_cpu (cpu0, 1);
                }
                break;

            // Go! (Just run 1000 opcodes)
            case 'g':
                for (i=0; i<1000; i++) {
                    da0->PC = cpu0->PC;
                    disasm (cpu0, da0);
                    run_cpu (cpu0, 1);
                }
                break;

            // Find (break at PC)
            case 'f':
                while (cpu0->PC != 0xE2FE) {
                    da0->PC = cpu0->PC;
                    disasm (cpu0, da0);
                    run_cpu (cpu0, 1);
                }
                break;

            // nes rom Info
            case 'i':
                print_rom_info (rom0);
                break;

            // Step (run 1 opcode)
            case 's':
                da0->PC = cpu0->PC;
                disasm (cpu0, da0);
                run_cpu (cpu0, 1);
                break;

            // List next 25 lines in memory
            case 'l':
                da0->PC = cpu0->PC;
                for (i=0; i<25; i++) {
                    disasm (cpu0, da0);
                }
                break;

            // Print registers
            case 'p':
                print_regs (cpu0);
                break;

            // Reset the CPU
            case 'r':
                reset_cpu (cpu0);
                printf ("CPU Reset\n");
                break;

            // Print the stacK
            case 'k':
                print_stack (cpu0);
                break;

            // Misc debug commands
            case '0':
                printf ("  PPUCTRL: %.2X\n", ppu0->PPUCTRL);
                printf ("  PPUMASK: %.2X\n", ppu0->PPUMASK);
                printf ("PPUSTATUS: %.2X\n", ppu0->PPUSTATUS);
                printf ("  OAMADDR: %.2X\n", ppu0->OAMADDR);
                printf ("  OAMDATA: %.2X\n", ppu0->OAMDATA);
                printf (" PPULATCH: %.2X\n", ppu0->PPULATCH);
                printf ("FINESCROL: %.2X\n", ppu0->FINESCROLL);
                printf ("  PPUADDR: %.2X\n", ppu0->PPUADDR);
                printf ("  PPUDATA: %.2X\n", ppu0->PPUDATA);
                printf ("\n");
                printf (" scanline: %i\n", ppu0->scanline);
                printf ("linecycle: %u\n", ppu0->linecycle);
                printf ("0xFFFE: %.2X\n", *cpu0->mmap[0xFFFE]);
                printf ("0xFFFF: %.2X\n", *cpu0->mmap[0xFFFF]);
//                printf ("NMI: 0x%.2X%.2X\n", *cpu0->mmap[0xFFFB], *cpu0->mmap[0xFFFA]);
                break;

            // Dump PPU Palettes
            case '9':
                for (i=0x00; i<0x20; i++) {
                    printf ("PPU %.4X\t%.2X\n", (i | 0x3F00), *ppu0->mmap[(0x3F00 | i)]);
                }
                break;

            // Dump OAM (1st column: 0-31, 2nd column: 32-63)
            case '8':
                printf ("ypos tidx attb xpos\typos tidx attb xpos\n");
                printf ("-------------------\t-------------------\n");
                for (i=0; i<32; i++) {
                    printf ("0x%.2X 0x%.2X 0x%.2X 0x%.2X\t0x%.2X 0x%.2X 0x%.2X 0x%.2X\n",
                            ppu0->OAM[4*(i+0)+0], ppu0->OAM[4*(i+0)+1],
                            ppu0->OAM[4*(i+0)+2], ppu0->OAM[4*(i+0)+3],
                            ppu0->OAM[4*(i+32)+0], ppu0->OAM[4*(i+32)+1],
                            ppu0->OAM[4*(i+32)+2], ppu0->OAM[4*(i+32)+3]);
                }
                break;

            // Print Nametable 1
            case '1':
                printf ("Nametable 1:\n");
                for (j=0; j<30; j++) {
                    for (i=0; i<32; i++) {
                        printf ("%.2X ", *ppu0->mmap[0x2000 | (j*32 + i)]);
                    }
                    printf ("\n");
                }
                break;

            // Print Nametable 2
            case '2':
                printf ("Nametable 2:\n");
                for (j=0; j<30; j++) {
                    for (i=0; i<32; i++) {
                        printf ("%.2X ", *ppu0->mmap[0x2400 | (j*32 + i)]);
                    }
                    printf ("\n");
                }
                break;

            // Print Nametable 3
            case '3':
                printf ("Nametable 3:\n");
                for (j=0; j<30; j++) {
                    for (i=0; i<32; i++) {
                        printf ("%.2X ", *ppu0->mmap[0x2800 | (j*32 + i)]);
                    }
                    printf ("\n");
                }
                break;

            // Print Nametable 4
            case '4':
                printf ("Nametable 4:\n");
                for (j=0; j<30; j++) {
                    for (i=0; i<32; i++) {
                        printf ("%.2X ", *ppu0->mmap[0x2C00 | (j*32 + i)]);
                    }
                    printf ("\n");
                }
                break;



            // Quit retrodbg
            case 'q':
                exit(0);
                break;
        }
    }

#if defined (commentout)
    for (i=0; i < 256; i++) {
        printf ("[%i] ", cpu0->cycles[i]);
        cpu0->opcode[i](cpu0);
        cpu0->amode[i](cpu0);
        printf ("\n");
    }
#endif

    /* Detach & destroy disassembler */
    destroy_disasm (&da0);

    /* Unload disassembly engine */
    unload_disasm_engine (&dluts);

    /* Free 6502 RAMs */
    free (cpu0->mmap[0x0000]);  //  RAM (malloced pointer)
    free (cpu0->mmap[0x2000]);  //  I/O (malloced pointer)
    free (cpu0->mmap[0x6000]);  // SRAM (malloced pointer)

    /* Destroy our virtual 6502 CPU */
    destroy_cpu (&cpu0);

    /* Unload the 6502 engine */
    unload_6502_engine (&cluts);

    /* Destroy the Display */
    destroy_display (display0);
    unload_display ();

    return 0;
}
