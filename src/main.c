//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

//libdragon n64 lib
#include <libdragon.h>

//everdrive system libs
#include "types.h"
#include "sys.h"
#include "everdrive.h"

//filesystem
#include "sd.h"
#include "ff.h"

//utils
#include "utils.h"

//config file on sd
#include "ini.h"
#include "strlib.h"

//main header
#include "main.h"

//sound
#include "sound.h"
#include "mp3.h"

// YAML parser
#include <yaml.h>

#include "constants.h"
#include "debug.h"
#include "mem.h"
#include "chksum64.h"
#include "image.h"
#include "rom.h"
#include "memorypak.h"
#include "menu.h"
#include "cic.h"

#include "localevar.h"

#define MAX_LIST 20

typedef struct
{
    uint32_t type;
    uint32_t color;
    char filename[MAX_FILENAME_LEN + 1];
} direntry_t;


//ini file
typedef struct
{
    int version;
    const char *name;
    char *background_image;
    char *border_color_1;
    char *border_color_2;
    char *box_color;
    char *selection_color;
    char *list_font_color;
    char *list_dir_font_color;
    char *selection_font_color;
    char *mempak_path;
    char *save_path;
    int sound_on;
    int bgm_on;
    int page_display;
    int tv_mode;
    int quick_boot;
    int enable_colored_list;
    int ext_type;
    int cd_behaviour;
    int scroll_behaviour;
    int text_offset;
    int hide_sysfolder;
    int sd_speed;
    int language;
    int save_backup;


} configuration;

volatile u32 *romaddr_ptr = (u32 *)ROM_ADDR;
unsigned int gBootCic = CIC_6102;

static void *bg_buffer;
void *__safe_bg_buffer;

#define __get_buffer(x) __safe_buffer[((int)x)-1]
extern uint32_t __bitdepth;
extern uint32_t __width;
extern uint32_t __height;
extern void *__safe_buffer[];

int firm_found = 0;
char rom_config[10];
u32 emu_offset;

display_context_t lockVideo(int wait);

int select_mode = 0;
int toplist_reload = 1;
int toplist_cursor = 0;
char toplist15[15][256];

int fat_initialized = 0;
int exit_ok = 0;
int boot_cic = 0;
int boot_save = 0;

int cursor_line = 0;
int cursor_lastline = 0;
u16 cursor_history[32];
u16 cursor_history_pos = 0;

u8 empty = 0;
int mp3playing = 0;

FATFS *fs;

//start with filebrowser menu key settings
enum InputMap
{
    none,
    file_manager,
    mempak_menu,
    char_input,
    rom_loaded,
    mpk_format,
    mpk_restore,
    rom_config_box,
    toplist,
    mpk_choice,
    mpk_quick_backup,
    mp3,
    abort_screen,
    control_screen,
    delete_prompt,
};
enum InputMap input_mapping = file_manager;

//holds the string of the character input screen result
//int text_input_on = 0;

//char input vars
int x, y, position, set;

unsigned char input_text[32];
uint32_t chr_forecolor;
uint32_t chr_backcolor;

//save type still set - save to do after reboot
int save_after_reboot = 0;

//cart id from the rom header
unsigned char cartID[4];
char curr_dirname[64];
char pwd[64];
TCHAR rom_filename[MAX_SUPPORTED_PATH_LEN];

u32 rom_buff[128]; //rom buffer
u8 *rom_buff8;     //rom buffer

u8 *firmware;
u8 gbload = 0;

int cheats_on = 0;
int checksum_fix_on = 0;
short int gCheats = 0; /* 0 = off, 1 = select, 2 = all */
short int force_tv = 0;
short int boot_country = 0;

static resolution_t res = RESOLUTION_320x240;

//background sprites
sprite_t *loadPng(u8 *png_filename);
sprite_t *background;   //background

//config file theme settings
u32 border_color_1 = 0xFFFFFFFF; //hex 0xRRGGBBAA AA=transparenxy
u32 border_color_2 = 0x3F3F3FFF;
u32 box_color = 0x00000080;
u32 selection_color = 0x6495ED60;
u32 selection_font_color = 0xFFFFFFFF;
u32 list_font_color = 0xFFFFFFFF;
u32 list_dir_font_color = 0xFFFFFFFF;

char *border_color_1_s;
char *border_color_2_s;
char *box_color_s;
char *selection_color_s;
char *selection_font_color_s;
char *list_font_color_s;
char *list_dir_font_color_s;

char *save_path;

u8 sound_on = 0;
u8 bgm_on = 0;
u8 page_display = 0;
u8 tv_mode = 0; // 1=ntsc 2=pal 3=mpal 0=default automatic
u8 quick_boot = 0;
u8 enable_colored_list = 0;
u8 cd_behaviour = 0;     //0=first entry 1=last entry
u8 scroll_behaviour = 0; //1=classic 0=new page-system
u8 ext_type = 0;         //0=classic 1=org os
u8 sd_speed = 1;         // 1=25Mhz 2=50Mhz
u8 hide_sysfolder = 0;
u8 language = 0;
u8 save_backup = 1;
char *background_image;

//mp3
int buf_size;
char *buf_ptr;

//toplist helper
int list_pos_backup[3];
char list_pwd_backup[256];

char dirz[512] = "rom://";

int count = 0;
int page = 0;
int cursor = 0;
direntry_t *list;


int filesize(FILE *pFile)
{
    fseek(pFile, 0, SEEK_END);
    int lSize = ftell(pFile);
    rewind(pFile);

    return lSize;
}

sprite_t *read_sprite(const char *const spritename)
{
    FILE *fp = fopen(spritename, "r");

    if (fp)
    {
        sprite_t *sp = malloc(filesize(fp));
        fread(sp, 1, filesize(fp), fp);
        fclose(fp);

        return sp;
    }
    else
    {
        return 0;
    }
}

void drawSelection(display_context_t disp, int p)
{
    int s_x = 23 + (text_offset * 8);

    if (scroll_behaviour)
    {
        if (select_mode)
        {
            if (cursor_lastline > cursor && cursor_line > 0)
            {
                cursor_line--;
            }

            if (cursor_lastline < cursor && cursor_line < 19)
            {
                cursor_line++;
            }

            p = cursor_line;
            graphics_draw_box_trans(disp, s_x, (((p + 3) * 8) + 24), 272, 8, selection_color); //(p+3) diff
            cursor_lastline = cursor;
        }
    }
    else
    { //new page-system
        //accept p
        graphics_draw_box_trans(disp, s_x, (((p + 3) * 8) + 24), 272, 8, selection_color);
    }
}

void drawConfigSelection(display_context_t disp, int l)
{
    int s_x = 62 + (text_offset * 8);

    l = l + 5;
    graphics_draw_box_trans(disp, s_x, (((l + 3) * 8) + 24), 193, 8, 0x00000080); //(p+3) diff
}

void drawToplistSelection(display_context_t disp, int l)
{
    int s_x = 30 + (text_offset * 8);

    l = l + 2;
    graphics_draw_box_trans(disp, s_x, (((l + 3) * 8) + 24), 256, 8, 0xFFFFFF70); //(p+3) diff
}

void chdir(const char *const dirent)
{
    /* Ghetto implementation */
    if (strcmp(dirent, "..") == 0)
    {
        /* Go up one */
        int len = strlen(dirz) - 1;

        /* Stop going past the min */
        if (dirz[len] == '/' && dirz[len - 1] == '/' && dirz[len - 2] == ':')
        {
            //return if ://
            return;
        }

        if (dirz[len] == '/')
        {
            dirz[len] = 0;
            len--;
        }

        while (dirz[len] != '/')
        {
            dirz[len] = 0;
            len--;
        }
    }
    else
    {
        /* Add to end */
        strcat(dirz, dirent);
        strcat(dirz, "/");
    }
}

int compare(const void *a, const void *b)
{
    direntry_t *first = (direntry_t *)a;
    direntry_t *second = (direntry_t *)b;

    if (first->type == DT_DIR && second->type != DT_DIR)
    {
        /* First should be first */
        return -1;
    }

    if (first->type != DT_DIR && second->type == DT_DIR)
    {
        /* First should be second */
        return 1;
    }

    return strcmp(first->filename, second->filename);
}

int compare_int(const void *a, const void *b)
{
    const int *ia = (const int *)a; // casting pointer types
    const int *ib = (const int *)b;
    return *ia - *ib;
}

int compare_int_reverse(const void *a, const void *b)
{
    const int *ia = (const int *)a; // casting pointer types
    const int *ib = (const int *)b;
    return *ib - *ia;
}

void new_scroll_pos(int *cursor, int *page, int max, int count)
{
    /* Make sure windows too small can be calculated right */
    if (max > count)
    {
        max = count;
    }

    /* Bounds checking */
    if (*cursor >= count)
    {
        *cursor = count - 1;
    }

    if (*cursor < 0)
    {
        *cursor = 0;
    }

    /* Scrolled up? */
    if (*cursor < *page)
    {
        *page = *cursor;
        return;
    }

    /* Scrolled down/ */
    if (*cursor >= (*page + max))
    {
        *page = (*cursor - max) + 1;
        return;
    }

    /* Nothing here, should be good! */
}

void display_dir(direntry_t *list, int cursor, int page, int max, int count, display_context_t disp)
{
    //system color
    uint32_t forecolor = 0;
    uint32_t forecolor_menu = 0;
    uint32_t backcolor;
    backcolor = graphics_make_color(0x00, 0x00, 0x00, 0x00);      //bg
    forecolor_menu = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF); //fg

    graphics_set_color(list_font_color, backcolor);

    u8 c_pos[MAX_LIST + 1];
    int c_pos_counter = 0;

    c_pos[c_pos_counter++] = 0;

    u8 c_dirname[64];

    if (page_display)
    {

        u8 pi = page / 20;
        u8 ci = 0;

        if (count % 20 == 0)
            ci = (count - 1) / 20;
        else
            ci = count / 20;
        sprintf(c_dirname, "%i/%i SD:/%s", pi + 1, ci + 1, pwd);
    }
    else
    {
        sprintf(c_dirname, "SD:/%s", pwd);
    }
    char sel_str[128];

    printText(c_dirname, 3, 4, disp);

    int firstrun = 1;
    /* Page bounds checking */
    if (max > count)
    { //count = directories starting at 1
        max = count;
    }

    /* Cursor bounds checking */
    if (cursor >= (page + max))
    {
        cursor = (page + max) - 1;
    }

    if (cursor < page)
    {
        cursor = page;
    }

    if (max == 0)
    {
        printText(dirempty, 3, 6, disp);
        sprintf(sel_str, "  ");
        empty = 1;
    }
    else
    {
        empty = 0;
    }

    //last page anti ghosting entries
    if (page == (count / 20) * 20)
        max = count % 20;

    for (int i = page; i < (page + max); i++)
    { //from page to page + max
        if (list[i].type == DT_DIR)
        {
            char tmpdir[(CONSOLE_WIDTH - 5) + 1];
            strncpy(tmpdir, list[i].filename, CONSOLE_WIDTH - 5);

            tmpdir[CONSOLE_WIDTH - 5] = 0;

            char *dir_str;
            dir_str = malloc(slen(tmpdir) + 3);

            if (i == cursor)
            {
                sprintf(dir_str, " [%s]", tmpdir);
                sprintf(sel_str, " [%s]", tmpdir);

                if (scroll_behaviour)
                    drawSelection(disp, i);

                c_pos[c_pos_counter++] = 1;
            }
            else
            {
                sprintf(dir_str, "[%s]", tmpdir);
                c_pos[c_pos_counter++] = 0;
            }
            graphics_set_color(list_dir_font_color, backcolor);
            if (firstrun)
            {
                printText(dir_str, 3, 6, disp);
                firstrun = 0;
            }
            else
            {
                printText(dir_str, 3, -1, disp);
            }
            free(dir_str);
        }
        else
        { //if(list[i].type == DT_REG)
            int fcolor = list[i].color;

            if (fcolor != 0)
            {
                switch (fcolor)
                {
                case 1:
                    forecolor = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF); //common (white)
                    break;
                case 2:
                    forecolor = graphics_make_color(0x00, 0xFF, 0x00, 0xCF); //uncommon (green)
                    break;
                case 3:
                    forecolor = graphics_make_color(0x1E, 0x90, 0xFF, 0xFF); //rare (blue)
                    break;
                case 4:
                    forecolor = graphics_make_color(0x9B, 0x30, 0xFF, 0xFF); //epic (purple)
                    break;
                case 5:
                    forecolor = graphics_make_color(0xFF, 0xA5, 0x00, 0xFF); //legendary (orange)
                    break;
                default:
                    break;
                }
            }
            else
                forecolor = list_font_color;

            char tmpdir[(CONSOLE_WIDTH - 3) + 1];
            strncpy(tmpdir, list[i].filename, CONSOLE_WIDTH - 3);
            tmpdir[CONSOLE_WIDTH - 3] = 0;

            char *dir_str;
            dir_str = malloc(slen(tmpdir) + 1);

            if (i == cursor)
            {
                sprintf(dir_str, " %s", tmpdir);
                sprintf(sel_str, " %s", tmpdir);

                if (scroll_behaviour)
                    drawSelection(disp, i);

                c_pos[c_pos_counter++] = 1;
            }
            else
            {
                sprintf(dir_str, "%s", tmpdir);
                c_pos[c_pos_counter++] = 0;
            }

            graphics_set_color(list_font_color, backcolor);

            if (firstrun)
            {
                if (enable_colored_list)
                    graphics_set_color(forecolor, backcolor);

                printText(dir_str, 3, 6, disp); //3,6
                firstrun = 0;
            }
            else
            {
                if (enable_colored_list)
                    graphics_set_color(forecolor, backcolor);

                printText(dir_str, 3, -1, disp); //3,1
            }
            free(dir_str);
        }
    }

    //for page-wise scrolling
    if (scroll_behaviour == 0)
    {
        int c = 0;
        for (c = 0; c < max + 1; c++)
        {
            if (c_pos[c] == 1)
            {
                drawSelection(disp, c - 1);
                //todo: set selection color
                graphics_set_color(selection_font_color, backcolor);
                printText(sel_str, 3, 6 + c - 1, disp);
                graphics_set_color(forecolor, backcolor);
            }
        }
    }
    graphics_set_color(forecolor_menu, backcolor);
}

//background sprite
void drawBg(display_context_t disp)
{
    graphics_draw_sprite(disp, 0, 0, background);
}

void drawBox(short x, short y, short width, short height, display_context_t disp)
{
    x = x + (text_offset * 8);

    graphics_draw_line(disp, x, y, width + x, y, border_color_1);                   //A top left tp bottom right ok
    graphics_draw_line(disp, width + x, y, width + x, height + y, border_color_1);  //B top right to bottom right
    graphics_draw_line(disp, x, y, x, height + y, border_color_2);                  //C  //152-20
    graphics_draw_line(disp, x, height + y, width + x, height + y, border_color_2); //D
    graphics_draw_box_trans(disp, x + 1, y + 1, width - 1, height - 1, box_color);  //red light transparent
}

