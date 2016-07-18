/* 
 * File:   packet_layer.h
 * Author: filip
 *
 * Created on January 2, 2008, 6:03 PM
 */

#ifndef _PACKET_LAYER_H
#define	_PACKET_LAYER_H

#ifdef	__cplusplus
extern "C" {
#endif

#define GET_HEADER_PACKET_NUMMER(header) (header&15)
#define SET_HEADER_PACKET_NUMBER(number,header) header |= (number&15)

#define PACKET_TYPE 7
#define END_OF_MESSAGE 6
#define EXTENDED_HEADER 5

enum boolean {FALSE, TRUE};
enum packet_type { INFO_PACKET, DATA_PACKET };

enum _info_enum_ {ACK, DUP, NAK, CRC_ERROR, CRITICAL_ERROR};

#define WINDOW_MAX_SIZE 16
#define WINDOW_SIZE 8

#define DATA_TIMEOUT 60 /* Zeit vor dem neusenden */
#define INFO_TIMEOUT 6

typedef struct {
	int packet_number;
	int length;
	uint8_t data[255];
	int end_of_message;
	int ack_arrived;
} STRUCT_BUFFER;

typedef struct packet_queue {
	long time;
	short packetnumber;
	uint8_t send_or_receive;
	uint8_t info;
} STRUCT_PACKET_QUEUE;

//Puffergröße von 4KiB
#define MAX_BUFFER_SIZE 8112


#ifdef	__cplusplus
}
#endif

#endif	/* _PACKET_LAYER_H */

