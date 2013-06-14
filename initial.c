#include "initial.h"
#include "utils.h"
#include "basic_info.h"
#include "songs.h"
#include "directory.h"
#include "playlists.h"
#include "visualizer.h"
#include "keyboards.h"

#include <ncursesw/ncurses.h>

struct mpd_connection* setup_connection(void)
{
	struct mpd_connection *conn;

	conn = mpd_connection_new(NULL, 0, 0);
	if (conn == NULL) {
		fputs("Out of memory\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
		printErrorAndExit(conn);

	return conn;
}

void
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
  basic_info =
	(struct BasicInfo*) malloc(sizeof(struct BasicInfo));
  
  /** songlist arguments */
  songlist =
	(struct Songlist*) malloc(sizeof(struct Songlist));
  songlist->update_signal = 0;
  songlist->total = 0;
  
  songlist->begin = 1;
  songlist->length = 0;
  songlist->current = 0;
  songlist->cursor = 1;

  songlist->tags[0] = MPD_TAG_NAME;
  songlist->tags[1] = MPD_TAG_TITLE;
  songlist->tags[2] = MPD_TAG_ARTIST;
  songlist->tags[3] = MPD_TAG_ALBUM;
  songlist->crt_tag_id = 0;
  songlist->key[0] = '\0';
   
  /** it's turn out to be good that updating
	  the songlist in the first place **/
  songlist_update();

  /** directory arguments **/
  // TODO add this automatically according to the mpd setting
  directory =
	(struct Directory*) malloc(sizeof(struct Directory));
  directory->update_signal = 0;
  
  directory->begin = 1;
  directory->length = 0;
  directory->cursor = 1;
  strncpy(directory->root_dir, "/home/ted/Music", 128);
  strncpy(directory->crt_dir, directory->root_dir, 512);
  directory_update();

  /** playlist arguments **/
  playlist =
	(struct Playlist*) malloc(sizeof(struct Playlist));
  playlist->update_signal = 0;
  
  playlist->begin = 1;
  playlist->length = 0;
  playlist->cursor = 1;
  playlist_update();
  
  /** the visualizer **/
  visualizer =
	(struct Visualizer*) malloc(sizeof(struct Visualizer));
  // TODO add this automatically according to the mpd setting
  strncpy(visualizer->fifo_file, "/tmp/mpd.fifo", 64);
  get_fifo_id();
  
  /** windows set initialization **/
  winmod_init();
  being_mode_update(&basic_info->wmode);
  //being_mode_update(&songlist->wmode);
}

void
dynamic_destroy(void)
{
  winmod_free();
  free(songlist);
  free(directory);
  free(playlist);
  free(visualizer);
}

void
wchain_init(void)
{
  void (*redraw_func[WIN_NUM])() =
	{
	  &print_basic_song_info,    // BASIC_INFO       
	  &print_extra_info,		 // EXTRA_INFO       
	  &print_basic_bar,			 // VERBOSE_PROC_BAR 
	  &print_visualizer,		 // VISUALIZER
	  &print_basic_help,		 // HELPER
	  &songlist_simple_bar,		 // SIMPLE_PROC_BAR  
	  &songlist_up_state_bar,    // SLIST_UP_STATE_BAR
	  &songlist_redraw_screen,	 // SONGLIST
	  &songlist_down_state_bar,  // SLIST_DOWN_STATE_BAR
	  &directory_redraw_screen,  // DIRECTORY
	  &directory_helper,         // DIRHELPER
	  &playlist_redraw_screen,   // PLAYLIST
	  &playlist_helper,          // TAPEHELPER
	  &search_prompt,			 // SEARCH_INPUT
	  NULL						 // DEBUG_INFO  
	};

  void (*checking_func[WIN_NUM])() =
	{
	  &basic_state_checking,    	 // BASIC_INFO       
	  NULL,		                	 // EXTRA_INFO       
	  NULL,			            	 // VERBOSE_PROC_BAR 
	  NULL,		                	 // VISUALIZER
	  NULL,		                	 // HELPER
	  NULL,		                	 // SIMPLE_PROC_BAR  
	  NULL,                     	 // SLIST_UP_STATE_BAR
	  &songlist_update_checking,	 // SONGLIST
	  NULL,                          // SLIST_DOWN_STATE_BAR
	  &directory_update_checking,    // DIRECTORY
	  NULL,                          // DIRHELPER
	  &playlist_update_checking,     // PLAYLIST
	  NULL,                          // TAPEHELPER
	  NULL,			                 // SEARCH_INPUT
	  NULL						     // DEBUG_INFO  
	};
  
  int i;
  wchain = (struct WindowUnit*)malloc(WIN_NUM * sizeof(struct WindowUnit));
  for(i = 0; i < WIN_NUM; i++)
	{
	  wchain[i].win = newwin(0, 0, 0, 0);// we're gonna change soon
	  wchain[i].redraw_routine = redraw_func[i];
	  wchain[i].update_checking = checking_func[i];
	  wchain[i].visible = 1;
	  wchain[i].flash = 0;
	}

  // some windows need refresh frequently
  wchain[VERBOSE_PROC_BAR].flash = 1;
  wchain[SIMPLE_PROC_BAR].flash = 1;
  wchain[VISUALIZER].flash = 1;

  // and some need to hide
  wchain[HELPER].visible = 0;
  wchain[VISUALIZER].visible = 0;
  wchain[SEARCH_INPUT].visible = 0;
  
  wchain_size_update();
}

// basically sizes are what we are concerned
void
wchain_size_update(void)
{
  static int old_width = -1, old_height = -1;
  int height = stdscr->_maxy + 1, width = stdscr->_maxx + 1;

  if(old_height == height && old_width == width)
	return;
  else
	old_height = height, old_width = width;

  int wparam[WIN_NUM][4] =
	{
	  {2, width, 0, 0},             // BASIC_INFO
	  {1, width - 47, 2, 46},       // EXTRA_INFO
	  {1, 42, 2, 0},				// VERBOSE_PROC_BAR
	  {1, 72, 3, 0},				// VISUALIZER
	  {9, width, 5, 0},				// HELPER
	  {1, 29, 4, 43},				// SIMPLE_PROC_BAR
	  {1, 42, 4, 0},				// SLIST_UP_STATE_BAR
	  {height - 8, 73, 5, 0},	    // SONGLIST
	  {1, width, height - 3, 0},	// SLIST_DOWN_STATE_BAR
	  {height - 8, 36, 6, 41},	    // DIRECTORY
	  {height - 6, 36, 3, 0},       // DIRHELPER
	  {height - 8, 36, 6, 41},	    // PLAYLIST
	  {height - 6, 36, 3, 0},       // TAPEHELPER
	  {1, width, height - 1, 0},	// SEARCH_INPUT
	  {1, width, height - 2, 0}		// DEBUG_INFO       
	}; 

  int i;
  for(i = 0; i < WIN_NUM; i++)
	{
	  wresize(wchain[i].win, wparam[i][0], wparam[i][1]);
	  mvwin(wchain[i].win, wparam[i][2], wparam[i][3]);
	}
}

void
wchain_free(void)
{
  free(wchain);
}
	
void
winmod_init(void)
{
  // for basic mode (main menu)
  basic_info->wmode.size = 5;
  basic_info->wmode.wins = (struct WindowUnit**)
	malloc(basic_info->wmode.size * sizeof(struct WindowUnit*));
  basic_info->wmode.wins[0] = &wchain[BASIC_INFO];
  basic_info->wmode.wins[1] = &wchain[EXTRA_INFO];  
  basic_info->wmode.wins[2] = &wchain[VERBOSE_PROC_BAR];
  basic_info->wmode.wins[3] = &wchain[VISUALIZER];
  basic_info->wmode.wins[4] = &wchain[HELPER];
  basic_info->wmode.listen_keyboard = &basic_keymap;

  // songlist wmode
  songlist->wmode.size = 7;
  songlist->wmode.wins = (struct WindowUnit**)
	malloc(songlist->wmode.size * sizeof(struct WindowUnit*));
  songlist->wmode.wins[0] = &wchain[SLIST_DOWN_STATE_BAR];
  songlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  songlist->wmode.wins[2] = &wchain[SLIST_UP_STATE_BAR];
  songlist->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  songlist->wmode.wins[4] = &wchain[BASIC_INFO];
  songlist->wmode.wins[5] = &wchain[SONGLIST];
  songlist->wmode.wins[6] = &wchain[SEARCH_INPUT];  
  songlist->wmode.listen_keyboard = &songlist_keymap;

  // directory wmode
  directory->wmode.size = 5;
  directory->wmode.wins = (struct WindowUnit**)
	malloc(directory->wmode.size * sizeof(struct WindowUnit*));
  directory->wmode.wins[0] = &wchain[EXTRA_INFO];  
  directory->wmode.wins[1] = &wchain[DIRECTORY];  
  directory->wmode.wins[2] = &wchain[DIRHELPER];  
  directory->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  directory->wmode.wins[4] = &wchain[BASIC_INFO];
  directory->wmode.listen_keyboard = &directory_keymap;

  // playlist wmode
  playlist->wmode.size = 5;
  playlist->wmode.wins = (struct WindowUnit**)
	malloc(playlist->wmode.size * sizeof(struct WindowUnit*));
  playlist->wmode.wins[0] = &wchain[TAPEHELPER];
  playlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  playlist->wmode.wins[2] = &wchain[PLAYLIST];  
  playlist->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  playlist->wmode.wins[4] = &wchain[BASIC_INFO];
  playlist->wmode.listen_keyboard = &playlist_keymap;
}

void
winmod_free(void)
{
  free(basic_info->wmode.wins);
  free(songlist->wmode.wins);
  free(directory->wmode.wins);
  free(playlist->wmode.wins);
}
