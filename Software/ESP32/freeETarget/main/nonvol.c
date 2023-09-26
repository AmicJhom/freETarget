/*-------------------------------------------------------
 * 
 * nonvol.c
 * 
 * Nonvol storage managment
 * 
 *-------------------------------------------------------
 *
 * See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
  * 
 * 
 * ----------------------------------------------------*/
#include <stdio.h>
#include "freETarget.h"
#include "json.h"

//#include "serial_io.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

//#include "gpio.h"
#include "diag_tools.h"
#include "serial_io.h"
#include "timer.h"
#include "nonvol.h"

/*
 *  Local variables
 */
nvs_handle_t my_handle;                    // Handle to NVS space

/*----------------------------------------------------------------
 * 
 * functoon: read_nonvol
 * 
 * brief: Read nonvol and set up variables
 * 
 * return: Nonvol values copied to RAM
 * 
 *---------------------------------------------------------------
 *
 * Read the nonvol into RAM.  
 * 
 * If the results is uninitalized then force the factory default.
 * Then check for out of bounds and reset those values
 *
 *------------------------------------------------------------*/
void read_nonvol(void)
{
  long          nonvol_init;
  unsigned int  i;             // Iteration Counter
  long          x;             // 32 bit number
  size_t        length;        // Length of input string
  union pack    nvm64;         // Save a float as a long
  esp_err_t     err;           // ESP32 error type

  if ( DLT(DLT_CRITICAL) )
  {
    printf("read_nonvol()\r\n");
  }

 /*
  * Initialize NVS
  */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());        // NVS partition was truncated and needs to be erased
        err = nvs_flash_init();
    }

/*
 * Read the nonvol marker and if uninitialized then set up values
 */

  if (nvs_open(NAME_SPACE, NVS_READWRITE, &my_handle) != ESP_OK)
  {
    DLT(DLT_CRITICAL);
    printf("read_nonvol(): Failed to open NVM");
  }
        
  nvs_get_i32(my_handle, "NONVOL_INIT", &nonvol_init);
  
  if ( nonvol_init != INIT_DONE)                       // EEPROM never programmed
  {
    factory_nonvol(true);                              // Force in good values
  }

  nvs_get_i32(my_handle, "NVM_SERIAL_NO", &nonvol_init);

  if ( nonvol_init == (-1) )                          // Serial Number never programmed
  {
    factory_nonvol(true);                             // Force in good values
  }

  nvs_get_i32(my_handle, "NVM_PS_VERSION", &nonvol_init);
  if ( nonvol_init != PS_VERSION )                    // persistent storage version
  {
    update_nonvol(nonvol_init);
  }
  
/*
 * Use the JSON table to initialize the local variables
 */
 i=0;
 while ( JSON[i].token != 0 )
 {
   if ( (JSON[i].value != 0) || (JSON[i].d_value != 0)  )    // There is a value stored in memory
   {
     switch ( JSON[i].convert & IS_MASK )
     {
        case IS_VOID:
          break;
          
        case IS_TEXT:
        case IS_SECRET:
          if ( JSON[i].non_vol != 0 )                           // Is persistent storage enabled?
          {
            length = JSON[i].convert & FLOAT_MASK;
            nvs_get_str(my_handle, JSON[i].non_vol, (char*)JSON[i].value, &length);
          }
          break;

        case IS_INT32:
        case IS_FIXED:
          if ( JSON[i].non_vol != 0 )                          // Is persistent storage enabled?
          {
            nvs_get_i32(my_handle, JSON[i].non_vol, &x);       // Read in the value
            *JSON[i].value = x;
          }
          else
          {
            *JSON[i].value = JSON[i].init_value;              // Persistent storage is not enabled, force a known value
          }
          break;

        case IS_FLOAT:
          nvs_get_i64(my_handle, JSON[i].non_vol, (int64_t*)&nvm64.int64);       // Read in the value as an iny
          *JSON[i].d_value = nvm64.double64;
          break;
      }
   }
   i++;
 }

/*
 * Go through and verify that the special cases are taken care of
 */
  multifunction_switch();                                   // Look for an override on the target type
  
/*
 * All done, begin the program
 */
  return;
}



#if(0)
/*----------------------------------------------------------------
 * 
 * funciton: check_nonvol
 * 
 * brief:    Read nonvol and set up variables
 * 
 * return:   Nonvol values copied to RAM
 * 
 *---------------------------------------------------------------
 *
 * Check to see if the NONVOL has been initialized correctly
 *
 *------------------------------------------------------------*/
