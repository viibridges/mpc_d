/** this is my source file */
#include <mpd/tag.h>
#include <curses.h>

#define SEEK_UNIT 3
#define VOLUME_UNIT 3
#define INTERVAL_MIN_UNIT 20000
#define INTERVAL_MAX_UNIT 200000
#define INTERVAL_INCREMENT 800
#define MAX_PLAYLIST_STORE_LENGTH 700
#define AXIS_LENGTH 44

struct mpd_connection;
int my_color_pairs[6];

enum my_search_mode
  {
	DISABLE,
	TYPING,
	SEARCHING,
	PICKING
  };

enum window_id
  {
	BASIC_INFO,
	VERBOSE_PROC_BAR,
	HELPER,
	SIMPLE_PROC_BAR,
	PLIST_UP_STATE_BAR,
	PLAYLIST,
	PLIST_DOWN_STATE_BAR,
	SEARCHLIST,
	SEARCH_INPUT,
	DEBUG_INFO,
	WIN_NUM            
  };

struct VerboseArgs;

struct WindowUnit
{
  int id;
  int flash; // if 1 means this window redraw alway every time, its redraw_signal always be 1;
  int redraw_signal;
  
  WINDOW *win;

  // each window has its own redraw routine
  void (*redraw_routine)(struct VerboseArgs*);
};

struct WinMode
{
  int size;
  struct WindowUnit **wins;

  void (*update_checking)(struct VerboseArgs*);
  void (*listen_keyboard)(struct VerboseArgs*);
};

struct PlaylistArgs
{
  char album[MAX_PLAYLIST_STORE_LENGTH][128],
	artist[MAX_PLAYLIST_STORE_LENGTH][128],
	title[MAX_PLAYLIST_STORE_LENGTH][128],
	pretty_title[MAX_PLAYLIST_STORE_LENGTH][128];
  int id[MAX_PLAYLIST_STORE_LENGTH];
  int length;
  int begin;
  int current; // current playing song id
  int cursor;  // marked as song id
  struct WinMode wmode; // windows in this mode
};

struct SearchlistArgs
{
  enum my_search_mode search_mode;
  enum mpd_tag_type tags[4]; // searching type
  int crt_tag_id;
  char key[128];
  struct WinMode wmode; // windows in this mode
};

struct VerboseArgs
{
  struct mpd_connection *conn;

  int redraw_signal;
  int quit_signal;
  int key_hit;
  int org_screen_x; // original terminal columns
  int org_screen_y; // original terminal rows
  
  /** set to 1 once commands have been triggered by keyboad*/
  struct PlaylistArgs *playlist;
  struct SearchlistArgs *searchlist;
  
  struct WinMode wmode; // windows in main mode
};

int cmd_dynamic( int argc,  char ** argv, struct mpd_connection *conn);
void playlist_keymap(struct VerboseArgs* vargs);
void basic_keymap(struct VerboseArgs* vargs);
/* void menu_playlist_print_routine(struct VerboseArgs *vargs); */
/* void menu_main_print_routine(struct VerboseArgs *vargs); */
/* void menu_search_print_routine(struct VerboseArgs *vargs); */
void searchlist_keymap(struct VerboseArgs *vargs);
void turnoff_search_mode(struct VerboseArgs *vargs);
void turnon_search_mode(struct VerboseArgs *vargs);
void update_searchlist(struct VerboseArgs* vargs);
void screen_redraw(struct VerboseArgs *vargs);
