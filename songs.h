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

  int search_mode; // 1 for on
  
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

struct Songlist* songlist_setup(void);
void songlist_free(struct Songlist *slist);

// commands
void song_in_cursor_move_to(int offset);
void song_move_up(void);
void song_move_down(void);
void toggle_select_item(int id);
void toggle_select(void);
void songlist_scroll_to(int line);
void songlist_scroll_down_line(void);
void songlist_scroll_up_line(void);
void songlist_scroll_up_page(void);
void songlist_scroll_down_page(void);
void songlist_play_cursor(void);
void songlist_scroll_to_current(void);
void songlist_cursor_hide(void);
void change_searching_scope(void);
void songlist_delete_song_in_cursor(void);
void songlist_delete_song_in_batch(void);
void songlist_delete_song(void);

#endif
