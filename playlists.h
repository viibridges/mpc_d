#include "global.h"
#include "windows.h"

#ifndef LFKJNCAEFLJPOUzlSKJF
#define LFKJNCAEFLJPOUzlSKJF

struct Playlist
{
  char tapename[128][512];

  struct WinMode wmode;

  int update_signal;
  
  int length;
  int begin;
  int cursor;
};

struct Playlist *playlist;  

void playlist_redraw_screen(void);
void playlist_update_checking(void);
void playlist_update(void);
void playlist_helper(void);
void playlist_rename(void);
void playlist_load(void);
void playlist_save(void);
void playlist_delete(void);

#endif