void drawBoxNumber(display_context_t disp, int box)
{
    int old_color = box_color;
    //backup color

    switch (box)
    {
    case 1:
        drawBox(20, 24, 277, 193, disp);
        break; //filebrowser
    case 2:
        drawBox(60, 56, 200, 128, disp);
        break; //info screen
    case 3:
        box_color = graphics_make_color(0x00, 0x00, 0x60, 0xC9);
        drawBox(79, 29, 161, 180, disp);
        break; //cover
    case 4:
        box_color = graphics_make_color(0x30, 0x00, 0x00, 0xA3);
        drawBox(79, 29, 161, 180, disp);
        break; //mempak content
    case 5:
        box_color = graphics_make_color(0x60, 0x00, 0x00, 0xD3);
        drawBox(60, 64, 197, 114, disp); //red confirm screen
        break;                           //mempak content
    case 6:
        box_color = graphics_make_color(0x60, 0x60, 0x00, 0xC3);
        drawBox(60, 64, 197, 125, disp); //yellow screen
        break;                           //rom config box
    case 7:
        box_color = graphics_make_color(0x00, 0x00, 0x60, 0xC3);
        drawBox(60, 105, 197, 20, disp); //blue info screen
        break;                           //info screen
    case 8:
        box_color = graphics_make_color(0x60, 0x00, 0x00, 0xD3);
        drawBox(60, 105, 197, 20, disp); //red error screen
        break;                           //info screen
    case 9:
        box_color = graphics_make_color(0x00, 0x00, 0x00, 0xB6);
        drawBox(28, 49, 260, 150, disp);
        break; //yellow toplist
    case 10:
        box_color = graphics_make_color(0x00, 0x60, 0x00, 0xC3);
        drawBox(60, 105, 197, 20, disp); //green info screen
        break;                           //info screen
    case 11:
        box_color = graphics_make_color(0x00, 0x60, 0x00, 0xC3);
        drawBox(28, 105, 260, 30, disp);
        break; //green full filename
    case 12:
        box_color = graphics_make_color(0x00, 0x60, 0x00, 0xC3);
        drawBox(20, 24, 277, 193, disp);
        break; //filebrowser
    case 13:
        box_color = graphics_make_color(0x60, 0x00, 0x00, 0xD3);
        drawBox(28, 105, 260, 40, disp);
        break; //delete
    default:
        break;
    }

    //restore color
    box_color = old_color;
}

//is setting the config file vars into the pconfig-structure
static int configHandler(void *user, const char *section, const char *name, const char *value)
{
    configuration *pconfig = (configuration *)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("ed64", "border_color_1"))
    {
        pconfig->border_color_1 = strdup(value);
    }
    else if (MATCH("ed64", "border_color_2"))
    {
        pconfig->border_color_2 = strdup(value);
    }
    else if (MATCH("ed64", "box_color"))
    {
        pconfig->box_color = strdup(value);
    }
    else if (MATCH("ed64", "selection_color"))
    {
        pconfig->selection_color = strdup(value);
    }
    else if (MATCH("ed64", "list_font_color"))
    {
        pconfig->list_font_color = strdup(value);
    }
    else if (MATCH("ed64", "list_dir_font_color"))
    {
        pconfig->list_dir_font_color = strdup(value);
    }
    else if (MATCH("ed64", "selection_font_color"))
    {
        pconfig->selection_font_color = strdup(value);
    }
    else if (MATCH("ed64", "mempak_path"))
    {
        pconfig->mempak_path = strdup(value);
    }
    else if (MATCH("ed64", "save_path"))
    {
        pconfig->save_path = strdup(value);
    }
    else if (MATCH("ed64", "sound_on"))
    {
        pconfig->sound_on = atoi(value);
    }
    else if (MATCH("ed64", "bgm_on"))
    {
        pconfig->bgm_on = atoi(value);
    }
    else if (MATCH("ed64", "page_display"))
    {
        pconfig->page_display = atoi(value);
    }
    else if (MATCH("ed64", "tv_mode"))
    {
        pconfig->tv_mode = atoi(value);
    }
    else if (MATCH("ed64", "quick_boot"))
    {
        pconfig->quick_boot = atoi(value);
    }
    else if (MATCH("ed64", "enable_colored_list"))
    {
        pconfig->enable_colored_list = atoi(value);
    }
    else if (MATCH("ed64", "ext_type"))
    {
        pconfig->ext_type = atoi(value);
    }
    else if (MATCH("ed64", "cd_behaviour"))
    {
        pconfig->cd_behaviour = atoi(value);
    }
    else if (MATCH("ed64", "scroll_behaviour"))
    {
        pconfig->scroll_behaviour = atoi(value);
    }
    else if (MATCH("ed64", "text_offset"))
    {
        pconfig->text_offset = atoi(value);
    }
    else if (MATCH("ed64", "sd_speed"))
    {
        pconfig->sd_speed = atoi(value);
    }
    else if (MATCH("ed64", "hide_sysfolder"))
    {
        pconfig->hide_sysfolder = atoi(value);
    }
    else if (MATCH("ed64", "background_image"))
    {
        pconfig->background_image = strdup(value);
    }
    else if (MATCH("ed64", "language"))
    {
        pconfig->language = atoi(value);
    }
    else if(MATCH("ed64", "save_backup"))
    {
        pconfig->save_backup = atoi(value);
    }
    else if (MATCH("user", "name"))
    {
        pconfig->name = strdup(value);
    }
    else
    {
        return 0; /* unknown section/name, error */
    }

    return 1;
}

void updateFirmware(char *filename)
{ //check that firmware exists on the disk? mainly because it has to be ripped from the official image and may not have been.
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(filename, &fno); //TODO: given this is on the ROM (not SD) does it even work??????

    if (fr == FR_OK)
    {
        int fpf = dfs_open(filename);
        firmware = malloc(dfs_size(fpf));
        dfs_read(firmware, 1, dfs_size(fpf), fpf);
        dfs_close(fpf);
        bi_load_firmware(firmware);
    }
}

//everdrive init functions
void configure()
{
    u16 msg = 0;
    u8 buff[16];
    u16 firm;

    evd_init();
    //REG_MAX_MSG
    evd_setCfgBit(ED_CFG_SDRAM_ON, 0);
    dma_read_s(buff, ROM_ADDR + 0x20, 16);
    asm_date = memRomRead32(0x38); //TODO: this should be displayed somewhere...
    evd_setCfgBit(ED_CFG_SDRAM_ON, 1);

    firm = evd_getFirmVersion();

    if (firm >= 0x0200)
    {
        sleep(1);
        evd_setCfgBit(ED_CFG_SDRAM_ON, 0);
        sleep(1);

        msg = evd_readReg(REG_MAX_MSG);

        if (!(msg & (1 << 14)))
        {
            msg |= 1 << 14;
            evd_writeReg(REG_MAX_MSG, msg);

            switch(firm) //need to take into account different default firmware versions for each ED64 version
            {
                case 0x0214:
                updateFirmware("/firmware/firmware_v2.bin");
                break;
                case 0x0250:
                updateFirmware("/firmware/firmware_v2_5.bin");
                break;
                case 0x0300:
                updateFirmware("/firmware/firmware_v3.bin");
                break;
                default:
                break;
            }

            sleep(1);
            evd_init();
        }
        sleep(1);

        evd_setCfgBit(ED_CFG_SDRAM_ON, 1);
    }

    if (streql("ED64 SD boot", buff, 12) && firm >= 0x0116) //TODO: can this be moved before the firmware is loaded?
    {
        sdSetInterface(DISK_IFACE_SD);
    }
    else
    {
        sdSetInterface(DISK_IFACE_SPI);
    }
    memSpiSetDma(0);
}

//rewrites the background and the box to clear the screen
void clearScreen(display_context_t disp)
{
    drawBg(disp);           //background
    drawBoxNumber(disp, 1); //filebrowser box
}

void romInfoScreen(display_context_t disp, u8 *buff, int silent)
{
    TCHAR filename[MAX_SUPPORTED_PATH_LEN];
    sprintf(filename, "%s", buff);

    int swapped = 0;

    FRESULT result;

    int fsize = 4096;                 //rom-headersize 4096 but the bootcode is not needed
    unsigned char headerdata[fsize]; //1*512

    FIL file;
    UINT bytesread;
    result = f_open(&file, filename, FA_READ);

    if (result == FR_OK)
    {
        int fsizeMB = f_size(&file) / 1048576; //Bytes in a MB

        result =
        f_read (
            &file,        /* [IN] File object */
            headerdata,  /* [OUT] Buffer to store read data */
            fsize,        /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);

        int sw_type = is_valid_rom(headerdata);

        if (sw_type != 0)
        {
            swapped = 1;
            swap_header(headerdata, 512);
        }

        if (silent != 1)
        {
            //char 32-51 name
            unsigned char rom_name[32];

            for (int u = 0; u < 19; u++)
            {
                if (u != 0)
                    sprintf(rom_name, "%s%c", rom_name, headerdata[32 + u]);
                else
                    sprintf(rom_name, "%c", headerdata[32 + u]);
            }

            //rom name
            sprintf(rom_name, "%s", trim(rom_name));
            printText(rom_name, 11, 19, disp);

            //rom size
            sprintf(rom_name, sizeMB, fsizeMB);
            printText(rom_name, 11, -1, disp);


            //unique cart id for gametype
            unsigned char cartID_str[12];
            sprintf(cartID_str, "ID: %c%c%c%c", headerdata[0x3B], headerdata[0x3C], headerdata[0x3D], headerdata[0x3E]);
            printText(cartID_str, 11, -1, disp);

            get_country_and_region(headerdata[0x3E], rom_name);
            printText(rom_name, 11, -1, disp);
        }

        int cic, save;

        cic = get_cic(&headerdata[0x40]);

        unsigned char cartID_short[4];
        sprintf(cartID_short, "%c%c\0", headerdata[0x3C], headerdata[0x3D]);

        if (get_cic_save(cartID_short, &cic, &save))
        {
            if (silent != 1)
            {
                printText(fnddb, 11, -1, disp);
                unsigned char save_type_str[12];
                sprintf(save_type_str, savetypeinf, saveTypeToExtension(save, ext_type));
                printText(save_type_str, 11, -1, disp);

                unsigned char cic_type_str[12];

                switch (cic)
                {
                    case 4:
                    sprintf(cic_type_str, "CIC: CIC-5101", cic);
                    break;
                    case 7:
                    sprintf(cic_type_str, "CIC: CIC-5167", cic);
                    break;
                    default:
                    sprintf(cic_type_str, "CIC: CIC-610%i", cic);
                    break;
                }

                printText(cic_type_str, 11, -1, disp);
            }
            //thanks for the db :>
            //cart was found, use CIC and SaveRAM type
        }

        if (silent != 1)
        {
            char box_path[32];

            sprite_t *n64cover;

            sprintf(box_path, "/"ED64_FIRMWARE_PATH"/boxart/lowres/%c%c.png", headerdata[0x3C], headerdata[0x3D]);

            FILINFO fnoba;
            result = f_stat (box_path, &fnoba);

            if (result != FR_OK)
            {
                //not found
                sprintf(box_path, "/"ED64_FIRMWARE_PATH"/boxart/lowres/00.png");
            }

            n64cover = loadPng(box_path);
            graphics_draw_sprite(disp, 81, 32, n64cover);
            display_show(disp);
        }
        else
        {
            rom_config[1] = cic - 1;
            rom_config[2] = save;
            rom_config[3] = 0; //tv force off
            rom_config[4] = 0; //cheat off
            rom_config[5] = 0; //chk_sum off
            rom_config[6] = 0; //rating
            rom_config[7] = 0; //country
            rom_config[8] = 0; //reserved
            rom_config[9] = 0; //reserved
        }
    }
}

sprite_t *loadPng(u8 *png_filename)
{
    TCHAR *filename = (TCHAR *)malloc(slen(png_filename));
    sprintf(filename, "%s", png_filename);

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, filename, FA_READ);

    if (result == FR_OK)
    {
        int fsize = f_size(&file);
        u8 png_rawdata[fsize];

        result =
        f_read (
            &file,        /* [IN] File object */
            png_rawdata,  /* [OUT] Buffer to store read data */
            fsize,        /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);

        free(filename);
        return loadImage32(png_rawdata, fsize);
    }

    return 0;


}

void loadgenericrom(display_context_t disp, TCHAR *rom_path, u8 *extension)
{
    printText(loadgb, 3, 4, disp);
    FRESULT result;
    FIL emufile;
    UINT emubytesread;
    TCHAR tmp[MAX_SUPPORTED_PATH_LEN];
    // everdrive forces .v64, byte order doesnt actually matter
    snprintf(tmp, sizeof(tmp), "/"ED64_FIRMWARE_PATH"/%s.v64", extension);
    result = f_open(&emufile, tmp, FA_READ);

    if (result == FR_OK)
    {
        int emufsize = f_size(&emufile);
        //load the emulator
        result =
        f_read (
            &emufile,        /* [IN] File object */
            (void *)0xb0000000,  /* [OUT] Buffer to store read data for the emulator */
            emufsize,         /* [IN] Number of bytes to read */
            &emubytesread    /* [OUT] Number of bytes read */
        );

        f_close(&emufile);

        //load the rom
        FIL romfile;
        UINT rombytesread;
        result = f_open(&romfile, rom_path, FA_READ);

        if (result == FR_OK)
        {
            int romfsize = f_size(&romfile);

            result =
            f_read (
                &romfile,        /* [IN] File object */
                (void *)emu_offset,  /* [OUT] Buffer to store read data for the rom */
                romfsize,         /* [IN] Number of bytes to read */
                &rombytesread    /* [OUT] Number of bytes read */
            );

            f_close(&romfile);

            boot_cic = rom_config[1] + 1;
            boot_save = rom_config[2];
            force_tv = rom_config[3];
            cheats_on = rom_config[4];
            checksum_fix_on = rom_config[5];
            boot_country = rom_config[7]; //boot_block

            bootRom(disp, 1);
        }
        
    }
    else
    {
        drawShortInfoBox(disp, emunofound, 1);
    }
}

