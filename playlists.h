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
void playlist_cover(void);
void playlist_delete(void);
void playlist_replace(void);

struct Playlist *playlist_setup(void);
void playlist_free(struct Playlist *plist);

// list manipulation commands
void playlist_scroll_to(int line);
void playlist_scroll_down_line(void);
void playlist_scroll_up_line(void);
void playlist_scroll_up_page(void);
void playlist_scroll_down_page(void);

#endif
