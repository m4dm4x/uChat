/* 
 * File:   tty_layer.h
 * Author: filip
 *
 * Created on January 1, 2008, 12:37 PM
 */

#ifndef _TTY_LAYER_H
#define	_TTY_LAYER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
    #define MAX_FRAME_LENGTH 262
     
    
    typedef struct {
        uint8_t data[MAX_FRAME_LENGTH];
        short length;
    } frame_struct;
    
    int init_hardware(char *devicename);
    short sendByteArray(uint8_t *data, int length);
    void register_frame_handler(void (*frame_handler_ptr)(frame_struct));

#ifdef	__cplusplus
}
#endif

#endif	/* _TTY_LAYER_H */

