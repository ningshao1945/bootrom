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
#include <errno.h>
#include "bootrom.h"
#include "chipapi.h"
#include "unipro.h"
#include "tsb_unipro.h"
#include "debug.h"
#include "utils.h"

/* Statically allocate the CPort buffers in BufRam, 8kB each */
struct cport cporttable[] = {
    DECLARE_CPORT(0),  DECLARE_CPORT(1),  DECLARE_CPORT(2),  DECLARE_CPORT(3),
};

/* compiler hack to verify array size */
typedef char ___cport_num_test[(CPORT_MAX <=
                                ARRAY_SIZE(cporttable)) ?
                               1 : -1];

#define CPORT_SW_RESET_BITS 3
/**
 * CPort Reset Proceedure, implemented according to section 5.7.8.5
 * from ARA_ES3_APBridge_rev091.pdf
 */
static int tsb_unipro_reset_cport(uint32_t cportid) {
    int rc;
    uint32_t tx_reset_offset, rx_reset_offset;
    uint32_t tx_queue_empty_offset, tx_queue_empty_bit;

    if (cportid >= CPORT_MAX) {
        return -EINVAL;
    }

    tx_queue_empty_offset = CPB_TXQUEUEEMPTY_0 + ((cportid >> 5) << 2);
    tx_queue_empty_bit = (1 << (cportid & 31));

    while (!(getreg32(AIO_UNIPRO_BASE + tx_queue_empty_offset) &
                tx_queue_empty_bit)) {
    }

    tx_reset_offset = TX_SW_RESET_00 + (cportid << 2);
    rx_reset_offset = RX_SW_RESET_00 + (cportid << 2);

    putreg32(CPORT_SW_RESET_BITS,
             (volatile unsigned int*)(AIO_UNIPRO_BASE + tx_reset_offset));

    rc = chip_unipro_attr_write(T_CONNECTIONSTATE, 0, cportid, ATTR_LOCAL);
    if (rc) {
        dbgprint("Can't reset T_CONNECTIONSTATE\n");
        return -EIO;
    }

    rc = chip_unipro_attr_write(T_LOCALBUFFERSPACE, 0, cportid, ATTR_LOCAL);
    if (rc) {
        dbgprint("Can't reset T_LOCALBUFFERSPACE\n");
        return -EIO;
    }

    rc = chip_unipro_attr_write(T_PEERBUFFERSPACE, 0, cportid, ATTR_LOCAL);
    if (rc) {
        dbgprint("Can't reset T_PEERBUFFERSPACE\n");
        return -EIO;
    }

    rc = chip_unipro_attr_write(T_CREDITSTOSEND, 0, cportid, ATTR_LOCAL);
    if (rc) {
        dbgprint("Can't reset T_CREDITSTOSEND\n");
        return -EIO;
    }

    putreg32(CPORT_SW_RESET_BITS,
             (volatile unsigned int*)(AIO_UNIPRO_BASE + rx_reset_offset));
    putreg32(0, (volatile unsigned int*)(AIO_UNIPRO_BASE + tx_reset_offset));
    putreg32(0, (volatile unsigned int*)(AIO_UNIPRO_BASE + rx_reset_offset));
    return 0;
}

int tsb_reset_all_cports(void) {
    uint32_t i;
    int rc;

    for (i = 0; i < CPORT_MAX; i++) {
        rc = tsb_unipro_reset_cport(i);
        if (rc) {
            dbgprintx32("Can't reset cport 0x", i, "\n");
            return rc;
        }
    }
    dbgprint("Reset all cports\n");

    return 0;
}

/**
 * @brief Initialize a specific CPort
 */
int tsb_unipro_init_cport(uint32_t cportid) {
    struct cport *cport;

    if (cportid >= CPORT_MAX) {
        return -EINVAL;
    }

    cport = cport_handle(cportid);
    if (!cport) {
        return -EINVAL;
    }

    tsb_unipro_restart_rx(cport);

    return 0;
}

/**
 * @brief Receive a dynamically-assigned CPort identifier
 */
/*** TODO: x-ref to ~"boot sequence document" */
int tsb_unipro_recv_cport(uint32_t *cportid) {
    struct cport *cport;
    int rc;
    uint32_t cport_recv = 0;

    rc = read_mailbox(&cport_recv);
    if (rc) {
        return rc;
    }
    *cportid = --cport_recv;

    if (cport_recv >= CPORT_MAX) {
        return -EINVAL;
    }

    cport = cport_handle(cport_recv);
    if (!cport) {
        return -EINVAL;
    }

    return ack_mailbox((uint16_t)(cport_recv + 1));
}

