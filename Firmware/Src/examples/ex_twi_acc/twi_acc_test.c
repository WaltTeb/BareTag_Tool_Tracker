/**  
 *
 * @LISD2H Accelerometer example for the DWM3001CDK
 *
 */
/*

Customization:

- sdk_config.h : for TWI, in sdk_config.h, TWI_DEFAULT_CONFIG_CLR_BUS_INIT = 0
- sdk_config.h : for TWI, TWI0_USE_EASY_DMA = 0

*/


// nRF5_SDK_17.1.0_ddde560/components/drivers_ext/lis2dh12 IS WORTH LOOKING INTO
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <example_selection.h>
#include "nrf_drv_twi.h" 
#include "nrf_delay.h"  
#include "ctype.h"  
#include "nrf_drv_gpiote.h"  //accelero 
#include "app_timer.h"
#include <custom_board.h>
//#include <deca_device_api.h>
#include "nrf_log.h"

#if defined(TEST_TWI_ACC)

extern void test_run_info(unsigned char *data);

// these are pins for the nRF52833
#define ACC_CS					 	8 // not sure why this is even needed to be defined as it is tied high in hardware...... breaks without it 
//#define ACC_INT2					6  // there is no int2 on the lis2dh in the dwm3001c
//#define CTS_PIN_NUMBER 		7
#define ACC_INT1					16  //p
#define ARDUINO_SCL_PIN   NRF_GPIO_PIN_MAP(1,4) // P1.04 likely a better way to access port1
#define ARDUINO_SDA_PIN   24
#define I2C_DELAY     10  // mandatory delay in ms for this interface I2C else the target blocks .... CANNOT be set lower than 10ms

#define SLEEP_MODE_TIMEOUT_SEC  86398  //in seconds (1440 mins), CAN be modified, minimum = 13 seconds and multiples of 13sec -> [13sec, 26sec, 39 sec...] 
#define SLEEP_MODE_TIMEOUT_COUNTER_VALUE  SLEEP_MODE_TIMEOUT_SEC/13  // this define CANNOT be modified (has a value of 1 if the above is 13)

//-- BEGIN ACCELERO PART -------------------------/
#define ACCEL_I2C_ADDR      0x19 // 0b0011001 This is the Slave Address referenced in the datasheet. Should be 7 bits 
                                 // in the form of [ SAD[6:1] SAD[0] ], where SAD[0] is tied high to 1 in hardware for the DWM3001C 

// the follwing defines are all from the LIS2DH datasheet. INT2 is not used. Again, disconnected in hardware
#define 	WHO_AM_I	0x0F
#define 	CTRL_REG1	0x20
#define 	CTRL_REG2	0x21
#define 	CTRL_REG3	0x22
#define 	CTRL_REG4	0x23
#define 	CTRL_REG5	0x24
#define 	CTRL_REG6	0x25
#define 	INT1_CFG	0x30
#define 	INT1_SRC	0x31
#define 	INT1_THS	0x32
#define 	INT1_DURATION	0x33
//#define 	INT2_CFG	0x34
//#define 	INT2_SRC	0x35
//#define 	INT2_THS	0x36
//#define 	INT2_DURATION	0x37

static uint8_t G_ACCELERO_THRESHOLD_INT1=0x0C;   //threshold for INT1 (wakeup) and INT2 (wakeup) is the same, preferred value = 0x0C;
//static uint8_t G_ACCELERO_THRESHOLD_INT2=0x09;   //threshold for INT2 (wakeup) and INT2 (wakeup) is the same, preferred value = 0x09;
static bool accel_int = false;// flag to set when there is a wakeup interrupt from accelerometer motion
//-- END ACCELERO PART -------------------------/

void prepare_sleep_mode (void);
void motion_timeout_handler(void *p_context);

//static uint16_t sleep_mode_timeout_counter=0; //variable for time to remain active before going Sleep mode, uint16_t unsigned short (16-bit)  

APP_TIMER_DEF(sleep_timer); // create a timer identifier and allocate memory for the timer

/* Indicates if operation on TWI has ended. */
static volatile bool m_xfer_done = false;

