/*************************************************************************
 * 
 * file: gpio_define.h
 * 
 * description:  Public interface for gpio definition
 * 
 **************************************************************************
 *
 * 
 ***************************************************************************/

#ifndef __GPIO_DEFINE_H__
#define __GPIO_DEFINE_H__

#define GPIO_NUM_1   1
#define GPIO_NUM_2   2
#define GPIO_NUM_3   3
#define GPIO_NUM_4   4
#define GPIO_NUM_5   5

#define GPIO_NUM_6   6
#define GPIO_NUM_7   7
#define GPIO_NUM_8   8
#define GPIO_NUM_9   9
#define GPIO_NUM_10 10

#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15

#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
#define GPIO_NUM_20 20

#define GPIO_NUM_21 21
#define GPIO_NUM_22 22
#define GPIO_NUM_23 23
#define GPIO_NUM_24 24
#define GPIO_NUM_25 25

#define GPIO_NUM_26 26
#define GPIO_NUM_27 27
#define GPIO_NUM_28 28
#define GPIO_NUM_29 29
#define GPIO_NUM_30 30

#define GPIO_NUM_31 31
#define GPIO_NUM_32 32
#define GPIO_NUM_33 33
#define GPIO_NUM_34 34
#define GPIO_NUM_35 35

#define GPIO_NUM_36 36
#define GPIO_NUM_37 37
#define GPIO_NUM_38 38
#define GPIO_NUM_39 39
#define GPIO_NUM_40 40

/*
 *  Schematic Capture
 */
#define RUN_STOP GPIO_NUM_05 


/*
 * Function Prototypes
 */
void gpio_init(void);

/*
 * Type defs
 */
typedef enum gpio_type {
    DIGITAL_IO,                                        // GPIO is used for igital IO
    ANALOG_IO,                                         // GPIO is used for Analog IO
    SERIAL_AUX,                                        // GPIO is used as Serial auxilary port
    PWM_OUT,                                           // GPIO is used as a PWM port
    I2C_PORT,                                          // GPIO is used as a i2c port
    PCNT,                                              // GPIO is used as a Pulse Counter
    LED_STRIP                                          // GPIO is used to drives a LED strip 

} gpio_type_t;

typedef struct DIO_struct  {
    gpio_type_t     type;                              // What type of structure am I
    int             mode;                              // Mode used by the DIO
    int             initial_value;                     // Value set on initialization
} DIO_struct_t;

typedef struct analogIO_struct  {
    gpio_type_t     type;                               // What type of structure am I
    int             gpio;                               // What GPIO is it assigned to
    int             adc_handle;                         // Handle given by OS
    int             adc_config[2];                      // Channel setup
} analogIO_struct_t;

typedef struct serialIO_struct  {
    gpio_type_t     type;                                // What type of structure am I
    int             serial_config[4];                    // baud, parity, length, stop bits
} serialIO_struct_t;

typedef struct I2C_struct  {
    gpio_type_t     type;                               // What type of structure am I
    int             gpio_number_SDA;                    // Number associated with SDA
    int             gpio_number_SCL;                    // Number associated with SDA
} I2C_struct_t;

typedef struct PWM_struct  {
    gpio_type_t     type;                               // What type of structure am I
    int             gpio_number;                        // Number associated with PWM
    int             initial_value;                      // Starting value (percent)
} PWM_struct_t;

typedef struct PCNT_struct  {
    gpio_type_t     type;                                // What type of structure am I
    int             pcnt_control;                        // Number associated with PCNT control
    int             pcnt_signal;                         // Number associated with PCNT signal
    int             pcnt_low;                            // Low limit
    int             pcnt_high;                           // Hight limit
    int             pcnt_unit;                           // What unit to use
} PCNT_struct_t;

typedef struct gpio_struct  {
    char* gpio_name;                                     // GPIO name
    int   gpio_number;                                   // Number associated with GPIO
    void* gpio_uses;                                     // Pointer to IO specific structure
} gpio_struct_t;

extern gpio_struct_t gpio_table[];                      // List of available devices
extern led_strip_config_t        led_strip_config;      // 3 LEDs on the board
extern led_strip_rmt_config_t    rmt_config;            // 10MHz
extern led_strip_handle_t        led_strip;
#endif