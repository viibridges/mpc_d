#include "global.h"

void cmd_seek_second(int perc);
void cmd_forward(void);
void cmd_backward(void);
int cmd_volup(void);
int cmd_voldown(void);
void cmd_Repeat(void);
void cmd_Single(void);
void cmd_Toggle(void);
void cmd_Playback(void);
void cmd_Random(void);
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
void songlist_play_current(void);
void songlist_scroll_to_current(void);
void songlist_cursor_hide(void);
void enter_selected_dir(void);
void exit_current_dir(void);
void append_to_songlist(void);
void dirlist_scroll_to(int line);
void dirlist_scroll_down_line(void);
void dirlist_scroll_up_line(void);
void dirlist_scroll_up_page(void);
void dirlist_scroll_down_page(void);
void tapelist_scroll_to(int line);
void tapelist_scroll_down_line(void);
void tapelist_scroll_up_line(void);
void tapelist_scroll_up_page(void);
void tapelist_scroll_down_page(void);
void cmd_Next(void);
void cmd_Prev(void);
void change_searching_scope(void);
void songlist_delete_song_in_cursor(void);
void songlist_delete_song_in_batch(void);
void songlist_delete_song(void);
void switch_to_main_menu(void);
void switch_to_songlist_menu(void);
void switch_to_dirlist_menu(void);
void switch_to_tapelist_menu(void);
void toggle_visualizer(void); 
void turnon_search_mode(void);
void turnoff_search_mode(void);
