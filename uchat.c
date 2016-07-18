/* 
 * File:   uchat.c
 * Author: filip
 *
 * Created on December 30, 2007, 4:07 PM
 */
    #include "uchat.h"
    #include <stdlib.h>
    #include <stdio.h>   /* Standard input/output definitions */
    #include <string.h>  /* String function definitions */
    #include <ncurses.h>
    #include <pthread.h>
    #include<unistd.h>    

    #define BUFFER_SIZE 5000
    #define MAX_STRING 1000

// textarea+textbox
WINDOW *systemarea;
WINDOW *textarea;
WINDOW *textbar;
WINDOW *textcontainer;
WINDOW *textcontainer2;
WINDOW *textcontainer3;

extern void new_message(char*msg,short len){
        wprintw(textarea,"-> %s\n",msg,strlen(msg));
        wrefresh(textarea);
	wrefresh(textbar);
}

extern void system_int_message(char *msg, short in){
        wprintw(systemarea,"%s %d\n",msg,in);
        wrefresh(systemarea);
        wrefresh(textbar);
}

extern void system_message(char *msg){
        wprintw(systemarea,"%s\n",msg);
        wrefresh(systemarea);
        wrefresh(textbar);
}

extern void console_message(char *msg){
        wprintw(textbar,"%s",msg);
        wrefresh(textbar);
}

void quit(void) {
	delwin(textbar);
        delwin(textarea);
        delwin(textcontainer);
        delwin(textcontainer2);
	endwin();
}


extern int init_hardware(char *devicename);
extern void init_packet_layer(void);
extern void set_fehlerrate(int prozent);
extern int sendData(uint8_t *datastream, long length);

int main(int argc, char** argv) {
            
        int init = init_hardware(argv[1]);
        init_packet_layer();
        
            
	char buff[BUFFER_SIZE]; /* Test - Sendebuffer */

	//inititalisieren des screens
	initscr();
	atexit(quit);
	start_color();
	clear();

	int height, width;
	getmaxyx(stdscr, height, width);
	//windows initialisieren
     
	textcontainer=newwin(height-3,width-25,0,0);
	textcontainer2=newwin(0,width-25,height-3,0);
        textcontainer3=newwin(height,25,0,width-25);
        systemarea = newwin(height-2,23,1,width-24);
	textarea=newwin(height-5, width-27, 1,1);
	textbar=newwin(1,width-27,height-2,1);


	//farben initialisieren (foreground, background)
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_WHITE, COLOR_BLUE);
        init_pair(3, COLOR_GREEN, COLOR_BLACK);

	wbkgd(textarea, COLOR_PAIR(2));
	wbkgd(textcontainer, COLOR_PAIR(2));
	wbkgd(textcontainer2, COLOR_PAIR(2));
        wbkgd(textcontainer3, COLOR_PAIR(2));
	wbkgd(textbar, COLOR_PAIR(2));
        wbkgd(systemarea, COLOR_PAIR(2));


	box(textcontainer, ACS_VLINE, ACS_HLINE);
	box(textcontainer2, ACS_VLINE, ACS_HLINE);
        box(textcontainer3, ACS_VLINE, ACS_HLINE);
	scrollok(textarea, TRUE);
        scrollok(systemarea, TRUE);
        
        wrefresh(textcontainer3);
	wrefresh(textcontainer2);
	wrefresh(textcontainer);
	wprintw(textarea, wellcome_text);
        if (init == 1)
            wprintw(textarea, "%s erfolgreich Initialisiert\n", argv[1]);
        else
            wprintw(textarea, "Fehler beim initialisieren von %s\n", argv[1]);
	wrefresh(textarea);
        scrollok(textbar,TRUE);
	while(1) {
		wgetnstr(textbar, buff, MAX_STRING);            
                if ((!strcmp(buff,"/quit")) || (!strcmp(buff,"/q"))) break; // \quit - beendet das Programm
                if (strstr(buff,"/g ") == buff) {
                    char bu[10];
                    int anzahl = 0;
                    sscanf(buff,"%s %d",bu,&anzahl);
                    int i;
                    for (i = 0; i < anzahl; i++)
                        buff[i] = 'A';
                    buff[anzahl] = '\0';
                    system_int_message("Generiere:",anzahl+1);
                }
                if (strstr(buff,"/f ") == buff) {
                    char bu[10];
                    int anzahl;
                    sscanf(buff,"%s %d",bu,&anzahl);
                    set_fehlerrate(anzahl);
                    system_int_message("Fehlerrate:",anzahl);
                    continue;
                }
                if ((!strcmp(buff,"/neo")) || (!strcmp(buff,"/matrix"))) {
                    	wbkgd(textarea, COLOR_PAIR(3));
                        wbkgd(textcontainer, COLOR_PAIR(3));
                        wbkgd(textcontainer2, COLOR_PAIR(3));
                        wbkgd(textcontainer3, COLOR_PAIR(3));
                        wbkgd(textbar, COLOR_PAIR(3));
                        wbkgd(systemarea, COLOR_PAIR(3));
                        wrefresh(textcontainer3);
                        wrefresh(textcontainer2);
                        wrefresh(textcontainer);
                        wrefresh(textarea);
                        wrefresh(textbar);
                        wclear(textarea);
                        wprintw(textarea, "-> Wake up, Neo.\n<- What the... ?\n-> The Matrix has you.\n<- What the hell?\n-> Follow the white rabbit.\n-> Knock, knock, Neo.\n");
                        wrefresh(textarea);
                        system_message("[Matrix-Mode ON]");
                        system_message("Hello Mr. Anderson...");
                        continue;
                }
                wprintw(textarea,"<- %s\n",buff);
                sendData(buff,strlen(buff)+1);
		wrefresh(textarea);
		wrefresh(textbar);

	}
        quit();


}
