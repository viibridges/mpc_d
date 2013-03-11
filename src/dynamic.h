/** this is my source file */
#include <mpd/tag.h>
#include <pthread.h>

#define SEEK_UNIT 3
#define VOLUME_UNIT 3
#define INTERVAL_UNIT 50000
#define MAX_PLAYLIST_STORE_LENGTH 1000
#define AXIS_LENGTH 44

struct mpd_connection;
pthread_mutex_t conn_mutex;
int my_color_pairs[6];

enum my_search_mode
{
  DISABLE,
  TYPING,
  SEARCHING,
  PICKING
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
};

struct SearchlistArgs
{
  enum my_search_mode mode;
  enum mpd_tag_type tags[4]; // searching type
  int crt_tag_id;
  char key[128];
};

struct VerboseArgs
{
  struct mpd_connection *conn;

  int playlist_height;
  /** set to 1 once commands have been triggered by keyboad*/
  int (*menu_keymap)(struct VerboseArgs*);
  void (*menu_print_routine)(struct VerboseArgs*);
  int (*old_menu_keymap)(struct VerboseArgs*);
  void (*old_menu_print_routine)(struct VerboseArgs*);  
  int redraw_signal;
  struct PlaylistArgs *playlist;
  struct SearchlistArgs *searchlist;
};

int cmd_dynamic( int argc,  char ** argv, struct mpd_connection *conn);
int menu_playlist_keymap(struct VerboseArgs* vargs);
int menu_main_keymap(struct VerboseArgs* vargs);
void menu_playlist_print_routine(struct VerboseArgs *vargs);
void menu_main_print_routine(struct VerboseArgs *vargs);
void menu_search_print_routine(struct VerboseArgs *vargs);
int search_routine(struct VerboseArgs *vargs);
void turnoff_search_mode(struct VerboseArgs *vargs);
void turnon_search_mode(struct VerboseArgs *vargs);
void update_searchlist(struct VerboseArgs* vargs);
