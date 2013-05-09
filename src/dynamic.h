/** this is my source file */
#include <mpd/tag.h>
#include <curses.h>
#include <fcntl.h>

#define SEEK_UNIT 3
#define VOLUME_UNIT 3
#define INTERVAL_MIN_UNIT 20000
#define INTERVAL_MAX_UNIT 200000
#define INTERVAL_INCREMENT 800
#define MAX_PLAYLIST_STORE_LENGTH 700

struct mpd_connection;

enum window_id
  {
	BASIC_INFO,
	VERBOSE_PROC_BAR,
	VISUALIZER,
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
  int visible;
  // if visible and 1 means this window redraw alway
  // every time, its redraw_signal always be 1;
  int flash;
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

struct VisualizerArgs
{
  int fifo_id;
  char fifo_file[32];
  int16_t buff[512];
};

struct PlaylistArgs
{
  char album[MAX_PLAYLIST_STORE_LENGTH][128];
  char artist[MAX_PLAYLIST_STORE_LENGTH][128];
  char title[MAX_PLAYLIST_STORE_LENGTH][128];
  char pretty_title[MAX_PLAYLIST_STORE_LENGTH][128];
  int id[MAX_PLAYLIST_STORE_LENGTH];

  int length;
  int begin;
  int current; // current playing song id
  int cursor;  // marked as song id

  struct WinMode wmode; // windows in this mode
};

struct SearchlistArgs
{
  enum mpd_tag_type tags[4]; // searching type
  char key[128];
  int crt_tag_id;
  int update_signal; // signal that activates update_searchlist()
  int picking_mode; // 1 when picking a song

  // share the same heap with the playlist, why not!  I don't want to waste memory; and some functions can be reused.
  struct PlaylistArgs *plist; // pointer to playlist

  struct WinMode wmode; // windows in this mode
};

struct VerboseArgs
{
  struct mpd_connection *conn;

  int redraw_signal;
  int quit_signal;
  int key_hit;
  
  /** set to 1 once commands have been triggered by keyboad*/
  struct PlaylistArgs *playlist;
  struct SearchlistArgs *searchlist;
  struct VisualizerArgs *visualizer;  
  
  struct WinMode wmode; // windows in main mode
};

int cmd_dynamic( int argc,  char **argv, struct mpd_connection *conn);
void playlist_keymap(struct VerboseArgs* vargs);
void basic_keymap(struct VerboseArgs* vargs);
void searchlist_keymap(struct VerboseArgs *vargs);
void turnoff_search_mode(struct VerboseArgs *vargs);
void turnon_search_mode(struct VerboseArgs *vargs);
void update_searchlist(struct VerboseArgs* vargs);
void screen_redraw(struct VerboseArgs *vargs);