uint32_t tsb_unipro_read(uint32_t offset) {
    return getreg32((volatile unsigned int*)(AIO_UNIPRO_BASE + offset));
}

void tsb_unipro_write(uint32_t offset, uint32_t v) {
    putreg32(v, (volatile unsigned int*)(AIO_UNIPRO_BASE + offset));
}

void tsb_unipro_restart_rx(struct cport *cport) {
    unsigned int cportid = cport->cportid;

    tsb_unipro_write(AHM_ADDRESS_00 + (cportid << 2), (uint32_t)cport->rx_buf);
    tsb_unipro_write(REG_RX_PAUSE_SIZE_00 + (cportid << 2),
                 RX_PAUSE_RESTART | CPORT_RX_BUF_SIZE);
}

/**
 * @brief Disable E2EFC on all CPorts
 */
void tsb_disable_all_e2efc(void) {
    tsb_unipro_write(CPB_RX_E2EFC_EN_0, 0);
    tsb_unipro_write(CPB_RX_E2EFC_EN_1, 0);
}

/**
 * @brief Chip-common parts of resetting before signalling readiness.
 */
void tsb_reset_before_ready(void) {
    tsb_disable_all_e2efc();
}

void tsb_reset_before_jump(void) {
    tsb_reset_all_cports();
}

/**
 * ES2/ES3 has the same definition for TSB_PowerState,
 * so let's have this function shared between ES2 and ES3 here
 */
void chip_wait_for_link_up(void) {
    int rc;
    uint32_t tempval;
    do {
        rc = chip_unipro_attr_read(TSB_POWERSTATE, &tempval, 0,
                                   ATTR_LOCAL);
    } while (!rc && (tempval != POWERSTATE_LINKUP));
}

/**
 * @brief send data down a CPort
 * @param cportid cport to send down
 * @param buf data buffer
 * @param len size of data to send
 * @param 0 on success, <0 on error
 */
int chip_unipro_send(unsigned int cportid, const void *buf, size_t len) {
    unsigned int i;
    struct cport *cport;
    char *data = (char*)buf;

    if (cportid >= CPORT_MAX || len > CPORT_BUF_SIZE) {
        return -1;
    }

    cport = cport_handle(cportid);
    if (!cport) {
        return -1;
    }

    /*
     * Data payload
     */
    for (i = 0; i < len; i++) {
        putreg8(data[i], &cport->tx_buf[i]);
    }

    /* Hit EOM */
    putreg8(1, CPORT_EOM_BIT(cport));

    return 0;
}

int chip_unipro_receive(unsigned int cportid,
                        unipro_rx_handler handler,
                        bool blocking) {
    uint32_t bytes_received;
    int rc = 0;
    struct cport *cport;

    uint32_t eom_nom_bit;
    uint32_t eom_err_bit;
    uint32_t eot_bit;

    uint32_t eom;
    uint32_t eot;

    cport = cport_handle(cportid);
    if (!cport) {
        return -1;
    }

    do {
        eom = tsb_unipro_read(AHM_RX_EOM_INT_BEF_0);
        eot = tsb_unipro_read(AHM_RX_EOT_INT_BEF_0);

        eom_nom_bit = (0x01 << (cportid << 1));
        eom_err_bit = (0x02 << (cportid << 1));
        eot_bit = (1 << cportid);

        if ((eom & eom_err_bit) != 0) {
            dbgprintx32("UniPro cport ", cportid, " Rx err\n");
            return -1;
        }
        if ((eot & eot_bit) != 0) {
            dbgprint("Rx data overflow\n");
            return -1;
        }
        if ((eom & eom_nom_bit) != 0) {
            bytes_received = tsb_unipro_read(CPB_RX_TRANSFERRED_DATA_SIZE_00 +
                                             (cportid << 2));
            tsb_unipro_write(AHM_RX_EOM_INT_BEF_0, eom_nom_bit);

            if (handler != NULL) {
                rc = handler(cportid,
                             cport->rx_buf,
                             bytes_received);
            }
            tsb_unipro_restart_rx(cport);
            return rc;
        }
    } while(blocking);
    return rc;
}
