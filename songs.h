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

struct Songlist
{
  struct MetaInfo meta[MAX_SONGLIST_STORE_LENGTH];
  int update_signal;

  // search mode parameters
  enum mpd_tag_type tags[4]; // searching type
  char key[128];
  int crt_tag_id;
  int picking_mode; // 1 when picking song

  int total; // total number of song, maybe out of the range
             // that Songlist can manage
  
  int length;
  int begin;
  int cursor;
  int current; // current playing song id

  struct WinMode wmode; // windows in this mode
};

struct Songlist *songlist;

void songlist_simple_bar(void);
void songlist_up_state_bar(void);
void songlist_down_state_bar(void);
void songlist_redraw_screen(void);
void songlist_update(void);
void songlist_update_checking(void);
int get_songlist_cursor_item_index(void);
int is_songlist_selected(void);
void swap_songlist_items(int i, int j);

/** Search Mode Stuffs **/
void turnon_search_mode(void);
void turnoff_search_mode(void);
void searchmode_update(void);
void searchmode_update_checking(void);
void search_prompt(void);

void songlist_clear(void);

#endif
