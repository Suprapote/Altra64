
#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"
#include "localevar.h"


void menu_delete(display_context_t disp, bool isdir)
{	
	if (isdir)
	{
		printText(directoriesNO, 7, 14, disp);
		printText(bexit, 13, 16, disp);	
	}
	else
	{
		printText(deletefile, 10, 14, disp);
		printText(aconfirm, 13, 16, disp);
		printText(bcancel, 13, 17, disp);
	}
} 
