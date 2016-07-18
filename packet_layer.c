/*
 * File:   packet_layer.c
 * Author: filip
 *
 * Created on December 31, 2007, 2:17 AM
 */

#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>

#include"crc32.h"
/*#include"hwlink.h" Hardware Link  (void sendByte())*/
#include"bitman.h"
#include "tty_layer.h"

#include "packet_layer.h"

// uchat.c
extern void new_message(char*msg,short len);
extern void system_message(char *msg);
extern void console_message(char *msg);
extern void system_int_message(char *msg, short in);

void rm_timer(uint8_t packetnumber, uint8_t s_o_r);
void add_timer(long time, short packetnumber, uint8_t send_or_receive, uint8_t info);
static void sendDataPacketFromBuffer(short packetNummer, short pig_type, short pig_packet);
void send_lowest_data_timer(void);

uint8_t send_packet_number = 0;

uint8_t receive_packet_win = 0;
uint8_t send_packet_win = 0;



STRUCT_BUFFER send_buffer[WINDOW_SIZE];
STRUCT_BUFFER received_buffer[WINDOW_SIZE];
STRUCT_PACKET_QUEUE timer[WINDOW_MAX_SIZE];

/* 1 - Falls die Paketnummer im Fenster liegt
 * 0 - Wenn die Paketnummer ausser dem Fenster Liegt
 */
#define checkPacketNr(number, window_pos) ((((number + WINDOW_MAX_SIZE) - window_pos)%WINDOW_MAX_SIZE) < WINDOW_SIZE ? 1 : 0)


static pthread_mutex_t send_data_buffer_mutex;
static pthread_mutex_t timer_mutex;
static pthread_mutex_t pn_mutex;   /* Packetnumber MUTEX */ 
static pthread_cond_t pn_condition;/* Packetnumber - condition */
static pthread_t dec_thread;       /* Timer-Thread */

static uint8_t getNextSendPacketNumber(){
    pthread_mutex_lock(&pn_mutex);
    uint8_t tmp = send_packet_number;
    send_packet_number = (send_packet_number+1)%WINDOW_MAX_SIZE;
    
    while (!checkPacketNr(tmp, send_packet_win)) {
        /*Schlafen bis der Sendepuffer wieder frei ist (und das Fenster weitergeschoben */
        system_message("!Sendepuffer voll!");
        console_message("\nDer Sendepuffer ist voll, bitte warten...");
        pthread_cond_wait(&pn_condition,&pn_mutex);
        console_message("\n");
        system_message("Sendepuffer frei");
    }
    
    pthread_mutex_unlock(&pn_mutex);
    return tmp;
};// getNextSendPacketNumber


static void shiftSendWindow(void){

    pthread_mutex_lock(&send_data_buffer_mutex);

    short index = send_packet_win;
    short max = send_packet_win+WINDOW_SIZE;
    short buffer_pos =  index % WINDOW_SIZE;
    
    //Theoretisch müsste nur die Erste bedingung reichen :)
    while((send_buffer[buffer_pos].ack_arrived == TRUE)&&(index < max)){
        send_buffer[buffer_pos].ack_arrived = FALSE;
        send_buffer[buffer_pos].end_of_message = FALSE;
        
        pthread_mutex_lock(&pn_mutex);
        send_packet_win = (send_packet_win + 1)%WINDOW_MAX_SIZE;
        pthread_cond_signal(&pn_condition);
        pthread_mutex_unlock(&pn_mutex);
        
        index++;
        buffer_pos =  index % WINDOW_SIZE;
    }
    
    pthread_mutex_unlock(&send_data_buffer_mutex);
}//shiftSendWindow


uint8_t message_buffer[MAX_BUFFER_SIZE];
short m_buf_pos = 0;

/* Benutzt die Länge im recceived_buffer um zu sehen ob ein packet schon benutzt wurde oder nicht
 * -> falls es benutzt werden kann .hängt er es an den message_string und kontrolliert ob die nachricht komplett ist (EOM)
 *    ,falls ja -> wird sie an die obere schicht geschickt
 */
