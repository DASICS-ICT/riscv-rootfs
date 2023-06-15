/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_REGS_H_
#define INCLUDE_REGS_H_

/* This is for struct TrapFrame in scheduler.h
 * Stack layout for all exceptions:
 *
 * ptrace needs to have all regs on the stack. If the order here is changed,
 * it needs to be updated in include/asm-mips/ptrace.h
 *
 * The first PTRSIZE*5 bytes are argument save space for C subroutines.
 */

#define OFFSET_REG_ZERO         0

/* return address */
#define OFFSET_REG_RA           8

/* pointers */
#define OFFSET_REG_SP           16 // stack
#define OFFSET_REG_GP           24 // global
#define OFFSET_REG_TP           32 // thread

/* temporary */
#define OFFSET_REG_T0           40
#define OFFSET_REG_T1           48
#define OFFSET_REG_T2           56

/* saved register */
#define OFFSET_REG_S0           64
#define OFFSET_REG_S1           72

/* args */
#define OFFSET_REG_A0           80
#define OFFSET_REG_A1           88
#define OFFSET_REG_A2           96
#define OFFSET_REG_A3           104
#define OFFSET_REG_A4           112
#define OFFSET_REG_A5           120
#define OFFSET_REG_A6           128
#define OFFSET_REG_A7           136

/* saved register */
#define OFFSET_REG_S2           144
#define OFFSET_REG_S3           152
#define OFFSET_REG_S4           160
#define OFFSET_REG_S5           168
#define OFFSET_REG_S6           176
#define OFFSET_REG_S7           184
#define OFFSET_REG_S8           192
#define OFFSET_REG_S9           200
#define OFFSET_REG_S10          208
#define OFFSET_REG_S11          216

/* temporary register */
#define OFFSET_REG_T3           224
#define OFFSET_REG_T4           232
#define OFFSET_REG_T5           240
#define OFFSET_REG_T6           248

/* Size of stack frame, word/double word alignment */
#define OFFSET_SIZE             256

/* N-extension */
#define OFFSET_REG_USTATUS      0
#define OFFSET_REG_UEPC         8
#define OFFSET_REG_UBADADDR     16
#define OFFSET_REG_UCAUSE       24
#define OFFSET_REG_UTVEC        32
#define OFFSET_REG_UIE          40
#define OFFSET_REG_UIP          48
#define OFFSET_REG_USCRATCH     56

#define OFFSET_N_EXTENSION      64

/* dasics supervisor registers */
#define OFFSET_DASICSUMAINCFG       0
#define OFFSET_DASICSUMAINBOUNDHI   8
#define OFFSET_DASICSUMAINBOUNDLO   16

/* dasics user registers */
#define OFFSET_DASICSLIBCFG0        24
#define OFFSET_DASICSLIBCFG1        32
#define OFFSET_DASICSLIBBOUNDS0            40
#define OFFSET_DASICSLIBBOUNDS1            48
#define OFFSET_DASICSLIBBOUNDS2            56
#define OFFSET_DASICSLIBBOUNDS3            64
#define OFFSET_DASICSLIBBOUNDS4            72
#define OFFSET_DASICSLIBBOUNDS5            80
#define OFFSET_DASICSLIBBOUNDS6            88
#define OFFSET_DASICSLIBBOUNDS7            96
#define OFFSET_DASICSLIBBOUNDS8            104
#define OFFSET_DASICSLIBBOUNDS9            112
#define OFFSET_DASICSLIBBOUNDS10           120
#define OFFSET_DASICSLIBBOUNDS11           128
#define OFFSET_DASICSLIBBOUNDS12           136
#define OFFSET_DASICSLIBBOUNDS13           144
#define OFFSET_DASICSLIBBOUNDS14           152
#define OFFSET_DASICSLIBBOUNDS15           160
#define OFFSET_DASICSLIBBOUNDS16           168
#define OFFSET_DASICSLIBBOUNDS17           176
#define OFFSET_DASICSLIBBOUNDS18           184
#define OFFSET_DASICSLIBBOUNDS19           192
#define OFFSET_DASICSLIBBOUNDS20           200
#define OFFSET_DASICSLIBBOUNDS21           208
#define OFFSET_DASICSLIBBOUNDS22           216
#define OFFSET_DASICSLIBBOUNDS23           224
#define OFFSET_DASICSLIBBOUNDS24           232
#define OFFSET_DASICSLIBBOUNDS25           240
#define OFFSET_DASICSLIBBOUNDS26           248
#define OFFSET_DASICSLIBBOUNDS27           256
#define OFFSET_DASICSLIBBOUNDS28           264
#define OFFSET_DASICSLIBBOUNDS29           272
#define OFFSET_DASICSLIBBOUNDS30           280
#define OFFSET_DASICSLIBBOUNDS31           288

#define OFFSET_DASICSMAINCALLENTRY         296
#define OFFSET_DASICSRETURNPC              304
#define OFFSET_DASICSFREEZONERETURNPC      312

#define OFFSET_DASICS               320


#endif
