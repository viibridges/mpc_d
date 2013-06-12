#include "commands.h"
#include "windows.h"
#include "utils.h"

#include "basic_info.h"
#include "playlist.h"
#include "searchlist.h"
#include "dirlist.h"
#include "tapelist.h"
#include "visualizer.h"

extern struct mpd_connection *conn;
extern struct BasicInfo *basic_info;    
extern struct Playlist *playlist;
extern struct Searchlist *searchlist;
extern struct Dirlist *dirlist;  
extern struct Tapelist *tapelist;  

void
cmd_seek_second(int perc)
{
  struct mpd_status *status;
  int crt_time, total_time, id;
  
  status = getStatus(conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  id = mpd_status_get_song_id(status);
  mpd_status_free(status);

  int seekto = crt_time + total_time * perc / 100;

  // boundary check
  if(seekto < 0)
	seekto = 0;
  else if(seekto > (int)total_time)
	seekto = total_time;

  mpd_run_seek_id(conn, id, seekto);
}

void
cmd_forward()
{
  cmd_seek_second(SEEK_UNIT);
}

void
cmd_backward()
{
  cmd_seek_second(-SEEK_UNIT);
}

int
cmd_volup()
{
  struct mpd_status *status;
  int volume;

  status = getStatus(conn);
  volume = mpd_status_get_volume(status);
  mpd_status_free(status);

  volume += VOLUME_UNIT;
  if(volume > 100)
	volume = 100;

  if (!mpd_run_set_volume(conn, volume))
	printErrorAndExit(conn);

  return 1;
}
  
int
cmd_voldown()
{
  struct mpd_status *status;
  int volume;

  status = getStatus(conn);
  volume = mpd_status_get_volume(status);
  mpd_status_free(status);

  volume -= VOLUME_UNIT;
  if(volume < 0)
	volume = 0;

  if (!mpd_run_set_volume(conn, volume))
	printErrorAndExit(conn);

  my_finishCommand(conn);
  
  return 1;
}

void
cmd_Repeat()
{
  struct mpd_status *status;

  status = getStatus(conn);
  int mode = !mpd_status_get_repeat(status);
  mpd_status_free(status);

  if(!mpd_run_repeat(conn, mode))
	printErrorAndExit(conn);
}

/* different from the old cmd_single, this new function
   turn on the Repeat command simultaneously */
void
cmd_Single()
{
  struct mpd_status *status;
  
  status = getStatus(conn);  
  if(!mpd_status_get_repeat(status))
	cmd_Repeat();
  
  int mode = !mpd_status_get_single(status);

  mpd_status_free(status);

  if (!mpd_run_single(conn, mode))
	printErrorAndExit(conn);

}

void
cmd_Toggle()
{
	struct mpd_status *status;
	
	status = getStatus(conn);

	if(mpd_status_get_state(status) == MPD_STATE_PLAY)
	  mpd_run_pause(conn, true);
	else
	  mpd_run_play(conn);

	mpd_status_free(status);
}

void
cmd_Playback()
{
  struct mpd_status *status;
	
  status = getStatus(conn);

  int song =  mpd_status_get_song_pos(status);

  mpd_status_free(status);

  if(song > -1)
	mpd_run_play_pos(conn, song);
}

void
cmd_Random()
{
  struct mpd_status *status;
  
  status = getStatus(conn);
  int mode = !mpd_status_get_random(status);
  mpd_status_free(status);

  if (!mpd_run_random(conn, mode))
	printErrorAndExit(conn);

}

// current cursor song move up
void
song_in_cursor_move_to(int offset)
{
  int from = get_playlist_cursor_item_index();
  int to = from + offset;

  // check whether new position valid
  if(to < 0 || to >= playlist->length)
	return;
  
  // now check from
  if(from < 0 || from >= playlist->length)
	return;

  // no problem
  mpd_run_move(conn, playlist->meta[from].id - 1,
			   playlist->meta[to].id - 1);

  // inform them to update the playlist
  //playlist->update_signal = 1;

  swap_playlist_items(from, to);
}

void
song_move_up()
{
  song_in_cursor_move_to(-1);

  // scroll up cursor also
  playlist_scroll_up_line();
}

void
song_move_down()
{
  song_in_cursor_move_to(+1);

  // scroll down cursor also
  playlist_scroll_down_line();
}

void
toggle_select_item(int id)
{
  if(id < 0 || id >= playlist->length)
	return;
  else
	playlist->meta[id].selected ^= 1;
}

void
toggle_select()
{
  int id = get_playlist_cursor_item_index();

  toggle_select_item(id);

  // move cursor forward by one step
  playlist_scroll_down_line();
}

void
playlist_scroll(int lines)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  // mid_pos is the relative position of cursor in the screen
  const int mid_pos = height / 2 - 1;

  playlist->cursor += lines;
	
  if(playlist->cursor > playlist->length)
	playlist->cursor = playlist->length;
  else if(playlist->cursor < 1)
	playlist->cursor = 1;

  playlist->begin = playlist->cursor - mid_pos;

  /* as mid_pos maight have round-off error, the right hand side
   * playlist_height - mid_pos compensates it.
   * always keep:
   *		begin = cursor - mid_pos    (previous instruction)
   * while:
   *		length - cursor = height - mid_pos - 1 (condition)
   *		begin = length - height + 1            (body)
   *
   * Basic Math :) */
  if(playlist->length - playlist->cursor < height - mid_pos)
	playlist->begin = playlist->length - height + 1;

  if(playlist->cursor < mid_pos)
	playlist->begin = 1;

  // this expression should always be false
  if(playlist->begin < 1)
	playlist->begin = 1;
}