static void shiftReceiverWindow(void){

    short index = receive_packet_win;
    short max = receive_packet_win+WINDOW_SIZE;
    short buffer_pos =  index % WINDOW_SIZE;
    
    //Theoretisch müsste nur die Erste bedingung reichen :)
    while((received_buffer[buffer_pos].ack_arrived == TRUE)&&(index < max)){
        
        memcpy((message_buffer+m_buf_pos), received_buffer[buffer_pos].data, received_buffer[buffer_pos].length*sizeof(uint8_t));
        
        m_buf_pos += received_buffer[buffer_pos].length;
        
        if (received_buffer[buffer_pos].end_of_message) {           
            received_buffer[buffer_pos].end_of_message = FALSE;
            new_message(message_buffer, m_buf_pos);
            system_int_message("Length:",m_buf_pos);
            m_buf_pos = 0;
        }       
        
        received_buffer[buffer_pos].ack_arrived = FALSE;
        
        receive_packet_win = (receive_packet_win + 1)%WINDOW_MAX_SIZE;
        
        index++;
        buffer_pos =  index % WINDOW_SIZE;
    }

}//shiftReceiverWindow
    

short checkCRC(frame_struct f){
    uint32_t crc_calc = CRC32_START_WERT;
    uint32_t crc_received = 0;
    
    crc_received |= f.data[f.length-4]<<24;
    crc_received |= f.data[f.length-3]<<16;
    crc_received |= f.data[f.length-2]<<8;
    crc_received |= f.data[f.length-1];
    
    short i;
    for(i=0;i<(f.length-4);i++){
        crc_calc = crc32_stream_calc(crc_calc, f.data[i]);
    }
    /*crc_calc ^= CRC32_START_WERT;*/
    
    
    //printf("CALC:%i REC:%i\n", crc_calc, crc_received);
    
    return (crc_calc == crc_received);
}//checkCRC

#define GET_INFOTYPE(byte) ((byte>>4)&7)
void parseInfoFrame(short inf_type, short packetNumber, short bufNumber){
                switch (inf_type) {
                case DUP:
                    system_message("[DUP]");
                case ACK: /* Positive bestätigung */
                    system_message("[ACK]");
                    //new_message("", 0);
                    //int_message(packetNumber);
                    rm_timer(packetNumber, DATA_PACKET);
                    
                    if (checkPacketNr(packetNumber,send_packet_win)){
                        pthread_mutex_lock(&send_data_buffer_mutex);
                        send_buffer[bufNumber].ack_arrived = TRUE;
                        pthread_mutex_unlock(&send_data_buffer_mutex);
                    
                        /* Verschiebt das Fenster, falls möglich*/
                        shiftSendWindow();
                    }
                    break;
                    
                    
                case NAK: /* Negative bestätigung */
                    system_message("[NAK]");
                    //new_message("NAK Packet angekommen", 0);
                    //                  if (send_buffer[send_packet_win%WINDOW_SIZE] == packetNumber){
                    //                    sendDataPacketFromBuffer(packetNumber);
                    
                    //              }
                case CRC_ERROR: /* CRC error beim CRC check auf Senderseite */
                    system_message("[CRC_ERR]");
                    send_lowest_data_timer();
                    break;
                case CRITICAL_ERROR: /* Sender hat sich verabschiedet... */
                    system_message("[CRITICAL ERROR] - Sender/Empfänger nicht verfügbar...");
                    //int_message(packetNumber);
                    
                    break;
        default:
            system_message("[FEHLER] Unbekantes infopacket...");
    }
}//parseInfoFrame

