/**
 * #FUJINET CONFIG
 * Diskulator Select Disk image screen
 */

#include <atari.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "diskulator_select.h"
#include "screen.h"
#include "fuji_sio.h"
#include "die.h"
#include "bar.h"
#include "input.h"
#include "error.h"

typedef enum _substate
  {
   SELECT_FILE,
   PREV_PAGE,
   NEXT_PAGE,
   ADVANCE_DIR,
   DEVANCE_DIR,
   DONE
  } SubState;

typedef unsigned short Page;
typedef unsigned char PageOffset;

extern char text_empty[];

#define DIRECTORY_LIST_Y_OFFSET 3
#define DIRECTORY_LIST_SCREEN_WIDTH 36
#define DIRECTORY_LIST_FULL_WIDTH 128
#define DIRECTORY_LIST_ENTRIES_PER_PAGE 14
#define DIRECTORY_LIST_SHOW_FULL_FILENAME_DELAY 24

/**
 * Clear full filename area
 */
void diskulator_select_clear_file_area(void)
{
  screen_clear_line(18);
  screen_clear_line(19);
  screen_clear_line(20);
}

/**
 * Display directory path
 */
void diskulator_select_display_directory_path(Context* context)
{
  screen_clear_line(1);
  screen_puts(0,1,context->directory);
}

/**
 * Display directory entry
 */
bool diskulator_select_display_directory_entry(unsigned char i, char* entry, Context *context)
{
  if (entry[0]==0x7F)
    {
      context->dir_eof=true;
      return false; // End of directory
    }

  context->dir_eof=false;
  
  // Display filename
  screen_puts(2,DIRECTORY_LIST_Y_OFFSET+i,entry);

  // Display folder icon if directory.
  if (entry[strlen(entry)-1]=='/')
    screen_puts(0,DIRECTORY_LIST_Y_OFFSET+i,"\x04");

  return true;
}

/**
 * Clear directory page area
 */
void diskulator_select_display_clear_page(void)
{
  unsigned char i;

  screen_dlist_diskulator_select();
  
  for (i=2;i<20;i++)
    screen_clear_line(i);
}

/**
 * Display previous page
 */
void diskulator_select_display_prev_page(void)
{
  screen_puts(0,DIRECTORY_LIST_Y_OFFSET-1,"\xd9\x9C\x19");
  screen_puts(3,DIRECTORY_LIST_Y_OFFSET-1,"Previous Page");
}

/**
 * Display previous page
 */
void diskulator_select_display_next_page(void)
{
  screen_puts(0,DIRECTORY_LIST_Y_OFFSET+DIRECTORY_LIST_ENTRIES_PER_PAGE,"\xd9\x9E\x19");
  screen_puts(3,DIRECTORY_LIST_Y_OFFSET+DIRECTORY_LIST_ENTRIES_PER_PAGE,"Next Page");
}

/**
 * Display current filter
 */
void diskulator_select_display_filter(Context *context)
{
  char filter_str[40]="Filter: ";

  strcat(filter_str,context->filter);
  screen_puts(14,DIRECTORY_LIST_Y_OFFSET-1,filter_str);
}

/**
 * Display directory page
 */
