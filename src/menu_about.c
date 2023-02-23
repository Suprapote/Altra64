#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"
#include "localevar.h"


void menu_about(display_context_t disp)
{
    char version_str[32];
    char firmware_str[32];

    sprintf(version_str, "Altra 64: v%s", Altra64_GetVersionString());
    printText(version_str, 9, 8, disp);                                     //1
    sprintf(firmware_str, "ED64 firmware: v%03x", evd_getFirmVersion());    //2
    printText(firmware_str, 9, -1, disp);   
    printText("by JonesAlmighty", 9, -1, disp);                             //3
    printText("Based on ALT64", 9, -1, disp);                       //4 
    printText("Credits to:", 9, -1, disp);                                     //5
    printText("Altra 64 Contibutors", 9, -1, disp);                            //6
    printText("", 9, -1, disp);                         //7
    printText("Libraries:", 9, -1, disp);                      //8
    printText("Libdragon", 9, -1, disp);                           //9
    printText("Libmikmod-N64", 9, -1, disp);                             //10
    printText("Libmad-N64", 9, -1, disp);                       //11
    printText("Libyaml", 9, -1, disp);                             //12
    printText(" ", 9, -1, disp);                                            //13
    printText(znpage, 9, -1, disp);                                 //14
                                                
} //TODO: make scrolling text, should include libraries used.