//load a z64/v64/n64 rom to the sdram
void loadrom(display_context_t disp, u8 *buff, int fast)
{
    clearScreen(disp);
    display_show(disp);
    printText(loadgb, 3, 4, disp);

    TCHAR filename[MAX_SUPPORTED_PATH_LEN];
    sprintf(filename, "%s", buff);

    FRESULT result;
    FIL file;
    UINT bytesread = 0;
    result = f_open(&file, filename, FA_READ);

    if (result == FR_OK)
    {
        int swapped = 0;
        int headerfsize = 4096; //rom-headersize 4096 but the bootcode is not needed
        unsigned char headerdata[headerfsize]; 
        int fsize = f_size(&file);
        int fsizeMB = fsize /1048576; //Bytes in a MB

        result =
        f_read (
            &file,        /* [IN] File object */
            headerdata,  /* [OUT] Buffer to store read data */
            headerfsize,        /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);

        int sw_type = is_valid_rom(headerdata);

        if (sw_type != 0)
        {
            swapped = 1;
            swap_header(headerdata, 512);
        }

        if (fast != 1)
        {
            //char 32-51 name
            unsigned char rom_name[32];

            for (int u = 0; u < 19; u++)
            {
                if (u != 0)
                    sprintf(rom_name, "%s%c", rom_name, headerdata[32 + u]);
                else
                    sprintf(rom_name, "%c", headerdata[32 + u]);
            }

            //rom name
            sprintf(rom_name, "%s", trim(rom_name));
            printText(rom_name, 3, -1, disp);

            //rom size
            sprintf(rom_name, sizeMB, fsizeMB);
            printText(rom_name, 3, -1, disp);


            //unique cart id for gametype
            unsigned char cartID_str[12];
            sprintf(cartID_str, "ID: %c%c%c%c", headerdata[0x3B], headerdata[0x3C], headerdata[0x3D], headerdata[0x3E]);
            printText(cartID_str, 3, -1, disp);
        }

        int cic, save;

        cic = get_cic(&headerdata[0x40]);

        unsigned char cartID_short[4];
        sprintf(cartID_short, "%c%c\0", headerdata[0x3C], headerdata[0x3D]);

        if (get_cic_save(cartID_short, &cic, &save))
        {
            if (fast != 1)
            {
                printText(fnddb, 3, -1, disp);
                unsigned char save_type_str[12];
                sprintf(save_type_str, savetypeinf, saveTypeToExtension(save, ext_type));
                printText(save_type_str, 3, -1, disp);

                unsigned char cic_type_str[12];

                switch (cic)
                {
                    case 4:
                    sprintf(cic_type_str, "CIC: CIC-5101", cic);
                    break;
                    case 7:
                    sprintf(cic_type_str, "CIC: CIC-5167", cic);
                    break;
                    default:
                    sprintf(cic_type_str, "CIC: CIC-610%i", cic);
                    break;
                }

                printText(cic_type_str, 3, -1, disp);
            }
            //thanks for the db :>
            //cart was found, use CIC and SaveRAM type
        }

        //new rom_config
        boot_cic = rom_config[1] + 1;
        boot_save = rom_config[2];
        force_tv = rom_config[3];
        cheats_on = rom_config[4];
        checksum_fix_on = rom_config[5];
        boot_country = rom_config[7]; //boot_block

        if (gbload == 1)
            boot_save = 1;

        if (swapped == 1)
        {
            while (evd_isDmaBusy())
                ;
            evd_mmcSetDmaSwap(1);

            TRACE(disp, "swapping on");
        }

        bytesread = 0;
        result = f_open(&file, filename, FA_READ);
        if (fsizeMB <= 32)
        {
            result =
            f_read (
                &file,        /* [IN] File object */
                (void *)0xb0000000,  /* [OUT] Buffer to store read data */
                fsize,        /* [IN] Number of bytes to read */
                &bytesread    /* [OUT] Number of bytes read */
            );
        }
        else
        {
            result =
            f_read (
                &file,        /* [IN] File object */
                (void *)0xb0000000,  /* [OUT] Buffer to store read data */
                32 * 1048576,        /* [IN] Number of bytes to read */
                &bytesread    /* [OUT] Number of bytes read */
            );
            if(result == FR_OK)
            {
                result =
                f_read (
                    &file,        /* [IN] File object */
                    (void *)0xb2000000,  /* [OUT] Buffer to store read data */
                    fsize - bytesread,        /* [IN] Number of bytes to read */
                    &bytesread    /* [OUT] Number of bytes read */
                );
            }
        }

        if(result == FR_OK)
        {
            printText(romloaded, 3, -1, disp);

            if (!fast)
            {
                printText(" ", 3, -1, disp);
                printText(cupactcheat, 3, -1, disp);
                printText(crightv, 3, -1, disp);
                printText(donepresstart, 3, -1, disp);
            }
            else
            {
                bootRom(disp, 1);
            }
        }
        else
        {
            printText(filopenerror, 3, -1, disp);
        }
    }
}

int backupSaveData(display_context_t disp)
{
    //backup cart-save on sd after reboot
    TCHAR config_file_path[MAX_SUPPORTED_PATH_LEN];
    sprintf(config_file_path, "/"ED64_FIRMWARE_PATH"/%s/LASTROM.CFG", save_path);

    u8 save_format;
    u8 cfg_data[2]; //TODO: this should be a strut?


    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, config_file_path, FA_READ);

    if (result == FR_OK)
    {
        printText(updatelastgamerecord, 3, 4, disp);

        int fsize = f_size(&file);


        result =
        f_read (
            &file,        /* [IN] File object */
            &cfg_data,  /* [OUT] Buffer to store read data */
            2,         /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        //split in save type and cart-id
        save_format = cfg_data[0];

        f_gets(rom_filename, 256, &file);
        f_close(&file);

        //set savetype to 0 disable for next boot
        if (save_format != 0)
        {
            result = f_open(&file, config_file_path, FA_WRITE | FA_OPEN_EXISTING);
            //set savetype to off
            cfg_data[0] = 0;

            UINT bw;
            result = f_write (
                &file,          /* [IN] Pointer to the file object structure */
                &cfg_data, /* [IN] Pointer to the data to be written */
                1,         /* [IN] Number of bytes to write */
                &bw          /* [OUT] Pointer to the variable to return number of bytes written */
              );

            f_close(&file);

            TRACE(disp, "Disabling save for subsequent system reboots");

            volatile u8 save_config_state = 0;
            int cfgreg = evd_readReg(REG_CFG);
            save_config_state = evd_readReg(REG_SAV_CFG);

            if (save_config_state != 0 || evd_getFirmVersion() >= 0x0300)
            { //save register set or the firmware is V3
                if (save_config_state == 0)
                {                                 //we are V3 and have had a hard reboot
                    evd_writeReg(REG_SAV_CFG, 1); //so we need to tell the save register it still has data.
                }
                save_after_reboot = 1;
            }
            else
            {
                TRACE(disp, "Save not required.");
                printText(readystr, 3, -1, disp);
                display_show(disp);
                return 1;
            }
        }
    }
    else
    {
        TRACE(disp, "No previous ROM loaded!");
        printText(readystr, 3, -1, disp);
        display_show(disp);
        return 0;
    }

    //reset with save request
    if (save_after_reboot)
    {
        printText(cpyingram2SD, 3, -1, disp);
        if (saveTypeToSd(disp, rom_filename, save_format))
        {
            printText(OpComsucc, 3, -1, disp);
        }
        else
        {
            TRACE(disp, "ERROR: the RAM was not successfully saved!");
        }
    }
    else
    {
        TRACE(disp, "no reset - save request");
        printText(done3p, 3, -1, disp);
    }
    display_show(disp);
    return 1;
}

//before boot_simulation
//write a cart-save from a file to the fpga/cart
int saveTypeFromSd(display_context_t disp, char *rom_name, int stype)
{
    TRACE(disp, rom_filename);

    const char* save_type_extension = saveTypeToExtension(stype, ext_type);
    TCHAR fname[256] = {0};
    sprintf(fname, "/"ED64_FIRMWARE_PATH"/%s/%s.%s", save_path, rom_name, save_type_extension);

    TCHAR fname1[50] = {0};
    sprintf(fname1, "/"ED64_FIRMWARE_PATH"/%s/", save_path);
    printText(fname1, 3, -1, disp);
    TCHAR fname2[50] = {0};
    sprintf(fname2, "%s.%s", rom_name, save_type_extension);
    printText(fname2, 3, -1, disp);

    int size = saveTypeToSize(stype); // int byte
    uint8_t cartsave_data[size];

    FIL file;
    FRESULT result;
    UINT bytesread;
    result = f_open(&file, fname, FA_READ);

    if (result == FR_OK)
    {
        int fsize = f_size(&file);

        result =
        f_read (
            &file,          /* [IN] File object */
            cartsave_data, /* [OUT] Buffer to store read data */
            size,           /* [IN] Number of bytes to read */
            &bytesread      /* [OUT] Number of bytes read */
        );

        f_close(&file);
    }
    else
    {
        switch(result)
        {
         case FR_NOT_READY:
        printText(errornoready, 3, -1, disp);
        break;
        case FR_NO_FILE:
        printText(filenoexist, 3, -1, disp);
        break;
        case FR_NO_PATH:
        printText(pathnoexist, 3, -1, disp);
        break;
        case FR_INVALID_NAME:
        printText(invalidname, 3, -1, disp);
        break;
        case FR_DENIED:
        printText(operdenied, 3, -1, disp);
        break;
        case FR_EXIST:
        printText(filealrrexist, 3, -1, disp);
        break;
        case FR_TIMEOUT:
        printText(errtimeout, 3, -1, disp);
        break;
        case FR_LOCKED:
        printText(devicelocked, 3, -1, disp);
        break;
        default:
        break;
        }
        printText(nosavfund, 3, -1, disp);
        //todo: clear memory area

        return 0;
    }

    if (pushSaveToCart(stype, cartsave_data))
    {
        printText("transferred save data...", 3, -1, disp);
    }
    else
    {
        printText("error transfering save data", 3, -1, disp);
    }

    return 1;
}

int saveTypeToSd(display_context_t disp, char *rom_name, int stype)
{
    //after reset create new savefile
    const char* save_type_extension = saveTypeToExtension(stype, ext_type);
    TCHAR fname[MAX_SUPPORTED_PATH_LEN];

    if (save_backup == 1)
    {
        int save_count = 0;
        int i;
        char currsave[256];
        char nextsave[256];
        FRESULT result;
        FILINFO fnoba;
        printText("Finding unused save slot...", 3, -1, disp);
        display_show(disp);
        //check for non-numbered save file
        sprintf(fname, "/"ED64_FIRMWARE_PATH"/%s/%s.%s", save_path, rom_name, save_type_extension);
        result = f_stat(fname, &fnoba);
        if (result != FR_OK)
        {
            //no saves found, create the first one
            printText("No saves found. Creating save...", 3, -1, disp);
            display_show(disp);
            sprintf(fname, "/"ED64_FIRMWARE_PATH"/%s/%s.%s", save_path, rom_name, save_type_extension);
            save_count = 1;
        }
        if (save_count == 0)
        {
            //check how many save files there are
            for (i = 2; i < 5; i++)
            {
                sprintf(fname, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, i, save_type_extension);
                result = f_stat(fname, &fnoba);
                if(result != FR_OK)
                {
                    printText("Saves found. Backing up...", 3, -1, disp);
                    display_show(disp);
                    for (int j = i; j > 1; j--)
                    {
                        //todo: rename save j to save j+1
                        sprintf(currsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, j, save_type_extension);
                        sprintf(nextsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, j+1, save_type_extension);
                        f_rename(currsave, nextsave);
                    }
                    save_count = 2;
                    break;
                }
            }
        }
        if (save_count == 0)
        {
            // already 4 saves; delete save #4
            printText("4 saves found. Deleting save #4...", 3, -1, disp);
            display_show(disp);
            char latestsave[256];
            sprintf(latestsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, 4, save_type_extension);
            f_unlink(latestsave);
            printText("Renaming remaining saves...", 3, -1, disp);
            display_show(disp);
            for (i=2; i<4; i++)
            {
                //rename file[i] to file[i+1]
                sprintf(currsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, i, save_type_extension);
                sprintf(nextsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, i+1, save_type_extension);
                f_rename(currsave, nextsave);
            }
            save_count = 3;
        }

        if (save_count != 1) {
            //rename numberless file to save 2
            sprintf(currsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%s", save_path, rom_name, save_type_extension);
            sprintf(nextsave, "/"ED64_FIRMWARE_PATH"/%s/%s.%04x.%s", save_path, rom_name, 2, save_type_extension);
            f_rename(currsave, nextsave);
        }
        //below: save latest file as numberless
       
    }
    sprintf(fname, "/"ED64_FIRMWARE_PATH"/%s/%s.%s", save_path, rom_name, save_type_extension);

    int size = saveTypeToSize(stype); // int byte
    TRACEF(disp, "size for save=%i", size);

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, fname, FA_WRITE | FA_OPEN_ALWAYS); //Could use FA_CREATE_ALWAYS but this could lead to the posibility of the file being emptied

    if (result == FR_OK)
    {
        //for savegame
        uint8_t cartsave_data[size]; //TODO: bring back old initialisation if this doesn't work



        TRACEF(disp, "cartsave_data=%p", &cartsave_data);

        printText("Transfering save data...", 3, -1, disp);
        if (getSaveFromCart(stype, cartsave_data))
        {
            UINT bw;
            result = f_write (
            &file,          /* [IN] Pointer to the file object structure */
            cartsave_data, /* [IN] Pointer to the data to be written */
            size,         /* [IN] Number of bytes to write */
            &bw          /* [OUT] Pointer to the variable to return number of bytes written */
            );
            f_close(&file);

            if (bw == 0)
			{
				printText("File was blank.", 3, -1, disp);
				f_unlink(fname);
			}
			else
			{
				printText("RAM area copied to SD card.", 3, -1, disp);
			}
            return 1;
        }
        else
        {
            f_close(&file);
            f_unlink(fname);
            printText("Couldn't retrieve save from SRAM.", 3, -1, disp);
            return 0;
        }
    }
    else
    {
        TRACE(disp, "COULDNT CREATE FILE :-(");
        printText("Error saving game to SD, couldn't create file!", 3, -1, disp);
    }
}

//check out the userfriendly ini file for config-information
int readConfigFile(void)
{
    TCHAR filename[MAX_SUPPORTED_PATH_LEN];
    sprintf(filename, "/"ED64_FIRMWARE_PATH"/ALTRA64.INI");

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, filename, FA_READ);

    if (result == FR_OK)
    {
        int fsize = f_size(&file);

        char config_rawdata[fsize];

        result =
        f_read (
            &file,        /* [IN] File object */
            config_rawdata,  /* [OUT] Buffer to store read data */
            fsize,         /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);

        configuration config;

        if (ini_parse_str(config_rawdata, configHandler, &config) < 0)
        {
            return 0;
        }
        else
        {
            border_color_1_s = config.border_color_1;
            border_color_2_s = config.border_color_2;
            box_color_s = config.box_color;
            selection_color_s = config.selection_color;
            selection_font_color_s = config.selection_font_color;
            list_font_color_s = config.list_font_color;
            list_dir_font_color_s = config.list_dir_font_color;

            mempak_path = config.mempak_path;
            save_path = config.save_path;
            sound_on = config.sound_on;
            bgm_on = config.bgm_on;
            page_display = config.page_display;
            tv_mode = config.tv_mode;
            quick_boot = config.quick_boot;
            enable_colored_list = config.enable_colored_list;
            ext_type = config.ext_type;
            cd_behaviour = config.cd_behaviour;
            scroll_behaviour = config.scroll_behaviour;
            text_offset = config.text_offset;
            hide_sysfolder = config.hide_sysfolder;
            sd_speed = config.sd_speed;
            background_image = config.background_image;
            language = config.language;
            save_backup = config.save_backup;

            return 1;
        }
    }
}

int str2int(char data)
{
    data -= '0';
    if (data > 9)
        data -= 7;

    return data;
}

uint32_t translate_color(char *hexstring)
{
    int r = str2int(hexstring[0]) * 16 + str2int(hexstring[1]);
    int g = str2int(hexstring[2]) * 16 + str2int(hexstring[3]);
    int b = str2int(hexstring[4]) * 16 + str2int(hexstring[5]);
    int a = str2int(hexstring[6]) * 16 + str2int(hexstring[7]);

    return graphics_make_color(r, g, b, a);
}

//init fat filesystem after everdrive init and before sdcard access
void initFilesystem(void)
{
    evd_ulockRegs();
    sleep(10);

    fs = malloc(sizeof (FATFS));           /* Get work area for the volume */
    FRESULT result = f_mount(fs,"",1);
    if(result != FR_OK)
    {
        //printText("mount error", 11, -1, disp);
    }
    else
    {
        fat_initialized = 1;
    }
}

int showThisFolder(int hide_sysfolder, char* fname)
{
    if (hide_sysfolder && strcmp(fname, "ED64") == 0) return 0;
    if (hide_sysfolder && strcmp(fname, "ED64P") == 0) return 0;
    if (hide_sysfolder && strcmp(fname, "System Volume Information") == 0) return 0;
    return 1;
}

