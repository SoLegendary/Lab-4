/* ###################################################################
**     Filename    : main.c
**     Project     : Lab3
**     Processor   : MK70FN1M0VMJ12
**     Version     : Driver 01.01
**     Compiler    : GNU C Compiler
**     Date/Time   : 2015-07-20, 13:27, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 1.0
** @brief
**         Main module.
**         This module utilises the K70 Tower Device to input and output basic data packets
**         in accordance to a Communication Protocol as described on UTSOnline.
**         It is to be used in conjunction with TowerPC.exe which sends and receives these
**         packets to and from the Tower via a Windows PC.
*/
/*!
**  @addtogroup main_module main module documentation
**
**  @author Thanit Tangson & Emile Fadel
**  @{
*/
/* MODULE main */


// CPU module - contains low level hardware initialization routines
#include "Cpu.h"
#include "packet.h"
#include "UART.h"
#include "Flash.h"
#include "LEDs.h"
#include "RTC.h"
#include "PIT.h"
#include "FTM.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"


// Tower Protocols based on the command byte of each packet
#define CMD_STARTUP   0x04
#define CMD_VERSION   0x09
#define CMD_NUMBER    0x0B
#define CMD_TOWERMODE 0x0D
#define CMD_PROGBYTE  0x07
#define CMD_READBYTE  0x08
#define CMD_SETTIME   0x0C
#define CMD_MODE      0x0A
#define CMD_ACCEL     0x10

// Global volatile variables

volatile uint16union_t *towerNumber = NULL; // Currently set tower number and mode
volatile uint16union_t *towerMode   = NULL;

bool synchronousMode = true; // variable to track current I2C mode (synchronous by default)


// Function Initializations

/*!
 * @brief The startup packet will send four packets back to the PC by default
 * 	  It will send the startup, version, tower number and mode packets.
 *        If flash is reset, tower number and mode will be set to studentNumber and defaultTowerMode
 *
 * Parameter1 = 0, Parameter2 = 0, Parameter3 = 0
 *
 * @return bool - TRUE if all of the packets were handled successfully.
 */
bool HandleStartupPacket(void)
{
  bool success;

  uint16union_t studentNumber;
  studentNumber.l    = 5696; // Last 4 digits of Emile Fadel's student number
  studentNumber.s.Hi = 0x16;
  studentNumber.s.Lo = 0x40;

  uint16union_t defaultTowerMode;
  defaultTowerMode.l    = 1;    // Default mode 1 according to Lab 2 notes
  defaultTowerMode.s.Hi = 0x00;
  defaultTowerMode.s.Lo = 0x01;

  if (towerNumber == NULL) // If towerNumber is not yet programmed, save it to flash
  {
    // Note that the void* typecast does not appear in lab 2 notes
    success = Flash_AllocateVar((void*)&towerNumber, sizeof(*towerNumber));
    success = Flash_Write16((uint16_t*)towerNumber, studentNumber.l);
    if (!success) // If Flash_Allocate or Flash_Write failed
      return false;
  }

  if (towerMode == NULL) // If towerMode is not yet programmed, save it to flash
  {
    success = Flash_AllocateVar((void*)&towerMode, sizeof(*towerMode));
    success = Flash_Write16((uint16_t*)towerMode, defaultTowerMode.l);
    if (!success)
      return false;
  }


  return ((Packet_Put(CMD_STARTUP, 0x00, 0x00, 0x00)) &&
	  (Packet_Put(CMD_VERSION, 'v', 0x01, 0x00)) &&
	  (Packet_Put(CMD_NUMBER, 0x01, towerNumber->s.Lo, towerNumber->s.Hi)) &&
	  (Packet_Put(CMD_TOWERMODE, 0x01, towerMode->s.Lo, towerMode->s.Hi)));
}



/*!
 * @brief Handles the tower version packet in accordance with the Tower Serial Communication Protocol document
 *
 * Parameter1 = 'v', Parameter2 = 1, Parameter3 = 0 (V1.0)
 * @return bool - TRUE if the packet was handled successfully.
 */
bool HandleVersionPacket(void)
{
  return Packet_Put(CMD_VERSION, 'v', 0x01, 0x00);
}



/*!
 * @brief Handles the tower number packet in accordance with the Tower Serial Communication Protocol document
 *
 * Parameter1 = 0x01, Parameter2 = LSB, Parameter3 = MSB
 * If Parameter1 is 0x02, you are able to set LSB and MSB by passing these in through Parameter2 and Parameter3
 *
 * @return bool - TRUE if the packet was handled successfully.
 */
bool HandleNumberPacket(void)
{
  if (Packet_Parameter1 == 0x02) // If the packet is in SET mode, set the tower number by storing it in flash before returning the packet
  {
    bool success = Flash_Write16((uint16_t*)towerNumber, Packet_Parameter23);

    return Packet_Put(CMD_NUMBER, 0x01, towerNumber->s.Lo, towerNumber->s.Hi) && success;
  }
  else if (Packet_Parameter1 == 0x01) // If the packet is in GET mode, just return the current tower number
  {
    return Packet_Put(CMD_NUMBER, 0x01, towerNumber->s.Lo, towerNumber->s.Hi);
  }

  // If the packet is not in either SET or GET mode, return false
  return false;
}



/*!
 * @brief Handles the tower mode packet in accordance with the Tower Serial Communication Protocol document
 *
 * Parameter1 = 0x01, Parameter2 = LSB, Parameter3 = MSB
 * If Parameter1 is 0x02, you are able to set LSB and MSB by passing these in through Parameter2 and Parameter3
 *
 * @return bool - TRUE if the packet was handled successfully.
 */
bool HandleTowerModePacket(void)
{
  if (Packet_Parameter1 == 0x02) // If the packet is in SET mode, set the tower mode by storing it in flash before returning the packet
  {
    bool success = Flash_Write16((uint16_t*)towerMode, Packet_Parameter23);
    return Packet_Put(CMD_TOWERMODE, 0x01, towerMode->s.Lo, towerMode->s.Hi) && success;
  }
  else if (Packet_Parameter1 == 0x01) // If the packet is in GET mode, just return the current tower mode
    return Packet_Put(CMD_TOWERMODE, 0x01, towerMode->s.Lo, towerMode->s.Hi);

  // If the packet is not in either SET or GET mode, return false
  return false;
}



/*!
 * @brief Handles a Flash Program Byte packet as per the Tower Serial Communication Protocol document
 * by writing the data in parameter3 to the address given in parameter1
 *
 * Parameter1 = address offset (0-7), Parameter2 = 0, Parameter3 = data
 *
 * @return bool - TRUE if the packet was handled successfully.
 */
bool HandleProgBytePacket(void)
{
  // Return false if the address is out of range
  if ((Packet_Parameter1 < 0) || (Packet_Parameter1 > 8))
    return false;
	  
  if (Packet_Parameter1 == 0x08) // 0x08 erases the flash
    return Flash_Erase();
	  
  // Writes the data in parameter3 to the given address starting at FLASH_DATA_START and offsetted according to Parameter1
  return Flash_Write8((uint8_t *)(FLASH_DATA_START + Packet_Parameter1), Packet_Parameter3);
}



/*!
 * @brief Handles a Flash Read Byte packet as per the Tower Serial Communication Protocol document
 * by putting in a packet with the address of the flash byte read and the data contained in that address.
 *
 * Parameter1 = address offset (0-7), Parameter2 = 0, Parameter3 = 0
 *
 * @return bool - TRUE if the packet was handled successfully.
 */
bool HandleReadBytePacket(void)
{
  // Return false if the address is out of range
  if ((Packet_Parameter1 < 0) || (Packet_Parameter1 > 7))
    return false;

  // Data is accessed using a Flash.h macro by starting at address FLASH_DATA_START and offsetting according to Parameter1
  return (Packet_Put(CMD_READBYTE, Packet_Parameter1, 0x00, _FB(FLASH_DATA_START + Packet_Parameter1)));
}

  
  
/*!
 * @brief Handles a Set Time packet as per the Tower Serial Communication Protocol document
 * by setting sending the parameters of the packet to the RTC module to be set.
 *
 * Parameter1 = hours(0-23), Parameter2 = minutes(0-59), Parameter3 = seconds (0-59)
 *
 * @return bool - TRUE if the packet was handled successfully, FALSE if parameters out of range.
 */
bool HandleSetTimePacket(void)
{
  // Return false if any time values are out of range
  if ((Packet_Parameter1 < 0) || (Packet_Parameter1 > 23) ||
      (Packet_Parameter2 < 0) || (Packet_Parameter2 > 59) ||
      (Packet_Parameter3 < 0) || (Packet_Parameter3 > 59))
    return false;
    
  RTC_Set(Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
  
  uint8_t seconds, minutes, hours;
  RTC_Get(&seconds, &minutes, &hours);
    
  return (Packet_Put(CMD_SETTIME, seconds, minutes, hours));
}



/*!
 * @brief Handles a Protocol - Mode packet as per the Tower Serial Communication Protocol document
 * - either getting or setting the mode of operation for the accelerometer module (polling vs interrupts)
 *
 * Parameter1 = 1 for GET, 2 for SET
 * Parameter2 = 0 for asynchronous (polling)
 *              1 for synchronous (interrupts)
 * Parameter3 = 0
 *
 * @return bool - TRUE if the packet was handled successfully, FALSE if parameters out of range.
 */
bool HandleModePacket(void)
{
  if (Packet_Parameter1 == 0x02) // If the packet is for SET change the mode using Accel_SetMode()
  {
    switch (Packet_Parameter2)
	{
	  case 0:
	    synchronousMode = false;
	    return Accel_SetMode(ACCEL_POLL);
      case 1:
	    synchronousMode = true;
	    return Accel_SetMode(ACCEL_INT);
      default:
	    return false;
	}
  }
  
  else if (Packet_Parameter1 == 0x01) // If the packet is for GET, just return the current mode
    return (Packet_Put(CMD_MODE, 1, synchronousMode, 0));

  // If the packet is not in either SET or GET mode, return false
  return false;
}


  
/*!
 * @brief Handles the packet by first checking to see what type of packet it is and processing it
 * as per the Tower Serial Communication Protocol document.
 */
void HandlePacket(void)
{
  bool success = false; // Holds the success or failure of the packet handlers
  bool ackReq  = false; // Holds whether an Acknowledgment is required

  if (Packet_Command & PACKET_ACK_MASK) // Check if ACK bit is set
  {
    ackReq = true;
    Packet_Command &= 0x7F; //Strips the top bit of the Command Byte to ignore ACK bit
  }

  switch (Packet_Command)
  {
    case CMD_STARTUP:
      success = HandleStartupPacket();
      break;
    case CMD_VERSION:
      success = HandleVersionPacket();
      break;
    case CMD_NUMBER:
      success = HandleNumberPacket();
      break;
    case CMD_TOWERMODE:
      success = HandleTowerModePacket();
      break;
    case CMD_PROGBYTE:
      success = HandleProgBytePacket();
      break;
    case CMD_READBYTE:
      success = HandleReadBytePacket();
      break;
    case CMD_SETTIME:
      success = HandleSetTimePacket();
      break;
	case CMD_MODE:
	  success = HandleModePacket();
	  break;
    default:
      success = false;
      break;
  }

  /*!
   * Check if the handling of the packet was a success and an ACK packet was requested
   * If that checks out, set the ACK bit to 1
   * Else, if the handling of the packet failed and an ACK packet was requested
   * Clear the ACK bit in order to indicate a NAK (command could not be carried out)
   * Finally, return the ACK packet to the Tower
   */

  if (ackReq)
  {
    if (success)
      Packet_Command |= PACKET_ACK_MASK; // Set the ACK bit
    else
      Packet_Command &= ~PACKET_ACK_MASK; // Clear the ACK bit

    Packet_Put(Packet_Command, Packet_Parameter1, Packet_Parameter2, Packet_Parameter3); // Send the ACK/NAK packet back out
  }
}
  
  
  
/*************************************/
/** CALLBACK FUNCTIONS FOR ALL ISRs **/
/*************************************/

/*! @brief User callback function for use as a PIT_Init parameter
 *  Every period set by PIT_Set, PIT_ISR is triggered and calls this function
 */
void PITCallback(void* arg)
{
  LEDs_Toggle(LED_GREEN);
}
  
/*! @brief User callback function for use as an RTC_Init parameter
 *  Every second the RTC interrupt occurs to send back clock time to the PC and toggles Yellow LED
 */
void RTCCallback(void* arg)
{
  // Get and send time back to PC, just as in HandleSetTimePacket
  uint8_t seconds, minutes, hours;
  RTC_Get(&seconds, &minutes, &hours);

  LEDs_Toggle(LED_YELLOW);
  Packet_Put(CMD_SETTIME, seconds, minutes, hours);
}

/*! @brief User callback function for use as an FTM_Set parameter
 *  After a 1s delay set by Packet.c receiving a valid packet and turning on Blue LED, this turns it off
 */
void FTM0Callback(void* arg)
{
  LEDs_Off(LED_BLUE);
}

/*! @brief User callback function for the accelerometer data reading
 *  After data is ready to be read, call Accel_ReadXYZ and save its data and send it back to the PC
 */
void AccelCallback(void* arg)
{
  // array of 3 unions and one separate union to save data from the accelerometer readings
  // saves data from the three most recent Accel_ReadXYZ calls to allow for median filtering
  static TAccelData accelData[3];
  TAccelData medianData;
  
  // shifts data in the array unions back (index 0 is most recent data, 2 is oldest data)
  for (uint8_t i = 0; i < 3; i++)
  {
	accelData[2].bytes[i] = accelData[1].bytes[i];
    accelData[1].bytes[i] = accelData[0].bytes[i];
  }

  Accel_ReadXYZ(&accelData[0].bytes);
  
  // Median filters the last 3 sets of XYZ data
  for (uint8_t i = 0; i < 3; i++)
  {
    medianData.bytes[i] = MedianFilter3(accelData[0].bytes[i], accelData[1].bytes[i], accelData[2].bytes[i]);
  }
  
  Packet_Put(CMD_ACCEL, medianData.bytes[0], medianData.bytes[1], medianData.bytes[2]);
}
 
/*! @brief User callback function for the I2C data complete
 *  After data read from AccelCallback, I2C_ISR is triggered to toggle the green LED 
 */
void I2CCallback(void* arg)
{
  LEDs_Toggle(LED_GREEN);
}






/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  const uint32_t BAUDRATE      = 115200;
  const uint32_t ACCELBAUDRATE = 100000;

  TFTMChannel FTM0Channel0; // Struct to set up channel 0 in the FTM module
  FTM0Channel0.channelNb           = 0;
  FTM0Channel0.timerFunction       = TIMER_FUNCTION_OUTPUT_COMPARE;
  FTM0Channel0.ioType.outputAction = TIMER_OUTPUT_LOW;
  FTM0Channel0.userFunction        = FTM0Callback;
  FTM0Channel0.userArguments       = NULL;
  
  TAccelSetup accelSetup; // Struct to set up the accelerometer via I2C0
  accelSetup.moduleClk                     = 50000000;
  accelSetup.dataReadyCallbackFunction     = AccelCallback;
  accelSetup.dataReadyCallbackArguments    = NULL;
  accelSetup.readCompleteCallbackFunction  = I2CCallback;
  accelSetup.readCompleteCallbackArguments = NULL;
  
  

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/


  // Write your code here

  __DI(); // Disable interrupts
  
  if (Packet_Init(BAUDRATE, CPU_BUS_CLK_HZ) &&
      Flash_Init() &&
      LEDs_Init() &&
      FTM_Init() &&
      FTM_Set(&FTM0Channel0) &&
      PIT_Init(CPU_BUS_CLK_HZ, PITCallback, NULL) && 
      RTC_Init(RTCCallback, NULL) &&
	  Accel_Init(accelSetup))
  {
    // PIT_Set(500000000, true);
    // PIT_Enable(true);
    LEDs_On(LED_ORANGE);
    HandleStartupPacket();
	
    __EI(); // Enable interrupts

    for (;;)
    {
      if (Packet_Get()) // If a packet is received.
        HandlePacket(); // Handle the packet appropriately.
      // UART_Poll(); // Continue polling the UART for activity - uncomment for use in Lab 1 or 2
	  
	  if (!synchronousMode)
		AccelCallback(NULL); // If I2C is in polling mode, keep polling here for new data
    }
  }



  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.5 [05.21]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
