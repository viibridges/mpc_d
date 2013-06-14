#include "commands.h"
#include "utils.h"

#include "basic_info.h"
#include "songs.h"
#include "directory.h"
#include "playlists.h"
#include "visualizer.h"

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
  int from = get_songlist_cursor_item_index();
  int to = from + offset;

  // check whether new position valid
  if(to < 0 || to >= songlist->length)
	return;
  
  // now check from
  if(from < 0 || from >= songlist->length)
	return;

  // no problem
  mpd_run_move(conn, songlist->meta[from].id - 1,
			   songlist->meta[to].id - 1);

  // inform them to update the songlist
  songlist->update_signal = 1;

  //swap_songlist_items(from, to);
}

void
song_move_up()
{
  song_in_cursor_move_to(-1);

  // scroll up cursor also
  songlist_scroll_up_line();
}

void
song_move_down()
{
  song_in_cursor_move_to(+1);

  // scroll down cursor also
  songlist_scroll_down_line();
}

void
toggle_select_item(int id)
{
  if(id < 0 || id >= songlist->length)
	return;
  else
	songlist->meta[id].selected ^= 1;
}

void
toggle_select()
{
  int id = get_songlist_cursor_item_index();

  toggle_select_item(id);

  // move cursor forward by one step
  songlist_scroll_down_line();
}

void
songlist_scroll_to(int line)
{
  int height = wchain[SONGLIST].win->_maxy + 1;
  songlist->cursor = 0;
  scroll_line_shift_style(&songlist->cursor, &songlist->begin,
						  songlist->length, height, line);
}

void
songlist_scroll_down_line()
{
  int height = wchain[SONGLIST].win->_maxy + 1;
  scroll_line_shift_style(&songlist->cursor, &songlist->begin,
						  songlist->length, height, +1);
}

void
songlist_scroll_up_line()
{
  int height = wchain[SONGLIST].win->_maxy + 1;
  scroll_line_shift_style(&songlist->cursor, &songlist->begin,
						  songlist->length, height, -1);
}

void
songlist_scroll_up_page()
{
  int height = wchain[SONGLIST].win->_maxy + 1;
  scroll_line_shift_style(&songlist->cursor, &songlist->begin,
						  songlist->length, height, -15);
}

void
songlist_scroll_down_page()
{
  int height = wchain[SONGLIST].win->_maxy + 1;
  scroll_line_shift_style(&songlist->cursor, &songlist->begin,
						  songlist->length, height, +15);
}

void
songlist_play_cursor()
{
  int id = get_songlist_cursor_item_index();
  
  if(id > -1)
	mpd_run_play_pos(conn, songlist->meta[id].id - 1);
}

void
songlist_scroll_to_current()
{
  struct mpd_status *status;
  int song_id;
  status = getStatus(conn);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  songlist_scroll_to(song_id);
}

void
songlist_cursor_hide()
{
  // because the minimum visible position is 1
  songlist->cursor = -1;
}

void
enter_selected_dir()
{
  char *temp =  get_abs_crt_path();
  
  if(is_dir_exist(temp))
	{
	  strcpy(directory->crt_dir, temp);

	  directory->begin = 1;
	  directory->cursor = 1;
	  directory->update_signal = 1;
	}
}

void
exit_current_dir()
{
  char *p;

  p = strrchr(directory->crt_dir, '/');

  // prehibit exiting from root directory
  if(p - directory->crt_dir < (int)strlen(directory->root_dir))
	return;

  if(p && p != directory->crt_dir)
	{
	  *p = '\0';

	  directory->begin = 1;
	  directory->cursor = 1;
	  directory->update_signal = 1;
	}
}

void
append_to_songlist()
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
	mpd_run_add(conn, path);
}

void
directory_scroll_to(int line)
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  directory->cursor = 0;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, line);
}

void
directory_scroll_down_line()
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, +1);
}

void
directory_scroll_up_line()
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, -1);  
}

void
directory_scroll_up_page()
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, -15);  
}

void
directory_scroll_down_page()
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, +15);  
}

void
playlist_scroll_to(int line)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  playlist->cursor = 0;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, line);
}

void
playlist_scroll_down_line()
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, +1);
}

void
playlist_scroll_up_line()
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, -1);
}

void
playlist_scroll_up_page()
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, -15);  
}

void
playlist_scroll_down_page()
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, +15);  
}

void
cmd_Next()
{
  mpd_run_next(conn);

  songlist_scroll_to_current();
}

void
cmd_Prev()
{
  mpd_run_previous(conn);
  
  songlist_scroll_to_current();
}

void
change_searching_scope()
{
  songlist->crt_tag_id ++;
  songlist->crt_tag_id %= 4;	  
}

void
songlist_delete_song_in_cursor()
{
  int i = get_songlist_cursor_item_index();
  
  if(i < 0 || i >= songlist->length)
	return;
	
  mpd_run_delete(conn, songlist->meta[i].id - 1);
}

// if any is deleted then return 1
void
songlist_delete_song_in_batch()
{
  int i;

  // delete in descended order won't screw things up
  for(i = songlist->length - 1; i >= 0; i--)
	if(songlist->meta[i].selected)
	  mpd_run_delete(conn, songlist->meta[i].id - 1);
}

void
songlist_delete_song()
{
  if(is_songlist_selected())
	songlist_delete_song_in_batch();
  else
	songlist_delete_song_in_cursor();

  /* easest way to realign the songlist */
  songlist_update();
  songlist_scroll_to(songlist->cursor);
}

void
switch_to_main_menu()
{
  clean_screen();

  being_mode_update(&basic_info->wmode);
}

void
switch_to_songlist_menu()
{
  clean_screen();

  being_mode_update(&songlist->wmode);
}

void
switch_to_directory_menu()
{
  clean_screen();

  being_mode_update(&directory->wmode);
}

void
switch_to_playlist_menu()
{
  clean_screen();

  being_mode_update(&playlist->wmode);
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