//prints the sdcard-filesystem content
void readSDcard(display_context_t disp, char *directory)
{ //TODO: readd coloured list? use a hash table...
    // FatRecord *frec;
    // u8 cresp = 0;

    // //load the directory-entry
    // cresp = fatLoadDirByName("/"ED64_FIRMWARE_PATH"/CFG");

    // int dsize = dir->size;
    // char colorlist[dsize][256];

    // if (enable_colored_list)
    // {

    //     for (int i = 0; i < dir->size; i++)
    //     {
    //         frec = dir->rec[i];
    //         u8 rom_cfg_file[MAX_SUPPORTED_PATH_LEN];

    //         //set rom_cfg
    //         sprintf(rom_cfg_file, "/"ED64_FIRMWARE_PATH"/CFG/%s", frec->name);

    //         static uint8_t cfg_file_data[512] = {0};

    //         FRESULT result;
    //         FIL file;
    //         UINT bytesread;
    //         result = f_open(&file, rom_cfg_file, FA_READ);

    //         if (result == FR_OK)
    //         {
    //             int fsize = f_size(&file);

    //             result =
    //             f_read (
    //                 &file,        /* [IN] File object */
    //                 &cfg_file_data,  /* [OUT] Buffer to store read data */
    //                 fsize,         /* [IN] Number of bytes to read */
    //                 &bytesread    /* [OUT] Number of bytes read */
    //             );

    //             f_close(&file);

    //             colorlist[i][0] = (char)cfg_file_data[5];     //row i column 0 = colour
    //             strcpy(colorlist[i] + 1, cfg_file_data + 32); //row i column 1+ = fullpath

    //         }
    //     }
    // }

    // u8 buff[32];

    // //some trash buffer
    // FatRecord *rec;
    // u8 resp = 0;

    count = 1;
    //dir_t buf;

    //clear screen and print the directory name
    clearScreen(disp);


    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;


    res = f_opendir(&dir, directory);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            if (showThisFolder(hide_sysfolder, fno.fname))
            {
                if (fno.fattrib & AM_DIR) {                    /* It is a directory */
                    list[count - 1].type = DT_DIR;
                } else {                                       /* It is a file. */
                    //printf("%s/%s\n", path, fno.fname);
                    list[count - 1].type = DT_REG;
                }
                strcpy(list[count - 1].filename, fno.fname);
                list[count - 1].color = 0;

                // if (enable_colored_list)
                // {
                //     for (int c = 0; c < dsize; c++)
                //     {

                //         u8 short_name[256];

                //         sprintf(short_name, "%s", colorlist[c] + 1);

                //         u8 *pch_s; // point-offset
                //         pch_s = strrchr(short_name, '/');

                //         if (strcmp(list[count - 1].filename, pch_s + 1) == 0)
                //         {

                //             list[count - 1].color = colorlist[c][0];
                //         }
                //     }
                //     //new color test end
                // }

                count++;
                list = realloc(list, sizeof(direntry_t) * count);
            }
        }
        f_closedir(&dir);
        count--;
    }
    else
    {
        char error_msg[32];
        sprintf(error_msg, "CHDIR ERROR: %i", res);
        printText(error_msg, 3, -1, disp);
        sleep(3000);
    }

    page = 0;
    cursor = 0;
    select_mode = 1;

    if (count > 0)
    {
        /* Should sort! */
        qsort(list, count, sizeof(direntry_t), compare);
    }

    //print directory
    display_dir(list, cursor, page, MAX_LIST, count, disp);
}

/*
 * Returns two cheat lists:
 * - One for the "at boot" cheats
 * - Another for the "in-game" cheats
 */
int readCheatFile(TCHAR *filename, u32 *cheat_lists[2])
{
    // YAML parser
    yaml_parser_t parser;
    yaml_event_t event;

    // State for YAML parser
    int is_code = 0;
    int code_on = 1;
    int done = 0;
    u32 *list1;
    u32 *list2;
    char *next;

    int repeater = 0;
    u32 address;
    u32 value;

    yaml_parser_initialize(&parser);


    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, filename, FA_READ);

    if (result == FR_OK)
    {
        int fsize = f_size(&file);

        char *cheatfile = malloc(fsize);
        if (!cheatfile)
        {
            return -2; // Out of memory
        }

        /*
         * Size of the cheat list can never be more than half the size of the YAML
         * Minimum YAML example:
         *   A:-80001234 FFFF
         * Which is exactly 16 bytes.
         * The cheat list in this case fits into exactly 8 bytes (2 words):
         *   0x80001234, 0x0000FFFF
         */
        list1 = calloc(1, fsize + 2 * sizeof(u32)); // Plus 2 words to be safe
        if (!list1)
        {
            // Free
            free(cheatfile);
            return -2; // Out of memory
        }
        list2 = &list1[fsize / sizeof(u32) / 2];
        cheat_lists[0] = list1;
        cheat_lists[1] = list2;

        result =
        f_read (
            &file,        /* [IN] File object */
            cheatfile,  /* [OUT] Buffer to store read data */
            fsize,         /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);

        yaml_parser_set_input_string(&parser, cheatfile, strlen(cheatfile));

            do
            {
                if (!yaml_parser_parse(&parser, &event))
                {
                    // Free
                    yaml_parser_delete(&parser);
                    yaml_event_delete(&event);
                    free(cheatfile);
                    free(cheat_lists[0]);
                    cheat_lists[0] = 0;
                    cheat_lists[1] = 0;

                    return -3; // Parse error
                }

                // Process YAML
                switch (event.type)
                {
                case YAML_MAPPING_START_EVENT:
                    // Begin code block
                    is_code = 0;
                    break;

                case YAML_SEQUENCE_START_EVENT:
                    // Begin code lines
                    is_code = 1;
                    code_on = (event.data.sequence_start.tag ? !!strcasecmp(event.data.sequence_start.tag, "!off") : 1);
                    break;

                case YAML_SEQUENCE_END_EVENT:
                    // End code lines
                    is_code = 0;
                    code_on = 1;
                    repeater = 0;
                    break;

                case YAML_SCALAR_EVENT:
                    // Code line
                    if (!is_code || !code_on)
                    {
                        break;
                    }

                    address = strtoul(event.data.scalar.value, &next, 16);
                    value = strtoul(next, NULL, 16);

                    // Do not check code types within "repeater data"
                    if (repeater)
                    {
                        repeater--;
                        *list2++ = address;
                        *list2++ = value;
                        break;
                    }

                    // Determine destination cheat_list for the code type
                    switch (address >> 24)
                    {

                    // Uncessary code types
                    case 0x20: // Clear code list
                    case 0xCC: // Exception Handler Selection
                    case 0xDE: // Entry Point
                        break;

                    // Boot-time cheats
                    case 0xEE: // Disable Expansion Pak
                    case 0xF0: // 8-bit Boot-Time Write
                    case 0xF1: // 16-bit Boot-Time Write
                    case 0xFF: // Cheat Engine Location
                        *list1++ = address;
                        *list1++ = value;
                        break;

                    // In-game cheats
                    case 0x50: // Repeater/Patch
                        // Validate repeater count
                        if (address & 0x0000FF00)
                        {
                            repeater = 1;
                            *list2++ = address;
                            *list2++ = value;
                        }
                        break;

                    // Everything else
                    default:
                        if (!address)
                        {
                            // TODO: Support special code types! :)
                        }
                    // Fall-through!

                    case 0xD0: // 8-bit Equal-To Conditional
                    case 0xD1: // 16-bit Equal-To Conditional
                    case 0xD2: // 8-bit Not-Equal-To Conditional
                    case 0xD3: // 16-bit Not-Equal-To Conditional
                        // Validate 16-bit codes
                        if ((address & 0x01000001) == 0x01000001)
                        {
                            break;
                        }

                        *list2++ = address;
                        *list2++ = value;
                        break;
                    }
                    break;

                case YAML_STREAM_END_EVENT:
                    // And we're outta here!
                    done = 1;
                    break;

                default:
                    break;
                }

                yaml_event_delete(&event);
            } while (!done);

            // Free
            yaml_parser_delete(&parser);
            free(cheatfile);

            return repeater; // Ok or repeater error

    }
    else
    {
        return -1; //err file not found
    }
}

void bootRom(display_context_t disp, int silent)
{
    if (boot_cic != 0)
    {
        // first load cheats if enabled, and error out if we can't
        u32 *cheat_lists[2] = {NULL, NULL};
        if (cheats_on)
        {
            gCheats = 1;
            printText(trloadchfile, 3, -1, disp);

            char cheat_filename[64];
            sprintf(cheat_filename, "/"ED64_FIRMWARE_PATH"/CHEATS/%s.yml", rom_filename);

            int ok = readCheatFile(cheat_filename, cheat_lists);
            if (ok == 0)
            {
                printText(chsfund, 3, -1, disp);
                sleep(600);
            }
            else
            {
                printText(cheatnofound, 3, -1, disp);
                printText(orparsingfailed, 3, -1, disp);
                printText(resetconsol, 3, -1, disp);
                gCheats = 0;
                while(true) {
                    sleep(20000);
                }
            }
        }
        else
        {
            gCheats = 0;
        }

        if (boot_save != 0)
        {
            TCHAR cfg_file[MAX_SUPPORTED_PATH_LEN];

            //set cfg file with last loaded cart info and save-type
            sprintf(cfg_file, "/"ED64_FIRMWARE_PATH"/%s/LASTROM.CFG", save_path);

            FRESULT result;
            FIL file;
            result = f_open(&file, cfg_file, FA_WRITE | FA_CREATE_ALWAYS);

            if (result == FR_OK)
            {
                uint8_t cfg_data[2] = {boot_save, boot_cic};


                UINT bw;
                result = f_write (
                    &file,          /* [IN] Pointer to the file object structure */
                    &cfg_data, /* [IN] Pointer to the data to be written */
                    2,         /* [IN] Number of bytes to write */
                    &bw          /* [OUT] Pointer to the variable to return number of bytes written */
                  );

                f_puts(rom_filename, &file);

                f_close(&file);

                //set the fpga cart-save type
                evd_setSaveType(boot_save);

                saveTypeFromSd(disp, rom_filename, boot_save);
            }
        }

        TRACE(disp, "Cartridge-Savetype set");
        TRACE(disp, "information stored for reboot-save...");

        u32 cart, country;
        u32 info = *(vu32 *)0xB000003C;
        cart = info >> 16;
        country = (info >> 8) & 0xFF;

        disable_interrupts();
        int bios_cic = getCicType(1);

        if (checksum_fix_on)
        {
            checksum_sdram();
        }

        evd_lockRegs();
        sleep(10);

        while (!(disp = display_lock()))
            ;
        //blank screen to avoid glitches

        graphics_fill_screen(disp, 0x000000FF);
        display_show(disp);

        f_mount(0, "", 0);                     /* Unmount the default drive */
        free(fs);                              /* Here the work area can be discarded */
        simulate_boot(boot_cic, bios_cic, cheat_lists); // boot_cic
    }
}


void playSound(int snd)
{
    //no thread support in libdragon yet, sounds pause the menu for a time :/

    if (snd == 1)
        sndPlaySFX("rom://sounds/boot.wav");

    if (snd == 2)
        sndPlaySFX("rom://sounds/bamboo.wav");

    if (snd == 3)
        sndPlaySFX("rom://sounds/warning.wav");

    if (snd == 4)
        sndPlaySFX("rom://sounds/done.wav");

    if (snd == 5)
        sndPlaySFX("rom://sounds/boot.wav");
    
    if (snd == 6)
        sndPlaySFX("rom://sounds/doned.wav");

}

//draws the next char at the text input screen
void drawInputAdd(display_context_t disp, char *msg)
{
    graphics_draw_box_trans(disp, 23, 5, 272, 18, 0x00000090);
    position++;
    sprintf(input_text, "%s%s", input_text, msg);
    drawTextInput(disp, input_text);
}

//del the last char at the text input screen
void drawInputDel(display_context_t disp)
{
    graphics_draw_box_trans(disp, 23, 5, 272, 18, 0x00000090);
    if (position)
    {
        input_text[position - 1] = '\0';
        drawTextInput(disp, input_text);

        position--;
    }
}

void drawTextInput(display_context_t disp, char *msg)
{
    graphics_draw_text(disp, 40, 15, msg);
}

void drawConfirmBox(display_context_t disp){
    while (!(disp = display_lock()))
                ;
    new_scroll_pos(&cursor, &page, MAX_LIST, count);
    clearScreen(disp); //part clear?
    display_dir(list, cursor, page, MAX_LIST, count, disp);
    drawBoxNumber(disp, 5);
    display_show(disp);

    if (sound_on)
        playSound(3);

    printText(" ", 9, 9, disp);
    printText(confreq, 9, -1, disp);
    printText(" ", 9, -1, disp);
    printText(" ", 9, -1, disp);
    printText(aresure, 9, -1, disp);
    printText(" ", 9, -1, disp);
    printText(cupcont, 9, -1, disp); //set mapping 3
    printText(" ", 9, -1, disp);
    printText(cancelmenu, 9, -1, disp);

    sleep(300);
}

void drawShortInfoBox(display_context_t disp, char *text, u8 mode)
{    while (!(disp = display_lock()))
                ;
    new_scroll_pos(&cursor, &page, MAX_LIST, count);
    clearScreen(disp); //part clear?
    display_dir(list, cursor, page, MAX_LIST, count, disp);

    if (mode == 0)

        drawBoxNumber(disp, 7);
    else if (mode == 1)
        drawBoxNumber(disp, 8);
    else if (mode == 2)
        drawBoxNumber(disp, 10);

    printText(text, 9, 14, disp);

    display_show(disp);

    if (sound_on)
    {
        if (mode == 0)
            playSound(6);
        else if (mode == 1)
            playSound(3);
        else if (mode == 2)
            playSound(4);
    }
    sleep(300);
}

void readRomConfig(display_context_t disp, char *short_filename, char *full_filename)
{
    TCHAR cfg_filename[MAX_SUPPORTED_PATH_LEN];
    sprintf(rom_filename, "%s", short_filename);
    //grabs the file extention
    char *extension = strrchr(rom_filename, '.') + 1;
    //checks if the rom is a n64 one by checking the last 2 chars
    //this works because n64 roms dont have any supported extensions that are more than 3 chars anyway
    char *ultracheck = strrchr(rom_filename, '.') + 2;
    if (!strcmp(ultracheck, "64")) {
    rom_filename[strlen(rom_filename) - 4] = '\0'; // cut extension
    sprintf(cfg_filename, "/"ED64_FIRMWARE_PATH"/CFG/%s.CFG", rom_filename);
    } else {
    //this only uses one config file for emulators making using altra64s built in configuaton tool work
    sprintf(cfg_filename, "/"ED64_FIRMWARE_PATH"/CFG/%s.CFG", extension);
    }
    uint8_t rom_cfg_data[512];

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, cfg_filename, FA_READ);

    if (result == FR_OK)
    {

        result =
        f_read (
            &file,        /* [IN] File object */
            rom_cfg_data,  /* [OUT] Buffer to store read data */
            512,         /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);


        rom_config[0] = 1; //preload cursor position 1 cic
        rom_config[1] = rom_cfg_data[0];
        rom_config[2] = rom_cfg_data[1];
        rom_config[3] = rom_cfg_data[2];
        rom_config[4] = rom_cfg_data[3];
        rom_config[5] = rom_cfg_data[4];
        rom_config[6] = rom_cfg_data[5];
        rom_config[7] = rom_cfg_data[6];
        rom_config[8] = rom_cfg_data[7];
        rom_config[9] = rom_cfg_data[8];

    }
    else
    {
        //preload with header data
        romInfoScreen(disp, full_filename, 1); //silent info screen with readout
    }
}
void readEmuConfig(display_context_t disp, char *short_filename, char *full_filename)
{   /*
    this feature is for emulator developers and emulators that dont use the "krikzz" specification
    essentially this define custom rom load offsets and stores them in the CFG folder they are 4 bytes long
    users shouldnt need to use this feature and is for edge cases
    */
    TCHAR cfg_filename[MAX_SUPPORTED_PATH_LEN];
    sprintf(rom_filename, "%s", short_filename);
    //grabs the file extention, that's all we care about
    char *extension = strrchr(rom_filename, '.') + 1;
    sprintf(cfg_filename, "/"ED64_FIRMWARE_PATH"/CFG/%s.OFFSET", extension);
    u32 emu_cfg_data[4];

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, cfg_filename, FA_READ);

    if (result == FR_OK)
    {

        result =
        f_read (
            &file,        /* [IN] File object */
            (void *)emu_cfg_data,  /* [OUT] Buffer to store read data */
            4,         /* [IN] Number of bytes to read, an offset should never be more than 4 bytes*/
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);


        emu_offset = emu_cfg_data[0];
    }
    else
    {   
        //this should default to this as this is the krikzz spec
        emu_offset = 0xb0200000;
    }
}