/* TWI instance. */
#define TWI_INSTANCE_ID     0
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void sleep_timer_init(void) // initialize, create, and start a timer for going to sleep 
{

	// Double t;
	// uint32_t sleep_time = 0;
	// uint16_t lp_osc_cal = 0;
	// uint16_t sleepTime16;
	// // Measure low power oscillator frequency
	// p_osc_cal = dwt_calibratesleepcnt();
	// // calibrate low power oscillator
	// // the lp_osc_cal value is number of XTAL cycles in one cycle of LP OSC
	// // to convert into seconds (38.4 MHz => 1/38.4 MHz ns)
	// // so to get a sleep time of 10s we need a value of:
	// // 10 / period and then >> 12 as the register holds upper 16-bits of 28-bit
	// // counter
	// t = ((double) 10.0 / ((double) lp_osc_cal/38.4e6));
	// sleep_time = (int) t;
	// sleepTime16 = sleep_time >> 12;
	// dwt_configuresleepcnt(sleepTime16); // configure sleep time
	// dwt_configuresleep(0x0001, 0x1);

	
    ret_code_t err_code;

    // Initialize timer module.
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

   // build a timer such that it references sleep_timer and calls the interrupt function
   err_code = app_timer_create(&sleep_timer, APP_TIMER_MODE_SINGLE_SHOT, motion_timeout_handler); // or .._MODE_REPEATED for loops
   APP_ERROR_CHECK(err_code);

   // start the timer,  The third parameter is a general-purpose pointer that is passed to the timeout handler. (p_context)
   err_code = app_timer_start(sleep_timer, APP_TIMER_TICKS(10000), NULL); // APP_TIMER_TICKS takes in ms and converts to ticks for the timer
  APP_ERROR_CHECK(err_code);
}


/**@brief Function before going in Sleep mode */
void prepare_sleep_mode (void)
{

   	nrf_drv_twi_disable(&m_twi); //disable TWI for Low power mode
		nrf_delay_ms(I2C_DELAY);	
}

/**
 * @brief TWI events handler.
 */
void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:
            if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX)
            {
							//data_handler(m_sample);
            }
            m_xfer_done = true;
            break;
        default:
            break;
    }
}

/**
 * @brief TWI initialization. =>  I2C
 */
void twi_init (void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t twi_LIS2DH_config = {
       .scl                = ARDUINO_SCL_PIN,
       .sda                = ARDUINO_SDA_PIN,
       .frequency          = NRF_TWI_FREQ_100K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };

    err_code = nrf_drv_twi_init(&m_twi, &twi_LIS2DH_config, twi_handler, NULL);
    APP_ERROR_CHECK(err_code);

    nrf_drv_twi_enable(&m_twi);
}
/**
 * @brief Function triggered by accelerometer (pin INT1) on wake-up 
 */
void in_pin_handler_INT1(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	accel_int = true;	
	nrf_drv_twi_enable(&m_twi); //enable TWI

	//sleep_mode_timeout_counter = SLEEP_MODE_TIMEOUT_COUNTER_VALUE; //whenever we receive an INT1 then we delay again the time to go to sleep	
	
	//read INT1_SRC to release latch. This means that when this register is read, the data is cleared and another measurement can be taken
	uint8_t reg[2];
	reg[0] = INT1_SRC; 
	uint8_t value=0;
	nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, 1, false);
	nrf_delay_ms(I2C_DELAY);	
	while (m_xfer_done == false);
	nrf_drv_twi_rx(&m_twi, ACCEL_I2C_ADDR, &value, 1);
	nrf_delay_ms(I2C_DELAY);
	while (m_xfer_done == false);	
	
}


/**
 * @brief Once a timer has expired, use a timer interrupt to call this function and send the device to sleep
 */
