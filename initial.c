#include "initial.h"
#include "utils.h"
#include "basic_info.h"
#include "playlist.h"
#include "searchlist.h"
#include "dirlist.h"
#include "tapelist.h"
#include "visualizer.h"
#include "keyboards.h"

#include <ncursesw/ncurses.h>

extern struct mpd_connection *conn;
extern struct BasicInfo *basic_info;    
extern struct Playlist *playlist;
extern struct Searchlist *searchlist;
extern struct Dirlist *dirlist;  
extern struct Tapelist *tapelist;  
extern struct Visualizer *visualizer;

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
  
  /** playlist arguments */
  playlist =
	(struct Playlist*) malloc(sizeof(struct Playlist));
  playlist->begin = 1;
  playlist->length = 0;
  playlist->current = 0;
  playlist->cursor = 1;
  /** it's turn out to be good that updating
	  the playlist in the first place **/
  playlist_update();

  /** searchlist arguments */
  searchlist =
	(struct Searchlist*) malloc(sizeof(struct Searchlist));

  searchlist->tags[0] = MPD_TAG_NAME;
  searchlist->tags[1] = MPD_TAG_TITLE;
  searchlist->tags[2] = MPD_TAG_ARTIST;
  searchlist->tags[3] = MPD_TAG_ALBUM;
  searchlist->crt_tag_id = 0;

  searchlist->key[0] = '\0';
  searchlist->plist = playlist; // windows in this mode

  /** dirlist arguments **/
  // TODO add this automatically according to the mpd setting
  dirlist =
	(struct Dirlist*) malloc(sizeof(struct Dirlist));

  dirlist->begin = 1;
  dirlist->length = 0;
  dirlist->cursor = 1;
  strncpy(dirlist->root_dir, "/home/ted/Music", 128);
  strncpy(dirlist->crt_dir, dirlist->root_dir, 512);
  dirlist_update();

  /** tapelist arguments **/
  tapelist =
	(struct Tapelist*) malloc(sizeof(struct Tapelist));

  tapelist->begin = 1;
  tapelist->length = 0;
  tapelist->cursor = 1;
  tapelist_update();
  
  /** the visualizer **/
  visualizer =
	(struct Visualizer*) malloc(sizeof(struct Visualizer));
  // TODO add this automatically according to the mpd setting
  strncpy(visualizer->fifo_file, "/tmp/mpd.fifo", 64);
  get_fifo_id();
  
  /** windows set initialization **/
  winset_init();
  winset_update(&basic_info->wmode);
}

void
dynamic_destroy(void)
{
  winset_free();
  free(playlist);
  free(searchlist);
  free(dirlist);
  free(tapelist);
  free(visualizer);
}

void
wchain_init(void)
{
  void (*func[WIN_NUM])() =
	{
	  &print_basic_song_info,    // BASIC_INFO       
	  &print_extra_info,		 // EXTRA_INFO       
	  &print_basic_bar,			 // VERBOSE_PROC_BAR 
	  &print_visualizer,		 // VISUALIZER
	  &print_basic_help,		 // HELPER
	  &playlist_simple_bar,		 // SIMPLE_PROC_BAR  
	  &playlist_up_state_bar,    // PLIST_UP_STATE_BAR
	  &playlist_redraw_screen,	 // PLAYLIST
	  &playlist_down_state_bar,  // PLIST_DOWN_STATE_BAR
	  &playlist_redraw_screen,	 // SEARCHLIST
	  &dirlist_redraw_screen,    // DIRLIST
	  &tapelist_redraw_screen,   // TAPELIST
	  &search_prompt,			 // SEARCH_INPUT
	  NULL						 // DEBUG_INFO  
	};
  
  int i;
  wchain = (struct WindowUnit*)malloc(WIN_NUM * sizeof(struct WindowUnit));
  for(i = 0; i < WIN_NUM; i++)
	{
	  wchain[i].win = newwin(0, 0, 0, 0);// we're gonna change soon
	  wchain[i].redraw_routine = func[i];
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
  
  // we share the inventory window among searchlist and playlist
  // the way we achieve this is repoint the searchlist window unit
  // to the playlist's.
  wchain[SEARCHLIST] = wchain[PLAYLIST];

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
	  {1, 42, 4, 0},				// PLIST_UP_STATE_BAR
	  {height - 8, 73, 5, 0},	    // PLAYLIST
	  {1, width, height - 3, 0},	// PLIST_DOWN_STATE_BAR
	  {height - 8, 72, 5, 0},	    // SEARCHLIST
	  {height - 8, 72, 5, 0},	    // DIRLIST
	  {height - 8, 72, 5, 0},	    // TAPELIST
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
winset_init(void)
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
  basic_info->wmode.update_checking = &basic_state_checking;
  basic_info->wmode.listen_keyboard = &basic_keymap;

  // playlist wmode
  playlist->wmode.size = 6;
  playlist->wmode.wins = (struct WindowUnit**)
	malloc(playlist->wmode.size * sizeof(struct WindowUnit*));
  playlist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  playlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  playlist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  playlist->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  playlist->wmode.wins[4] = &wchain[BASIC_INFO];
  playlist->wmode.wins[5] = &wchain[PLAYLIST];
  playlist->wmode.update_checking = &playlist_update_checking;
  playlist->wmode.listen_keyboard = &playlist_keymap;

  // searchlist wmode
  searchlist->wmode.size = 7;
  searchlist->wmode.wins = (struct WindowUnit**)
	malloc(searchlist->wmode.size * sizeof(struct WindowUnit*));
  searchlist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  searchlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  searchlist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  searchlist->wmode.wins[3] = &wchain[SEARCH_INPUT];  
  searchlist->wmode.wins[4] = &wchain[SIMPLE_PROC_BAR];
  searchlist->wmode.wins[5] = &wchain[BASIC_INFO];
  searchlist->wmode.wins[6] = &wchain[SEARCHLIST];  
  searchlist->wmode.update_checking = &searchlist_update_checking;
  searchlist->wmode.listen_keyboard = &searchlist_keymap;

  // dirlist wmode
  dirlist->wmode.size = 6;
  dirlist->wmode.wins = (struct WindowUnit**)
	malloc(dirlist->wmode.size * sizeof(struct WindowUnit*));
  dirlist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  dirlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  dirlist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  dirlist->wmode.wins[3] = &wchain[DIRLIST];  
  dirlist->wmode.wins[4] = &wchain[SIMPLE_PROC_BAR];
  dirlist->wmode.wins[5] = &wchain[BASIC_INFO];
  dirlist->wmode.update_checking = &dirlist_update_checking;
  dirlist->wmode.listen_keyboard = &dirlist_keymap;

  // tapelist wmode
  tapelist->wmode.size = 6;
  tapelist->wmode.wins = (struct WindowUnit**)
	malloc(tapelist->wmode.size * sizeof(struct WindowUnit*));
  tapelist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  tapelist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  tapelist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  tapelist->wmode.wins[3] = &wchain[TAPELIST];  
  tapelist->wmode.wins[4] = &wchain[SIMPLE_PROC_BAR];
  tapelist->wmode.wins[5] = &wchain[BASIC_INFO];
  tapelist->wmode.update_checking = &tapelist_update_checking;
  tapelist->wmode.listen_keyboard = &tapelist_keymap;
}

void
winset_free(void)
{
  free(basic_info->wmode.wins);
  free(playlist->wmode.wins);
  free(searchlist->wmode.wins);
  free(dirlist->wmode.wins);
  free(tapelist->wmode.wins);
}
