#include "initial.h"
#include "utils.h"
#include "windows.h"
#include "global.h"
#include "basic_info.h"
#include "playlist.h"
#include "searchlist.h"
#include "dirlist.h"
#include "tapelist.h"
#include "visualizer.h"

struct mpd_connection *conn;
struct BasicInfo *basic_info;    
struct Playlist *playlist;
struct Searchlist *searchlist;
struct Dirlist *dirlist;  
struct Tapelist *tapelist;  
struct Visualizer *visualizer;

int
main(int argc, char **args)
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

	  being_mode->update_checking();
	  wchain_size_update();
	  screen_redraw();

	  smart_sleep();
	}

  endwin();
  wchain_free();
  dynamic_destroy();

  return 0;
}
