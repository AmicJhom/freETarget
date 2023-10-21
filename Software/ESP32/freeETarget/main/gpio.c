/*-------------------------------------------------------
 * 
 * gpio.c
 * 
 * General purpose GPIO driver
 * 
 * ----------------------------------------------------*/
#include "freETarget.h"
#include "gpio.h"
#include "timer.h"
#include "diag_tools.h"
#include "stdio.h"
#include "json.h"
#include "token.h"
#include "compute_hit.h"
#include "nvs.h"
#include "nonvol.h"
#include "serial_io.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "C:\Users\allan\esp\esp-idf\esp-idf\components\hal\include\hal\gpio_types.h"
#include "C:\Users\allan\esp\esp-idf\esp-idf\components\hal\include\hal\adc_types.h"
#include "C:\Users\allan\esp\esp-idf\esp-idf\components\esp_adc\include\esp_adc\adc_oneshot.h"
#include "driver\gpio.h"
#include "driver\ledc.h"
#include "timer.h"
#include "token.h"
#include "analog_io.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include "pcnt.h"
#include "pwm.h"
#include "gpio_define.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "../managed_components/espressif__led_strip/src/led_strip_rmt_encoder.h"

/*
 * @function prototypes
 */
static void sw_state(unsigned int action);// Do something with the switches
static void send_fake_score(void);        // Send a fake score to the PC

/* 
 *  Typedefs
 */
typedef struct status_struct {
  int blue;                               // Bits to send to the LED
  int green;
  int red;   
  int blink;                              // TRUE if blinking enabled
  }  status_struct_t;

/* 
 * Variables
 */
status_struct_t status[3];
static unsigned int dip_mask;             // Used if the MFS2 uses the DIP_0 or DIP_3

/*-----------------------------------------------------
 * 
 * @function: is_running
 * 
 * @brief: Determine if the clocks are running
 * 
 * @return: TRUE if any of the counters are running
 * 
 *-----------------------------------------------------
 *
 * Read in the running registers, and return a 1 for every
 * register that is running.
 * 
 *-----------------------------------------------------*/
static unsigned int clock[] = { RUN_NORTH_HI, RUN_EAST_HI, RUN_SOUTH_HI, RUN_WEST_HI, 
                                RUN_NORTH_LO, RUN_EAST_LO, RUN_SOUTH_LO, RUN_WEST_LO};
static unsigned int run_mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

unsigned int is_running (void)
{
  unsigned int  return_value;
  unsigned int  i;

  return_value = 0;
/*
 * Read the running inputs
 */
  for (i=0; i != sizeof(clock)/sizeof(unsigned int); i++)
  {
    if ( gpio_get_level(clock[i]) != 0 )
    {
      return_value |= run_mask[i];
    }
  }

/*
 *  Return the run mask
 */
  return return_value;                   // Return the running mask
}

/*-----------------------------------------------------
 * 
 * @function: arm_timers
 * 
 * @brief: Strobe the control lines to start a new cycle
 * 
 * @return: NONE
 * 
 *-----------------------------------------------------
 *
 * The counters are armed by
 * 
 *   Stopping the current cycle
 *   Taking the counters out of read
 *   Making sure the oscillator is running
 *   Clearing the counters
 *   Enabling the counters to run again
 * 
 *-----------------------------------------------------*/
void arm_timers(void)
{
  return;
  gpio_set_level(STOP_N, 0);      // Reset the timer
  pcnt_clear();
  gpio_set_level(STOP_N, 1);      // Then enable it
  
  return;
}

/*
 *  Stop the oscillator and 
 */
void stop_timers(void)
{
  return;
  gpio_set_level(STOP_N, 0);      // Reset the timer

  return;
}

/*
 *  Trip the counters for a self test
 */
void trip_timers(void)
{  
  return;
  gpio_set_level(STOP_N, 1);            // Let the flipflops go
  gpio_set_level(STOP_N, 1);            // Let the flipflops go
  gpio_set_level(CLOCK_START, 1);       // and trigger the output 
  gpio_set_level(CLOCK_START, 0);
  gpio_set_level(CLOCK_START, 1);       // and trigger the output 
  return;
}

/*-----------------------------------------------------
 * 
 * @function: read_DIP
 * 
 * @brief: READ the jumper block setting
 * 
 * @return: TRUE for every position with a jumper installed
 * 
 *-----------------------------------------------------
 *
 * The DIP register is read and formed into a word.
 * The word is complimented to return a 1 for every
 * jumper that is installed.
 *
 *-----------------------------------------------------*/