extern void frame_arrived(frame_struct ar_frame){
    if (!checkCRC(ar_frame)) {
        system_message("-> CRC Fehler im Paket");
        add_timer(INFO_TIMEOUT, receive_packet_win, INFO_PACKET, CRC_ERROR);
        return;
    }
    
    /* CRC-Check war OK */
    
    short packetNumber = GET_HEADER_PACKET_NUMMER(ar_frame.data[0]);
    short bufNumber = packetNumber % WINDOW_SIZE;
    
    switch (GET_BIT(PACKET_TYPE, ar_frame.data[0])){
        case INFO_PACKET:
            parseInfoFrame(GET_INFOTYPE(ar_frame.data[0]),packetNumber,bufNumber);
            break;
        case DATA_PACKET:

            if (!checkPacketNr(packetNumber,receive_packet_win)) { /* Paket ausser dem Empfangsfenster */
                add_timer(INFO_TIMEOUT, packetNumber, INFO_PACKET, DUP);
                break;
            }
            
            if (received_buffer[bufNumber].ack_arrived) {
               add_timer(INFO_TIMEOUT, packetNumber, INFO_PACKET, DUP); /* Duplizites packet (im fenster) - entsprechend behandeln :) */
               break;
            }
            
            
            /* Falls die Tests am Anfang OK waren - Sende Positive Bestätigung */
            

            received_buffer[bufNumber].packet_number = packetNumber;
            received_buffer[bufNumber].end_of_message = GET_BIT(END_OF_MESSAGE, ar_frame.data[0]);
         
            
            received_buffer[bufNumber].ack_arrived = TRUE;
            
            
            /* Falls ein Packet vor dem */

            /* Noch kein Kurzer Längentest */
            
            /* Piggybacking */
            short piggy = GET_BIT(EXTENDED_HEADER, ar_frame.data[0]);
            
            if (piggy) {
                parseInfoFrame(GET_INFOTYPE(ar_frame.data[1]),GET_HEADER_PACKET_NUMMER(ar_frame.data[1]),GET_HEADER_PACKET_NUMMER(ar_frame.data[1])%WINDOW_SIZE);
                system_message("-> Piggy-packet");
                }
            
            received_buffer[bufNumber].length = ar_frame.data[1+piggy];
            
            memcpy(received_buffer[bufNumber].data,(ar_frame.data+(2+piggy)),sizeof(uint8_t)*(ar_frame.length-(5+piggy)));

            shiftReceiverWindow();
            
            add_timer(INFO_TIMEOUT, packetNumber, INFO_PACKET, ACK);
            break;
    }
} //frame_arrived


/*
 * Data header:
 * [1][E][X][U][N][N][N][N]
 *  N - Packet number
 *  E - End of Message (bool)
 *  X - Extended Header (bool)
 *  U - Unused
 */
static uint8_t generateDataHeader(uint8_t packetNummer, uint8_t end_of_message , uint8_t piggybacking) {
    //system_int_message("Create msg:",end_of_message);
    uint8_t header = 0;
    SBI(PACKET_TYPE, header);                       /* Daten Packet - 1000 0000 */
    SET_HEADER_PACKET_NUMBER(packetNummer, header); /* Packet Nummer (N) 1NNN0000 */
    if (end_of_message)
        SBI(END_OF_MESSAGE, header);            /* End of Message (E): 1NNNE000 */
    if (piggybacking)
        SBI(EXTENDED_HEADER, header);/*Erweiterter header (X) 1NNNEX00 */
    return header;
}

/*
 * Info Header:
 * [0][I][I][I][N][N][N][N]
 * N - Packet number
 * I - Info type
 */
uint8_t generateInfoHeader(uint8_t packet_number, uint8_t info_type){
    uint8_t header = 0;
    SET_HEADER_PACKET_NUMBER(packet_number, header);
    header |= (info_type&7)<<4;
    return header;
}

/* Bastelt ein Info-Paket und schickt es an die Untere Schicht*/
void sendInfoPackage(short packet_number, uint8_t info){
    uint32_t crc_wert = CRC32_START_WERT;
    
    uint8_t i[5]; /* 5-Byte Paket [Info-Header][CRC][CRC][CRC][CRC] */
    i[0] = generateInfoHeader((uint8_t)packet_number, info);
    
    crc_wert = crc32_stream_calc(crc_wert, i[0]);
    
    i[1] = (crc_wert>>24)&0xFF;
    i[2] = (crc_wert>>16)&0xFF;
    i[3] = (crc_wert>>8)&0xFF;
    i[4] = crc_wert&0xFF;
    
    sendByteArray(i, 5);
}

