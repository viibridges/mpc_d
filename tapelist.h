#include "global.h"
#include "windows.h"

#ifndef LFKJNCAEFLJPOUzlSKJF
#define LFKJNCAEFLJPOUzlSKJF

struct Tapelist
{
  char tapename[128][512];

  struct WinMode wmode;

  int update_signal;
  
  int length;
  int begin;
  int cursor;
};

struct Tapelist *tapelist;  

void tapelist_redraw_screen(void);
void tapelist_update_checking(void);
void tapelist_update(void);
void tapelist_helper(void);
void tapelist_rename(void);
void tapelist_append(void);

#endif
