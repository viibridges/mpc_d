#include "global.h"
#include "windows.h"

#ifndef LKAJSDOIFNAOCW9O8IFJ
#define LKAJSDOIFNAOCW9O8IFJ

struct Dirlist
{
  char root_dir[128];
  char crt_dir[512];
  char filename[MAX_SONGLIST_STORE_LENGTH][512]; // all items in current dir
  char prettyname[MAX_SONGLIST_STORE_LENGTH][128]; // all items in current dir

  struct WinMode wmode; // windows in this mode

  int update_signal;

  int length;
  int begin;
  int cursor;
};

struct Dirlist *dirlist;

char* get_abs_crt_path(void);
int is_path_valid_format(char *path);
char* get_mpd_crt_path(void);
void dirlist_redraw_screen(void);
void dirlist_helper(void);
void dirlist_update(void);
void dirlist_update_checking(void);

#endif