/*                                                                            */
/* NUR vom TIMER aus benutzen!! (sonst kann piggybacking nicht genutzt werden */
/* Sendet ein Daten Paket vom Buffer                                          */
/* Negativer pig_type - bedeutet kein piggybacking                            */
/*                                                                            */
static void sendDataPacketFromBuffer(short packetNummer, short pig_type, short pig_packet){    
    pthread_mutex_lock(&send_data_buffer_mutex);
    
    uint8_t buffer_number = packetNummer%WINDOW_SIZE;

    
    
    uint8_t packet_buffer[262];
    

    
    uint32_t crc_wert = CRC32_START_WERT;
    
    int index = 0;
    packet_buffer[index] = generateDataHeader(send_buffer[buffer_number].packet_number, send_buffer[buffer_number].end_of_message, (pig_type != -1));
    crc_wert = crc32_stream_calc(crc_wert, packet_buffer[index]);
    index++;
    
    if (pig_type != -1) {
        packet_buffer[index] = generateInfoHeader(pig_packet, pig_type);
        crc_wert = crc32_stream_calc(crc_wert, packet_buffer[index]);
        index++;
    }
    
    packet_buffer[index] = send_buffer[buffer_number].length;
    crc_wert = crc32_stream_calc(crc_wert, packet_buffer[index]);
    index++;
    

    int i;
    for(i = 0;i < send_buffer[buffer_number].length;i++){
        packet_buffer[index] = send_buffer[buffer_number].data[i];
        crc_wert = crc32_stream_calc(crc_wert, packet_buffer[index]);
        index++;
    }
    
    pthread_mutex_unlock(&send_data_buffer_mutex);

    packet_buffer[index] = (crc_wert>>24)&0xFF;
    index++;
    packet_buffer[index] = (crc_wert>>16)&0xFF;
    index++;
    packet_buffer[index] = (crc_wert>>8)&0xFF;
    index++;
    packet_buffer[index] = crc_wert&0xFF;
    index++;

    sendByteArray(packet_buffer, index);
}//sendDataPacketFromBuffer

static void sendDataPacket(uint8_t *datastream, uint8_t length, uint8_t eom){
    uint8_t packet_number = getNextSendPacketNumber();
    pthread_mutex_lock(&send_data_buffer_mutex);
    uint8_t buffer_number = packet_number % WINDOW_SIZE;

    

    send_buffer[buffer_number].packet_number = packet_number;
    send_buffer[buffer_number].length = length;
    
    memcpy(send_buffer[buffer_number].data,datastream,sizeof(uint8_t)*length);
    
 
    send_buffer[buffer_number].end_of_message = eom;
    
    send_buffer[buffer_number].ack_arrived = FALSE;

    pthread_mutex_unlock(&send_data_buffer_mutex);
   
    /* Senden */  
    add_timer(0, packet_number, DATA_PACKET, 0);

}

#define FRAME_MTU 255
/* Erledigt das aufsplitten der Nachricht in kleinere teile
 * erstellt pakete und schickt diese an die untere schicht.
 */
extern int sendData(uint8_t *datastream, long length){
    uint8_t buffer[FRAME_MTU];
    long index = 0;
    while(index < length){
        int i;
        for (i = 0;(i<FRAME_MTU)&&(index < length);i++, index++){
            //printf("index: %i\n",index);
            buffer[i] = datastream[index];
        }
        //system_int_message("sendData(eom):",(index==length ? TRUE : FALSE));
        sendDataPacket(buffer, i, (index==length) ? 1 : 0);
    }
    return 1;
}


/* !! Nur im TIMER benutzen !!*/
/* Findet den Kleinsten TYPE Timer -1 falls keine gefunden wurde */
int find_lowest_timer(short t_type){
    int index;
    int lowest = -1;
    for(index = 0;index<WINDOW_MAX_SIZE;index++)
        if ((timer[index].send_or_receive==t_type)&&(timer[index].time > 0)){
            if (lowest == -1)
                lowest = index;
            if (timer[lowest].time > timer[index].time)
                lowest = index;
        }
    return lowest;
}//find_lowest_info_timer



