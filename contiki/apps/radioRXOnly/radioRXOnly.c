#include "contiki.h"
#include "cpu.h"
#include "dev/leds.h"
#include "dev/gpio.h"
#include "dev/gptimer.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "nvic.h"
#include "lpm.h"
#include "cc2538-rf.h"
#include "simple_network_driver.h"
#include "rf-header-parse.h"
#include "ieee-addr.h"
#include "dev/crypto.h"
#include "dev/ccm.h"
#include "spi-arch.h"
#include "spi.h"
#include "dev/ssi.h"


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define SPI_CS_GPIO_NUM GPIO_C_NUM
#define SPI_CS_GPIO_PIN 0


PROCESS(radioRXOnlyProcess, "Radio RX Process");
AUTOSTART_PROCESSES(&radioRXOnlyProcess);
packet_header_t rx_pkt_header;

void rf_rx_handler(){
	process_poll(&radioRXOnlyProcess);
}   

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(radioRXOnlyProcess, ev, data)
{
	PROCESS_BEGIN();
	// AES
	static uint8_t myMic[8] = {0x0};
	static uint8_t myNonce[13] = {0};
	static uint8_t aes_key[] = AES_KEY;
	aes_load_keys(aes_key, AES_KEY_STORE_SIZE_KEY_SIZE_128, 1, 0);

	static uint8_t srcExtAddr[8];
	simple_network_set_callback(&rf_rx_handler);
	NETSTACK_RADIO.on();
	static uint8_t packetPayload[128];
	static uint8_t packetDuplicated[123];
	static uint8_t* cData = packetDuplicated;
	static uint8_t* aData = srcExtAddr;


	uint16_t i = 0;                                                             
	while(1){
		PROCESS_YIELD();
		leds_on(LEDS_RED);
		uint8_t *packet_hdr = packetbuf_hdrptr();                                   
		//uint8_t hdr_length = packetbuf_hdrlen();                                    
		uint8_t *packet_ptr = packetbuf_dataptr();                                  
		uint16_t packet_length = packetbuf_datalen();                               
		process_packet_header(&rx_pkt_header, packet_hdr);

		memcpy(packetPayload, packet_ptr, packet_length);
		printf("Received a packet from: ");
		uint8_t src_addr_len = rx_pkt_header.pkt_src_addr_len;
		if (src_addr_len>0){
			for (i=0; i<src_addr_len; i++){
				srcExtAddr[i] = rx_pkt_header.pkt_src_addr[src_addr_len - 1 - i];
				printf("0x%x ", srcExtAddr[i]);
			}
			printf("\r\n");
		}

		if (packet_ptr[0]==0xaa){
			// meter data
			int meterData = (packet_ptr[5]<<24 | packet_ptr[4]<<16 | packet_ptr[3]<<8 | packet_ptr[2]);
			printf("Meter data: %d mW\r\n", meterData);
		}
		else if ((packet_ptr[0]==AES_PKT_IDENTIFIER) || (packet_ptr[0]==AES_PKT_IDENTIFIER+1) || packet_ptr[0]==TRIUMVI_PKT_IDENTIFIER){
			printf("Encrypted message...\r\n");
			memcpy(myNonce, srcExtAddr, 8);
			memcpy(&myNonce[9], &packetPayload[1], 4); // Nonce[8] should be 0
			uint8_t auth_res;
			uint8_t myPDATA_LEN = PDATA_LEN;
			// Old triumvi format, without status register
			switch (packet_ptr[0]){
				case AES_PKT_IDENTIFIER:
					myPDATA_LEN -= 1;
				break;
				case AES_PKT_IDENTIFIER+1:
					myPDATA_LEN += 1;
				break;
			}
			
			uint8_t i;
			uint8_t numOfTrial;
			numOfTrial = (packet_ptr[0]==TRIUMVI_PKT_IDENTIFIER)? 2 : 1;
			for (i=0; i<numOfTrial; i++){
				memcpy(packetDuplicated, &packetPayload[5], packet_length-5);
				ccm_auth_decrypt_start(LEN_LEN, 0, myNonce, aData, ADATA_LEN, 
					cData, (myPDATA_LEN+MIC_LEN), MIC_LEN, NULL);
				while (ccm_auth_decrypt_check_status()!=AES_CTRL_INT_STAT_RESULT_AV){}
				auth_res = ccm_auth_decrypt_get_result(cData, myPDATA_LEN+MIC_LEN, myMic, MIC_LEN);
				if (auth_res==CRYPTO_SUCCESS)
					break;
				// Only for Triumvi packet identidier, since it chould have 
				// two different length
				else
					myPDATA_LEN += 2;
			}

			if (auth_res==CRYPTO_SUCCESS){
				printf("Authentication success\r\n");
				if (packet_ptr[0]==AES_PKT_IDENTIFIER+1){
					printf("Panel ID: %x\r\n", cData[4]);
					printf("Circuit #: %d\r\n", cData[5]);
				}
				else if (packet_ptr[0]==TRIUMVI_PKT_IDENTIFIER){
					printf("Status Register: %x\r\n", cData[4]);
					if (cData[4]&BATTERYPACK_STATUSREG){
						printf("Panel ID: %x\r\n", cData[5]);
						printf("Circuit #: %d\r\n", cData[6]);
					}
				}
				int meterData = (cData[3]<<24 | cData[2]<<16 | cData[1]<<8 | cData[0]);
				printf("Meter data: %d mW\r\n", meterData);
			}
			else{
				printf("Authentication Failed\r\n");
				printf("Nonce: ");
				for (i=0; i<13; i++)
					printf("0x%x ", myNonce[i]);
				printf("\r\n");
				printf("Authentication data: ");
				for (i=0; i<ADATA_LEN; i++)
					printf("0x%x ", srcExtAddr[i]);
				printf("\r\n");
			}
			printf("\r\n");
		}
		else{
			printf("Data Payload: ");
			for(i=0; i < packet_length; i++) {
				printf("0x%x ", packet_ptr[i]);                                            
			}
			printf("\r\n");
		}
		printf("\r\n");
		leds_off(LEDS_RED);
	}


  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
