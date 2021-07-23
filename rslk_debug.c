/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "Library/Reflectance.h"
#include "Library/Clock.h"
#include "Library/Bump.h"
#include "Library/Motor.h"
#include "Library/Encoder.h"

void Initialize_System();

uint8_t light_data; // QTR-8RC
uint8_t light_data0, light_data1, light_data2, light_data3, light_data4, light_data5, light_data6, light_data7;
int32_t Position; // 332 is right, and -332 is left of center
uint8_t bump_data;
uint8_t bump_data0, bump_data1, bump_data2, bump_data3, bump_data4, bump_data5;
int left_motor_pwm = 0;
int left_motor_dir = 0;
int right_motor_pwm = 0;
int right_motor_dir = 0;
// added to enable LED control from the GUI
int led_lp_blink_data = 0;
int led_lp_blue_data = 0;
int led_lp_green_data = 0;
int led_lp_red_data = 0;
int led_lp_blink_on = 0;

int gc_left_motor_count = 0;
int gc_right_motor_count = 0;
int gc_motor_count_reset = 0;


void RedLED_Init(void){
  P1->SEL0 &= ~0x01;
  P1->SEL1 &= ~0x01;   // 1) configure P1.0 as GPIO
  P1->DIR |= 0x01;     // 2) make P1.0 out
}

// bit-banded address
#define REDLED (*((volatile uint8_t *)(0x42098040)))

void ColorLED_Init(void){
  P2->SEL0 &= ~0x07;
  P2->SEL1 &= ~0x07;    // 1) configure P2.2-P2.0 as GPIO
  P2->DIR |= 0x07;      // 2) make P2.2-P2.0 out
  P2->DS |= 0x07;       // 3) activate increased drive strength
  P2->OUT &= ~0x07;     //    all LEDs off
}
// bit-banded addresses
#define BLUEOUT  (*((volatile uint8_t *)(0x42098068)))
#define GREENOUT (*((volatile uint8_t *)(0x42098064)))
#define REDOUT   (*((volatile uint8_t *)(0x42098060)))


int main(void)
{
    // SMCLK = 12Mhz
    //
    Clock_Init48MHz();

    Initialize_System();

    // move these into the Library later..
    RedLED_Init();
    ColorLED_Init();

    Reflectance_Init();

    Bump_Init();

    Motor_Init();

    encoder_init();

    left_motor_pwm = 0;
    right_motor_pwm = 0;

    while (1)
    {
        // Read Reflectance data into a byte.
        // Each bit corresponds to a sensor on the light bar
        light_data = Reflectance_Read(1000);
        // put into individual variables so we can view it in GC
        light_data0 = (light_data>>0)&0x01;
        light_data1 = (light_data>>1)&0x01;
        light_data2 = (light_data>>2)&0x01;
        light_data3 = (light_data>>3)&0x01;
        light_data4 = (light_data>>4)&0x01;
        light_data5 = (light_data>>5)&0x01;
        light_data6 = (light_data>>6)&0x01;
        light_data7 = (light_data>>7)&0x01;
        // Convert light_data into a Position using a weighted sum
        Position = Reflectance_Position(light_data);

        // Read Bump data into a byte
        // Lower six bits correspond to the six bump sensors
        bump_data = Bump_Read();
        // put into individual variables so we can view it in GC
        bump_data0 = (bump_data>>0)&0x01;
        bump_data1 = (bump_data>>1)&0x01;
        bump_data2 = (bump_data>>2)&0x01;
        bump_data3 = (bump_data>>3)&0x01;
        bump_data4 = (bump_data>>4)&0x01;
        bump_data5 = (bump_data>>5)&0x01;

        // Set PWM from GC global variable
        Set_Left_Motor_PWM(left_motor_pwm);
        Set_Right_Motor_PWM(right_motor_pwm);

        // Set up LEDs
        BLUEOUT = led_lp_blue_data;
        GREENOUT = led_lp_green_data;
        REDOUT = led_lp_red_data;
  //      REDLED = led_lp_blink_data;

        MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN0);


        //printf("test");
        // Set DIR from GC global variable
        if (left_motor_dir==1) {
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN4);
        } else {
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4);
        }
        if (right_motor_dir==1)
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN5);
        else
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN5);

        if (gc_motor_count_reset == 1) {
            clr_motor_counts();
            gc_motor_count_reset = 0;
        }
        gc_left_motor_count = get_left_motor_count();
        gc_right_motor_count = get_right_motor_count();

        Clock_Delay1ms(10);
    }
}

void Initialize_System()
{
    /* Halting the Watchdog */
    MAP_WDT_A_holdTimer();

    /* Configuring GPIO LED1 as an output */
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

//    MAP_GPIO_setAsOutputPin(GPIO_PORT_P8, GPIO_PIN0);
//    MAP_GPIO_setAsOutputPin(GPIO_PORT_P8, GPIO_PIN5);

    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN4);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN5);



    /* Configuring SysTick to trigger at 1500000 (MCLK is 1.5MHz so this will
     * make it toggle every 1s) */
    MAP_SysTick_enableModule();
    MAP_SysTick_setPeriod(4800000);
    MAP_SysTick_enableInterrupt();

    /*
     * Configure P1.1 for button interrupt
     */
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P1, GPIO_PIN1, GPIO_LOW_TO_HIGH_TRANSITION);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P1, GPIO_PIN1);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN1);

    /*
     * Configure P1.4 for button interrupt
     */
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN4);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P1, GPIO_PIN4, GPIO_LOW_TO_HIGH_TRANSITION);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P1, GPIO_PIN4);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN4);

    /* Enabling interrupts */
    MAP_Interrupt_enableInterrupt(INT_PORT1);
    MAP_Interrupt_enableMaster();

}
/* Port1 ISR - This ISR will progressively step up the duty cycle of the PWM
 * on a button press
 */
void PORT1_IRQHandler(void)
{
    uint32_t status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P1);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P1, status);

    // Button S1
    if (status & GPIO_PIN1)
    {
        if (left_motor_pwm > 1000)
            left_motor_pwm = 0;
        else
            left_motor_pwm += 100;
    }

    // Button S2
    if (status & GPIO_PIN4)
    {
        if (right_motor_pwm > 1000)
            right_motor_pwm = 0;
        else
            right_motor_pwm += 100;
    }
}

void SysTick_Handler(void)
{
   led_lp_blink_on++;
   if (led_lp_blink_data && (led_lp_blink_on & 0x04)) {
       REDLED = 1;
   } else {
       REDLED = 0;
   }

   //MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
}