/* sucht ein leeres feld für den timer */
void add_timer(long time, short packetnumber, uint8_t send_or_receive, uint8_t info) {
    pthread_mutex_lock(&timer_mutex);
    //system_int_message("addtimer:",send_buffer[packetnumber%WINDOW_SIZE].end_of_message);
    int i=0;
    
    if ((time == 0) && (send_or_receive == DATA_PACKET)){
         
            int it = find_lowest_timer(INFO_PACKET);
            
            time = DATA_TIMEOUT;
            if (it != -1){
                timer[it].time=-1;
                sendDataPacketFromBuffer(packetnumber, timer[it].info, timer[it].packetnumber);
                }
            else
                sendDataPacketFromBuffer(packetnumber,-1,-1);
            //system_int_message("addtimer:",send_buffer[packetnumber%WINDOW_SIZE].end_of_message);
 //           pthread_mutex_unlock(&timer_mutex);
   //         return;
    } 
    for (i=0 ; i<WINDOW_MAX_SIZE; i++)
        if(timer[i].time==-1) {
            timer[i].time=time;
            timer[i].packetnumber=packetnumber;
            timer[i].send_or_receive=send_or_receive;
            timer[i].info=info;
            break;
        }   
    pthread_mutex_unlock(&timer_mutex);
}


void send_lowest_data_timer(void){
    pthread_mutex_lock(&timer_mutex);
    
    int lowest = find_lowest_timer(DATA_PACKET);
    int it = find_lowest_timer(INFO_PACKET);

    if (lowest != -1){
            timer[lowest].time=DATA_TIMEOUT;
            if (it != -1) {

                timer[it].time = -1;
                sendDataPacketFromBuffer(timer[lowest].packetnumber,timer[it].info,timer[it].packetnumber);
            } else {
                
                sendDataPacketFromBuffer(timer[lowest].packetnumber,-1,-1);
            }
    }

    pthread_mutex_unlock(&timer_mutex);
}

/* Dekrementiert alle Timer, löscht Infotimer und sendet Datenpakete neu **/
void* dec_timer(void*data) {
    int i=0;
    int it = 0;
    while (1){
        usleep(10000);
        
        pthread_mutex_lock(&timer_mutex);
        for(i=0; i<WINDOW_MAX_SIZE; i++) {
            if (timer[i].time > 0) timer[i].time--;
            if(timer[i].time==0) {
                
                if (timer[i].send_or_receive==DATA_PACKET) {
                    //new_message("Data Packet Timer abgelaufen - timer",0);
                    timer[i].time=DATA_TIMEOUT;
                    it = find_lowest_timer(INFO_PACKET);
                    if (it != -1) {
                        timer[it].time=-1;
                        sendDataPacketFromBuffer(timer[i].packetnumber,timer[it].info,timer[it].packetnumber);
                    } else {
                        sendDataPacketFromBuffer(timer[i].packetnumber,-1,-1);
                    }
                } else {

                    //new_message("Info Packet Timer abgelaufen - timer",0);
                    sendInfoPackage(timer[i].packetnumber, timer[i].info);
                    timer[i].time=-1;
                }
            }
            
            
        }
        
        pthread_mutex_unlock(&timer_mutex);        
    }
    return NULL;
}



void init_timer() {
    
    int i=0;
    for(i=0; i< WINDOW_MAX_SIZE; i++) timer[i].time=-1;
    pthread_create(&dec_thread, NULL, dec_timer, NULL);
}

void rm_timer(uint8_t packetnumber, uint8_t s_o_r) {
    pthread_mutex_lock(&timer_mutex);
    int i=0;
    for(i=0; i<WINDOW_MAX_SIZE; i++)
        if((timer[i].packetnumber==packetnumber) && (timer[i].send_or_receive==s_o_r)) timer[i].time=-1;
    pthread_mutex_unlock(&timer_mutex);
    
}


extern void init_packet_layer(void){
    pthread_mutex_init(&pn_mutex, NULL);
    pthread_mutex_init(&timer_mutex, NULL);
    pthread_cond_init(&pn_condition, NULL);
    pthread_mutex_init(&send_data_buffer_mutex,NULL);

    // Initialisiert der Empfangsbuffer
    short i;
    for (i =0;i<WINDOW_SIZE;i++){
        received_buffer[i].ack_arrived = FALSE;
        received_buffer[i].length = -1;
    }
    
    //Initialisiert den Sendepuffer
    for (i =0;i<WINDOW_SIZE;i++){
        send_buffer[i].ack_arrived = FALSE;
        send_buffer[i].length = -1;
    }
    init_timer();
}
