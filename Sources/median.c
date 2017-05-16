/*! @file median.c
 *
 *  @brief Median filter.
 *
 *  This contains the functions for performing a median filter on byte-sized data.
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

#include "median.h"

uint8_t Median_Filter3(const uint8_t n1, const uint8_t n2, const uint8_t n3)
{
	if ((n1 >= n2 && n1 <= n3) || (n1 <= n2 && n1 >= n3))
	  return n1;

  if ((n2 >= n1 && n2 <= n3) || (n2 <= n1 && n2 >= n3))
    return n2;

	if ((n3 >= n1 && n3 <= n2) || (n3 <= n1 && n3 >= n2))
	  return n3;

  else
	  return n1; // n1 will be the most recent data, incase that n2 or n3 are NULL
}