void check_nonvol(void)
{
  unsigned long nonvol_init;
  
  if ( DLT(DLT_DIAG) )
  {
    printf("check_nonvol()");
  }
  
/*
 * Read the nonvol marker and if uninitialized then set up values
 */
  nvs_get_u32(my_handle, "NONVOL_INIT", &nonvol_init);
  if ( nonvol_init != INIT_DONE)                       // EEPROM never programmed
  {
    factory_nonvol(true);                              // Force in good values
  }

/*
 * Check to see if there has been a change to the persistent storage version
 */
  nvs_get_u32(my_handle, "NONVOL_PS_VERSION", &nonvol_init);
  if ( nonvol_init != PS_VERSION )                    // Is what is in memory not the same as now
  {
    update_nonvol(nonvol_init);                       // Then update the version
  }
  
/*
 * All OK now
 */
  return;
}
#endif

/*----------------------------------------------------------------
 * 
 * function: factory_nonvol
 * 
 * brief: Initialize the NONVOL back to factory settings
 * 
 * return: None
 *---------------------------------------------------------------
 *
 * If the init_nonvol location is not set to INIT_DONE then
 * initilze the memory
 * 
 *------------------------------------------------------------*/
void factory_nonvol
  (
   bool_t new_serial_number
  )
{
  unsigned int serial_number;             // Board serial number
  char         ch;
  unsigned int x;                         // Temporary Value
  unsigned int i;                         // Iteration Counter
  int          length;
  union pack nvm64;
  
  serial_number = 0;
  gen_position(0); 
  x = 0;
  nvs_set_u32(my_handle, "NONVOL_V_SET", 0);
  if ( new_serial_number == false )
  {
    nvs_set_u32(my_handle, "NONVOL_V_SET", serial_number);
  }
 
  if ( DLT(DLT_CRITICAL) )
  {
    printf("factory_nonvol()\r\n");
  }

/*
 * Use the JSON table to initialize the local variables
 */
  i=0;
  while ( JSON[i].token != 0 )
  {
    switch ( JSON[i].convert )
    {
       case IS_VOID:                                        // Variable does not contain anything 
       case IS_FIXED:                                       // Variable cannot be overwritten
       break;
       
       case IS_TEXT:
       case IS_SECRET:
        printf("\r\n%s \"\"", JSON[i].token);
        if ( JSON[i].non_vol != 0 )
        {
          length = JSON[i].convert & FLOAT_MASK;
          nvs_set_i32(my_handle, JSON[i].non_vol, length);                    // Zero out the text
        }
        break;
        
      case IS_INT32:
        x = JSON[i].init_value;                                               // Read in the value 
        printf("\r\n%s %d", JSON[i].token, x);
        if ( JSON[i].non_vol != 0 )
        {
          nvs_set_i32(my_handle, JSON[i].non_vol, x);                         // Read in the value
        }
        break;

      case IS_FLOAT:
        nvm64.double64 = (double)JSON[i].init_value;
        printf("\r\n%s %4.2f", JSON[i].token, nvm64.double64);
        nvs_set_i64(my_handle, JSON[i].non_vol, nvm64.int64);                    // Read in the value
        break;
    }
   i++;
 }
  
/*    
 *     Test the board only if it is a factor init
 */
#if(0)
  if ( new_serial_number )
  {
    printf("\r\n Testing motor drive ");
    for (x=10; x != 0; x--)
    {
      printf("%d+ ", x);
      paper_on_off(true);
      delay(ONE_SECOND/4);
      printf("- ");
      paper_on_off(false);
      delay(ONE_SECOND/4);
    }
    printf(" Test Complete\r\n");
  }
/*
 * Set the trip point
 */
  set_trip_point(0);                      // And stay forever in the set trip mode
#endif

/*
 * Ask for the serial number.  Exit when you get !
 */
  if ( new_serial_number )
  {
    ch = 0;
    serial_number = 0;
    serial_flush(ALL);
    
    printf("\r\nSerial Number? (ex 223! or x))");

    while (1)
    {
      if ( serial_available(CONSOLE) != 0 )
      {
        ch = serial_getch(CONSOLE);
        serial_putch(ch, CONSOLE);
        
        if ( ch == '!' )
        {  
          nvs_set_i32(my_handle, NONVOL_SERIAL_NO, serial_number);
          printf("\r\nSetting Serial Number to: %d", serial_number);
          break;
        }
        if ( (ch == 'x') || (ch == 'X') )
        {
          break;
        }
        serial_number *= 10;
        serial_number += ch - '0';
      }
    }
  }
  
/*
 * Initialization complete.  Mark the init done
 */
  nvs_set_i32(my_handle, NONVOL_PS_VERSION, PS_VERSION); // Write in the version number
  nvs_set_i32(my_handle, NONVOL_INIT, INIT_DONE);
  if ( nvs_commit(my_handle) 
    && DLT(DLT_CRITICAL) )
  {
    printf("Failed to write factory defaults to NONVOL");
  }
  
/*
 * Read the NONVOL and print the results
 */
  read_nonvol();                          // Read back the new values
  show_echo();                            // Display these settings
  
/*
 * All done, return
 */    

  return;
}

 
/*----------------------------------------------------------------
 * 
 * function: init_nonvol
 * 
 * brief: Initialize the NONVOL back to factory settings
 * 
 * return: None
 *---------------------------------------------------------------
 *
 * {"INIT":1234} Reset but leave serial number alone
 * {"INIT":1235} Reset and prompt for new serial number
 * 
 * The variable NONVOL_INIT is corrupted. and the values
 * copied out of the JSON[] table.  If the serial number has
 * not been initialized before, the user is prompted to enter
 * a serial number.
 * 
 * The function then continues to display the current trip
 * point value.
 * 
 *------------------------------------------------------------*/
 #define INIT_ALLOWED         1234        // Number user must enter to allow initialization
 #define INIT_SERIAL_NUMBER   1235        // Number used to re-enter the serial number
 