void
playlist_scroll_to(int line)
{
  playlist->cursor = 0;
  playlist_scroll(line);
}

void
playlist_scroll_down_line()
{
  playlist_scroll(+1);
}

void
playlist_scroll_up_line()
{
  playlist_scroll(-1);
}

void
playlist_scroll_up_page()
{
  playlist_scroll(-15);
}

void
playlist_scroll_down_page()
{
  playlist_scroll(+15);
}

void
playlist_play_current()
{
  int song = get_playlist_cursor_item_index();
  
  if(song > -1)
	mpd_run_play_pos(conn, song);
}

void
playlist_scroll_to_current()
{
  struct mpd_status *status;
  int song_id;
  status = getStatus(conn);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  playlist_scroll_to(song_id);
}

void
playlist_cursor_hide()
{
  // because the minimum visible position is 1
  playlist->cursor = -1;
}

void
enter_selected_dir()
{
  char *temp =  get_abs_crt_path();
  
  if(is_dir_exist(temp))
	{
	  strcpy(dirlist->crt_dir, temp);

	  dirlist->begin = 1;
	  dirlist->cursor = 1;
	  dirlist->update_signal = 1;
	}
}

void
exit_current_dir()
{
  char *p;

  p = strrchr(dirlist->crt_dir, '/');

  // prehibit exiting from root directory
  if(p - dirlist->crt_dir < (int)strlen(dirlist->root_dir))
	return;

  if(p && p != dirlist->crt_dir)
	{
	  *p = '\0';

	  dirlist->begin = 1;
	  dirlist->cursor = 1;
	  dirlist->update_signal = 1;
	}
}

void
append_to_playlist()
{
  char *path, *absp;

  /* check if path exist */
  absp = get_abs_crt_path();
  if(!is_path_exist(absp))
	return;

  /* check if a nondir file of valid format */
  if(!is_dir_exist(absp) && !is_path_valid_format(absp))
	return;
  
  path = get_mpd_crt_path();

  if(path)
	append_target(path);
}

void
append_target(char *path)
{
  if (!mpd_command_list_begin(conn, false))
	printErrorAndExit(conn);

  mpd_run_add(conn, path);

  if (!mpd_command_list_end(conn))
	printErrorAndExit(conn);

  if (!mpd_response_finish(conn))
	printErrorAndExit(conn);
}

/* this funciton cloned from playlist_scroll */
void
dirlist_scroll(int lines)
{
  int height = wchain[DIRLIST].win->_maxy + 1;
  // mid_pos is the relative position of cursor in the screen
  const int mid_pos = height / 2 - 1;

  dirlist->cursor += lines;
	
  if(dirlist->cursor > dirlist->length)
	dirlist->cursor = dirlist->length;
  else if(dirlist->cursor < 1)
	dirlist->cursor = 1;

  dirlist->begin = dirlist->cursor - mid_pos;

  if(dirlist->length - dirlist->cursor < height - mid_pos)
	dirlist->begin = dirlist->length - height + 1;

  if(dirlist->cursor < mid_pos)
	dirlist->begin = 1;

  // this expression should always be false
  if(dirlist->begin < 1)
	dirlist->begin = 1;
}

