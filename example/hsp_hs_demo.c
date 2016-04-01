/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 * hsp_hs_demo.c
 */

// *****************************************************************************
/* EXAMPLE_START(hsp_hs_demo): HSP Headset Demo
 *
 * @text This example implements a HSP Headset device that sends and receives 
 * audio signal over HCI SCO. It demonstrates how to receive 
 * an output from a remote audio gateway (AG), and, 
 * if HAVE_POSIX_STDIN is defined, how to control the AG. 
 */
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "btstack.h"
#ifdef HAVE_POSIX_STDIN
#include "stdin_support.h"
#endif

#define SCO_REPORT_PERIOD 255

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t hsp_service_buffer[150]; 
static const uint8_t rfcomm_channel_nr = 1;
static const char    hsp_hs_service_name[] = "Headset Test";
static hci_con_handle_t sco_handle = 0;

static char hs_cmd_buffer[100];
static bd_addr_t device_addr = {0x00,0x1b,0xDC,0x07,0x32,0xEF};

static int phase = 0;

// input signal: pre-computed sine wave, 160 Hz
static const uint8_t sine[] = {
      0,  15,  31,  46,  61,  74,  86,  97, 107, 114,
    120, 124, 126, 126, 124, 120, 114, 107,  97,  86,
     74,  61,  46,  31,  15,   0, 241, 225, 210, 195,
    182, 170, 159, 149, 142, 136, 132, 130, 130, 132,
    136, 142, 149, 159, 170, 182, 195, 210, 225, 241,
};

/* @section Audio Transfer Setup 
 *
 * @text A pre-computed sine wave (160Hz) is used as the input audio signal. 160 Hz. 
 * To send and receive an audio signal, HAVE_SCO_OVER_HCI has to be defined. 
 *
 * Tested working setups: 
 * - Ubuntu 14 64-bit, CC2564B connected via FTDI USB-2-UART adapter, 921600 baud
 * - Ubuntu 14 64-bit, CSR USB dongle
 * - OS X 10.11, CSR USB dongle
 *
 * Broken setups:
 * - OS X 10.11, CC2564B connected via FDTI USB-2-UART adapter, 921600 baud
 * - select(..) blocks > 400 ms -> num completed is received to late -> gaps between audio
 * - looks like bug in select->FTDI driver as it works correct on Linux
 *
 * SCO not routed over HCI yet:
 * - CSR UART dongle 
 * - Broadcom USB dongle
 * - Broadcom UART chipset
 * - ..
 *
 */ 


static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth HSP Headset Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("---\n");
    printf("c - Connect to %s\n", bd_addr_to_str(device_addr));
    printf("C - Disconnect\n");
    printf("a - establish audio connection\n");
    printf("A - release audio connection\n");
    printf("b - press user button\n");
    printf("z - set microphone gain 0\n");
    printf("m - set microphone gain 8\n");
    printf("M - set microphone gain 15\n");
    printf("o - set speaker gain 0\n");
    printf("s - set speaker gain 8\n");
    printf("S - set speaker gain 15\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

#ifdef HAVE_POSIX_STDIN
static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    char buffer;
    read(ds->fd, &buffer, 1);

    switch (buffer){
        case 'c':
            printf("Connect to %s\n", bd_addr_to_str(device_addr));
            hsp_hs_connect(device_addr);
            break;
        case 'C':
            printf("Disconnect.\n");
            hsp_hs_disconnect();
            break;
        case 'a':
            printf("Establish audio connection\n");
            hsp_hs_establish_audio_connection();
            break;
        case 'A': 
            printf("Release audio connection\n");
            hsp_hs_release_audio_connection();
            break;
    
        case 'z':
            printf("Setting microphone gain 0\n");
            hsp_hs_set_microphone_gain(0);
            break;
        case 'm':
            printf("Setting microphone gain 8\n");
            hsp_hs_set_microphone_gain(8);
            break;
        case 'M':
            printf("Setting microphone gain 15\n");
            hsp_hs_set_microphone_gain(15);
            break;
        case 'o':
            printf("Setting speaker gain 0\n");
            hsp_hs_set_speaker_gain(0);
            break;
        case 's':
            printf("Setting speaker gain 8\n");
            hsp_hs_set_speaker_gain(8);
            break;
        case 'S':
            printf("Setting speaker gain 15\n");
            hsp_hs_set_speaker_gain(15);
            break;
        case 'b':
            printf("Press user button\n");
            hsp_hs_send_button_press();
            break;
        default:
            show_usage();
            break;
    }
}
#endif