void alterRomConfig(int type, int mode)
{
    //mode 1 = increae mode 2 = decrease

    //cic
    u8 min_cic = 0;
    u8 max_cic = 6;

    //save
    u8 min_save = 0;
    u8 max_save = 6;

    //tv-type
    u8 min_tv = 0;
    u8 max_tv = 3;

    //cheat
    u8 min_cheat = 0;
    u8 max_cheat = 1;

    //chk fix
    u8 min_chk_sum = 0;
    u8 max_chk_sum = 1;

    //quality
    u8 min_quality = 0;
    u8 max_quality = 5;

    //country
    u8 min_country = 0;
    u8 max_country = 2;

    switch (type)
    {
    case 1:
        //start cic
        if (mode == 1)
        {
            //down
            if (rom_config[1] < max_cic)
                rom_config[1]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[1] > min_cic)
                rom_config[1]--;
        }
        //end cic
        break;
    case 2:
        //start save
        if (mode == 1)
        {
            //down
            if (rom_config[2] < max_save)
                rom_config[2]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[2] > min_save)
                rom_config[2]--;
        }
        //end save
        break;
    case 3:
        //start tv
        if (mode == 1)
        {
            //down
            if (rom_config[3] < max_tv)
                rom_config[3]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[3] > min_tv)
                rom_config[3]--;
        }
        //end tv
        break;
    case 4:
        //start cheat
        if (mode == 1)
        {
            //down
            if (rom_config[4] < max_cheat)
                rom_config[4]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[4] > min_cheat)
                rom_config[4]--;
        }
        //end cheat
        break;
    case 5:
        //start chk sum
        if (mode == 1)
        {
            //down
            if (rom_config[5] < max_chk_sum)
                rom_config[5]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[5] > min_chk_sum)
                rom_config[5]--;
        }
        //end chk sum
        break;
    case 6:
        //start quality
        if (mode == 1)
        {
            //down
            if (rom_config[6] < max_quality)
                rom_config[6]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[6] > min_quality)
                rom_config[6]--;
        }
        break;
    case 7:
        //start country
        if (mode == 1)
        {
            //down
            if (rom_config[7] < max_country)
                rom_config[7]++;
        }
        else if (mode == 2)
        {
            //up
            if (rom_config[7] > min_country)
                rom_config[7]--;
        }
        break;

    default:
        break;
    }
}


void drawToplistBox(display_context_t disp, int line)
{
    list_pos_backup[0] = cursor;
    list_pos_backup[1] = page;
    int dsize = 0;
    u8 list_size = 0;

    if (line == 0)
    {
        char* path = "/"ED64_FIRMWARE_PATH"/CFG";

        FRESULT res;
        DIR dir;
        UINT i;
        static FILINFO fno;

        //TODO: is there a better way we can count the entries perhaps a hashtable?
        res = f_opendir(&dir, path);                       /* Open the directory */
        if (res == FR_OK) {
            for (;;) {
                res = f_readdir(&dir, &fno);                   /* Read a directory item */
                if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
                if (!fno.fattrib & !AM_DIR) {
                    dsize++;
                }
            }
            f_closedir(&dir);
        }

        res = f_opendir(&dir, path);                       /* Open the directory */
        if (res == FR_OK) {
            char toplist[dsize][256];

            for (;;) {
                res = f_readdir(&dir, &fno);                   /* Read a directory item */
                if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
                if (fno.fattrib & AM_DIR) {                    /* It is a directory */
                    //i = strlen(path);
                    // sprintf(&path[i], "/%s", fno.fname);
                    // res = scan_files(path);                    /* Enter the directory */
                    // if (res != FR_OK) break;
                    // path[i] = 0;
                } else {                                       /* It is a file. */
                    TCHAR rom_cfg_file[MAX_SUPPORTED_PATH_LEN];
                    
                    //set rom_cfg
                    sprintf(rom_cfg_file, path, fno.fname);

                    FRESULT result;
                    FIL file;
                    UINT bytesread;
                    result = f_open(&file, rom_cfg_file, FA_READ);

                    if (result == FR_OK)
                    {

                        static uint8_t cfg_file_data[512] = {0};

                        int fsize = f_size(&file) + 1;

                        result =
                        f_read (
                            &file,        /* [IN] File object */
                            cfg_file_data,  /* [OUT] Buffer to store read data */
                            fsize,         /* [IN] Number of bytes to read */
                            &bytesread    /* [OUT] Number of bytes read */
                        );

                        f_close(&file);

                        toplist[i][0] = (char)cfg_file_data[5];     //quality
                        strcpy(&toplist[i][1], cfg_file_data + 32); //fullpath //TODO: check correctness!
                        i++;
                    }
                }
            }
            f_closedir(&dir);


            qsort(toplist, dsize, 256, compare_int_reverse);

            if (dsize > 15)
                list_size = 15;
            else
                list_size = dsize;

            for (int c = 0; c < list_size; c++)
                strcpy(toplist15[c], toplist[c]);

            list_pos_backup[2] = list_size;
        }
    }

    list_size = list_pos_backup[2];

    u8 min = 1;
    u8 max = 15;
    u8 real_max = 0;

    for (int c = 0; c < list_size; c++)
        if (toplist15[c][0] != 0)
            real_max++;

    max = real_max;

    //cursor line
    if (line != 0)
    {
        if (line == 1)
        {
            //down
            if (toplist_cursor < max)
                toplist_cursor++;
        }
        else if (line == 2)
        {
            //up
            if (toplist_cursor > min)
                toplist_cursor--;
        }
    }

    drawBoxNumber(disp, 9); //toplist

    printText("          -Toplist 15-", 4, 7, disp);
    printText(" ", 4, -1, disp);

    uint32_t forecolor;
    uint32_t backcolor;
    backcolor = graphics_make_color(0x00, 0x00, 0x00, 0x00); //bg
    forecolor = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF); //fg

    for (int t = 0; t < 15; t++)
    {
        int quality_level = toplist15[t][0]; //quality
        if (quality_level != 0)
        {
            switch (quality_level)
            {
            case 1:
                forecolor = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF); //common (white)
                break;
            case 2:
                forecolor = graphics_make_color(0x00, 0xFF, 0x00, 0xCF); //uncommon (green)
                break;
            case 3:
                forecolor = graphics_make_color(0x1E, 0x90, 0xFF, 0xFF); //rare (blue)
                break;
            case 4:
                forecolor = graphics_make_color(0x9B, 0x30, 0xFF, 0xFF); //epic (purple)
                break;
            case 5:
                forecolor = graphics_make_color(0xFF, 0xA5, 0x00, 0xFF); //legendary (orange)
                break;
            default:
                break;
            }

            graphics_set_color(forecolor, backcolor);
            //max 32 chr

            u8 short_name[256];

            sprintf(short_name, "%s", toplist15[t] + 1);

            u8 *pch_s; // point-offset
            pch_s = strrchr(short_name, '/');

            u8 *pch; // point-offset
            pch = strrchr(pch_s, '.');
            pch_s[30] = '\0'; //was 31
            pch_s[pch - pch_s] = '\0';

            if (t + 1 == toplist_cursor)
                printText(pch_s + 1, 5, -1, disp);
            else
                printText(pch_s + 1, 4, -1, disp);

            //restore color
            graphics_set_color(graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF), graphics_make_color(0x00, 0x00, 0x00, 0x00));
        }
    }
}

void drawRomConfigBox(display_context_t disp, int line)
{
    u8 min = 1;
    u8 max = 7;

    //cursor line
    if (line != 0)
    {
        if (line == 1)
        {
            //down
            if (rom_config[0] < max)
                rom_config[0]++;
        }
        else if (line == 2)
        {
            //up
            if (rom_config[0] > min)
                rom_config[0]--;
        }
    }

    drawBoxNumber(disp, 6);

    drawConfigSelection(disp, rom_config[0]);

    printText(" ", 9, 9, disp);
    printText(romconfig, 9, -1, disp);
    printText(" ", 9, -1, disp);

    switch (rom_config[1])
    {
    case 0:
        printText("     CIC: 6101", 9, -1, disp);
        break;
    case 1:
        printText("     CIC: 6102", 9, -1, disp);
        break;
    case 2:
        printText("     CIC: 6103", 9, -1, disp);
        break;
    case 3:
        printText("     CIC: 5101", 9, -1, disp);
        break;
    case 4:
        printText("     CIC: 6105", 9, -1, disp);
        break;
    case 5:
        printText("     CIC: 6106", 9, -1, disp);
        break;
    case 6:
        printText("     CIC: 5167", 9, -1, disp);
        break;
    default:
        break;
    }

    switch (rom_config[2])
    {
    case 0:
        printText(savemem, 9, -1, disp);
        break;
    case 1:
        printText(save32, 9, -1, disp);
        break;
    case 2:
        printText(save128, 9, -1, disp);
        break;
    case 3:
        printText(save4k, 9, -1, disp);
        break;
    case 4:
        printText(save16k, 9, -1, disp);
        break;
    case 5:
        printText(saveflash, 9, -1, disp);
        break;
    case 6:
        printText(save768, 9, -1, disp);
        break;
    default:
        break;
    }

    switch (rom_config[3])
    {
    case 0:
        printText(tvforceoff, 9, -1, disp);
        break;
    case 1:
        printText("      Tv: NTSC", 9, -1, disp);
        break;
    case 2:
        printText("      Tv: PAL", 9, -1, disp);
        break;
    case 3:
        printText("      Tv: M-PAL", 9, -1, disp);
        break;
    default:
        break;
    }

    switch (rom_config[4])
    {
    case 0:
        printText(cheatoff, 9, -1, disp);
        break;
    case 1:
        printText(cheaton, 9, -1, disp);
        break;
    default:
        break;
    }

    switch (rom_config[5])
    {
    case 0:
        printText(checksumdisable, 9, -1, disp);
        break;
    case 1:
        printText(checksumenable, 9, -1, disp);
        break;
    default:
        break;
    }

    switch (rom_config[6])
    {
    case 0:
        printText(ratoff, 9, -1, disp);
        break;
    case 1:
        printText(ratcomo, 9, -1, disp);
        break;
    case 2:
        printText(ratuncomo, 9, -1, disp);
        break;
    case 3:
        printText(ratrare, 9, -1, disp);
        break;
    case 4:
        printText(ratepic, 9, -1, disp);
        break;
    case 5:
        printText(ratlegend, 9, -1, disp);
        break;

    default:
        break;
    }

    switch (rom_config[7])
    {
    case 0:
        printText(countrydef, 9, -1, disp);
        break;
    case 1:
        printText(countryntsc, 9, -1, disp);
        break;
    case 2:
        printText(countrypal, 9, -1, disp);
        break;
    default:
        break;
    }

    printText(" ", 9, -1, disp);
    printText(bcancel, 9, -1, disp);
    printText(Asaveconf, 9, -1, disp);
}

//draws the charset for the textinputscreen
void drawSet1(display_context_t disp)
{
    set = 1;
    uint32_t forecolor;
    uint32_t backcolor;
    backcolor = graphics_make_color(0x00, 0x00, 0x00, 0xFF);
    forecolor = graphics_make_color(0xFF, 0xFF, 0x00, 0xFF); //yellow

    graphics_draw_text(disp, 80, 40, "<"); //L
    graphics_set_color(forecolor, backcolor);
    graphics_draw_text(disp, 233, 40, "A");  //R
    graphics_draw_text(disp, 223, 62, "B");  //G up
    graphics_draw_text(disp, 210, 74, "C");  //G left
    graphics_draw_text(disp, 235, 74, "D");  //G right
    graphics_draw_text(disp, 193, 86, "E");  //B
    graphics_draw_text(disp, 223, 86, "F");  //G down
    graphics_draw_text(disp, 209, 100, "G"); //A
}

void drawSet2(display_context_t disp)
{
    set = 2;
    uint32_t forecolor;
    uint32_t backcolor;
    backcolor = graphics_make_color(0x00, 0x00, 0x00, 0xFF);
    forecolor = graphics_make_color(0xFF, 0xFF, 0x00, 0xFF);

    graphics_draw_text(disp, 80, 40, "<");
    graphics_set_color(forecolor, backcolor);
    graphics_draw_text(disp, 233, 40, "H");
    graphics_draw_text(disp, 223, 62, "I");
    graphics_draw_text(disp, 210, 74, "J");
    graphics_draw_text(disp, 235, 74, "K");
    graphics_draw_text(disp, 193, 86, "L");
    graphics_draw_text(disp, 223, 86, "M");
    graphics_draw_text(disp, 209, 100, "N");
}

void drawSet3(display_context_t disp)
{
    set = 3;
    uint32_t forecolor;
    uint32_t backcolor;
    backcolor = graphics_make_color(0x00, 0x00, 0x00, 0xFF);
    forecolor = graphics_make_color(0xFF, 0xFF, 0x00, 0xFF);

    graphics_draw_text(disp, 80, 40, "<");
    graphics_set_color(forecolor, backcolor);
    graphics_draw_text(disp, 233, 40, "O");
    graphics_draw_text(disp, 223, 62, "P");
    graphics_draw_text(disp, 210, 74, "Q");
    graphics_draw_text(disp, 235, 74, "R");
    graphics_draw_text(disp, 193, 86, "S");
    graphics_draw_text(disp, 223, 86, "T");
    graphics_draw_text(disp, 209, 100, "U");
}

display_context_t lockVideo(int wait)
{
    display_context_t dc;

    if (wait)
        while (!(dc = display_lock()))
            ;
    else
        dc = display_lock();
    return dc;
}

void drawSet4(display_context_t disp)
{
    set = 4;
    uint32_t forecolor;
    uint32_t backcolor;
    backcolor = graphics_make_color(0x00, 0x00, 0x00, 0xFF);
    forecolor = graphics_make_color(0xFF, 0xFF, 0x00, 0xFF);

    graphics_draw_text(disp, 80, 40, "<");
    graphics_set_color(forecolor, backcolor);
    graphics_draw_text(disp, 233, 40, "V");
    graphics_draw_text(disp, 223, 62, "W");
    graphics_draw_text(disp, 210, 74, "X");
    graphics_draw_text(disp, 235, 74, "Y");
    graphics_draw_text(disp, 193, 86, "Z");
    graphics_draw_text(disp, 223, 86, "-");
    graphics_draw_text(disp, 209, 100, "_");
}


void showDeletePrompt(display_context_t disp)
{
    while (!(disp = display_lock()))
                ;
    new_scroll_pos(&cursor, &page, MAX_LIST, count);
    clearScreen(disp);
    display_dir(list, cursor, page, MAX_LIST, count, disp);
    drawBoxNumber(disp, 13);
    display_show(disp);

    bool isdir = list[cursor].type == DT_DIR;
    
    if (sound_on)
        playSound(3 + isdir);

    menu_delete(disp, isdir);
}

void showAboutScreen(display_context_t disp)
{
    while (!(disp = display_lock()))
                ;
    new_scroll_pos(&cursor, &page, MAX_LIST, count);
    clearScreen(disp); //part clear?
    display_dir(list, cursor, page, MAX_LIST, count, disp);
    drawBoxNumber(disp, 2);
    display_show(disp);

    if (sound_on)
        playSound(2);

    menu_about(disp);
}
void showControlScreen(display_context_t disp)
{
    while (!(disp = display_lock()))
                ;
    new_scroll_pos(&cursor, &page, MAX_LIST, count);
    clearScreen(disp); //part clear?
    display_dir(list, cursor, page, MAX_LIST, count, disp);
    drawBoxNumber(disp, 12);
    display_show(disp);

    if (sound_on)
        playSound(2);

    menu_controls(disp);
}