void
dirlist_scroll_to(int line)
{
  dirlist->cursor = 0;
  dirlist_scroll(line);
}

void
dirlist_scroll_down_line()
{
  dirlist_scroll(+1);
}

void
dirlist_scroll_up_line()
{
  dirlist_scroll(-1);
}

void
dirlist_scroll_up_page()
{
  dirlist_scroll(-15);
}

void
dirlist_scroll_down_page()
{
  dirlist_scroll(+15);
}

/* this funciton cloned from playlist_scroll */
void
tapelist_scroll(int lines)
{
  int height = wchain[TAPELIST].win->_maxy + 1;
  // mid_pos is the relative position of cursor in the screen
  const int mid_pos = height / 2 - 1;

  tapelist->cursor += lines;
	
  if(tapelist->cursor > tapelist->length)
	tapelist->cursor = tapelist->length;
  else if(tapelist->cursor < 1)
	tapelist->cursor = 1;

  tapelist->begin = tapelist->cursor - mid_pos;

  if(tapelist->length - tapelist->cursor < height - mid_pos)
	tapelist->begin = tapelist->length - height + 1;

  if(tapelist->cursor < mid_pos)
	tapelist->begin = 1;

  // this expression should always be false
  if(tapelist->begin < 1)
	tapelist->begin = 1;
}

void
tapelist_scroll_to(int line)
{
  tapelist->cursor = 0;
  tapelist_scroll(line);
}

void
tapelist_scroll_down_line()
{
  tapelist_scroll(+1);
}

void
tapelist_scroll_up_line()
{
  tapelist_scroll(-1);
}

void
tapelist_scroll_up_page()
{
  tapelist_scroll(-15);
}

void
tapelist_scroll_down_page()
{
  tapelist_scroll(+15);
}

void
cmd_Next()
{
  mpd_run_next(conn);

  playlist_scroll_to_current();
}

void
cmd_Prev()
{
  mpd_run_previous(conn);
  
  playlist_scroll_to_current();
}

void
change_searching_scope()
{
  searchlist->crt_tag_id ++;
  searchlist->crt_tag_id %= 4;	  
}

void
playlist_delete_song_in_cursor()
{
  int i = get_playlist_cursor_item_index();
  
  if(i < 0 || i >= playlist->length)
	return;
	
  mpd_run_delete(conn, playlist->meta[i].id - 1);
}

// if any is deleted then return 1
void
playlist_delete_song_in_batch()
{
  int i;

  // delete in descended order won't screw things up
  for(i = playlist->length - 1; i >= 0; i--)
	if(playlist->meta[i].selected)
	  mpd_run_delete(conn, playlist->meta[i].id - 1);
}

void
playlist_delete_song()
{
  if(is_playlist_selected())
	playlist_delete_song_in_batch();
  else
	playlist_delete_song_in_cursor();
}

void
switch_to_main_menu()
{
  clean_screen();

  winset_update(&basic_info->wmode);
}

void
switch_to_playlist_menu()
{
  clean_screen();

  winset_update(&playlist->wmode);
}

void
switch_to_dirlist_menu()
{
  clean_screen();

  winset_update(&dirlist->wmode);
}

void
switch_to_tapelist_menu()
{
  clean_screen();

  winset_update(&tapelist->wmode);
}

void
toggle_visualizer(void)
{
  if(wchain[VISUALIZER].visible)
	{
	  clean_window(VISUALIZER);
	  wchain[VISUALIZER].visible = 0;
	}
  else
	wchain[VISUALIZER].visible = 1;
}

void
turnon_search_mode(void)
{
  searchlist->key[0] = '\0';
  searchlist->picking_mode = 0;
  //searchlist->wmode.listen_keyboard = &searchlist_keymap;
  playlist_cursor_hide();

  clean_window(VERBOSE_PROC_BAR);
  clean_window(VISUALIZER);  
  winset_update(&searchlist->wmode);
}

void
turnoff_search_mode(void)
{
  searchlist->key[0] = '\0';
  playlist_update();
  playlist_scroll_to_current();

  clean_window(SEARCH_INPUT);
  winset_update(&playlist->wmode);
}