void init_nonvol
  (
    int verify                            // Verification code entered by user
  )
{
/*
 * Ensure that the user wants to init the unit
 */
  if ( (verify != INIT_ALLOWED) && (verify != INIT_SERIAL_NUMBER) )
  {
    printf("\r\nUse {\"INIT\":1234}\r\n");
    return;
  }

  factory_nonvol(verify == INIT_SERIAL_NUMBER);
  
/*
 * All done, return
 */    

  return;
}

/*----------------------------------------------------------------
 * 
 * function: update_nonvol
 * 
 * brief:  Update the nonvol values if there has been a change
 * 
 * return: None
 *---------------------------------------------------------------
 *
 * If the init_nonvol location is not set to INIT_DONE then
 * initilze the memory
 * 
 *------------------------------------------------------------*/

void update_nonvol
  (
    unsigned int current_version          // Version present in persistent storage
  )
{
  unsigned int  i;                        // Iteration counter
  long          ps_value;                 // Value read from persistent storage  
  
  if ( DLT(DLT_CRITICAL) )
  {
    printf("update_nonvol(%d)", current_version);
  }
  
/*
 * Check to see if this persistent storage has never had a version number
 */
  if ( PS_UNINIT(current_version) )
  {
    printf("\n\rUpdating legacy persistent storage");
    i=0;
    while ( JSON[i].token != 0 )
    { 
      switch ( JSON[i].convert )
      {        
      case IS_INT32:
        nvs_get_i32(my_handle, JSON[i].non_vol, &ps_value);              // Pull up the value from memory
        if ( PS_UNINIT(ps_value) )                          // Uninitilazed?
        {
          nvs_set_i32(my_handle, JSON[i].non_vol, JSON[i].init_value);  // Initalize it from the table
        }
        break;

      default:
        break;
      }
      i++;
   }
   current_version = PS_VERSION;                            // Initialized, force in the current version
   nvs_set_i32(my_handle, NONVOL_PS_VERSION, current_version);
   nvs_commit(my_handle);

   printf("\r\nDone\r\n");
  }

/*
 * Up to date, return
 */
  return;
}

/*----------------------------------------------------------------
 *
 * function: gen_postion
 * 
 * brief: Generate new position varibles based on new sensor diameter
 * 
 * return: Position values stored in NONVOL
 * 
 *---------------------------------------------------------------
 *
 *  This function resets the offsets to 0 whenever a new 
 *  sensor diameter is entered.
 *  
 *------------------------------------------------------------*/
void gen_position(int v)
{
 /*
  * Work out the geometry of the sensors
  */
  json_north_x = 0;
  json_north_y = 0;
  
  json_east_x = 0;
  json_east_y = 0;

  json_south_x = 0;
  json_south_y = 0;
  
  json_west_x = 0;
  json_west_y = 0;
   
 /* 
  *  All done, return
  */
  return;
}

