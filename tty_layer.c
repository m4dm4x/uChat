/* tty_layer.c
 *
 */

/* hier wird später der Code für das ansprechen des TTY ports stehen */

#include<stdio.h>
#include<stdint.h>

#include<stdlib.h>
#include<time.h>

#include <pthread.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include "tty_layer.h"

#define FLAG_BYTE     255 /* 255 */
#define ESCAPE_BYTE   222
 
/* Sendet ein Byte mit Bytestuffing - vorbereitet für zb. CRC berechnung in dieser Schicht
#define sendByteWithBytetuffing(byt)   if ((byt == FLAG_BYTE) || (byt == ESCAPE_BYTE))\
                                           if (!sendByte(ESCAPE_BYTE)) return -1;\
                                       if (!sendByte(byt)) return -1;*/



extern void system_message(char *msg); // from uchat.c
extern void frame_arrived(frame_struct ar_frame); // from packet_layer.c

#define _FEHLERSIMULATION_

int port_file_desciptor = -1; /* File descriptor for the port */
struct termios options;

pthread_t port_reader;
static pthread_mutex_t send_mutex;


/* 'open_port()' - Open serial port 1.
 *
 * Returns the file descriptor on success or -1 on error.
 */
static int open_port(char * devicename)
    {
      int fd; /* File descriptor for the port */

      fd = open(devicename, O_RDWR | O_NOCTTY | O_NDELAY);
      if (fd == -1)
      {
       /*
        * Could not open the port.
        */
        return -1;
      }
      else
        fcntl(fd, F_SETFL, 0);

      return (fd);
    }



#ifdef _FEHLERSIMULATION_
static int fehlerrate = 0; /* Fehlerrate in % */
extern void set_fehlerrate(int prozent){
    fehlerrate = prozent % 101;
}
#endif

static short sendByte(uint8_t byte){      
    uint8_t s_buf = byte;
    #ifdef _FEHLERSIMULATION_
    if ((fehlerrate != 0)&&(rand()%101 <= fehlerrate)&&(rand()%20 == 0)){
        system_message("Byte Fehler");
        s_buf = rand();
    }
    #endif
    return write(port_file_desciptor, &s_buf, 1);
}

static uint8_t readByte(void){
    uint8_t bufer = 0;
    read(port_file_desciptor,&bufer,1);
    return bufer;
}


/* 
 * RCF :D :D
 */
frame_struct readFrame(){
    frame_struct buff;
    buff.length = 0;
    uint8_t rbyte;
    if (readByte() != FLAG_BYTE) return buff; /* Fehler - Kein "start" flagbyte */
    //printf("\nStartflag Found!!\n");
    do {
    while((rbyte = readByte()) != FLAG_BYTE){
    
    if (rbyte == ESCAPE_BYTE)
        rbyte = readByte();
    
    //printf("read: %i\n", rbyte);
    
    buff.data[buff.length] = rbyte;
    buff.length++;
    
    if(buff.length > MAX_FRAME_LENGTH) { /* Fehler  ein Flagbyte ist verloren gegangen */
        buff.length = 0;
        return buff;
    }
   
    }
    } while (buff.length == 0);
    
    return buff;
}

// Thread :)
void* readNextFrame(void* data) {
    while (1){  
			frame_struct f = readFrame();
			while (f.length == 0) {
				f = readFrame();
			}
			frame_arrived(f);
		}
    return NULL;
}


/******************************************************************************/
extern int init_hardware(char *devicename) {
    srand (time (0)); /* Zufallszahlen für Fehlergenerierung */

    pthread_mutex_init(&send_mutex, NULL);
     
    port_file_desciptor = open_port(devicename);
    if (port_file_desciptor == -1) return -1; /* Fehler beim öffen des Ports */

    /*
     * Get the current options for the port...
     */

    tcgetattr(port_file_desciptor, &options);

    /*
     * Set the baud rates to 19200...
     */

    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    /*
     * Enable the receiver and set local mode...
     */
    
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    options.c_cflag &= ~(CSIZE | PARENB);
    options.c_cflag |= CS8;


    /*
     * Set the new options for the port...
     */

    tcsetattr(port_file_desciptor, TCSANOW, &options);
    
    int iret = pthread_create( &port_reader, NULL, readNextFrame, NULL);
    return 1;
}




/* Sendet ein Daten-Array
 * Rückgabe:     1   falls Alle Daten gesendet wurden
 *              -1  beim Auftrit eines Fehlers
 *TODO: Ändern -> frame_struct Struktur
 */
extern short sendByteArray(uint8_t *data, int length){
#define returnm(wert)   do { pthread_mutex_unlock(&send_mutex); return(wert); } while(0)
    
    pthread_mutex_lock(&send_mutex);
    
    #ifdef _FEHLERSIMULATION_
    if ( (fehlerrate != 0)&&( rand()%101 < 6 ) ){
        system_message("Frame Verschwindet");
    } else {
    #endif
    
    if (!sendByte(FLAG_BYTE)) returnm(-1);     /* Fehler beim Senden */  
    int i;  
    for(i = 0; i < length; i++){
        if ((data[i] == FLAG_BYTE) || (data[i] == ESCAPE_BYTE)) /* Byte Stuffing */
            if (!sendByte(ESCAPE_BYTE)) returnm(-1); /* Fehler beim senden des Escape-Bytes -> return -1*/   
        if (!sendByte(data[i])) returnm(-1); /* Fehler beim senden der Daten -> return -1*/
        //sendByteWithBytetuffing(data[i]);
    }
    if (!sendByte(FLAG_BYTE)) returnm(-1);     /* Fehler beim Senden */
    
    #ifdef _FEHLERSIMULATION_
    }
    #endif
    
    returnm(1);
}