void loadFile(display_context_t disp)
{
    char name_file[256];

    if (strcmp(pwd, "/") == 0)
        sprintf(name_file, "/%s", list[cursor].filename);
    else
        sprintf(name_file, "%s/%s", pwd, list[cursor].filename);

    int ft = 0;
    char _upper_name_file[256];

    strcpy(_upper_name_file, name_file);

    strhicase(_upper_name_file, strlen(_upper_name_file));
    sprintf(_upper_name_file, "%s", _upper_name_file);

    u8 extension[4];
    u8 *pch;
    pch = strrchr(_upper_name_file, '.'); //asd.n64
    sprintf(extension, "%s", (pch + 1)); //0123456


    if (!strcmp(extension, "Z64") || !strcmp(extension, "V64") || !strcmp(extension, "N64")) //TODO: an enum would be better
        ft = 1;
    else if (!strcmp(extension, "MPK"))
        ft = 2;
    else if (!strcmp(extension, "MP3"))
        ft = 7;
    else
        ft = 999;

    if (ft != 9 || ft != 2)
    {
        while (!(disp = display_lock()))
            ;

        clearScreen(disp);
        u16 msg = 0;
        evd_ulockRegs();
        sleep(10);
        sprintf(rom_filename, "%s", list[cursor].filename);
        display_show(disp);
        select_mode = 9;
    }
    switch (ft)
    {
    case 1:
        if (quick_boot) //write to the file
        {
            FRESULT result;
            FIL file;
            result = f_open(&file, "/"ED64_FIRMWARE_PATH"/LASTROM.CFG", FA_WRITE | FA_CREATE_ALWAYS);
            if (result == FR_OK)
            {
                f_puts (
                    name_file, /* [IN] String */
                    &file           /* [IN] File object */
                  );

                  f_close(&file);

                if (result == FR_OK)
                {
                    //read rom_config data
                    readRomConfig(disp, rom_filename, name_file);

                    loadrom(disp, name_file, 1);
                    display_show(disp);

                    //rom loaded mapping
                    input_mapping = rom_loaded;
                }
                else
                {
                    TRACE(disp, "Issue writing file...");
                }

            }
            else
            {
                TRACE(disp, "Couldnt Open file");
            }

        }
        else
        {
            //read rom_config data
            readRomConfig(disp, rom_filename, name_file);

            loadrom(disp, name_file, 1);
            display_show(disp);

            //rom loaded mapping
            input_mapping = rom_loaded;
        }
        break;
    case 2:
        while (!(disp = display_lock()))
            ;
        clearScreen(disp); //part clear?
        display_dir(list, cursor, page, MAX_LIST, count, disp);
        display_show(disp);
        drawShortInfoBox(disp, restoreback, 2);
        input_mapping = mpk_choice;
        sprintf(rom_filename, "%s", name_file);
        break;
    case 7:
    {
        while (!(disp = display_lock()))
        ;
        clearScreen(disp);
        drawShortInfoBox(disp, loading, 0);
        display_show(disp);
        long long start = 0, end = 0, curr, pause = 0, samples;
        int rate = 44100, last_rate = 44100, channels = 2;

        audio_init(44100, 8);
        buf_size = audio_get_buffer_length() * 6;
        buf_ptr = malloc(buf_size);

        mp3_Start(name_file, &samples, &rate, &channels);
        mp3playing = 1;
        select_mode = 9;

        while (!(disp = display_lock()))
        ;
        clearScreen(disp);
        drawShortInfoBox(disp, plgmp3, 0);
        display_show(disp);
        input_mapping = mp3; //mp3 stop
    }
    break;
    
    case 999:
        readRomConfig(disp, rom_filename, name_file);
        readEmuConfig(disp, rom_filename, name_file);
        loadgenericrom(disp, name_file, extension);
        display_show(disp);
        break;

    default:
        break;
    }
}

