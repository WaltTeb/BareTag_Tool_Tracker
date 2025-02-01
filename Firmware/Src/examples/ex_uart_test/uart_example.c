#include "app_uart.h"
#include "nrf_delay.h"
#include "deca_probe_interface.h"
#include <deca_device_api.h>
#include <deca_spi.h>
#include <example_selection.h>
#include <port.h>
#include <stdlib.h>

#if defined(UART_TEST)

extern void test_run_info(unsigned char *data);

#define APP_NAME   "UART        V1.0"
#define INIT_ERROR "UART INIT ERROR"
#define TX_DELAY   500
#define NUM_DELAYS 8

#define UART_TXPIN 19 // P0.19 on DWM3001
#define UART_RXPIN 15 // P0.15 on DWM3001

// Set UART flow control to false, not needed for a single user (our case)
static const app_uart_flow_control_t flow_control = APP_UART_FLOW_CONTROL_DISABLED;

// unsigned int delays[] = {100, 200, 300, 400, 500, 600, 700, 800};
// unsigned int cur_delay = 0;
// unsigned int cal_delay;
uint8_t rand_delay;
char tx_str[20];

static app_uart_comm_params_t config = {
    .rx_pin_no = UART_RXPIN,                       // Use pin 19 as RX pin
    .tx_pin_no = UART_TXPIN,                       // Use pin 15 as TX pin
    .rts_pin_no = 21,                              // Random pin -- RTS not used
    .cts_pin_no = 20,                              // Random pin -- CTS not used
    .flow_control = flow_control,                  // Flow control disabled -- ignores RTS and CTS pins
    .use_parity = 0,                               // Parity bit set to false
    .baud_rate = UART_BAUDRATE_BAUDRATE_Baud115200 // Baud rate set to 115200
};


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

/** @brief Main function for this code example, outputs a UART string at a randomized time between 255 and 0 seconds
 * 
 * @retval None. Runs infinite while loop
 */
int uart_example(void){
    uint32_t init_error_code; // Variable to store APP_UART_INIT error code
    rand_delay = rand();
    test_run_info((unsigned char *) APP_NAME);

    // Macro defined in app_uart.h - using low priority for IRQ as we aren't even handling IRQ
    // Only to be used for single-user UART mode
    APP_UART_INIT(&config,
                uart_event_handler,
                APP_IRQ_PRIORITY_LOW,
                init_error_code);
    if (init_error_code != NRF_SUCCESS)
    {
        // Log the INIT_ERROR to the debug log
        test_run_info((unsigned char *) INIT_ERROR);
    }

    Sleep(200); // Allow everything to get setup -- idk if this is actually helping

    // Main control loop - just outputs "Hello\r\n" at 1 hz
    while(1){
        snprintf(tx_str, sizeof(tx_str), "Rand=%i\r\n", rand_delay);
        uart_put_string((const char *) tx_str);
        Sleep(TX_DELAY + rand_delay);
        rand_delay = rand();
        test_run_info((const char *) "HELLO!");
    }

    return 0;
}

#endif
