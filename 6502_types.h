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

// file created: Sept 17th, 2010
#ifndef _6502_types_h_
#define _6502_types_h_

/* These may change depending on platform */
typedef unsigned char  byte;
typedef unsigned short word;

/* Standard bit masks */
#define BIT0    0x01
#define BIT1    0x02
#define BIT2    0x04
#define BIT3    0x08
#define BIT4    0x10
#define BIT5    0x20
#define BIT6    0x40
#define BIT7    0x80

/* Status register masks */
#define FLAG_SIGN   BIT7    /* Sign Flag            */
#define FLAG_OVR    BIT6    /* Overflow Flag        */
#define FLAG_5      BIT5    /* Unused Flag          */
#define FLAG_SWI    BIT4    /* SWI Flag             */
#define FLAG_BCD    BIT3    /* Binary Coded Decimal */
#define FLAG_IRQE   BIT2    /* Interrupt Enable     */
#define FLAG_ZERO   BIT1    /* Zero Flag            */
#define FLAG_CARRY  BIT0    /* Carry Flag           */

#endif
