#include "global.h"
#include "utils.h"
#include "windows.h"

#include "basic_info.h"
#include "songs.h"
#include "directory.h"
#include "playlists.h"
#include "visualizer.h"

static void
dynamic_initial(void)
{
  conn = setup_connection();
  /* initialization require redraw too */
  interval_level = 1;
  quit_signal = 0;

  /* check the screen size */
  if(stdscr->_maxy < 3 || stdscr->_maxx < 68)
	{
	  endwin();
	  fprintf(stderr, "screen too small for normally displaying.\n");
	  exit(1);
	}

  /** BasicInfo initialization **/
  basic_info = basic_info_setup();
  
  /** songlist arguments */
  songlist = songlist_setup();
  songlist_update();
   
  /** directory arguments **/
  directory = directory_setup();
  directory_update();

  /** playlist arguments **/
  playlist = playlist_setup();
  playlist_update();

  /** the visualizer **/
  visualizer = visualizer_setup();
  get_fifo_id();
  
  /** windows set initialization **/
  being_mode_update(&basic_info->wmode);
  //being_mode_update(&songlist->wmode);
}

static
void dynamic_destroy(void)
{
  songlist_free(songlist);
  directory_free(directory);
  playlist_free(playlist);
  visualizer_free(visualizer);
}


int main(int argc, char **args)
{
  // ncurses for unicode support
  setlocale(LC_ALL, "");

  // ncurses basic setting
  initscr();

  timeout(1); // enable the non block getch()
  curs_set(0); // cursor invisible
  noecho();
  keypad(stdscr, TRUE); // enable getch() get KEY_UP/DOWN
  color_init();

  /** windows chain initilization **/
  wchain_init();

  dynamic_initial();
  
  /** main loop for keyboard hit daemon */
  for(;;)
	{
	  being_mode->listen_keyboard();

	  if(quit_signal) break;

	  screen_update_checking();
	  wchain_size_update();
	  screen_redraw();

	  smart_sleep();
	}

  endwin();
  wchain_free();
  dynamic_destroy();

  return 0;
}