void motion_timeout_handler(void *p_context)
{

	//nrf_gpio_pin_toggle(LED_1); // toggle an LED on the interrupt 

	//sleep_mode_timeout_counter--;	
	
	// Read the value and release the value so that there is no data being held by the accellerometer when it is going to sleep?y
	uint8_t reg[2];
	reg[0] = INT1_SRC; 
	uint8_t value=0;
	nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, 1, false);
	nrf_delay_ms(I2C_DELAY);	
	while (m_xfer_done == false);
	nrf_drv_twi_rx(&m_twi, ACCEL_I2C_ADDR, &value, 1);
	nrf_delay_ms(I2C_DELAY);
	while (m_xfer_done == false);	


	// dwt_configuresleep(0x0001, 0x1);
	// dwt_entersleep(DWT_DW_IDLE);

	//__WFE();

 //if (sleep_mode_timeout_counter<=0)
	//{
	//prepare_sleep_mode();
	//   /*The end result of sd_power_system_off() and NRF_POWER->POWEROFF = 1 is the same. When the softdevice is initialized, you should use sd_power_system_off(), which make sure the softdevice is not in the middle of a critical operation when putting the device into system off mode.
	// 	Without the softdevice, sd_power_system_off() is not available, and it is safe to use NRF_POWER->POWEROFF = 1.
	// 	Notice that the device is not OFF when you put it in system off mode, it is put in a low power/deep sleep state, where it can only be waked up by reset, GPIO, NFC field or LPCOMP module.*/
	//sd_power_system_off(); //go in Low power mode. This works for us because we have the softdevice running, as described above
	//}
	
}


/**
 * @brief Function for configuring: PIN_IN pin for input, PIN_OUT pin for output,
 * and configures GPIOTE to give an interrupt on pin change.
 */
static void gpio_init(void)
{


    ret_code_t err_code;

    if(!nrf_drv_gpiote_is_init())
        nrf_drv_gpiote_init();
	
		// GPIOTE config for INT1
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true); //Indicates that the input pin will generate an event when the input value is changed to high. As parameter, you can specify if the input should have high accuracy or not.
    in_config.pull = GPIO_PIN_CNF_PULL_Disabled;  //This input pin managed by accelerometer

    err_code = nrf_drv_gpiote_in_init(ACC_INT1, &in_config, in_pin_handler_INT1);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_event_enable(ACC_INT1, true); // catch in events on pin 16
		
    //this is very important for Sleep mode (the GPOITE doesn't wake up itself from Sleep mode)
    nrf_gpio_cfg_sense_input(ACC_INT1, GPIO_PIN_CNF_PULL_Disabled, NRF_GPIO_PIN_SENSE_HIGH); // sense a high toggle on pin 16
    //nrf_gpio_cfg_sense_input(ACC_INT2, GPIO_PIN_CNF_PULL_Disabled, NRF_GPIO_PIN_SENSE_HIGH); 
                            
    // GPIOTE config for INT2
    //err_code = nrf_drv_gpiote_in_init(ACC_INT2, &in_config, in_pin_handler_INT2);
    //APP_ERROR_CHECK(err_code);

    //nrf_drv_gpiote_in_event_enable(ACC_INT2, true);	
}

/**
 * @brief Function to initialize the accelerometer
 */
void accelero_init()
{

              //  bits I2_INT1 a I2_INT2 in register CTRL_REG6 (25h) select the generators for pin INT2.
	
		uint8_t reg[2];
		uint8_t err_code;	
	  
  
	// Enable I2C for accelero
		nrf_gpio_cfg_output(ACC_CS);
		nrf_gpio_pin_set(ACC_CS);
		nrf_delay_ms(500); 

//		/***  BEGIN INIT   ***********************************************************/	
                reg[0] = CTRL_REG1;
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG2;
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG3;
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG4;
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG5; 
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);
		
		reg[0] = CTRL_REG6;  
		reg[1] = 0x00;		
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);			
		
		reg[0] = INT1_THS;
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = INT1_DURATION;
		reg[1] = 0x00;	
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);
		
		reg[0] = INT1_CFG;
		reg[1] = 0x00;			
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);			

		//reg[0] = INT2_THS;
		//reg[1] = 0x00;	
		//err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		//nrf_delay_ms(I2C_DELAY);	
		//while (m_xfer_done == false);

		//reg[0] = INT2_DURATION;
		//reg[1] = 0x00;	
		//err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		//nrf_delay_ms(I2C_DELAY);	
		//while (m_xfer_done == false);
		
		//reg[0] = INT2_CFG;
		//reg[1] = 0x00;			
		//err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		//nrf_delay_ms(I2C_DELAY);	
		//while (m_xfer_done == false);		
