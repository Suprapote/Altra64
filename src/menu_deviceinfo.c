
#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"


void menu_deviceinfo(display_context_t disp)
{
    char version_str[32];
    char firmware_str[32];

    printText("        - Device info -", 4, 4, disp);
    printText(" ", 4, -1, disp);
    sprintf(version_str, "Altra64: v%s", Altra64_GetVersionString());
    printText(version_str, 4, -1, disp);
    printText(" ", 4, -1, disp);
    sprintf(firmware_str, "ED64 firmware: v%03x", evd_getFirmVersion());
    printText(firmware_str, 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("Menu version: 2.0", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("System type: NTSC", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("Assembly date: 1.8.2023", 4, -1, disp);
    printText(" ", 4, -1, disp);
}
