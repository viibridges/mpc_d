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
cmd_forward(void)
{
  cmd_seek_second(SEEK_UNIT);
}

void
cmd_backward(void)
{
  cmd_seek_second(-SEEK_UNIT);
}

int
cmd_volup(void)
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
cmd_voldown(void)
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
cmd_repeat(void)
{
  struct mpd_status *status;

  status = getStatus(conn);
  int mode = !mpd_status_get_repeat(status);
  mpd_status_free(status);

  if(!mpd_run_repeat(conn, mode))
	printErrorAndExit(conn);
}

void
cmd_single(void)
{
  struct mpd_status *status;
  
  status = getStatus(conn);  
  if(!mpd_status_get_repeat(status))
	cmd_repeat();
  
  int mode = !mpd_status_get_single(status);

  mpd_status_free(status);

  if (!mpd_run_single(conn, mode))
	printErrorAndExit(conn);

}

void
cmd_toggle(void)
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
cmd_playback(void)
{
  struct mpd_status *status;
	
  status = getStatus(conn);

  int song =  mpd_status_get_song_pos(status);

  mpd_status_free(status);

  if(song > -1)
	mpd_run_play_pos(conn, song);
}

void
cmd_random(void)
{
  struct mpd_status *status;
  
  status = getStatus(conn);
  int mode = !mpd_status_get_random(status);
  mpd_status_free(status);

  if (!mpd_run_random(conn, mode))
	printErrorAndExit(conn);

}

void
cmd_next(void)
{
  mpd_run_next(conn);

  songlist_scroll_to_current();
}

void
cmd_prev(void)
{
  mpd_run_previous(conn);
  
  songlist_scroll_to_current();
}

void
switch_to_main_menu(void)
{
  clean_screen();

  being_mode_update(&basic_info->wmode);
}

void
switch_to_songlist_menu(void)
{
  clean_screen();

  being_mode_update(&songlist->wmode);
}

void
switch_to_directory_menu(void)
{
  clean_screen();

  being_mode_update(&directory->wmode);
}

void
switch_to_playlist_menu(void)
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

