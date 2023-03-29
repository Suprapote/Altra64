
#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"
#include "localevar.h"

void menu_controls(display_context_t disp)
{
    printText(controlsmenucon, 4, 4, disp);
    printText(" ", 4, -1, disp);
    printText(showmpkmenu, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(aboutscreen, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(Astartromdirectory, 4, -1, disp);
    printText(Amempak, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(Bbackcancel, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(Startlastrom, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(CLEFT, 4, -1, disp);
    printText(CLEFTVIEMPK, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(CRIGHT, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(CUP, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(CDOWN, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(LplusR, 4, -1, disp);
}