void diskulator_select_display_directory_page(Context* context)
{
  char displayed_entry[DIRECTORY_LIST_SCREEN_WIDTH];
  unsigned char i;

  bar_clear();

  diskulator_select_display_clear_page();

  if (context->filter[0]!=0x00)
    {
      diskulator_select_display_filter(context);
      memset(context->directory_plus_filter,0,sizeof(context->directory_plus_filter));
      strcpy(context->directory_plus_filter,context->directory);
      strcpy(&context->directory_plus_filter[strlen(context->directory_plus_filter)+1],context->filter);
      fuji_sio_open_directory(context->host_slot,context->directory_plus_filter);
    }
  else
    fuji_sio_open_directory(context->host_slot,context->directory);
  
  if (fuji_sio_error())
    error_fatal(ERROR_OPENING_DIRECTORY);
  
  fuji_sio_set_directory_position(context->dir_page*DIRECTORY_LIST_ENTRIES_PER_PAGE);
  if (fuji_sio_error())
    error_fatal(ERROR_SETTING_DIRECTORY_POSITION);
  
  for (i=0;i<DIRECTORY_LIST_ENTRIES_PER_PAGE;i++)
    {
      fuji_sio_read_directory(displayed_entry,DIRECTORY_LIST_SCREEN_WIDTH);
      if (fuji_sio_error())
	error_fatal(ERROR_READING_DIRECTORY);

      context->entry_widths[i]=strlen(displayed_entry);

      if (!diskulator_select_display_directory_entry(i,displayed_entry,context))
	break;
    }
    
  fuji_sio_close_directory(context->host_slot);
  context->entries_displayed=i;

  if (context->dir_page > 0)
    diskulator_select_display_prev_page();

  if (context->dir_eof == false)
    diskulator_select_display_next_page();

  if (i==0)
    {
      diskulator_select_display_directory_entry(i,text_empty,context);
    }
}

/**
 * Handle RETURN key - Select item.
 */
void diskulator_select_handle_return(unsigned char i, Context* context, SubState *ss)
{
  if (context->entries_displayed==0)
    return;
  
  if (context->filter[0]==0x00)
    fuji_sio_open_directory(context->host_slot,context->directory);
  else
    fuji_sio_open_directory(context->host_slot,context->directory_plus_filter);
  
  if (fuji_sio_error())
    error_fatal(ERROR_OPENING_DIRECTORY);
  
  fuji_sio_set_directory_position((context->dir_page*DIRECTORY_LIST_ENTRIES_PER_PAGE)+i);
  if (fuji_sio_error())
    error_fatal(ERROR_SETTING_DIRECTORY_POSITION);

  fuji_sio_read_directory(context->filename,DIRECTORY_LIST_FULL_WIDTH);
  if (fuji_sio_error())
    error_fatal(ERROR_READING_DIRECTORY);

  fuji_sio_close_directory(context->host_slot);
  
  // Handle if this is a directory
  if (context->filename[strlen(context->filename)-1]=='/')
    {
      strcat(context->directory,context->filename);
      memset(context->filename,0,sizeof(context->filename));
      diskulator_select_display_directory_path(context);
      *ss=ADVANCE_DIR; // Stay here, go to the new directory.
    }
  else
    {
      *ss=DONE; // We are done with the select screen.
      context->state=DISKULATOR_SLOT;
    }
}

/**
 * Handle page nav
 */
void diskulator_select_handle_page_nav(unsigned char k, unsigned char i, Context *context, SubState *ss)
{
  if (context->entries_displayed==0)
    return;
  
  switch(k)
    {
    case '-':
    case 0x1C:
      if (i==0 && context->dir_page > 0)
	*ss=PREV_PAGE;
      break;
    case '=':
    case 0x1D:
      if (i==DIRECTORY_LIST_ENTRIES_PER_PAGE-1 && context->dir_eof == false)
	*ss=NEXT_PAGE;
      break;
    }
}

/**
 * Devance (move up) directory
 */
void diskulator_select_devance_directory(Context* context)
{
  unsigned char i=strlen(context->directory)-2; // skip over the last '/'

  while (context->directory[i--]!='/')
    {
      // Skip over it.
    }

  // i is now at next /, truncate it.
  i++;

  while (i<255)
    context->directory[++i]=0;
}

/**
 * New Disk
 */
