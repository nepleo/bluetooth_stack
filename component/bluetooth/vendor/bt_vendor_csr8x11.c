#include "bt_vendor_csr8x11.h"
#include "bt_common.h"
#include "bt_pbuf.h"
#include "bt_hci.h"
#include "bt_phybusif_h4.h"

/*************************************************************************************************
HCI command  PACKETS Format:
	opcode 16 bit
	para total len 8 bit
	para 0
**************************************************************************************************/

/*************************************************************************************************
BCCMD PACKETS Format:
           |	  type   |	length   |	seqno   |		varid	|	status   |	payload������	|
 uint 16   |	    1    |       2     |      3     |       4       |     5      |        6~        |
**************************************************************************************************/

/*************************************************************************************************
BCCMD PS PACKETS Format:
           |	  header  |	key   |	length    |	stores      |		ps value  	|
 uint 16   |	    1-5   |    6    |      7     |       8        |      9~           |
**************************************************************************************************/
// minimal CSR init script to configure PSKEYs and activate them
uint8_t csr8x11_initscript[] =
{
    //  Set PSKEY_DEEP_SLEEP_STATE never deep sleep
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x02, 0x00, 0x03, 0x70, 0x00, 0x00, 0x29, 0x02, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,

    //  Set ANA_Freq to 26MHz
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x03, 0x00, 0x03, 0x70, 0x00, 0x00, 0xfe, 0x01, 0x01, 0x00, 0x08, 0x00, 0x90, 0x65,

    //  Set CSR_PSKEY_ANA_FTRIM 0x24 for csr8811
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x04, 0x00, 0x03, 0x70, 0x00, 0x00, 0xf6, 0x01, 0x01, 0x00, 0x08, 0x00, 0x24, 0x00,

    // Set CSR_PSKEY_DEFAULT_TRANSMIT_POWER 0x4
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x05, 0x00, 0x03, 0x70, 0x00, 0x00, 0x21, 0x00, 0x01, 0x00, 0x08, 0x00, 0x04, 0x00,

    // Set CSR_PSKEY_MAXIMUM_TRANSMIT_POWER 0x4
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x06, 0x00, 0x03, 0x70, 0x00, 0x00, 0x17, 0x00, 0x01, 0x00, 0x08, 0x00, 0x04, 0x00,

    // Set CSR_PSKEY_BLE_DEFAULT_TRANSMIT_POWER 0x4
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x07, 0x00, 0x03, 0x70, 0x00, 0x00, 0xc8, 0x22, 0x01, 0x00, 0x08, 0x00, 0x04, 0x00,

    // Set CSR_PSKEY_BDADDR
    0x19, 0xc2, 0x02, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x03, 0x70, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x08, 0x00, 0x20, 0x00, 0x99, 0x1a, 0x86, 0x00, 0x1d, 0x00,

    // Set CSR_PSKEY_PCM_CONFIG32
    0x15, 0xc2, 0x02, 0x00, 0x0a, 0x00, 0x09, 0x00, 0x03, 0x70, 0x00, 0x00, 0xb3, 0x01, 0x02, 0x00, 0x08, 0x00, 0x80, 0x08, 0x80, 0x18,

    // Set CSR_PSKEY_PCM_FORMAT 0x60
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x03, 0x70, 0x00, 0x00, 0xb6, 0x01, 0x01, 0x00, 0x08, 0x00, 0x60, 0x00,

    // Set CSR_PSKEY_USER_LOW_JITTER_MODE
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x0b, 0x00, 0x03, 0x70, 0x00, 0x00, 0xc9, 0x23, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,

    //  Set HCI_NOP_DISABLE
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x0c, 0x00, 0x03, 0x70, 0x00, 0x00, 0xf2, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,

    // Set UART baudrate to 921600
    0x15, 0xc2, 0x02, 0x00, 0x0a, 0x00, 0x0d, 0x00, 0x03, 0x70, 0x00, 0x00, 0xea, 0x01, 0x02, 0x00, 0x08, 0x00,0x0e,0x00,0x00,0x10,/*0x1b, 0x00, 0x40, 0x77,*/

    //  WarmReset
    0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x0e, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
int prepare_pos = 0;
unsigned char *pcsr8x11_initscript;

extern struct bt_pbuf_t *hci_cmd_ass(struct bt_pbuf_t *p, uint8_t ocf, uint8_t ogf, uint8_t len);
void csr8x11_vendor_init(init_done_cb cb)
{
    struct bt_pbuf_t *p;
    uint16_t size = 0;
    uint16_t varid;
    pcsr8x11_initscript = csr8x11_initscript+prepare_pos;
    size = pcsr8x11_initscript[0];
    prepare_pos += (size+1);

    varid = bt_le_read_16(pcsr8x11_initscript,8);
    if((p = bt_pbuf_alloc(BT_PBUF_TRANSPORT_H4, size+3, BT_PBUF_RAM)) == NULL)
    {
    BT_VENDOR_TRACE_ERROR("ERROR:file[%s],function[%s],line[%d] bt_pbuf_alloc fail\n",__FILE__,__FUNCTION__,__LINE__);
        return;
    }
    /* Assembling command packet */
    p = (struct bt_pbuf_t *)hci_cmd_ass(p, 0, HCI_VENDOR_OGF, size+3);

    memcpy((uint8_t *)p->payload+3,pcsr8x11_initscript+1,size);
    /* Assembling cmd prameters */
    phybusif_output(p, p->tot_len,PHYBUSIF_PACKET_TYPE_CMD);
    bt_pbuf_free(p);

    if(varid == WARM_RESET)
    {
        cb();
    }
}

chip_mgr_t csr8x11_mgr =
{
    /* const char * name */ "CSR8x11",
    /*void  (*vendor_init)(void);*/&csr8x11_vendor_init,
};

chip_mgr_t * csr8x11_instance()
{
    return &csr8x11_mgr;
}