void handleInput(display_context_t disp, sprite_t *contr)
{
    //request controller
    //request controller 1 [0]
    controller_scan();
    struct controller_data keys = get_keys_down();
    struct controller_data keys_held = get_keys_held();

    if( keys.c[0].err != ERROR_NONE )
        return;

    if (keys.c[0].up || keys_held.c[0].up || keys_held.c[0].y > +25)
    {
        switch (input_mapping)
        {
        case file_manager:

        if (sound_on)
        playSound(4);

            if (select_mode)
            {
                if (count != 0)
                {
                    if (scroll_behaviour == 1)
                        cursor--;
                    else if (cursor != 0)
                    {
                        if ((cursor + 0) % 20 == 0)
                        {
                            cursor--;
                            page -= 20;
                        }
                        else
                        {
                            cursor--;
                        }
                    }

                    //end
                    while (!(disp = display_lock()))
                        ;

                    new_scroll_pos(&cursor, &page, MAX_LIST, count);

                    clearScreen(disp); //part clear?
                    display_dir(list, cursor, page, MAX_LIST, count, disp);

                    display_show(disp);
                }
            }
            break;
        case mempak_menu:
            break;
        case char_input:
            //chr input screen
            set = 1;
            break;
        case rom_config_box:
            while (!(disp = display_lock()))
                ;

            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            drawRomConfigBox(disp, 2);
            display_show(disp);
            input_mapping = rom_config_box;
            break;
        case toplist:
            while (!(disp = display_lock()))
                ;

            drawBg(disp); //background
            drawToplistBox(disp, 2);

            display_show(disp);
            input_mapping = toplist;
            break;

        default:
            break;
        }
    }

    if (keys.c[0].down || keys_held.c[0].down || keys_held.c[0].y < -25)
    {
        switch (input_mapping)
        {
        case file_manager:

        if (sound_on)
        playSound(4);

            if (select_mode)
            {
                if (count != 0)
                {
                    if (scroll_behaviour == 1)
                        cursor++;
                    else if (cursor + 1 != count)
                    {
                        if ((cursor + 1) % 20 == 0)
                        {
                            cursor++;
                            page += 20;
                        }
                        else
                        {
                            cursor++;
                        }
                    }

                    while (!(disp = display_lock()))
                        ;

                    new_scroll_pos(&cursor, &page, MAX_LIST, count);
                    clearScreen(disp); //part clear?
                    display_dir(list, cursor, page, MAX_LIST, count, disp);

                    display_show(disp);
                }
            }
            break;
        case mempak_menu:
            break;

        case char_input:
            //chr input screen
            set = 3;
            break;
        case rom_config_box:
            while (!(disp = display_lock()))
                ;

            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?

            display_dir(list, cursor, page, MAX_LIST, count, disp);

            drawRomConfigBox(disp, 1);

            display_show(disp);
            input_mapping = rom_config_box;
            break;
        case toplist:
            while (!(disp = display_lock()))
                ;

            drawBg(disp);
            drawToplistBox(disp, 1);

            display_show(disp);
            input_mapping = toplist;
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].left || keys_held.c[0].left || keys_held.c[0].x < -25)
    {
        switch (input_mapping)
        {
        case file_manager:
            if (select_mode)
            {
                if (count != 0 && scroll_behaviour == 0 && cursor - 20 >= 0)
                {
                    page -= 20;
                    cursor = page;
                }

                while (!(disp = display_lock()))
                    ;

                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);

                display_show(disp);
            }
            break;
        case mempak_menu:
            break;
        case char_input:
            //chr input screen
            set = 4;
            break;
        case rom_config_box:
            while (!(disp = display_lock()))
                ;

            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            alterRomConfig(rom_config[0], 2);

            drawRomConfigBox(disp, 0);
            display_show(disp);
            input_mapping = rom_config_box;
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].right || keys_held.c[0].right || keys_held.c[0].x > +25)
    {
        switch (input_mapping)
        {
        case file_manager:

            if (select_mode)
            {
                if ((count != 0) &&
                    (scroll_behaviour == 0) &&
                    (page + 20 != count) &&
                    (cursor + 20 <= ((count / 20) * 20 + 19)))
                {
                    page += 20;
                    cursor = page;
                }

                while (!(disp = display_lock()))
                    ;

                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);

                display_show(disp);
            }
            break;

        case mempak_menu:
            break;

        case char_input:

            //chr input screen
            set = 2;
            break;

        case rom_config_box:

            while (!(disp = display_lock()))
                ;

            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            alterRomConfig(rom_config[0], 1);

            drawRomConfigBox(disp, 0);
            display_show(disp);
            input_mapping = rom_config_box;
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].start)
    {
        switch (input_mapping)
        {
        case file_manager:

            //quick boot
            if (quick_boot)
            {
                FRESULT result;
                FIL file;
                UINT bytesread;
                result = f_open(&file, "/"ED64_FIRMWARE_PATH"/LASTROM.CFG", FA_READ);

                if (result == FR_OK)
                {
                    int fsize = f_size(&file) + 1; //extra char needed for null terminator '/0'
                    uint8_t lastrom_cfg_data[fsize];

                    //result =
                    //f_read (
                    //    &file,        /* [IN] File object */
                    //    lastrom_cfg_data,  /* [OUT] Buffer to store read data */
                    //    fsize,         /* [IN] Number of bytes to read */
                    //    &bytesread    /* [OUT] Number of bytes read */
                    //);

                    f_gets(lastrom_cfg_data, fsize, &file);

                    f_close(&file);

                    u8 *file_name = strrchr(lastrom_cfg_data, '/');

                    while (!(disp = display_lock()))
                        ;
                    clearScreen(disp);

                    evd_ulockRegs();
                    sleep(10);

                    select_mode = 9;
                                        //short          fullpath
                    readRomConfig(disp, file_name + 1, lastrom_cfg_data);

                    loadrom(disp, lastrom_cfg_data, 1);
                    display_show(disp);
                }
                else
                {
                    drawShortInfoBox(disp, "    rom not found", 0);
                }
            }
            else if (list[cursor].type != DT_DIR && empty == 0)
            {
                loadFile(disp);
            }
            break;

        case mempak_menu:
            break;

        case char_input:

            //better config color-set
            graphics_set_color(
                graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF),
                graphics_make_color(0x00, 0x00, 0x00, 0x00));
            clearScreen(disp);
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            if (input_text[0] != '\0')
            { //input_text is set - do backup
                drawBoxNumber(disp, 2);

                display_show(disp);

                printText(mpkbackup, 9, 9, disp);
                printText(" ", 9, -1, disp);
                printText(searchfds, 9, -1, disp);
                mpk_to_file(disp, input_text, 0);
                while (!(disp = display_lock()))
                ;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);
                drawShortInfoBox(disp, done, 0);
                sleep(1000);

                //reread filesystem
                cursor_line = 0;
                readSDcard(disp, "/");
            }

            input_mapping = file_manager;
            break;

        case rom_loaded:

            //rom start screen

            //normal boot
            //boot the loaded rom
            bootRom(disp, 0);
            //never return
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].L && !keys.c[0].R)
    {
        switch (input_mapping)
        {
        case file_manager:

            input_mapping = mempak_menu;
            if (sound_on)
                playSound(2);
            while (!(disp = display_lock()))
                ;
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            drawBoxNumber(disp, 2);

            display_show(disp);

            printText(mpksub, 9, 9, disp);
            printText(" ", 9, -1, disp);
            printText(" ", 9, -1, disp);
            printText(viewmpk, 9, -1, disp);
            printText(" ", 9, -1, disp);
            printText(backnew, 9, -1, disp); //set mapping 3
            printText(" ", 9, -1, disp);
            printText(formatt, 9, -1, disp);
            printText(" ", 9, -1, disp);
            printText(abortmen, 9, -1, disp);
            break;

        case mempak_menu:
            break;

        case char_input:

            //chr input screen
            drawInputDel(disp);
            break;

        case mpk_choice:

            //c-up or A
            drawConfirmBox(disp);
            //confirm restore mpk
            input_mapping = mpk_restore;
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].R && !keys.c[0].L)
    {
        switch (input_mapping)
        {
        case file_manager:
            break;

        case mempak_menu:

            //c-up or A
            while (!(disp = display_lock()))
                ;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);
                drawConfirmBox(disp);
                //confirm format mpk
                input_mapping = mpk_format;
            break;

        case char_input:

            //chr input screen
            if (set == 1)
                drawInputAdd(disp, "A"); //P X )
            if (set == 2)
                drawInputAdd(disp, "H");
            if (set == 3)
                drawInputAdd(disp, "O");
            if (set == 4)
                drawInputAdd(disp, "V");
            break;

        case mpk_choice:

            //c-up or A
            drawConfirmBox(disp);
            //confirm quick-backup
            input_mapping = mpk_quick_backup;
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].C_up)
    {
        switch (input_mapping)
        {
        case file_manager:

            if (list[cursor].type != DT_DIR && empty == 0)
            {
                while (!(disp = display_lock()))
                ;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);
                drawBoxNumber(disp, 11);
                display_show(disp);
                char *part;

                part = malloc(slen(list[cursor].filename));
                sprintf(part, "%s", list[cursor].filename);
                part[31] = '\0';

                printText(part, 4, 14, disp);

                if (slen(list[cursor].filename) > 31)
                {
                    sprintf(part, "%s", list[cursor].filename + 31);
                    part[31] = '\0';
                    printText(part, 4, -1, disp);
                }

                free(part);
                input_mapping = abort_screen;
            }
            break;

        case mempak_menu:
            break;

        case char_input:

            //chr input screen
            if (set == 1)
                drawInputAdd(disp, "B"); //P X )
            if (set == 2)
                drawInputAdd(disp, "I");
            if (set == 3)
                drawInputAdd(disp, "P");
            if (set == 4)
                drawInputAdd(disp, "W");
            break;

        case rom_loaded:

            //rom start screen
            if (cheats_on == 0)
            {
                printText(cheatsysact, 3, -1, disp);
                cheats_on = 1;
            }
            break;

        case mpk_format:
            // format mpk
            while (!(disp = display_lock()))
                ;
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);
            drawBoxNumber(disp, 2);
            display_show(disp);

            printText(mpkformat, 9, 9, disp);
            printText(" ", 9, -1, disp);

            printText(formating, 9, -1, disp);

            /* Make sure they don't have a rumble pak inserted instead */
            switch (identify_accessory(0))
            {
            case ACCESSORY_NONE:
                printText("No Mempak", 9, -1, disp);
                break;

            case ACCESSORY_MEMPAK:
                printText(plswait, 9, -1, disp);
                if (format_mempak(0))
                {
                    printText(errorformating, 9, -1, disp);
                }
                else
                {
                    while (!(disp = display_lock()))
                ;
                    new_scroll_pos(&cursor, &page, MAX_LIST, count);
                    clearScreen(disp); //part clear?
                    display_dir(list, cursor, page, MAX_LIST, count, disp);
                    drawShortInfoBox(disp, done, 0);
                    input_mapping = abort_screen;
                }
                break;

            case ACCESSORY_RUMBLEPAK:
                printText(reallyformatrpak, 9, -1, disp);
                break;
            }

            sleep(1000);

            input_mapping = abort_screen;
            break;

        case mpk_restore:
            //restore mpk
            while (!(disp = display_lock()))
                ;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);
            drawBoxNumber(disp, 2);
            display_show(disp);

            printText(mpkrestore, 9, 9, disp);
            printText(" ", 9, -1, disp);

            file_to_mpk(disp, rom_filename);
            while (!(disp = display_lock()))
                ;
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);
            drawShortInfoBox(disp, done, 0);
            sleep(1000);

            input_mapping = abort_screen;

            display_show(disp);
            break;

        case mpk_quick_backup:
            //quick-backup
            while (!(disp = display_lock()))
                ;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);
                drawBoxNumber(disp, 2);
                display_show(disp);

            printText(quickback, 9, 9, disp);
            printText(" ", 9, -1, disp);
            printText(searchfds, 9, -1, disp);

            mpk_to_file(disp, list[cursor].filename, 1); //quick
            while (!(disp = display_lock()))
                ;
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);
            drawShortInfoBox(disp, done, 0);
            sleep(1000);
            input_mapping = abort_screen;
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].C_right)
    {
        switch (input_mapping)
        {
        case file_manager:

            if (list[cursor].type != DT_DIR)
            {
                //show rom cfg screen

                char name_file[64];

                if (strcmp(pwd, "/") == 0)
                    sprintf(name_file, "/%s", list[cursor].filename);
                else
                    sprintf(name_file, "%s/%s", pwd, list[cursor].filename);

                /*filetype
                     * 1 rom
                     */

                //TODO: this code is very similar to that used in loadFile, we should move it to a seperate function!
                char _upper_name_file[64];

                strcpy(_upper_name_file, name_file);

                strhicase(_upper_name_file, strlen(_upper_name_file));
                sprintf(_upper_name_file, "%s", _upper_name_file);

                u8 extension[4];
                u8 *pch;
                pch = strrchr(_upper_name_file, '.'); //asd.n64
                sprintf(extension, "%s", (pch + 1)); //0123456

                if (!strcmp(extension, "Z64") || !strcmp(extension, "V64") || !strcmp(extension, "N64"))
                { //rom
                    //cfg rom
                    sprintf(rom_filename, "%s", list[cursor].filename);

                    //preload config or file header
                    readRomConfig(disp, rom_filename, name_file);
                    while (!(disp = display_lock()))
                ;
                    new_scroll_pos(&cursor, &page, MAX_LIST, count);
                    clearScreen(disp); //part clear?
                    display_dir(list, cursor, page, MAX_LIST, count, disp);


                    drawRomConfigBox(disp, 0);
                    display_show(disp);
                    input_mapping = rom_config_box;
                }
            }
            break;

        case mempak_menu:
            break;

        case char_input:

            //chr input screen
            if (set == 1)
                drawInputAdd(disp, "D"); //P X )
            if (set == 2)
                drawInputAdd(disp, "K");
            if (set == 3)
                drawInputAdd(disp, "R");
            if (set == 4)
                drawInputAdd(disp, "Y");
            break;

        case rom_loaded:

            //rom start screen

            if (force_tv != 0)
            {
                printText(forcetvmd, 3, -1, disp);
            }
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].C_down)
    {
        switch (input_mapping)
        {
        case file_manager:
            scopy(pwd, list_pwd_backup);
            while (!(disp = display_lock()))
                ;

            drawBg(disp); //background

            toplist_cursor = 1;
            drawToplistBox(disp, 0); //0 = load entries
            display_show(disp);

            input_mapping = toplist;
            break;

        case mempak_menu:
            break;

        case char_input:

            //chr input screen
            if (set == 1)
                drawInputAdd(disp, "F"); //P X )
            else if (set == 2)
                drawInputAdd(disp, "M");
            else if (set == 3)
                drawInputAdd(disp, "T");
            else if (set == 4)
                drawInputAdd(disp, "-"); //GR Set4
            break;

        default:
            break;
        }
    }
    else if (keys.c[0].C_left)
    {
        switch (input_mapping)
        {
        case file_manager:

            if (list[cursor].type != DT_DIR)
            {
                //TODO: this code is similar (if not the same) as loadFile and can be optimised!
                //open
                char name_file[64];

                if (strcmp(pwd, "/") == 0)
                    sprintf(name_file, "/%s", list[cursor].filename);
                else
                    sprintf(name_file, "%s/%s", pwd, list[cursor].filename);

                char _upper_name_file[64];

                strcpy(_upper_name_file, name_file);
                strhicase(_upper_name_file, strlen(_upper_name_file));
                sprintf(_upper_name_file, "%s", _upper_name_file);

                u8 extension[4];
                u8 *pch;
                pch = strrchr(_upper_name_file, '.');
                sprintf(extension, "%s", (pch + 1)); //0123456

                if (!strcmp(extension, "Z64") || !strcmp(extension, "V64") || !strcmp(extension, "N64"))
                { //rom
                    //load rom
                    while (!(disp = display_lock()))
                    ;
                    new_scroll_pos(&cursor, &page, MAX_LIST, count);
                    clearScreen(disp); //part clear?
                    display_dir(list, cursor, page, MAX_LIST, count, disp);

                    drawBoxNumber(disp, 3); //rominfo
                    display_show(disp);

                    u16 msg = 0;
                    evd_ulockRegs();
                    sleep(10);
                    sprintf(rom_filename, "%s", list[cursor].filename);
                    romInfoScreen(disp, name_file, 0);

                    if (sound_on)
                        playSound(2);

                    input_mapping = abort_screen;
                }
                else if (!strcmp(extension, "MPK"))
                { //mpk file
                  while (!(disp = display_lock()))
                  ;
                    clearScreen(disp);
                    drawBoxNumber(disp, 4);
                    display_show(disp);

                    if (strcmp(pwd, "/") == 0)
                        sprintf(rom_filename, "/%s", list[cursor].filename);
                    else
                        sprintf(rom_filename, "%s/%s", pwd, list[cursor].filename);

                    view_mpk_file(disp, rom_filename);

                    if (sound_on)
                        playSound(2);

                    input_mapping = abort_screen;
                }
            } //mapping and not dir
            break;

        case mempak_menu:
            break;

        case char_input:

            //chr input screen
            if (set == 1)
                drawInputAdd(disp, "C"); //P X )
            else if (set == 2)
                drawInputAdd(disp, "J");
            else if (set == 3)
                drawInputAdd(disp, "Q");
            else if (set == 4)
                drawInputAdd(disp, "X");
            break;
        }
    }
    else if (keys.c[0].Z)
    {
        switch (input_mapping)
        {
            case file_manager:
                showAboutScreen(disp);
                input_mapping = control_screen;
                break;

            case mempak_menu:
                while (!(disp = display_lock()));
                if (sound_on)
                    playSound(2);

                drawBoxNumber(disp, 4);
                display_show(disp);
                view_mpk(disp);

                input_mapping = abort_screen;
            break;

          case control_screen:
            showControlScreen(disp);
            input_mapping = none;
            break;
            

            default:
            break;
        }
    }
    else if (keys.c[0].R && keys.c[0].L)
    {
        switch (input_mapping)
        {
            case file_manager:
                showDeletePrompt(disp);
                input_mapping = delete_prompt;
                break;
                
            default:
                break;
        }       
    }
    else if (keys.c[0].A)
    {
        switch (input_mapping)
        {
        // open
        case file_manager:
        {
            while (!(disp = display_lock()))
                ;

            if (list[cursor].type == DT_DIR && empty == 0)
            {
                char name_dir[256];

                /* init pwd=/
                         * /
                         *
                         * cursor=ED64
                         * /ED64
                         *
                         * cursor=SAVE
                         * /"ED64_FIRMWARE_PATH"/SAVE
                         */

                if (strcmp(pwd, "/") == 0)
                    sprintf(name_dir, "/%s", list[cursor].filename);
                else
                    sprintf(name_dir, "%s/%s", pwd, list[cursor].filename);

                sprintf(curr_dirname, "%s", list[cursor].filename);
                sprintf(pwd, "%s", name_dir);

                //load dir
                cursor_lastline = 0;
                cursor_line = 0;

                //backup tree cursor postions
                if (cd_behaviour == 1)
                {
                    cursor_history[cursor_history_pos] = cursor;
                    cursor_history_pos++;
                }

                readSDcard(disp, name_dir);
                display_show(disp);
            } //mapping 1 and dir
            else if (list[cursor].type != DT_DIR && empty == 0)
            { //open
                loadFile(disp);
            }
            break; //mapping 1 end
        }
        case mempak_menu:
        {
            //open up charinput screen
            while (!(disp = display_lock()))
                ;
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp); //part clear?
            input_mapping = char_input;
            input_text[0] = '\0';
            graphics_draw_sprite(disp, 0, 0, contr);
            display_show(disp);
            break;
        }
        case char_input:
        {
            //chr input screen
            if (set == 1)
                drawInputAdd(disp, "G"); //P X )
            else if (set == 2)
                drawInputAdd(disp, "N");
            else if (set == 3)
                drawInputAdd(disp, "U");
            else if (set == 4)
                drawInputAdd(disp, "_");
            break;
        }
        case rom_config_box:
        {
            char name_file[256];

            if (strcmp(pwd, "/") == 0)
                sprintf(name_file, "/%s", list[cursor].filename);
            else
                sprintf(name_file, "%s/%s", pwd, list[cursor].filename);

            TCHAR rom_cfg_file[MAX_SUPPORTED_PATH_LEN];

            u8 resp = 0;

            //set rom_cfg
            sprintf(rom_cfg_file, "/"ED64_FIRMWARE_PATH"/CFG/%s.CFG", rom_filename);


            FRESULT result;
            FIL file;
            result = f_open(&file, rom_cfg_file, FA_WRITE | FA_OPEN_ALWAYS);

            if (result == FR_OK)
            {
                static uint8_t cfg_file_data[512] = {0};
                cfg_file_data[0] = rom_config[1]; //cic
                cfg_file_data[1] = rom_config[2]; //save
                cfg_file_data[2] = rom_config[3]; //tv
                cfg_file_data[3] = rom_config[4]; //cheat
                cfg_file_data[4] = rom_config[5]; //chksum
                cfg_file_data[5] = rom_config[6]; //rating
                cfg_file_data[6] = rom_config[7]; //country
                cfg_file_data[7] = rom_config[8];
                cfg_file_data[8] = rom_config[9];

                //copy full rom path to offset at 32 byte - 32 bytes reversed
                scopy(name_file, cfg_file_data + 32); //filename to rom_cfg file

                UINT bw;
                result =
                f_write (
                    &file,          /* [IN] Pointer to the file object structure */
                    cfg_file_data, /* [IN] Pointer to the data to be written */
                    512,         /* [IN] Number of bytes to write */
                    &bw          /* [OUT] Pointer to the variable to return number of bytes written */
                  );

                f_close(&file);

                while (!(disp = display_lock()))
                ;
                drawRomConfigBox(disp, 0);
                display_show(disp);
                drawShortInfoBox(disp, done, 0);
                toplist_reload = 1;
            }

            input_mapping = abort_screen;
            break;
        }
        case toplist:
        {
            //run from toplist
            u8 *pch_s;
            pch_s = strrchr(toplist15[toplist_cursor - 1] + 1, '/');

            readRomConfig(disp, pch_s + 1, toplist15[toplist_cursor - 1] + 1);

            loadrom(disp, toplist15[toplist_cursor - 1] + 1, 1);

            //rom loaded mapping
            input_mapping = rom_loaded;
            break;
        }
        case abort_screen:
        {
            //rom info screen

            while (!(disp = display_lock()))
                ;

            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            display_show(disp);

            input_mapping = file_manager;
            break;
        }
        case delete_prompt:
        {
            if (list[cursor].type != DT_DIR)
            {
                char name_file[256];

                if (strcmp(pwd, "/") == 0)
                    sprintf(name_file, "/%s", list[cursor].filename);
                else
                    sprintf(name_file, "%s/%s", pwd, list[cursor].filename);    
                
                f_unlink(name_file);

                while (!(disp = display_lock()))
                    ;
                graphics_set_color(graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF), graphics_make_color(0x00, 0x00, 0x00, 0x00));
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
                clearScreen(disp);
                display_show(disp);             

                input_mapping = file_manager;
                readSDcard(disp, pwd);
                display_show(disp);
            }
            break;
        }
        default:
            break;
        }
    } //key a
    else if (keys.c[0].B)
    { //go back
        switch (input_mapping)
        {
        case file_manager:

            if (!(strcmp(pwd, "/") == 0))
            {
                while (!(disp = display_lock()))
                    ;

                //replace by strstr()? :>
                int slash_pos = 0;
                int i = 0;
                while (pwd[i] != '\0')
                {
                    if (pwd[i] == '/')
                        slash_pos = i;

                    i++;
                }
                char new_pwd[64];

                int j;
                for (j = 0; j < slash_pos; j++)
                    new_pwd[j] = pwd[j];

                if (j == 0)
                    j++;

                new_pwd[j] = '\0';

                sprintf(pwd, "%s", new_pwd);

                cursor_lastline = 0;
                cursor_line = 0;

                readSDcard(disp, pwd);

                if (cd_behaviour == 1)
                {
                    cursor_history_pos--;
                    cursor = cursor_history[cursor_history_pos];

                    if (cursor_history[cursor_history_pos] > 0)
                    {
                        cursor_line = cursor_history[cursor_history_pos] - 1;
                        if (scroll_behaviour == 0)
                        {
                            int p = cursor_line / 20;
                            page = p * 20;
                        }
                        else
                        {
                            if (cursor_line > 19)
                                cursor_line = 19;
                        }
                    }
                    else
                        cursor_line = 0;
                }

                while (!(disp = display_lock()))
                    ;

                new_scroll_pos(&cursor, &page, MAX_LIST, count);

                clearScreen(disp); //part clear?
                display_dir(list, cursor, page, MAX_LIST, count, disp);

                display_show(disp);
            } //not root
            break;

        case mempak_menu:
            while (!(disp = display_lock()))
                ;

            graphics_set_color(graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF), graphics_make_color(0x00, 0x00, 0x00, 0x00));
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp);
            display_show(disp);

            display_dir(list, cursor, page, MAX_LIST, count, disp);
            input_mapping = file_manager;
            display_show(disp);
            break;
        case delete_prompt:

            while (!(disp = display_lock()))
                ;

            graphics_set_color(graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF), graphics_make_color(0x00, 0x00, 0x00, 0x00));
            new_scroll_pos(&cursor, &page, MAX_LIST, count);
            clearScreen(disp);
            display_show(disp);

            display_dir(list, cursor, page, MAX_LIST, count, disp);
            input_mapping = file_manager;
            display_show(disp);
            break;

        case char_input:

            //chr input screen

            /* Lazy switching */
            if (set == 1)
                drawInputAdd(disp, "E"); //P X )
            else if (set == 2)
                drawInputAdd(disp, "L");
            else if (set == 3)
                drawInputAdd(disp, "S");
            else if (set == 4)
                drawInputAdd(disp, "Z");
            break;

        case toplist:

            //leave toplist
            while (!(disp = display_lock()))
                ;

            readSDcard(disp, list_pwd_backup);

            if (scroll_behaviour == 0)
            {
                cursor = list_pos_backup[0];
                page = list_pos_backup[1];
            }
            else
            {
                cursor_line = 0;
            }

            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            display_show(disp);

            input_mapping = file_manager;
            break;

        case mp3:
            mp3_Stop();
            mp3playing = 0;
            audio_close();
            free(buf_ptr);
            buf_ptr = 0;
            audio_init(44100, 8);

          while (!(disp = display_lock()))
              ;

          graphics_set_color(graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF), graphics_make_color(0x00, 0x00, 0x00, 0x00));
          new_scroll_pos(&cursor, &page, MAX_LIST, count);
          clearScreen(disp);
          display_show(disp);

          display_dir(list, cursor, page, MAX_LIST, count, disp);
          input_mapping = file_manager;
          display_show(disp);
            break;

        default:
            while (!(disp = display_lock()))
                ;

            clearScreen(disp); //part clear?
            display_dir(list, cursor, page, MAX_LIST, count, disp);

            display_show(disp);

            //rom info screen
            input_mapping = file_manager;
            break;
        }
    } //key b
}

