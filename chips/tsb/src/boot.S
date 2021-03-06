/**
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

.syntax		unified
.file		"boot.S"

#include "chipdef.h"

.text
.thumb
.thumb_func
.align	2

.globl bootrom_main

.globl	Reset_Handler
.type	Reset_Handler, %function
Reset_Handler:
    ldr sp, =_stack_top
    b bootrom_main

.globl chip_jump_to_image
.type chip_jump_to_image, %function
chip_jump_to_image:
    mov r10, r0

    ldr r0, =0
    mov r1, r0
    mov r2, r0
    mov r3, r0
    mov r4, r0
    mov r5, r0
    mov r6, r0
    mov r7, r0
    /* clear the BootRom .data, .bss */
    ldr r8, =_bootrom_data_area
    ldr r9, =_communication_area
clear_bootrom_data:
    str r0, [r8], #4
    cmp r8, r9
    bmi clear_bootrom_data

    ldr r8, =_bufram_start
    ldr r9, =_bufram_end
clear_bufram_before_jump:
    stmia r8!, {r0, r1, r2, r3, r4, r5, r6, r7}
    cmp r8, r9
    bmi clear_bufram_before_jump

    orr r10, #1    /* set Thumb mode for bx */
    bx r10

#if BOOT_STAGE == 1
.type exception_common_handler, %function
/*** Tight loop will lock up the bus - a feature */
exception_common_handler:
	b exception_common_handler

.section .vectors
.globl _vectors
_vectors:
	.word _stack_top
	.word __start
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler
	.word exception_common_handler

#ifdef BOOT_FROM_SLOW_ROM
.section .bootstrap.loader, "ax"
#else
.text
#endif

#ifdef BOOT_FROM_SLOW_ROM
.globl _bootstrap_vectors
_bootstrap_vectors:
	.word _stack_top
	.word __start
#endif

.globl __start
.type __start, %function
__start:
# TA-01 The Boot ROM begins executing at address 0x00000000...
#if CONFIG_CHIP_REVISION >= CHIP_REVISION_ES3
    ldr r1, =BOOTRET_O
    ldr r0, [r1]
    tst r0, #1
    bne resume_from_standby

.globl cold_boot
cold_boot:
#endif
#if CONFIG_CHIP_REVISION < CHIP_REVISION_ES3
    // ES2's ARM core doesn't seem to load the correct stack pointer
    ldr sp, =_stack_top
#endif

    /* Clear workram
     *
     * WARNING!!!: Assumption here is that workram will always be
     * a multiple of 32 bytes. If this becomes untrue for some interesting
     * reason, we have to modify the code here.
     *
     * NOTE: We do not clear workram if we're running on a chip simulator,
     * because the simulator has already zapped memory and this loop slows
     * down the simulation.
     */
    ldr r0, =0
    mov r1, r0
    mov r2, r0
    mov r3, r0
    mov r4, r0
    mov r5, r0
    mov r6, r0
    mov r7, r0
    /* TA-10 Write/Read data in workRAM */

    /* Clear all of work RAM */
    ldr r8, =_workram_start
    ldr r9, =_workram_end
clear_workram:
    stmia r8!, {r0, r1, r2, r3, r4, r5, r6, r7}
    cmp r8, r9
    bmi clear_workram

    /* clear the bufram */
    ldr r8, =_bufram_start
    ldr r9, =_bufram_end
clear_bufram:
    stmia r8!, {r0, r1, r2, r3, r4, r5, r6, r7}
    cmp r8, r9
    bmi clear_bufram

    /* initialize .data section */
    ldr r1, =_data_in_rom
    ldr r2, =_sdata
    ldr r3, =_edata
init_data_sec:
    cmp r2, r3
    bge end_init_data_sec
    ldr r0, [r1], #4
    str r0, [r2], #4
    b init_data_sec
end_init_data_sec:
#ifdef BOOT_FROM_SLOW_ROM
	b bootstrap
#else
	b bootrom_main
#endif

#ifdef BOOT_FROM_SLOW_ROM
.globl bootstrap
.extern copy_to_ram

/**
 * Under all chips prior to ES3, the bootrom is located in slow memory-mapped
 * SPI flash, and has to load itself into work RAM for performance.  The
 * bootstrap section is thus first copied into RAM, and then the CPU jumps into
 * the loaded bootstrap code.
 *
 * To support this, chip configurations es1tsb and es2tsb define the
 * BOOT_FROM_SLOW_ROM variable, which will include and build the bootstrap.
 */

bootstrap:
    ldr r0, =_bootstrap_lma
    ldr r1, =_bootstrap_size
    ldr r2, =_bootstrap_vma

bootstrap_copy:
    cbz r1, done_bootstrap_copy
    ldr r3, [r0], #4
    str r3, [r2], #4
    sub r1, r1, #4
    b bootstrap_copy

done_bootstrap_copy:
    ldr r0, =_text_size
    add r0, r0, #3
    bic r0, r0, #3
    ldr r1, =_stext_lma
    ldr r2, =_stext
    b copy_to_ram
#endif
#endif /* BOOT_STAGE == 1 */