unsigned int read_DIP(void)
{
  unsigned int return_value = 0;
  unsigned int dips[] = {DIP_A, DIP_B, DIP_C, DIP_D};
  unsigned int bit_mask[] = {0x08, 0x04, 0x02, 0x01};
  unsigned int i;

  for (i=0; i != sizeof(dips)/sizeof(unsigned int); i++)
  {
    if (gpio_get_level(dips[i]) != 0) 
    {
     return_value |= bit_mask[i];
    }
  }

  return return_value;
}  


/*-----------------------------------------------------
 * 
 * @function: init_status_LED
 * 
 * @brief:    Initialize the LED driver
 * 
 * @return:   None
 * 
 *-----------------------------------------------------
 *
 * The status LED driver makes use of the remote control
 * transmitter supported in the ESP32
 * 
 *-----------------------------------------------------*/
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

rmt_channel_handle_t led_chan = NULL;
rmt_tx_channel_config_t tx_chan_config = {
    .clk_src           = RMT_CLK_SRC_DEFAULT, // select source clock
    .mem_block_symbols = 64, // increase the block size can make the LED less flickering
    .resolution_hz     = RMT_LED_STRIP_RESOLUTION_HZ,
    .trans_queue_depth = 1, // set the number of transactions that can be pending in the background
};

rmt_encoder_handle_t led_encoder = NULL;
led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ
};

void status_LED_init
(
  unsigned int led_gpio   // What GPIO is used for output
)
{
  tx_chan_config.gpio_num = led_gpio;
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));
  ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));
  ESP_ERROR_CHECK(rmt_enable(led_chan));
  return;
}

/*-----------------------------------------------------
 * 
 * @function: set_status_LED
 * 
 * @brief:    Set the state of all the status LEDs
 * 
 * @return:   None
 * 
 *-----------------------------------------------------
 *
 * The state of the LEDs can be turned on or off 
 * 
 * 'R' - Set the LED to Red'
 * '-' - Leave the LED alone
 *  
 *-----------------------------------------------------*/
#define   LED_ON 0x1F

rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };

  
unsigned char led_strip_pixels[3 * 3];

void set_status_LED
  (
    char* new_state       // New LED colours
  )
{ 
  int i;

/*
 * Decode the calling string into a list of pixels
 */
  i=0;
  while (*new_state != 0)
  {
    if ( *new_state != '-' )
    {
      status[i].blink = 0;
      status[i].red = 0;
      status[i].green = 0;
      status[i].blue = 0;          // Turn off the LED
      switch (*new_state)
      {
        case 'r':                 // RED LED
          status[i].blink = 1;    // Turn on Blinking
        case 'R':
          status[i].red   = LED_ON;
          break;

        case 'y':                 // RED LED
          status[i].blink = 1;    // Turn on Blinking
        case 'Y':
          status[i].red   = LED_ON/2;
          status[i].green = LED_ON/2;
          break;

        case 'g':               // GREEN LED
          status[i].blink = 1;    // Turn on Blinking
        case 'G':
          status[i].green = LED_ON;
          break;

        case 'b':
          status[i].blink = 1;
        case 'B':
          status[i].blue  = LED_ON;
          break;

        case 'w':
          status[i].blink = 1;
        case 'W':
          status[i].red   = LED_ON/3;
          status[i].green = LED_ON/3;
          status[i].blue  = LED_ON/3;
          break;

        case ' ':             // The LEDs are already off
          break;
      }
    }
    i++;
    new_state++;
  }

/*
 * Ready to output the LEDs
 */
  commit_status_LEDs(1);
  return;
}

/*-----------------------------------------------------
 * 
 * @function: commit_status_LED
 * 
 * @brief:    Write the status LED settings to the hardware
 * 
 * @return:   None
 * 
 *-----------------------------------------------------
 *
 * This function looks at the blink state and outputs
 * the bits to the hardware.
 * 
 * blink = 1 -> put to hardware
 * blink = 0 -> turn off the LED 
 *  
 *-----------------------------------------------------*/
void commit_status_LEDs
  (
    unsigned int blink_state
  )
{
  unsigned int i;

/*
 *  Send out the new settings
 */
  for (i=0; i < 3; i++)
  {
    led_strip_pixels[i * 3 + 0] = 0;
    led_strip_pixels[i * 3 + 2] = 0;
    led_strip_pixels[i * 3 + 1] = 0;
    if ( (status[i].blink == 0) || (blink_state == 1) )
    {
      led_strip_pixels[i * 3 + 0] = status[i].green;
      led_strip_pixels[i * 3 + 2] = status[i].blue;
      led_strip_pixels[i * 3 + 1] = status[i].red;
    }
  }
    
  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));

/*
 * All done, return
 */
  return;

}


