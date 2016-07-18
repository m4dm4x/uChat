/*               _________  
 *  ___ _ __ ___|___ /___ \ 
 * / __| '__/ __| |_ \ __) |
 *| (__| | | (__ ___) / __/ 
 * \___|_|  \___|____/_____|
 *
 * Berechnet den CRC32 Wert
 * ist jetzt nicht optimal implenetiert, aber es funktioniert
 * (kopiert aus Wikipedia und für "streams" geändert)
 */

extern uint32_t crc32_stream_calc(uint32_t old_crc, uint8_t databyte);

#define CRC32_START_WERT 0xFFFFFFFF