//		/***  END INIT   *************************************************************/
	
		reg[0] = CTRL_REG1;
		reg[1] = 0x2F;		//0x2F Low power mode (10 Hz); all axis //0x5F Low power mode (100 Hz) 
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG2;
		//reg[1] = 0x03;		// High-pass filter for int1 and int2
                reg[1] = 0x01;		// High-pass filter for int1
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG3;
		reg[1] = 0x40 ;		 // AOI1 interrupt on INT1 I think this means interrupt based on user defined threshold levels
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG4;
		reg[1] = 0x80  ;		//  Full Scale = +/-2 g, ad Block Data Update. Read all bits before value change 
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = CTRL_REG5; 
		reg[1] = 0x08 ;		// Latch interrupt on INT1  but NOT on INT2
		//reg[1] = 0x00 ;		// NOT Latch interrupt on INT1  and  NOT on INT2
		//reg[1] = 0x0A ;		// Latch interrupt on INT1 / Latch on INT2		
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);
		
		reg[0] = CTRL_REG6;
                reg[1] = 0x00 ; // do not activate INT2
		//reg[1] = 0x20 ;		// activate INT2
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);			

//******** INT1 Config for WAKE-UP INTERRUPT ********************
		reg[0] = INT1_THS;
		reg[1] = G_ACCELERO_THRESHOLD_INT1;  //1LSb = 16mg @FS=2g  (e.g.-> 0x10*16mg = threshold 256mg)  (e.g.-> 0x40*16mg = threshold 1024mg)  preferred default = 0x10 
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = INT1_DURATION; //beware: on 6 digits only!
		reg[1] = 0x00 ;		 //   [ODR=100HZ -> 1LSB = 10ms]  [ODR=10HZ -> 1LSB = 100ms]
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);

		reg[0] = INT1_CFG;
		reg[1] = 0xAA  ;		// AND combination of interrupt events  / all interrupts HIGH on XYZ 
		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
		nrf_delay_ms(I2C_DELAY);	
		while (m_xfer_done == false);				

//		reg[0] = WHO_AM_I; // commented out by originalwriter
//		uint8_t value=0;
//		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, 1, false);
//		nrf_delay_ms(I2C_DELAY);	
//		while (m_xfer_done == false);
//		err_code = nrf_drv_twi_rx(&m_twi, ACCEL_I2C_ADDR, &value, 1);
//		nrf_delay_ms(I2C_DELAY);
//		while (m_xfer_done == false);

// GO TO SLEEP NEEDS TO BE CONFIGURED ON PIN 1
////******** INT2 Config for GO TO SLEEP INTERRUPT ********************
//		reg[0] = INT2_THS;
//		reg[1] =  G_ACCELERO_THRESHOLD_INT2;  //1LSb = 16mg @FS=2g  (-> 0x10 * 16mg = 256mg)  (-> 0x40 * 16mg = 898mg)  preferred value = 0x10
//		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
//		nrf_delay_ms(I2C_DELAY);	
//		while (m_xfer_done == false);

//		reg[0] = INT2_DURATION; //beware: on 6 digits only!
//		reg[1] = 0x7F ;		 //7F = 12.7 seconds 3F=6.3 seconds   [ODR=10HZ -> 1LSB = 1/10HZ = 100ms]  [ODR=100HZ -> 1LSB = 10ms]  DEFAULT = 0x7F
//		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
//		nrf_delay_ms(I2C_DELAY);	
//		while (m_xfer_done == false);

//		reg[0] = INT2_CFG;
//		reg[1] = 0x95  ;		// AND combination of interrupt events  / all interrupts LOW XYZ 
//		err_code = nrf_drv_twi_tx(&m_twi, ACCEL_I2C_ADDR, reg, sizeof(reg), false);
//		nrf_delay_ms(I2C_DELAY);	
//		while (m_xfer_done == false);			
////************************************************************

} //end accelero_init()




/**@brief Function for application main entry.
 */
int twi_example(void){ //  main()


    printf("shit is running");
    sd_power_system_off();
    printf("shit is running");
	
    sleep_timer_init(); // this starts a 5 second timer
		gpio_init(); //for accelerometer
	
    twi_init(); //Initialization of the I2C
		
		accelero_init();	//accelerometer
		
 

    while(1) // wait for a flag set by the ISR before preforming any code
    { 

    if (accel_int == true){  // on wakeup
		nrf_gpio_pin_toggle(LED_2); // toggle an LED on the interrupt 
		accel_int = false;
    }
	}
}

#endif