void diskulator_select_new_disk(Context* context, SubState* ss)
{
  char tmp_str[8];
  
  screen_clear_line(20);
  screen_clear_line(21);
  screen_dlist_diskulator_select_aux();
  
  screen_puts(1,20,"Enter name of new disk image file");
  screen_input(0,21,context->filename);

  if (context->filename[0]==0x00)
    {
      *ss=ADVANCE_DIR;
      diskulator_select_setup(context); // Reset screen.
      screen_clear_line(20);
      return;
    }

  screen_clear_line(20);
  screen_clear_line(21);
  
  screen_puts(0, 20, "Size?\xD9\x91\x19"
	      "90K  \xD9\x92\x19"
	      "130K  \xD9\x93\x19"
	      "180K  \xD9\x94\x19"
	      "360K  ");
  screen_puts(0, 21, "     \xD9\x95\x19"
	      "720K \xD9\x96\x19"
	      "1440K \xD9\x97\x19"
	      "Custom");

  memset(tmp_str,0,sizeof(tmp_str));
  screen_input(32,21,tmp_str);

  switch(tmp_str[0])
    {
    case '1':
      context->newDisk_ns=720;
      context->newDisk_sz=128;
      break;
    case '2':
      context->newDisk_ns=1040;
      context->newDisk_sz=128;
      break;
    case '3':
      context->newDisk_ns=720;
      context->newDisk_sz=256;
      break;
    case '4':
      context->newDisk_ns=1440;
      context->newDisk_sz=256;
      break;
    case '5':
      context->newDisk_ns=2880;
      context->newDisk_sz=256;
      break;
    case '6':
      context->newDisk_ns=5760;
      context->newDisk_sz=256;
      break;
    case '7':
      screen_clear_line(20);
      screen_clear_line(21);
      
      memset(tmp_str,0,sizeof(tmp_str));
      screen_puts(0,20,"# Sectors?");
      screen_input(12,20,tmp_str);
      context->newDisk_ns=atoi(tmp_str);

      memset(tmp_str,0,sizeof(tmp_str));
      screen_puts(0,21,"Sector Size (128/256)?");
      screen_input(24,21,tmp_str);
      context->newDisk_sz=atoi(tmp_str);
      break;
    }

  memset(tmp_str,0,sizeof(tmp_str));
  screen_clear_line(20);
  screen_clear_line(21);
  screen_puts(0,20,"Are you sure (Y/N)?");
  screen_input(21,20,tmp_str);

  if (tmp_str[0]=='Y' || tmp_str[0]=='y')
    {
      *ss=DONE;
      context->state=DISKULATOR_SLOT;
      context->newDisk=true;
    }
}

/**
 * Set filter
 */
void diskulator_select_set_filter(Context *context, SubState *ss)
{
  diskulator_select_display_filter(context);
  screen_input(21,DIRECTORY_LIST_Y_OFFSET-1,context->filter);
  *ss=ADVANCE_DIR;
}

/**
 * Show full filename
 */
void diskulator_select_show_full_filename(Context *context, unsigned char i)
{
  if (context->filter[0]!=0x00)
    fuji_sio_open_directory(context->host_slot,context->directory_plus_filter);
  else
    fuji_sio_open_directory(context->host_slot,context->directory);

  if (fuji_sio_error())
    error_fatal(ERROR_OPENING_DIRECTORY);

  fuji_sio_set_directory_position((context->dir_page*DIRECTORY_LIST_ENTRIES_PER_PAGE)+i);
  if (fuji_sio_error())
    error_fatal(ERROR_SETTING_DIRECTORY_POSITION);

  fuji_sio_read_directory(context->filename,DIRECTORY_LIST_FULL_WIDTH);
  if (fuji_sio_error())
    error_fatal(ERROR_READING_DIRECTORY);

  fuji_sio_close_directory(context->host_slot);

  screen_clear_line(19);
  screen_clear_line(20);
  screen_clear_line(21);
  
  screen_puts(0,19,context->filename);
}

/**
 * Select file
 */
