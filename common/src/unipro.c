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

#include <stddef.h>
#include <string.h>
#include "bootrom.h"
#include "chipapi.h"
#include "debug.h"
#include "data_loading.h"
#include "greybus.h"
#include "fw_over_unipro.h"

static int cport_connected = 0;
int fwou_cport_connected(void) {
    if (cport_connected == 1) {
        /* Don't know what to do if it is already connected */
        return -1;
    }
    dbgprint("data port connected\r\n");

    chip_unipro_init_cport(FW_OVER_UNIPRO_CPORT);

    cport_connected = 1;
    return 0;
}

int fwou_cport_disconnected(void) {
    if (cport_connected == 0) {
        return -1;
    }
    return 0;
}

static int data_load_unipro_init(void) {
    int rc;

    /* poll until data cport connected */
    while (cport_connected == 0) {
        rc = chip_unipro_receive(CONTROL_CPORT, control_cport_handler);
        if (rc == -1) {
            dbgprint("unipro init failed\r\n");
            return -1;
        }
    }

    return 0;
}

static int data_load_unipro_load(void *dest, uint32_t length, bool hash) {
    return 0;
}

static void data_load_unipro_finish(void) {
}

data_load_ops unipro_ops = {
    .init = data_load_unipro_init,
    .read = NULL,
    .load = data_load_unipro_load,
    .finish = data_load_unipro_finish
};