/*-----------------------------------------------------
 * 
 * @function: read_timers
 * 
 * @brief:   Read the timer registers
 * 
 * @return:  All four timer registers read and stored
 * 
 *-----------------------------------------------------
 *
 * Force read each of the timers
 * 
 *-----------------------------------------------------*/
void read_timers
(
    unsigned int* timer_ptr
)
{
  unsigned int i;
  unsigned int delta_t;             // Time difference between VREF_LO and VREF_HI
  unsigned int adjust_t;            // Time adjustment applied to clock

  for (i=0; i<=8; i++)
  {
    *(timer_ptr + i) = pcnt_read(i);
  }

#if ( COMPENSATE_RISE_TIME )
  for (i=0; i<=4; i++)   // Add the rise time to the signal to get a better estimate
  {
    delta_t = *(timer_ptr + i + 4) - TIME_ISR;    // Time from VREF_LO to VREF_HI
    adjust_t = delta_t * ((json_vref_hi - json_vref_lo) / json_vref_lo);
    *(timer_ptr + i) += adjust_t;
  }
#endif

  return;
}

/*-----------------------------------------------------
 * 
 * @function: drive_paper
 * 
 * @brief:    Turn on the witness paper motor for json_paper_time
 * 
 * @return:  None
 * 
 *-----------------------------------------------------
 *
 * The function turns on the motor for the specified
 * time.  The motor is cycled json_paper_step times
 * to drive a stepper motor using the same circuit.
 * 
 * Use an A4988 to drive te stepper in place of a DC
 * motor
 * 
 * There is a hardare change between Version 2.2 which
 * used a transistor and 3.0 that uses a FET.
 * The driving circuit is reversed in the two boards.
 * 
 * DC Motor
 * Step Count = 0
 * Step Time = 0
 * Paper Time = Motor ON time
 *
 * Stepper
 * Paper Time = 0
 * Step Count = X
 * Step Time = Step On time
 * 
 *-----------------------------------------------------*/

void drive_paper(void)
{
  unsigned int s_time, s_count;              // Step time and count
  volatile unsigned long paper_time;

/*
 * Set up the count or times based on whether a DC or stepper motor is used
 */

  s_time = json_paper_time;                       // On time.
  if ( json_step_time != 0 )                      // Non-zero means it's a stepper motor
  {
    s_time = json_step_time;                      // the one we use
  }

  s_count = 1;                                    // Default to one cycle (DC or Stepper Motor)
  if ( json_step_count != 0 )                     // Non-zero means it's a stepper motor
  {
    s_count = json_step_count;                    // the one we use
  }

  if ( s_time == 0 )                              // Nothing to do if the time is zero.
  {
    return;
  }
  
  if ( DLT(DLT_CRITICAL) )
  {
    printf("Advancing paper: %dms", s_time);
  }

/*
 * Drive the motor on and off for the number of cycles
 * at duration
 */
  timer_new(&paper_time, 0);              // Create the timer

  while ( s_count )
  {
    paper_on_off(true);                   // Motor ON
    paper_time = s_time; 
    while (paper_time )
    {
      continue;
    }
    paper_on_off(false);                  // Motor OFF
    paper_time = 5;
    while ( paper_time )
    {
      continue;
    }
    s_count--;                            // Repeat for the steps
  }

  timer_delete(&paper_time);              // Finished with the timer
  
 /*
  * All done, return
  */
  return;
 }

/*-----------------------------------------------------
 * 
 * @function: paper_on_off
 * 
 * @brief:    Turn the withness paper motor on or off
 * 
 * @return:  None
 * 
 *-----------------------------------------------------
 *
 * The witness paper motor changed polarity between 2.2
 * and Version 3.0.
 * 
 * This function reads the board revision and controls 
 * the FET accordingly
 * 
 *-----------------------------------------------------*/

void paper_on_off                               // Function to turn the motor on and off
(
  bool on                                      // on == true, turn on motor drive
)
{
  if ( on == true )
  {
    gpio_set_level(PAPER, PAPER_ON);            // Turn it on
  }
  else
  {
    gpio_set_level(PAPER, PAPER_OFF);            // Turn it off
  }

/*
 * No more, return
 */
  return;
}


/*-----------------------------------------------------
 * 
 * @function: face_ISR
 * 
 * @brief:    Face Strike Interrupt Service Routint
 * 
 * @return:   None
 * 
 *-----------------------------------------------------
 *
 * Sensor #5 is attached to the digital input #19 and
 * is used to generate an interrrupt whenever a face
 * strike has been detected.
 * 
 * The ISR simply counts the number of cycles.  Anything
 * above 0 is an indication that sound was picked up
 * on the front face.
 * 
 *-----------------------------------------------------*/
 void face_ISR(void)
 {
  face_strike++;      // Got a face strike

  if ( DLT(DLT_CRITICAL) )
  {
    printf("\r\nface_ISR(): %d", face_strike);
  }

  return;
 }

