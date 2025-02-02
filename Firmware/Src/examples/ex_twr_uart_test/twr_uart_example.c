/*! ----------------------------------------------------------------------------
 *  @file    ss_twr_initiator.c
 *  @brief   Single-sided two-way ranging (SS TWR) initiator example code
 *
 *           This is a simple code example which acts as the initiator in a SS TWR distance measurement exchange. This application sends a "poll"
 *           frame (recording the TX time-stamp of the poll), after which it waits for a "response" message from the "DS TWR responder" example
 *           code (companion to this application) to complete the exchange. The response message contains the remote responder's time-stamps of poll
 *           RX, and response TX. With this data and the local time-stamps, (of poll TX and response RX), this example application works out a value
 *           for the time-of-flight over-the-air and, thus, the estimated distance between the two devices, which it writes to the LCD.
 *
 * @attention
 *
 * Copyright 2015 - 2021 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author Decawave
 */

#include "deca_probe_interface.h"
#include <config_options.h>
#include <deca_device_api.h>
#include <deca_spi.h>
#include <example_selection.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>
#include "app_uart.h"
#include "nrf_delay.h"

#if defined(TWR_UART_TEST)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "TWR UART v1.0"

// UART config pre-processor directives
#define INIT_ERROR "UART INIT ERROR"
#define TX_DELAY   500

#define UART_TXPIN 19 // P0.19 on DWM3001
#define UART_RXPIN 15 // P0.15 on DWM3001

// Set UART flow control to false, not needed for a single user (our case)
static const app_uart_flow_control_t flow_control = APP_UART_FLOW_CONTROL_DISABLED;

unsigned char lora_uart_str[32];
unsigned char dest_addr_str[32];
unsigned int distance_val_len;
uint16_t random_delay;

/* Default communication configuration. We use default non-STS DW mode. */
static dwt_config_t config = {
    5,                /* Channel number. */
    DWT_PLEN_128,     /* Preamble length. Used in TX only. */
    DWT_PAC8,         /* Preamble acquisition chunk size. Used in RX only. */
    9,                /* TX preamble code. Used in TX only. */
    9,                /* RX preamble code. Used in RX only. */
    1,                /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,       /* Data rate. */
    DWT_PHRMODE_STD,  /* PHY header mode. */
    DWT_PHRRATE_STD,  /* PHY header rate. */
    (129 + 8 - 8),    /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF, /* STS disabled */
    DWT_STS_LEN_64,   /* STS length see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0       /* PDOA mode off */
};

/* Experimental high power config -- trying to increase range */
// static dwt_config_t config = {
//     5,                /* Channel number. */
//     DWT_PLEN_4096,     /* Preamble length. Used in TX only. */
//     DWT_PAC32,         /* Preamble acquisition chunk size. Used in RX only. */
//     9,                /* TX preamble code. Used in TX only. */
//     9,                /* RX preamble code. Used in RX only. */
//     3,                /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
//     DWT_BR_850K,       /* Data rate. */
//     DWT_PHRMODE_STD,  /* PHY header mode. */
//     DWT_PHRRATE_STD,  /* PHY header rate. */
//     4073,    /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
//     DWT_STS_MODE_OFF, /* STS disabled */
//     DWT_STS_LEN_2048,   /* STS length see allowed values in Enum dwt_sts_lengths_e */
//     DWT_PDOA_M0       /* PDOA mode off */
// };

static app_uart_comm_params_t uart_config = {
    .rx_pin_no = UART_RXPIN,                       // Use pin 19 as RX pin
    .tx_pin_no = UART_TXPIN,                       // Use pin 15 as TX pin
    .rts_pin_no = 21,                              // Random pin -- RTS not used
    .cts_pin_no = 20,                              // Random pin -- CTS not used
    .flow_control = flow_control,                  // Flow control disabled -- ignores RTS and CTS pins
    .use_parity = 0,                               // Parity bit set to false
    .baud_rate = UART_BAUDRATE_BAUDRATE_Baud115200 // Baud rate set to 115200
};

/* Inter-ranging delay period, in milliseconds. */
#define RNG_DELAY_MS 1000

/* Default antenna delay values for 64 MHz PRF. See NOTE 2 below. */
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