//entry point
int main(void)
{
    int fast_boot = 0;

    //reserve memory
    list = malloc(sizeof(direntry_t));

    //dfs init for the rom-attached virtual filesystem
    if (dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS)
    {
        // everdrive initial function
        configure();

        //fast boot for backup-save data
        int sj = evd_readReg(REG_CFG); // not sure if this is needed!
        int save_job = evd_readReg(REG_SAV_CFG); //TODO: or the firmware is V3

        if (save_job != 0)
            fast_boot = 1;
        
        //not gamepads more or less the n64 hardware-controllers
        controller_init();

        //filesystem on
        initFilesystem();

        readConfigFile();
        //n64 initialization

        //sd card speed settings from config
        if (sd_speed == 2)
        {
            bi_speed50();
        }
        else
        {
            bi_speed25();
        }

        /*
         * TV Mode from Config and libDragon differ:
         * 
         * Config | libDragon | Mode
         * -------|-----------|-------
         * 0      | None      | Auto
         * 1      | 1         | NTSC
         * 2      | 0         | PAL
         * 3      | 2         | M-PAL
         */
        switch (tv_mode)
        {
            case 1:
                *(u32 *)0x80000300 = 1U; // NTSC
                break;
            case 2:
                *(u32 *)0x80000300 = 0U; // PAL
                break;
            case 3:
                *(u32 *)0x80000300 = 2U; // M-PAL
                break;
            default:
                // Do nothing. Required to make sure no mode other than 0..2 is used.
                // Only there for defensive programming.
                break;
        }

        switch(language)
        {
            default:
                dirempty = "Dir empty...";
                fnddb = "Found in db";
                done = "         Done";
                done3p = "...Done";
                romloaded = "ROM loaded";
                loadgb = "Loading ROM...";
                loading = "      Loading...";
                plgmp3 = "    Playing MP3";
                savemem = "    Save: Off/Mempak";
                save32 = "    Save: SRAM 32";
                save128 = "    Save: SRAM 128";
                save768 = "    Save: SRAM 768";
                save4k = "    Save: EEPROM 4K";
                save16k = "    Save: EEPROM 16K";
                saveflash = "    Save: FLASHRAM";
                OpComsucc = "Operation completed succesfully...";
                mpksub = "Mempak-Subsystem:";
                viewmpk = "  (Z): View content";
                backnew = "  (A): Backup - new";
                formatt = "  (R): Format";
                abortmen = "  (B): Abort";
                restoreback = " L=Restore  R=Backup";
                confreq = "Confirmation required:";
                aresure = "    Are you sure?";
                cupcont = "  (C-UP): Continue ";
                cancelmenu = "    (B): Cancel";
                romconfig = "ROM configuration:";
                updatelastgamerecord = "Updating last played game record...";
                ramarea2sd = "RAM area copied to SD card.";
                cpyingram2SD = "Copying save RAM to SD card...";
                transfersavedat = "Transfering save data...";
                tvforceoff = "      Tv: Force Off";
                cheatoff = "  Cheats: Off";
                cheaton = "  Cheats: On";
                checksumdisable = "Checksum: Disable fix";
                checksumenable = "Checksum: Enable fix";
                presstart = "PRESS START";
                reallyformatrpak = "RumblePak inserted";
                searchfds = "Searching...";
                formating = "Formating...";
                mpkformat = "Mempak-Format:";
                plswait = "Please wait...";
                errorformating = "Error formatting!";
                ratoff = "  Rating: Off";
                ratcomo = "  Rating: Common";
                ratuncomo = "  Rating: Uncommon";
                ratrare = "  Rating: Rare";
                ratepic = "  Rating: Epic";
                ratlegend = "  Rating: Legendary";
                bcancel = "(B): Cancel";
                countrydef = " Country: default";
                countryntsc = " Country: NTSC";
                countrypal = " Country: PAL";
                Asaveconf = "(A): Save config";
                directoriesNO = "Cannot delete directories!";
                bexit = "(B): Exit";
                deletefile = "Delete this file?";
                aconfirm = "(A): Confirm";
                znpage = "(Z): Next page";
                fcontents = "File contents:";
                freebock = "[Free]";
                freespace = "Free space:";
                blocks = "%i blocks";
                emptyfile = "Empty";
                filexist = "File exists";
                overridefile = "Override";
                fileMPK = "File: %s%i.MPK";
                backupdone = "Backup done...";
                sizeMB = "Size: %iMB";
                savetypeinf = "Save: %s";
                mpkbackup = "Mempak-Backup:";
                mpkrestore = "Mempak-Restore:";
                quickback = "Quick-Backup:";
                cheatnofound = "Cheats not found";
                orparsingfailed = "or parsing failed";
                resetconsol = " Reset Console...";
                cheatfound = "    Cheats found...";
                emunofound = "  Emulator not found";
                controlsmenucon = "          - Controls -";
                showmpkmenu = " (L): Show mempak menu";
                aboutscreen = " (Z): About screen";
                Astartromdirectory = "(A): Start ROM/directory";
                Amempak = "         mempak";
                Bbackcancel = " (B): Back/Cancel";
                Startlastrom = "  (START): Start last ROM";
                CLEFT = " (C-Left): ROM info/Mempak";
                CLEFTVIEMPK = "         Content View";
                CRIGHT = " (C-Right): ROM config creen";
                CUP = "   (C-Up): View full filename";
                CDOWN = " (C-Down): Toplist 15";
                LplusR = "  (R) + (L): Delete file";
                errornoready = "ERROR: Not ready.";
                filenoexist = "ERROR: File doesn't exist.";
                pathnoexist = "ERROR: Path doesn't exist.";
                invalidname = "ERROR: Invalid name.";
                operdenied = "ERROR: Operation denied.";
                filealrrexist = "ERROR: File already exists.";
                errtimeout = "ERROR: Timeout.";
                devicelocked = "ERROR: Device locked.";
                readystr = "Ready.";
                cheatsysact = "Cheat system activated...";
                cupactcheat = "(C-UP to activate cheats)";
                crightv = "(C-RIGHT to force menu TV mode)";
                donepresstart = "Done: PRESS START";
                filopenerror = "File open error";
                nosavfund = "No save found.";
                trssavedata = "Transferred save data...";
                errtrssavedata = "Error transfering save data";
                errsavSD = "Error saving game to SD";
                errsavSDnocrfile = "Error saving game to SD, couldn't create file!";
                trloadchfile = "Try to load cheat-file...";
                chsfund = "Cheats found...";
                forcetvmd = "Force TV mode...";
            break;

            case 1:
                dirempty = "Directorio vacio...";
                fnddb = "Encontrado en BD";
                done = "        Hecho";
                done3p = "...Hecho";
                romloaded = "ROM cargada";
                loadgb = "Cargando ROM...";
                loading = "     Cargando...";
                plgmp3 = "  Reproduciendo MP3";
                savemem = "Guardado: Off/Mempak";
                save32 = "Guardado: SRAM 32";
                save128 = "Guardado: SRAM 128";
                save768 = "Guardado: SRAM 768";
                save4k = "Guardado: EEPROM 4K";
                save16k = "Guardado: EEPROM 16K";
                saveflash = "Guardado: FLASHRAM";
                OpComsucc = "Operacion completada exitosamente...";
                mpksub = "Subsistema Mempak:";
                viewmpk = "  (Z): Ver contenido";
                backnew = "  (A): Copiar contenido";
                formatt = "  (R): Formatear";
                abortmen = "  (B): Abortar";
                restoreback = "L=Restaurar  R=Copiar";
                confreq = "Confirmacion requirida:";
                aresure = "     Seguro?";
                cupcont = "(C-ARRIBA): Continuar ";
                cancelmenu = "  (B): Cancelar";
                romconfig = "Ajustes de la ROM:";
                updatelastgamerecord = "Actualizando archivo de guardado...";
                ramarea2sd = "Area de RAM copiada a la SD.";
                cpyingram2SD = "Copiando RAM a la SD...";
                transfersavedat = "Transfiriendo datos...";
                tvforceoff = "      Tv: No forzar";
                cheatoff = "  Trucos: Off";
                cheaton = "  Trucos: On";
                checksumdisable = "Checksum: No arreglar";
                checksumenable = "Checksum: Arreglar";
                presstart = "PULSA START";
                reallyformatrpak = "RumblePak insertado";
                searchfds = "Buscando...";
                formating = "Formateando...";
                mpkformat = "Formatear Mempak:";
                plswait = "Porfavor espera...";
                errorformating = "Error de formateo!";
                ratoff = "  Puntos: Off";
                ratcomo = "  Puntos: Normal";
                ratuncomo = "  Puntos: No normal";
                ratrare = "  Puntos: Raro";
                ratepic = "  Puntos: Epico";
                ratlegend = "  Puntos: Legendario";
                bcancel = "(B): Cancelar";
                countrydef = "    Pais: Por defecto";
                countryntsc = "    Pais: NTSC";
                countrypal = "    Pais: PAL";
                Asaveconf = "(A): Guardar ajustes";
                directoriesNO = "No puedes borrar carpetas!";
                bexit = "(B): Salir";
                deletefile = "Borrar este archivo?";
                aconfirm = "(A): Confirmar";
                znpage = "(Z): Siguiente pagina";
                fcontents = "Contenido:";
                freebock = "[Libre]";
                freespace = "Espacio libre:";
                blocks = "%i bloques";
                emptyfile = "Vacio";
                filexist = "El archivo existe";
                overridefile = "Sobreescribiendo";
                fileMPK = "Archivo: %s%i.MPK";
                backupdone = "Copia hecha...";
                sizeMB = "Peso: %iMB";
                savetypeinf = "Guardado: %s";
                mpkbackup = "Copiar Mempak:";
                mpkrestore = "Restaurar Mempak:";
                quickback = "Copiado rapido:";
                cheatnofound = "Trucos no encontrados";
                orparsingfailed = "o el analisis fue mal";
                resetconsol = " Reinicia la consola...";
                cheatfound = " Trucos encontrados...";
                emunofound = "Emulador no encontrado";
                controlsmenucon = "         - Controles -";
                showmpkmenu = " (L): Ver menu del mempak ";
                aboutscreen = " (Z): Informacion";
                Astartromdirectory = "(A): Iniciar rom/abrir carpeta";
                Amempak = "       mempak";
                Bbackcancel = " (B): Volver/cancelar";
                Startlastrom = " (START): Iniciar la ultima ROM";
                CLEFT = " (C-Izquierda): ROM info/Mempak";
                CLEFTVIEMPK = "         Ver contenido";
                CRIGHT = "(C-Derecha): Ajustes de la ROM";
                CUP = " (C-Arriba): Ver nombre entero";
                CDOWN = " (C-Abajo): Toplist 15";
                LplusR = "  (R) + (L): Borrar archivo";
                errornoready = "ERROR: No listo.";
                filenoexist = "ERROR: El archivo no existe.";
                pathnoexist = "ERROR: El directorio no existe.";
                invalidname = "ERROR: Nombre invalido.";
                operdenied = "ERROR: Operacion denegada.";
                filealrrexist = "ERROR: El archivo ya existe.";
                errtimeout = "ERROR: Tiempo de espera excedido.";
                devicelocked = "ERROR: Dispositivo bloqueado.";
                readystr = "Listo.";
                cheatsysact = "Sistema de trucos activado...";
                cupactcheat = "(C-ARRIBA activar trucos)";
                crightv = "(C-DERECHA forzar modo de TV)";
                donepresstart = "Hecho: PULSA START";
                filopenerror = "Error al abrir el archivo";
                nosavfund = "Partida no encontrada.";
                trssavedata = "Datos transferidos...";
                errtrssavedata = "Error transfiriendo datos";
                errsavSD = "Error guardando en la SD";
                errsavSDnocrfile = "Error, no se puede crear el archivo!";
                trloadchfile = "Intentando cargar los trucos...";
                chsfund = "Trucos encontrados...";
                forcetvmd = "Forzar modo de TV...";
            break;
        }

        init_interrupts();

        if (sound_on)
        {
            //load soundsystem
            audio_init(44100, 8);
            sndInit();
        }

        //timer_init(); no use of timers yet...

        //background
        display_init(res, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);

        //bg buffer
        static display_context_t disp;

        //Grab a render buffer
        while (!(disp = display_lock()))
            ;

        FRESULT fr;
        FILINFO fno;

        //backgrounds from ramfs/libdragonfs

        char background_path[64];
        sprintf(background_path, "/"ED64_FIRMWARE_PATH"/wallpaper/%s", background_image);

        fr = f_stat(background_path, &fno);

        if (fr == FR_OK)
        {
            background = loadPng(background_path);
        }
        else
        {
            background = read_sprite("rom://sprites/background.sprite");
        }

        //todo: if bgm is enabled, we should start it...
        if (sound_on && bgm_on){
        sndPlayBGM("rom://sounds/bgm21.it");
        }
        border_color_1 = translate_color(border_color_1_s);
        border_color_2 = translate_color(border_color_2_s);
        box_color = translate_color(box_color_s);
        selection_color = translate_color(selection_color_s);
        selection_font_color = translate_color(selection_font_color_s);
        list_font_color = translate_color(list_font_color_s);
        list_dir_font_color = translate_color(list_dir_font_color_s);

        while (!(disp = display_lock()))
            ;

        drawBg(disp);           //new
        drawBoxNumber(disp, 1); //new

        uint32_t *buffer = (uint32_t *)__get_buffer(disp); //fg disp = 2

        display_show(disp); //new

        if (fast_boot)
		{
			backupSaveData(disp);
			while (!(disp = display_lock()))
				;
		}

        sprintf(pwd, "%s", "/");
        readSDcard(disp, "/");

        display_show(disp);
        //chr input coord
        x = 30;
        y = 30;

        position = 0;
        set = 1;
        sprintf(input_text, "");

        //sprite for chr input
        int fp = dfs_open("/sprites/n64controller.sprite");
        sprite_t *contr = malloc(dfs_size(fp));
        dfs_read(contr, 1, dfs_size(fp), fp);
        dfs_close(fp);

        //system main-loop with controller inputs-scan
        for ( ;; )
        {
            if (sound_on)
                sndUpdate();

            handleInput(disp, contr);

            if (mp3playing && audio_can_write())
            {
                mp3playing = mp3_Update(buf_ptr, buf_size);

                if (mp3playing)
                {
                    audio_write((short *)buf_ptr);
                }
            }

            if (input_mapping == file_manager)
                sleep(60);

            if (input_mapping == char_input)
            {
                while (!(disp = display_lock()))
                    ;

                graphics_draw_sprite(disp, 0, 0, contr);
                /* Set the text output color */
                graphics_set_color(0x0, 0xFFFFFFFF);

                chr_forecolor = graphics_make_color(0xFF, 0x14, 0x94, 0xFF); //pink
                graphics_set_color(chr_forecolor, chr_backcolor);

                graphics_draw_text(disp, 85, 55, "SETS");
                graphics_draw_text(disp, 94, 70, "1");  //u
                graphics_draw_text(disp, 104, 82, "2"); //r
                graphics_draw_text(disp, 94, 93, "3");  //d
                graphics_draw_text(disp, 82, 82, "4");  //l

                graphics_draw_text(disp, 208, 206, presstart);

                if (set == 1)
                    drawSet1(disp);
                if (set == 2)
                    drawSet2(disp);
                if (set == 3)
                    drawSet3(disp);
                if (set == 4)
                    drawSet4(disp);

                drawTextInput(disp, input_text);

                /* Force backbuffer flip */
                display_show(disp);
            } //mapping 2 chr input drawings

            //sleep(10);
        }
    }
    else
    {
        printf("Filesystem failed to start!\n");
        f_mount(0, "", 0);                     /* Unmount the default drive */
        free(fs);                              /* Here the work area can be discarded */
        for ( ;; )
            ; //never leave!
    }
}