void diskulator_select_select_file(Context* context, SubState* ss)
{
  unsigned char k=0;  // Key to process
  unsigned char i=0;  // cursor on page
  bool long_filename_displayed=false;
  
  diskulator_select_display_directory_page(context);

  bar_show(DIRECTORY_LIST_Y_OFFSET+1);
  
  while (*ss==SELECT_FILE)
    {
      k=input_handle_key(); 
      diskulator_select_handle_page_nav(k,i,context,ss);
      
      if (context->entries_displayed>0)
	input_handle_nav_keys(k,DIRECTORY_LIST_Y_OFFSET+1,context->entries_displayed,&i);

      if (input_handle_console_keys() == 0x03)
	{
	  *ss=DONE;
	  context->state = MOUNT_AND_BOOT;
	}

      // Clear file area if we move cursor
      if (k>0)
	diskulator_select_clear_file_area();
      
      // See if we need to display a long filename
      if ((context->entry_widths[i]>DIRECTORY_LIST_SCREEN_WIDTH-2) &&
	  (OS.rtclok[2]>DIRECTORY_LIST_SHOW_FULL_FILENAME_DELAY) &&
	  (long_filename_displayed==false))
	{
	  diskulator_select_clear_file_area();
	  diskulator_select_show_full_filename(context,i);
	  long_filename_displayed=true;
	}
      
      switch(k)
	{
	case '=':
	case '-':
	case 0x1C:
	case 0x1D:
	  long_filename_displayed=false;
	  break;
	case 0x9B:
	  diskulator_select_handle_return(i,context,ss);
	  break;
	case '<':
	case 0x43: // PgUp
	  if (context->dir_page > 0)
	    *ss=PREV_PAGE;
	  break;
	case '>': 
	case 0x44: // PgDn
	  if (!context->dir_eof)
	    *ss=NEXT_PAGE;
	  break;
	case 0x1B:
	  *ss=DONE;
	  context->dir_page=0;
	  context->state=DISKULATOR_HOSTS;
	  break;
	case 0x7E:
	  *ss=DEVANCE_DIR;
	  context->dir_page=0;
	  break;
	case 'N':
	case 'n':
	  diskulator_select_new_disk(context,ss);
	  break;
	case 'F':
	case 'f':
	  diskulator_select_set_filter(context,ss);
	  break;
	}
    }
}
  
/**
 * Setup Diskulator Disk Images screen
 */
void diskulator_select_setup(Context *context)
{
  screen_dlist_diskulator_select();

  memset(context->filename,0,sizeof(context->filename));

  if (context->directory[0]==0x00)
    strcpy(context->directory,"/");

  context->newDisk = false;
  
  screen_puts(4, 0, "DISK IMAGES");

  screen_puts(0,22,"" "\xd9" "\xAE" "\x19" "ew" "\xd9" "\xA6" "\x19" "ilter" "\xd9" "\xA4\xA5\xAC\xA5\xB4\xA5" "\x19" "Up Dir" "\xd9" "\xAF\xB0\xB4\xA9\xAF\xAE" "\x19" "Boot");
  diskulator_select_display_directory_path(context);
}

/**
 * Diskulator select disk image
 */
State diskulator_select(Context *context)
{
  SubState ss=SELECT_FILE;

  diskulator_select_setup(context);

  while (ss != DONE)
    {      
      switch(ss)
	{
	case SELECT_FILE:
	  diskulator_select_select_file(context,&ss);
	  break;
	case PREV_PAGE:
	  context->dir_page--;
	  ss=SELECT_FILE;
	  break;
	case NEXT_PAGE:
	  context->dir_page++;
	  ss=SELECT_FILE;
	  break;
	case ADVANCE_DIR:
	  context->dir_page=0;
	  context->dir_eof=false;
	  ss=SELECT_FILE;
	  break;
	case DEVANCE_DIR:
	  diskulator_select_devance_directory(context);
	  diskulator_select_display_directory_path(context);
	  context->dir_page=0;
	  context->dir_eof=false;
	  ss=SELECT_FILE;
	  break;
	}
    }
  
  return context->state;
}
