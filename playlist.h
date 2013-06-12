#include "global.h"
#include "windows.h"

#ifndef LKAJDSFOIAJFNC98I93
#define LKAJDSFOIAJFNC98I93

struct MetaInfo
{
  char album[128];
  char artist[128];
  char title[512];
  char pretty_title[128];
  
  int id;
  int selected;
};

struct Playlist
{
  struct MetaInfo meta[MAX_PLAYLIST_STORE_LENGTH];
  int update_signal;

  int length;
  int begin;
  int cursor;
  int current; // current playing song id

  struct WinMode wmode; // windows in this mode
};

struct Playlist *playlist;

void playlist_simple_bar(void);
void playlist_up_state_bar(void);
void playlist_down_state_bar(void);
void playlist_redraw_screen(void);
void playlist_update(void);
void playlist_update_checking(void);
int get_playlist_cursor_item_index(void);
int is_playlist_selected(void);
void swap_playlist_items(int i, int j);

#endif