/* Frames used in the ranging process. See NOTE 3 below. */
static uint8_t tx_poll_msg[] = { 0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'A', 'B', 0xE0, 0, 0 };
static uint8_t rx_resp_msg[] = { 0x41, 0x88, 0, 0xCA, 0xDE, 'A', 'B', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Length of the common part of the message (up to and including the function code, see NOTE 3 below). */
#define ALL_MSG_COMMON_LEN 10
/* Indexes to access some of the fields in the frames defined above. */
#define ALL_MSG_SN_IDX          2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN         4
#define RX_DEST_ADDR_INDX       5
/* Frame sequence number, incremented after each transmission. */
static uint8_t frame_seq_nb = 0;

/* Buffer to store received response message.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
#define RX_BUF_LEN 20
static uint8_t rx_buffer[RX_BUF_LEN];

/* Hold copy of status register state here for reference so that it can be examined at a debug breakpoint. */
static uint32_t status_reg = 0;

/* Delay between frames, in UWB microseconds. See NOTE 1 below. */
#define POLL_TX_TO_RESP_RX_DLY_UUS 240
/* Receive response timeout. See NOTE 5 below. */
#define RESP_RX_TIMEOUT_UUS 400

/* Hold copies of computed time of flight and distance here for reference so that it can be examined at a debug breakpoint. */
static double tof;
static double distance;

/* Values for the PG_DELAY and TX_POWER registers reflect the bandwidth and power of the spectrum at the current
 * temperature. These values can be calibrated prior to taking reference measurements. See NOTE 2 below. */
extern dwt_txconfig_t txconfig_options;

/**
 * @breif Function for handling UART event, currently does nothing, as we are just TXing
 *
 * @param p_event -- pointer to UART event
*/
void uart_event_handler(app_uart_evt_t * p_event){
    // Handle the UART event (Do nothing.)
    return;
}

/** @brief Function for putting an entire String over UART TX
 *
 * @param str -- pointer to String to be transmitted
 * @param len -- length of the String to be transmitted
 *
 * @note May want to remove the nrf_delay_us() at the end - currently just ensures data is clear
*/
void uart_put_string(const char *str){
    while(*str){
        while(app_uart_put(*str) != NRF_SUCCESS){
            nrf_delay_us(100);
        }
        str++;
        nrf_delay_us(500);
    }
    nrf_delay_ms(2);
    return;
}

/**
 * @breif Function to return the length of a distance float -- needed for transmitting distance measurement over LoRa
 *
 * @param flt -- float distance value
 *
 * @note Assumes float always has 2 decimal points - consistent with our distance measurements (always out of cm)
 */
int get_dist_len(float flt){
	int int_part = flt;
	int ten = 10;
	char char_cnt = 1;

	while((int_part % ten != int_part) || (char_cnt>4)){
		char_cnt++;
		ten = ten*10;
	}
	return char_cnt+3;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn main()
 *
 * @brief Application entry point.
 *
 * @param  none
 *
 * @return none
 */
int twr_uart_example(void)
{
    /* Init UART stuff */
    uint32_t init_error_code; // Variable to store APP_UART_INIT error code

    test_run_info((unsigned char *) APP_NAME);

    // Macro defined in app_uart.h - using low priority for IRQ as we aren't even handling IRQ
    // Only to be used for single-user UART mode
    APP_UART_INIT(&uart_config,
                uart_event_handler,
                APP_IRQ_PRIORITY_LOW,
                init_error_code);
    if (init_error_code != NRF_SUCCESS)
    {
        // Log the INIT_ERROR to the debug log
        test_run_info((unsigned char *) INIT_ERROR);
    }

    /* Configure SPI rate, DW3000 supports up to 36 MHz */
    port_set_dw_ic_spi_fastrate();

    /* Reset and initialize DW chip. */
    reset_DWIC(); /* Target specific drive of RSTn line into DW3000 low for a period. */

    Sleep(2); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)

    /* Probe for the correct device driver. */
    dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);

    while (!dwt_checkidlerc()) /* Need to make sure DW IC is in IDLE_RC before proceeding */ { };
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
        test_run_info((unsigned char *)"INIT FAILED     ");
        while (1) { };
    }

    /* Enabling LEDs here for debug so that for each TX the D1 LED will flash on DW3000 red eval-shield boards. */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. See NOTE 13 below. */
    /* if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device */
    if (dwt_configure(&config))
    {
        test_run_info((unsigned char *)"CONFIG FAILED     ");
        while (1) { };
    }

    /* Configure the TX spectrum parameters (power, PG delay and PG count) */
    dwt_configuretxrf(&txconfig_options);

    /* Apply default antenna delay value. See NOTE 2 below. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    /* Set expected response's delay and timeout. See NOTE 1 and 5 below.
     * As this example only handles one incoming frame with always the same delay and timeout, those values can be set here once for all. */
    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

    /* Next can enable TX/RX states output on GPIOs 5 and 6 to help debug, and also TX/RX LEDs
     * Note, in real low power applications the LEDs should not be used. */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    /* Loop forever initiating ranging exchanges. */
    while (1)
    {
        /* Write frame data to DW IC and prepare transmission. See NOTE 7 below. */
        tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
        dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0); /* Zero offset in TX buffer. */
        dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);          /* Zero offset in TX buffer, ranging. */

        /* Start transmission, indicating that a response is expected so that reception is enabled automatically after the frame is sent and the delay
         * set by dwt_setrxaftertxdelay() has elapsed. */
        dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

        /* We assume that the transmission is achieved correctly, poll for reception of a frame or error/timeout. See NOTE 8 below. */
        waitforsysstatus(&status_reg, NULL, (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR), 0);

        /* Increment frame sequence number after transmission of the poll message (modulo 256). */
        frame_seq_nb++;

        if (status_reg & DWT_INT_RXFCG_BIT_MASK)
        {
            uint16_t frame_len;

            /* Clear good RX frame event in the DW IC status register. */
            dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

            /* A frame has been received, read it into the local buffer. */
            frame_len = dwt_getframelength();
            if (frame_len <= sizeof(rx_buffer))
            {
                dwt_readrxdata(rx_buffer, frame_len, 0);

                /* Check that the frame is the expected response from the companion "SS TWR responder" example.
                 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
                rx_buffer[ALL_MSG_SN_IDX] = 0;
                if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0)
                {
                    uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
                    int32_t rtd_init, rtd_resp;
                    float clockOffsetRatio;

                    /* Retrieve poll transmission and response reception timestamps. See NOTE 9 below. */
                    poll_tx_ts = dwt_readtxtimestamplo32();
                    resp_rx_ts = dwt_readrxtimestamplo32();

                    /* Read carrier integrator value and calculate clock offset ratio. See NOTE 11 below. */
                    clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << 26);

                    /* Get timestamps embedded in response message. */
                    resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
                    resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

                    /* Compute time of flight and distance, using clock offset ratio to correct for differing local and remote clock rates */
                    rtd_init = resp_rx_ts - poll_tx_ts;
                    rtd_resp = resp_tx_ts - poll_rx_ts;

                    tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
                    distance = tof * SPEED_OF_LIGHT;

                    /* Display computed distance on LCD. */
                    snprintf(dist_str, sizeof(dist_str), "DIST: %3.2f m", distance);
                    test_run_info((unsigned char *)dist_str);

                    /* Transmit computed distance over UART */
                    distance_val_len = get_dist_len(distance);
                    snprintf(lora_uart_str, sizeof(lora_uart_str), "AT+SEND=40,%d,%3.2f\r\n", distance_val_len, distance);
                    uart_put_string((const char *) lora_uart_str);
                    test_run_info((unsigned char *) lora_uart_str);

                    snprintf(dest_addr_str, sizeof(dest_addr_str), "Dest Addr = %c, %c\n", rx_buffer[RX_DEST_ADDR_INDX], rx_buffer[RX_DEST_ADDR_INDX+1]);
                    test_run_info((unsigned char *) dest_addr_str);
                }
            }
        }
        else
        {
            /* Clear RX error/timeout events in the DW IC status register. */
            dwt_writesysstatuslo(SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
        }

        /* Execute a delay between ranging exchanges. */
        // Sleep(RNG_DELAY_MS);
        random_delay = rand();
        random_delay = random_delay % (((1500 + 1) - 500) + 500);
        Sleep(random_delay);
    }
}
#endif
/*****************************************************************************************************************************************************
 * NOTES:
 *
 * 1. The single-sided two-way ranging scheme implemented here has to be considered carefully as the accuracy of the distance measured is highly
 *    sensitive to the clock offset error between the devices and the length of the response delay between frames. To achieve the best possible
 *    accuracy, this response delay must be kept as low as possible. In order to do so, 6.8 Mbps data rate is used in this example and the response
 *    delay between frames is defined as low as possible. The user is referred to User Manual for more details about the single-sided two-way ranging
 *    process.  NB:SEE ALSO NOTE 11.
 *
 *    Initiator: |Poll TX| ..... |Resp RX|
 *    Responder: |Poll RX| ..... |Resp TX|
 *                   ^|P RMARKER|                    - time of Poll TX/RX
 *                                   ^|R RMARKER|    - time of Resp TX/RX
 *
 *                       <--TDLY->                   - POLL_TX_TO_RESP_RX_DLY_UUS (RDLY-RLEN)
 *                               <-RLEN->            - RESP_RX_TIMEOUT_UUS   (length of response frame)
 *                    <----RDLY------>               - POLL_RX_TO_RESP_TX_DLY_UUS (depends on how quickly responder can turn around and reply)
 *
 *
 * 2. The sum of the values is the TX to RX antenna delay, this should be experimentally determined by a calibration process. Here we use a hard coded
 *    value (expected to be a little low so a positive error will be seen on the resultant distance estimate). For a real production application, each
 *    device should have its own antenna delay properly calibrated to get good precision when performing range measurements.
 * 3. The frames used here are Decawave specific ranging frames, complying with the IEEE 802.15.4 standard data frame encoding. The frames are the
 *    following:
 *     - a poll message sent by the initiator to trigger the ranging exchange.
 *     - a response message sent by the responder to complete the exchange and provide all information needed by the initiator to compute the
 *       time-of-flight (distance) estimate.
 *    The first 10 bytes of those frame are common and are composed of the following fields:
 *     - byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
 *     - byte 2: sequence number, incremented for each new frame.
 *     - byte 3/4: PAN ID (0xDECA).
 *     - byte 5/6: destination address, see NOTE 4 below.
 *     - byte 7/8: source address, see NOTE 4 below.
 *     - byte 9: function code (specific values to indicate which message it is in the ranging process).
 *    The remaining bytes are specific to each message as follows:
 *    Poll message:
 *     - no more data
 *    Response message:
 *     - byte 10 -> 13: poll message reception timestamp.
 *     - byte 14 -> 17: response message transmission timestamp.
 *    All messages end with a 2-byte checksum automatically set by DW IC.
 * 4. Source and destination addresses are hard coded constants in this example to keep it simple but for a real product every device should have a
 *    unique ID. Here, 16-bit addressing is used to keep the messages as short as possible but, in an actual application, this should be done only
 *    after an exchange of specific messages used to define those short addresses for each device participating to the ranging exchange.
 * 5. This timeout is for complete reception of a frame, i.e. timeout duration must take into account the length of the expected frame. Here the value
 *    is arbitrary but chosen large enough to make sure that there is enough time to receive the complete response frame sent by the responder at the
 *    6.8M data rate used (around 400 �s).
 * 6. In a real application, for optimum performance within regulatory limits, it may be necessary to set TX pulse bandwidth and TX power, (using
 *    the dwt_configuretxrf API call) to per device calibrated values saved in the target system or the DW IC OTP memory.
 * 7. dwt_writetxdata() takes the full size of the message as a parameter but only copies (size - 2) bytes as the check-sum at the end of the frame is
 *    automatically appended by the DW IC. This means that our variable could be two bytes shorter without losing any data (but the sizeof would not
 *    work anymore then as we would still have to indicate the full length of the frame to dwt_writetxdata()).
 * 8. We use polled mode of operation here to keep the example as simple as possible but all status events can be used to generate interrupts. Please
 *    refer to DW IC User Manual for more details on "interrupts". It is also to be noted that STATUS register is 5 bytes long but, as the event we
 *    use are all in the first bytes of the register, we can use the simple dwt_read32bitreg() API call to access it instead of reading the whole 5
 *    bytes.
 * 9. The high order byte of each 40-bit time-stamps is discarded here. This is acceptable as, on each device, those time-stamps are not separated by
 *    more than 2**32 device time units (which is around 67 ms) which means that the calculation of the round-trip delays can be handled by a 32-bit
 *    subtraction.
 * 10. The user is referred to DecaRanging ARM application (distributed with EVK1000 product) for additional practical example of usage, and to the
 *     DW IC API Guide for more details on the DW IC driver functions.
 * 11. The use of the clock offset value to correct the TOF calculation, significantly improves the result of the SS-TWR where the remote
 *     responder unit's clock is a number of PPM offset from the local initiator unit's clock.
 *     As stated in NOTE 2 a fixed offset in range will be seen unless the antenna delay is calibrated and set correctly.
 * 12. In this example, the DW IC is put into IDLE state after calling dwt_initialise(). This means that a fast SPI rate of up to 20 MHz can be used
 *     thereafter.
 * 13. Desired configuration by user may be different to the current programmed configuration. dwt_configure is called to set desired
 *     configuration.
 ****************************************************************************************************************************************************/
