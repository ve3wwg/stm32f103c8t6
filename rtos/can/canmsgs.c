/* canmsgs.c : Common CAN support
 * Warren W. Gay VE3WWG
 * Sun May 21 17:03:55 2017
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/f1/nvic.h>

#include "FreeRTOS.h"
#include "canmsgs.h"

static QueueHandle_t canrxq = 0;

/*********************************************************************
 * Queue a CAN message to be sent:
 *********************************************************************/

void
can_xmit(uint32_t id,bool ext,bool rtr,uint8_t length,void *data) {

	while ( can_transmit(CAN1,id,ext,rtr,length,(uint8_t*)data) == -1 )
		taskYIELD();
}

/*********************************************************************
 * Main CAN RX ISR routine for FIFO x
 *********************************************************************/

static void
can_rx_isr(uint8_t fifo,unsigned msgcount) {
        struct s_canmsg cmsg;
        bool xmsgidf, rtrf;

        while ( msgcount-- > 0 ) {
                can_receive(
                        CAN1,
                        fifo,                   // FIFO # 1
                        true,                   // Release      
                        &cmsg.msgid,
                        &xmsgidf,               // true if msgid is extended
                        &rtrf,                  // true if requested transmission
                        (uint8_t *)&cmsg.fmi,   // Matched filter index
                        &cmsg.length,           // Returned length
                        cmsg.data,
                        NULL);			// Unused timestamp
                cmsg.xmsgidf = xmsgidf;
                cmsg.rtrf = rtrf;
                cmsg.fifo = fifo;
                // If the queue is full, the message is lost
                xQueueSendToBackFromISR(canrxq,&cmsg,NULL);
        }
}

/*********************************************************************
 * CAN FIFO 0 ISR
 *********************************************************************/

void
usb_lp_can_rx0_isr(void) {
        can_rx_isr(0,CAN_RF0R(CAN1)&3);
}

/*********************************************************************
 * CAN FIFO 1 ISR
 *********************************************************************/

void
can_rx1_isr(void) {
        can_rx_isr(1,CAN_RF1R(CAN1)&3);
}

/*********************************************************************
 * Issue can_rx_callback() for each CAN message received
 *********************************************************************/

static void
can_rx_task(void *arg __attribute((unused))) {
        struct s_canmsg cmsg;

        for (;;) {
                if ( xQueueReceive(canrxq,&cmsg,portMAX_DELAY) == pdPASS )
			can_recv(&cmsg);
        }
}

/*********************************************************************
 * Initialize for CAN I/O
 *********************************************************************/

void
initialize_can(bool nart,bool locked,bool altcfg) {

        rcc_periph_clock_enable(RCC_AFIO);
        rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_CAN1EN);

	/*************************************************************
	 * When:
	 *	altcfg	CAN_RX=PB8,  CAN_TX=PB9
	 * 	!altcfg	CAN_RX=PA11, CAN_TX=PA12
	 *************************************************************/
	if ( altcfg ) {
	        rcc_periph_clock_enable(RCC_GPIOB);
	        gpio_set_mode(GPIOB,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,GPIO_CAN_PB_TX);
	        gpio_set_mode(GPIOB,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO_CAN_PB_RX);

	        gpio_primary_remap(                             // Map CAN1 to use PB8/PB9
	                AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF,      // Optional
	                AFIO_MAPR_CAN1_REMAP_PORTB);            // CAN_RX=PB8, CAN_TX=PB9
	} else	{
	        rcc_periph_clock_enable(RCC_GPIOA);
	        gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,GPIO_CAN_TX);
	        gpio_set_mode(GPIOA,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO_CAN_RX);

	        gpio_primary_remap(                             // Map CAN1 to use PA11/PA12
	                AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF,      // Optional
			AFIO_MAPR_CAN1_REMAP_PORTA);            // CAN_RX=PA11, CAN_TX=PA12
	}

	can_reset(CAN1);
        can_init(
                CAN1,
                false,                                  // ttcm=off
                false,                                  // auto bus off management
                true,                                   // Automatic wakeup mode.
                nart,                                   // No automatic retransmission.
                locked,                                 // Receive FIFO locked mode
                false,                                  // Transmit FIFO priority (msg id)
                PARM_SJW,	                        // Resynchronization time quanta jump width (0..3)
                PARM_TS1,				// segment 1 time quanta width
                PARM_TS2,	                        // Time segment 2 time quanta width
		PARM_BRP,				// Baud rate prescaler for 33.333 kbs
		false,					// Loopback
		false);					// Silent

	can_filter_id_mask_16bit_init(
		0,					// Filter bank 0
		0x000 << 5, 0x001 << 5,			// LSB == 0
		0x000 << 5, 0x001 << 5,			// Not used
		0,					// FIFO 0
		true);

	can_filter_id_mask_16bit_init(
		1,					// Filter bank 1
		0x010 << 5, 0x001 << 5,			// LSB == 1 (no match)
		0x001 << 5, 0x001 << 5,			// Match when odd
		1,					// FIFO 1
		true);

	canrxq = xQueueCreate(33,sizeof(struct s_canmsg));

	nvic_enable_irq(NVIC_USB_LP_CAN_RX0_IRQ);
	nvic_enable_irq(NVIC_CAN_RX1_IRQ);
	can_enable_irq(CAN1,CAN_IER_FMPIE0|CAN_IER_FMPIE1);

	xTaskCreate(can_rx_task,"canrx",400,NULL,configMAX_PRIORITIES-1,NULL);
}

// canmsgs.c
