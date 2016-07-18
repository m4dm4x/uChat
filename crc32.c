/*               _________
 *  ___ _ __ ___|___ /___ \   ___
 * / __| '__/ __| |_ \ __) | / __|
 *| (__| | | (__ ___) / __/ | (__
 * \___|_|  \___|____/_____(_)___|
 *
 */

#include<stdint.h> /* Standart Integer Typen */
#include"bitman.h"

#define CRC32POLYREV 0xEDB88320 /* CRC-32 Polynom, umgekehrte Bitfolge */

static uint32_t calc_crc32_rev(int bit, uint32_t crc32_rev)
{   
    int lbit;
    lbit=crc32_rev & 1;
    if (lbit != bit)
        crc32_rev=(crc32_rev>>1) ^ CRC32POLYREV;
    else
        crc32_rev=crc32_rev>>1;
    return crc32_rev;
}


extern uint32_t crc32_stream_calc(uint32_t old_crc, uint8_t databyte){
  uint32_t new_crc32 = old_crc;
  int i;
  for (i=7; i>=0; --i)
    new_crc32 = calc_crc32_rev(GET_BIT(i, databyte), new_crc32);
  return new_crc32;
}
