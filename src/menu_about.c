
#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"


void menu_about(display_context_t disp)
{
    printText("  - About Menu -", 9, -1, disp);
    printText("Code engine", 9, -1, disp);
    printText("by JonesAlmighty", 9, -1, disp);
    printText("Based on ALT64", 9, -1, disp);
    printText("By Saturnu", 9, -1, disp);
    printText("credits to:", 9, -1, disp);
    printText("Altra64 Contributors", 9, -1, disp);
    printText(" ", 9, -1, disp);
    printText("Libraries used:", 9, -1, disp);
    printText("Libdragon", 9, -1, disp);
    printText("Libmikmod-N64", 9, -1, disp);
    printText("Libmad-N64", 9, -1, disp);
    printText("Libyaml", 9, -1, disp);
    printText(" ", 9, -1, disp);
    printText("Z: Controlls menu", 9, -1, disp);
} //TODO: make scrolling text, should include libraries used.
