/** this is my source file */

#define SEEK_UNIT 3
#define VOLUME_UNIT 3
#define INTERVAL_UNIT 30000
#define MAX_PLAYLIST_STORE_LENGTH 1000
#define PLAYLIST_HEIGHT 16
#define AXIS_LENGTH 40

struct mpd_connection;

enum my_menu_id
{
  MENU_MAIN,
  MENU_PLAYLIST,
  //	MENU_QUEUE,
  MENU_NUMBER
};

struct PlaylistMenuArgs
{
  char **items;
  char album[MAX_PLAYLIST_STORE_LENGTH][128],
	artist[MAX_PLAYLIST_STORE_LENGTH][128],
	title[MAX_PLAYLIST_STORE_LENGTH][128],
	pretty_title[MAX_PLAYLIST_STORE_LENGTH][128];  
  int length;
  int begin;
  int current; // current playing song id
  int cursor;  // marked as song id
};

struct VerboseArgs
{
  struct mpd_connection *conn;
  /** set to 1 once commands have been triggered by keyboad*/
  void (*menu_print_routine)(struct VerboseArgs*);
  int (*menu_keymap)(struct VerboseArgs*);
  int new_command_signal;
  int menu_id;
  struct PlaylistMenuArgs *playlist;
};

int cmd_dynamic( int argc,  char ** argv, struct mpd_connection *conn);
int menu_playlist_keymap(struct VerboseArgs* vargs);
int menu_main_keymap(struct VerboseArgs* vargs);