/*----------------------------------------------------------------
 * 
 * @function: aquire()
 * 
 * @brief: Aquire the data from the counter registers
 * 
 * @return: Nothing
 * 
 *----------------------------------------------------------------
 *
 *  This function reads the values from the counters and saves
 *  saves them into the record structure to be reduced later 
 *  on.
 *
 *--------------------------------------------------------------*/
void aquire(void)
 {
/*
 * Pull in the data amd save it in the record array
 */

  stop_timers();                                    // Stop the counters
  read_timers(&record[this_shot].timer_count[0]);   // Record this count
  record[this_shot].shot_time = 0;                  //FULL_SCALE - in_shot_timer; // Capture the time into the shot
  record[this_shot].face_strike = face_strike;      // Record if it's a face strike
  record[this_shot].sensor_status = is_running();   // Record the sensor status
  record[this_shot].shot_number = shot_number++;    // Record the shot number and increment
  this_shot = (this_shot+1) % SHOT_STRING;          // Prepare for the next shot

/*
 * All done for now
 */
  return;
}


/*----------------------------------------------------------------
 * 
 * @function: rapid_red()
 *           rapid_green()
 * 
 * @brief: Set the RED and GREEN lights
 * 
 * @return: Nothing
 * 
 *----------------------------------------------------------------
 *
 *  If MFS2 has enabled the rapid fire lights then allow the 
 *  value to be set
 *
 *--------------------------------------------------------------*/

void rapid_red
(
  unsigned int state          // New state for the RED light
) 
{
  if ( HOLD1(json_multifunction2) == RAPID_RED )
  {
      gpio_set_level(DIP_0, state);
  }
  if ( HOLD2(json_multifunction2) == RAPID_RED )
  {
      gpio_set_level(DIP_C, state);
  }

  return;
}

void rapid_green
(
  unsigned int state          // New state for the RED light
) 
{
  if ( HOLD1(json_multifunction2) == RAPID_GREEN )
  {
      gpio_set_level(DIP_B, state);
  }
  if ( HOLD2(json_multifunction2) == RAPID_GREEN )
  {
      gpio_set_level(DIP_A, state);
  }

  return;
}

/*-----------------------------------------------------
 * 
 * @function: digital_test()
 * 
 * @brief:    Exercise the GPIO digital ports
 * 
 * @return:   None
 * 
 *-----------------------------------------------------
 *
 * Read in all of the digial ports and report the 
 * results
 * 
 *-----------------------------------------------------*/
void digital_test(void)
{
  printf("\r\nDigital test");

/*
 * Read in the fixed digital inputs
 */
  printf("\r\nTime: %4.2fs", (float)(esp_timer_get_time()/1000000));
  printf("\r\nDIP: 0x%02X", read_DIP()); 
  printf("\r\nDone\r\n");

   return;
}


/*----------------------------------------------------------------
 * 
 * @function: status_LED_test()
 * 
 * @brief:    Cycle the status LEDs
 * 
 * @return:   Nothing
 * 
 *----------------------------------------------------------------
 *
 *--------------------------------------------------------------*/
void status_LED_test(void)
{
  printf("\r\nStatus LED test");
  set_status_LED("R--");
  vTaskDelay(ONE_SECOND);
  set_status_LED("RG-");
  vTaskDelay(ONE_SECOND);
  set_status_LED("RGB");
  vTaskDelay(ONE_SECOND);
  set_status_LED("WWW");
  vTaskDelay(ONE_SECOND);
  set_status_LED("rwb");
  vTaskDelay(5*ONE_SECOND);         // Blink for 5 seconds
  set_status_LED(LED_READY);
  printf("\r\nDone\r\n");
  return;
}

/*----------------------------------------------------------------
 * 
 * @function: paper_test
 * 
 * @brief:    Drive the motor
 * 
 * @return:   Nothing
 * 
 *----------------------------------------------------------------
 *
 *  Drive the motor in 500 ms increments
 *--------------------------------------------------------------*/
void paper_test(void)
{
  volatile int time_delay;
  int i;

  timer_new(&time_delay, 500); 

  printf("\r\nAdvancing paper 500 ms at a time");
  for (i=0; i != 10; i++)
  {
    printf("  %d+", (i+1));
    paper_on_off(true);
    time_delay = 500;
    timer_delay(time_delay);
    printf("-");
    paper_on_off(false);
    time_delay = 500;
    timer_delay(time_delay);
  }

  timer_delete(&time_delay);
  printf("\r\nDone\r\n");

  return;
}