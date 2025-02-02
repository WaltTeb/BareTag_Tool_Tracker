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
#define MAX_BUF_SIZE 32

#define UART_TXPIN 19 // P0.19 on DWM3001
#define UART_RXPIN 15 // P0.15 on DWM3001

// Set UART flow control to false, not needed for a single user (our case)
static const app_uart_flow_control_t flow_control = APP_UART_FLOW_CONTROL_DISABLED;

char err_buf[MAX_BUF_SIZE];

uint8_t rand_delay;
char tx_str[MAX_BUF_SIZE];
char rx_buf[MAX_BUF_SIZE];
uint8_t rx_byte;
char rx_indx = 0;
char rx_flag = 0;

static app_uart_comm_params_t config = {
    .rx_pin_no = UART_RXPIN,                       // Use pin 19 as RX pin
    .tx_pin_no = UART_TXPIN,                       // Use pin 15 as TX pin
    .rts_pin_no = 21,                              // Random pin -- RTS not used
    .cts_pin_no = 20,                              // Random pin -- CTS not used
    .flow_control = flow_control,                  // Flow control disabled -- ignores RTS and CTS pins
    .use_parity = 0,                               // Parity bit set to false
    .baud_rate = UART_BAUDRATE_BAUDRATE_Baud115200 // Baud rate set to 115200
};

/** @brief Function to handle UART events, including: UART_RX, UART_TX complete, and UART errors 
 * 
 * @param p_event -- ptr to app_uart_evt_t struct -- detailed in app_uart.h file
 * 
 * @note has TODOs that should be addressed -- not urgent
 */
void uart_event_handler(app_uart_evt_t * p_event){
    // Handle the UART event 
    switch(p_event->evt_type){
        //UART RX a byte if FIFO enabled -- FIFO not currently enabled
        case APP_UART_DATA_READY:
            test_run_info((const char*) "APP UART DATA READY -- NO FIFO");
            break;
        
        case APP_UART_FIFO_ERROR:
            /**
             * TODO: Connect this with the appropriate error in nrf_error.h -> use some sort of patern matching to output a string indicating the error
             */
            sprintf(err_buf, "APP UART FIFO ERROR: %d", p_event->data.error_code);
            test_run_info((const char*) err_buf);
            break;
            
        case APP_UART_COMMUNICATION_ERROR:
            /**
             * TODO: Apparently, this error is stored in nrf5x_bitfields.h, I can't find this file... 
             */
            sprintf(err_buf, "APP UART COMMS ERROR: %d", p_event->data.error_communication);
            test_run_info((const char*) err_buf);
            break;

        // When UART finished TXing a byte -- for debugging
        case APP_UART_TX_EMPTY:
            test_run_info((const char*) "UART TX EMPTY");
            break;

        // UART RX a byte
        case APP_UART_DATA:
            while(app_uart_get(&rx_byte) != NRF_SUCCESS){
                nrf_delay_us(100);
            }
            rx_buf[rx_indx] = rx_byte;

            if(rx_indx == MAX_BUF_SIZE-2){
                rx_indx = 0;
                test_run_info((const char*) "REACHED MAX BUF SIZE");
            }
            else if(rx_buf[rx_indx] == '\n'){
                rx_buf[++rx_indx] = '\0'; // Null terminate the received string
                rx_flag = 1; // Set rx_flag to indicate a string has been received
                rx_indx = 0; // Reset the rx_buf indx to 0
            }else{
                rx_indx++; // Increment rx_indx to receive next byte into rx_buf
            }

            sprintf(err_buf, "RX DATA = %c", rx_byte);
            test_run_info((const char*) err_buf);
            break;

        default:
            test_run_info((const char*) "UART EVENT DEFAULT");
            break;
    }

    return;
}

/** @brief Function for putting an entire String over UART TX
 *
 * @param str -- pointer to String to be transmitted
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

    // Main control loop - just outputs what was received on the UART buffer
    while(1){
        if(rx_flag){
            strncpy(tx_str, rx_buf, sizeof(rx_buf));
            uart_put_string((const char*) tx_str);
            Sleep(50);
            rx_flag = 0;
        }
        
    }

    return 0;
}

#endif