static void try_send_sco(void){
    if (!sco_handle) return;

    while (hci_can_send_sco_packet_now()) {
        
        const int sco_packet_length = hci_get_sco_packet_length();
        const int sco_payload_length = sco_packet_length - 3;
        const int frames_per_packet = sco_payload_length;    // for 8-bit data. for 16-bit data it's /2

        hci_reserve_packet_buffer();
        uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
        // set handle + flags
        little_endian_store_16(sco_packet, 0, sco_handle);
        // set len
        sco_packet[2] = sco_payload_length;
        int i;
        for (i=0;i<frames_per_packet;i++){
            sco_packet[3+i] = sine[phase];
            phase++;
            if (phase >= sizeof(sine)) phase = 0;
        }
        hci_send_sco_packet_buffer(sco_packet_length);
        static int count = 0;
        count++;
        if ((count & 15) == 0) printf("Sent %u\n", count);
    }
}

static void sco_packet_handler(uint8_t packet_type, uint8_t * packet, uint16_t size){
    static int count = 0;
    // hexdumpf(packet, size);
    count++;
    if ((count & SCO_REPORT_PERIOD)) return;
    printf("SCO packets %u\n", count);
}

static void packet_handler(uint8_t * event, uint16_t event_size){
    switch (event[0]) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(event) != HCI_STATE_WORKING) break;
            show_usage();
            break;
        case HCI_EVENT_SCO_CAN_SEND_NOW:
            try_send_sco();
            break;
        case HCI_EVENT_HSP_META:
            switch (event[2]) { 
                case HSP_SUBEVENT_RFCOMM_CONNECTION_COMPLETE:
                    if (hsp_subevent_audio_connection_complete_get_handle(event) == 0){
                        printf("RFCOMM connection established.\n");
                    } else {
                        printf("RFCOMM connection establishement failed.\n");
                    }
                    break;
                case HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE:
                    if (hsp_subevent_audio_connection_complete_get_handle(event) == 0){
                        printf("RFCOMM disconnected.\n");
                    } else {
                        printf("RFCOMM disconnection failed.\n");
                    }
                    break;
                case HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE:
                    if (hsp_subevent_audio_connection_complete_get_handle(event) == 0){
                        sco_handle = little_endian_read_16(event, 4);
                        printf("Audio connection established with SCO handle 0x%04x.\n", sco_handle);
                        try_send_sco();
                    } else {
                        printf("Audio connection establishment failed with status %u\n", hsp_subevent_audio_connection_complete_get_status(event));
                        sco_handle = 0;
                    }
                    break;
                case HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE:
                    if (event[3] == 0){
                        printf("Audio connection released.\n\n");
                        sco_handle = 0;    
                    } else {
                        printf("Audio connection releasing failed with status %u\n", event[3]);
                    }
                    break;
                case HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED:
                    printf("Received microphone gain change %d\n", hsp_subevent_microphone_gain_changed_get_gain(event));
                    break;
                case HSP_SUBEVENT_SPEAKER_GAIN_CHANGED:
                    printf("Received speaker gain change %d\n", hsp_subevent_speaker_gain_changed_get_gain(event));
                    break;
                case HSP_SUBEVENT_RING:
                    printf("HS: RING RING!\n");
                    break;
                case HSP_SUBEVENT_AG_INDICATION: {
                    memset(hs_cmd_buffer, 0, sizeof(hs_cmd_buffer));
                    int size = hsp_subevent_ag_indication_get_value_length(event);
                    if (size >= sizeof(hs_cmd_buffer)-1){
                        size =  sizeof(hs_cmd_buffer)-1;
                    }
                    memcpy(hs_cmd_buffer, hsp_subevent_ag_indication_get_value(event), size);
                    printf("Received custom indication: \"%s\". \nExit code or call hsp_hs_send_result.\n", hs_cmd_buffer);

                }
                    break;
                default:
                    printf("event not handled %u\n", event[2]);
                    break;
            }
            break;
        default:
            break;
    }
}

static void handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    packet_handler(packet, size);
}

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code. 
 * To run a HSP Headset service you need to initialize the SDP, and to create and register HSP HS record with it. 
 * In this example, the SCO over HCI is used to receive and send an audio signal.
 * 
 * Two packet handlers are registered:
 * - The HCI SCO packet handler receives audio data.
 * - The HSP HS packet handler is used to trigger sending of audio data and commands to the AG. It also receives the AG's answers.
 * 
 * The stdin_process callback allows for sending commands to the AG. 
 * At the end the Bluetooth stack is started.
 */

/* LISTING_START(MainConfiguration): Setup HSP Headset */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    // register for HCI events
    hci_event_callback_registration.callback = &handle_hci_event;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_register_sco_packet_handler(&sco_packet_handler);

    l2cap_init();

    sdp_init();
    memset(hsp_service_buffer, 0, sizeof(hsp_service_buffer));
    hsp_hs_create_sdp_record(hsp_service_buffer, 0x10001, rfcomm_channel_nr, hsp_hs_service_name, 0);
    sdp_register_service(hsp_service_buffer);

    rfcomm_init();

    hsp_hs_init(rfcomm_channel_nr);
    hsp_hs_register_packet_handler(packet_handler);

#ifdef HAVE_POSIX_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    gap_set_local_name("BTstack HSP HS");
    gap_discoverable_control(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_class_of_device(0x240404);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */