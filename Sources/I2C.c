/*! @file
 *
 *  @brief I/O routines for the K70 I2C interface.
 *
 *  This contains the functions for operating the I2C (inter-integrated circuit) module.
 *
 *  @author Thanit Tangson
 *  @date 2017-5-9
 */
/*!
**  @addtogroup main_module main module documentation
**
**  @author Thanit Tangson
**  @{
*/

/*

Frequency specified in the lab notes refers to the SAMPLE RATE of the accelerometer (ie. how often it creates new data)
NOT the frequency of any interrupts or polling done

*/

#include "types.h"
#include "I2C.h"
#include "MK70F12.h"

// Private global variables for user callback function and its arguments
void (*readCompleteCallbackFunction)(void*) = 0;
void *readCompleteCallbackArguments         = 0;


static uint8_t primarySlaveAddress = 0; // private global variable to track accelerometer slave address
static uint8_t slaveAddressWrite   = 0; // write mode address has first bit set
static uint8_t slaveAddressRead    = 0; // read mode address has first bit cleared


// private function to return the SCL divider value that matches the current ICR value
static uint16_t sclDivider(uint8_t icr)
{
  // icr determines SCL divider (see K70 manual pg. 1885)
  switch (icr)
  {
	case 0x10: return 48;
	case 0x11: return 56;
	case 0x12: return 64; 
	case 0x13: return 72; 
	case 0x14: return 80; 
	case 0x15: return 88; 
	case 0x16: return 104; 
	case 0x17: return 128; 
	case 0x18: return 80; 
	case 0x19: return 96; 
	case 0x1A: return 112; 
	case 0x1B: return 128; 
	case 0x1C: return 144; 
	case 0x1D: return 160; 
	case 0x1E: return 192; 
	case 0x1F: return 240; 
	case 0x20: return 160; 
	case 0x21: return 192; 
	case 0x22: return 224; 
	case 0x23: return 256; 
	case 0x24: return 288; 
	case 0x25: return 320; 
	case 0x26: return 384; 
	case 0x27: return 480; 
	case 0x28: return 320; 
	case 0x29: return 384; 
	case 0x2A: return 448; 
	case 0x2B: return 512; 
	case 0x2C: return 576; 
	case 0x2D: return 640; 
	case 0x2E: return 768; 
	case 0x2F: return 960; 
	case 0x30: return 640; 
	case 0x31: return 768; 
	case 0x32: return 896; 
	case 0x33: return 1024; 
	case 0x34: return 1152; 
	case 0x35: return 1280; 
	case 0x36: return 1536; 
	case 0x37: return 1920; 
	case 0x38: return 1280; 
	case 0x39: return 1536; 
	case 0x3A: return 1792; 
	case 0x3B: return 2048; 
	case 0x3C: return 2304; 
	case 0x3D: return 2560; 
	case 0x3E: return 3072; 
	case 0x3F: return 3840; 
	
	default: return 0;
  }
}




/*! @brief Sets up the I2C before first use.
 *
 *  @param aI2CModule is a structure containing the operating conditions for the module.
 *  @param moduleClk The module clock in Hz.
 *  @return BOOL - TRUE if the I2C module was successfully initialized.
 */
bool I2C_Init(const TI2CModule* const aI2CModule, const uint32_t moduleClk)
{
  // System clock gate enable
  SIM_SCGC4 |= SIM_SCGC4_IIC0_MASK;
	
  // I2C enable and interrupt enable for read completions
  I2C0_C1 |= I2C_C1_IICEN_MASK;
  I2C0_C1 |= I2C_C1_IICIE_MASK;
	
  // Saving primary slave address and callback function + arguments into private global variables
  I2C_SelectSlaveDevice(aI2CModule->primarySlaveAddress);
  readCompleteCallbackFunction  = aI2CModule->readCompleteCallbackFunction;
  readCompleteCallbackArguments = aI2CModule->readCompleteCallbackArguments;
	

  uint8_t mult; // value to use in baud rate formula
  uint8_t multSave; // saves the best value for the mult register
  uint8_t icrSave; // saves the best value for the icr register
  uint32_t baudRateError; // baud rate error for current combination of mult and icr in the loop
  uint32_t baudRateErrorMin = aI2CModule->baudRate; // current lowest value for baud rate error range
  
  for (uint8_t multReg = 0; multReg < 3; multReg++)
  {
	  switch (multReg) // mult is used in the formula for baud rate,
	  {                // whereas multReg is the value to be written into the MULT register
	    case 0:
	      mult = 1;
	    	break;
	    case 1:
	      mult = 2;
		    break;
	    case 2:
	      mult = 4;
	  	  break;
  	}
	
	  for (uint8_t icr = 10; icr <= 0x3F; icr++)
	  {
	    baudRateError = aI2CModule->baudRate - (moduleClk/(mult*sclDivider(icr)));

	    if (baudRateError < baudRateErrorMin)
	    {
		    baudRateErrorMin = baudRateError;
		    multSave = multReg;
		    icrSave = icr;
	    }
	  }
  }
  // Write in register values for the most accurate baud rate
  I2C0_F |= I2C_F_MULT(multSave);
  I2C0_F |= I2C_F_ICR(icrSave);
  
  // Enabling fast transmit ACK signals
  //I2C0_C1  |= I2C0_C1_TXAK_MASK;
  //I2C0_SMB |= I2C0_SMB_FACK_MASK;
  
  // Setting up NVIC for I2C0 see K70 manual pg 97
  // Vector=40, IRQ=24
  // NVIC non-IPR=0 IPR=6
  // Clear any pending interrupts on I2C0
  NVICICPR0 = (1 << 24); // 24mod32 = 24
  // Enable interrupts from the I2C0
  NVICISER0 = (1 << 24);
  
  return true;
}



/*! @brief Selects the current slave device
 *
 * @param slaveAddress The slave device address.
 */
void I2C_SelectSlaveDevice(const uint8_t slaveAddress)
{
  primarySlaveAddress = slaveAddress; 
  
  slaveAddressWrite = (slaveAddress << 1);
  slaveAddressWrite |= 0x1; // write mode address has first bit set

  slaveAddressRead = (slaveAddress << 1);
  slaveAddressRead &= ~0x1; // read mode address has first bit cleared
}





/*! @brief Write a byte of data to a specified register
 *
 * @param registerAddress The register address.
 * @param data The 8-bit data to write.
 *
 * @note follows pg. 19 of accelerometer manual - single-byte write
 * MAYBE put this in the form of a switch() inside a for() loop and add in returns + clear I2C0_D in case of NAK
 */
void I2C_Write(const uint8_t registerAddress, const uint8_t data)
{
  while (I2C0_S & I2C_S_BUSY_MASK){;} // wait until bus is idle - maybe also wait after each byte sent?
  
  I2C0_C1 |= I2C_C1_MST_MASK; // START signal
  I2C0_C1 |= I2C_C1_TX_MASK; // I2C is in Tx mode (write)

  for (uint8_t i = 0; i < 3; i++)
  {
	switch (i)
	{
	  case 0:
	    I2C0_D = slaveAddressWrite; // send accelerometer address with R/W bit set to Write
		break;
	  case 1:
	    I2C0_D = registerAddress; // send address of register to be written
		break;
	  case 2:
	    I2C0_D = data; // send data to write into register
		break;
	}
	
	if (I2C0_S & I2C_S_RXAK_MASK) // if no AK received, end the communication
      break;
  }
  
  I2C0_C1 &= ~I2C_C1_MST_MASK; // STOP signal

  // I2C0_D = 0; // clear data register (maybe lack of this is why some Write calls get stuck?
}



/*! @brief Reads data of a specified length starting from a specified register
 *
 * Uses polling as the method of data reception.
 * @param registerAddress The register address.
 * @param data A pointer to store the bytes that are read.
 * @param nbBytes The number of bytes to read.
 *
 * @note follows pg. 19 of accelerometer manual - multi-byte read
 *
 */
void I2C_PollRead(const uint8_t registerAddress, uint8_t* const data, const uint8_t nbBytes)
{
  while (I2C0_S & I2C_S_BUSY_MASK){;} // wait until bus is idle
  
  I2C0_C1 &= ~I2C_C1_TXAK_MASK; // AK signal
  I2C0_C1 |= I2C_C1_TX_MASK; // I2C is in Tx mode (write)
  
  for (uint8_t i = 0; i < (nbBytes + 3); i++)
  {
	switch (i)
	{
	  case 0:
	    I2C0_D = slaveAddressWrite; // send accelerometer address with R/W bit set to Write
		break;
	  case 1:
	    I2C0_D = registerAddress; // send address of register to be written
		break;
	  case 2:
	    I2C0_C1 &= ~I2C_C1_TX_MASK; // I2C is in Rx mode (read)
	    I2C0_C1 |= I2C_C1_RSTA_MASK; // REPEAT START signal
	    I2C0_D   = slaveAddressRead; // send accelerometer address with R/W bit cleared to Read
		break;
	  default:
	  	if (i == (nbBytes - 1)) // 2nd last byte to be read
	      I2C0_C1 |= I2C_C1_TXAK_MASK; // NAK signal
	  
	    if (i == nbBytes) // last byte to be read
	      I2C0_C1 &= ~I2C_C1_MST_MASK; // STOP signal
		
	    *data = I2C0_D; // place data into pointer
		data++; // increment data pointer (if it's an array, this effectively increments to the next index)
	    I2C0_C1 &= ~I2C_C1_TXAK_MASK; // AK signal
		break;
	}
	
	if ((i < 3) && (I2C0_S & I2C_S_RXAK_MASK)) // if no AK received during write stages, end the communication
      break;
  }
  
  //I2C0_C1 |= I2C_C1_TXAK_MASK; // NAK signal
  //I2C0_C1 &= ~I2C_C1_MST_MASK; // STOP signal
}



/*! @brief Reads data of a specified length starting from a specified register
 *
 * Uses interrupts as the method of data reception.
 * @param registerAddress The register address.
 * @param data A pointer to store the bytes that are read.
 * @param nbBytes The number of bytes to read.
 
 // synchronous mode where freq = 1.56Hz and always returns packets whether or not XYZ data has changed
 */
void I2C_IntRead(const uint8_t registerAddress, uint8_t* const data, const uint8_t nbBytes)
{
  while (I2C0_S & I2C_S_BUSY_MASK){;} // wait until bus is idle
  
  I2C0_C1 &= ~I2C_C1_TXAK_MASK; // AK signal
  I2C0_C1 |= I2C_C1_TX_MASK; // I2C is in Tx mode (write)
  
  for (uint8_t i = 0; i < (nbBytes + 3); i++)
  {
	switch (i)
	{
	  case 0:
	    I2C0_D = slaveAddressWrite; // send accelerometer address with R/W bit set to Write
		break;
	  case 1:
	    I2C0_D = registerAddress; // send address of register to be written
		break;
	  case 2:
	    I2C0_C1 &= ~I2C_C1_TX_MASK; // I2C is in Rx mode (read)
	    I2C0_C1 |= I2C_C1_RSTA_MASK; // REPEAT START signal
	    I2C0_D   = slaveAddressRead; // send accelerometer address with R/W bit cleared to Read
		break;
	  default:
	    // ISR should trigger here to test if we need to send NAK or STOP
	    *data = I2C0_D; // place data into pointer
		data++; // increment data pointer (if it's an array, this effectively increments to the next index)
	    I2C0_C1 &= ~I2C_C1_TXAK_MASK; // AK signal
		break;
	}
	
	if ((i < 3) && (I2C0_S & I2C_S_RXAK_MASK)) // if no AK received during write stages, end the communication
      break;
  }
}


/*! @brief Interrupt service routine for the I2C.
 *
 *  Only used for reading data.
 *  At the end of reception, the user callback function will be called.
 *  @note Assumes the I2C module has been initialized.
 *
 *  Interrupt flag is IICIF which only sets if a transfer is COMPLETE
 */
void __attribute__ ((interrupt)) I2C_ISR(void)
{
  I2C0_S |= I2C_S_IICIF_MASK; // w1c interrupt flag
  
  // Flowchart 55-42 on pg 1896 of K70 manual
  if (I2C0_C1 & I2C_C1_MST_MASK)
  {
    if (I2C0_C1 & ~I2C_C1_TX_MASK); // ISR is only for reading
    {
      if (1)//last byte to read)
        I2C0_C1 &= ~I2C_C1_MST_MASK; // STOP signal
      else if (1)//2nd last byte to read
        I2C0_C1 |= I2C_C1_TXAK_MASK;
      // Read from data reg and store
    }
  }

  if (readCompleteCallbackFunction)
    (*readCompleteCallbackFunction)(readCompleteCallbackArguments);
  // may need to only be called if last byte to read
  // maybe send out the data via the callback arguments?
  // if this is the case, then keep a static counter variable
  // up in I2CCallback to track whether all axes are done yet
}




