#include "dynamic.h"
#include "command.h"
#include "charset.h"
#include "options.h"
#include "util.h"
#include "search.h"
#include "status.h"
#include "gcc.h"

#include <mpd/client.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <curses.h>
#include <locale.h>
#include <math.h>
#include <fcntl.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/stat.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); return -1; } while(0)

static struct WindowUnit *wchain; // array stores all subwindows
static struct WinMode *being_mode; // pointer to current used window set
static int my_color_pairs[9];


/*************************************
 **     WINDOW  CONTROLER           **
 *************************************/
static WINDOW*
specific_win(int win_id)
{
  WINDOW *win = wchain[win_id].win;
  werase(win);
  return win;
}

static void signal_win(int id)
{
  wchain[id].redraw_signal = 1;
}

static void signal_all_wins(void)
{
  int i;
  for(i = 0; i < being_mode->size; i++)
	being_mode->wins[i]->redraw_signal = 1;
}
  
static void
clean_window(int id)
{
  werase(wchain[id].win);
  wrefresh(wchain[id].win);
}

static void
clean_screen(void)
{
  int i;
  for(i = 0; i < being_mode->size; i++)
	{
	  werase(being_mode->wins[i]->win);
	  wrefresh(being_mode->wins[i]->win);
	}
}

static void
winset_update(struct WinMode *wmode)
{
  being_mode = wmode;
  signal_all_wins();
}

static void 
color_print(WINDOW *win, int color_scheme, const char *str)
{
  if(color_scheme > 0)
	{
	  wattron(win, my_color_pairs[color_scheme - 1]);
	  wprintw(win, "%s", str);
	  wattroff(win, my_color_pairs[color_scheme - 1]);	  
	}
  else
	wprintw(win, "%s", str);
}


/*************************************
 **    SYSTEM  AND  MISCELLANY      **
 *************************************/
static void my_finishCommand(struct mpd_connection *conn) {
  if (!mpd_response_finish(conn))
	printErrorAndExit(conn);
}

static struct mpd_status *
getStatus(struct mpd_connection *conn) {
  struct mpd_status *ret = mpd_run_status(conn);
  if (ret == NULL)
	printErrorAndExit(conn);

  return ret;
}

static mpd_unused struct mpd_status*
init_mpd_status(struct mpd_connection *conn)
{
  struct mpd_status *status;

  if (!mpd_command_list_begin(conn, true) ||
	  !mpd_send_status(conn) ||
	  !mpd_send_current_song(conn) ||
	  !mpd_command_list_end(conn))
	printErrorAndExit(conn);

  status = mpd_recv_status(conn);
  if (status == NULL)
	printErrorAndExit(conn);

  return status;
}

static int
is_dir_exist(char *path)
{
  struct stat s;

  if (!stat(path, &s) && (s.st_mode & S_IFDIR))
	return 1;
  else
	return 0;
}

static mpd_unused int
is_path_exist(char *path)
{
  struct stat s;

  if (!stat(path, &s) && (s.st_mode & (S_IFDIR | S_IFREG | S_IFLNK)))
	return 1;
  else
	return 0;
}

static mpd_unused void debug(const char *debug_info)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  if(debug_info)
	{
	  wprintw(win, debug_info);
	  wrefresh(win);
	}
}

static mpd_unused void debug_static(const char *debug_info)
{
  static int t = 1;
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, "[%i] ", t++);
  wprintw(win, debug_info);
  wrefresh(win);
}

static mpd_unused void debug_int(const int num)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, "%d", num);
  wrefresh(win);
}

static int get_playlist_cursor_item_index(struct VerboseArgs *vargs)
{
  return vargs->playlist->cursor - 1;
}

static const char *
get_song_format(const struct mpd_song *song)
{
  const char *profix;
  // normally the profix is a song's format;
  profix = strrchr(mpd_song_get_uri(song), (int)'.');
  if(profix == NULL) return "unknown";
  else return profix + 1;
}

static const char *
get_song_tag(const struct mpd_song *song, enum mpd_tag_type type)
{
  const char *song_tag, *uri;
  song_tag = mpd_song_get_tag(song, type, 0);

  if(song_tag == NULL)
	{
	  if(type == MPD_TAG_TITLE)
		{
		  uri = strrchr(mpd_song_get_uri(song), (int)'/');
		  if(uri == NULL) return mpd_song_get_uri(song);
		  else return uri + 1;
		}
	  else
		return "Unknown";
	}
  else
	return song_tag;
}

static
char *is_substring_ignorecase(const char *main, char *sub)
{
  char lower_main[64], lower_sub[64];
  int i;

  for(i = 0; main[i] && i < 64; i++)
  	lower_main[i] = isalpha(main[i]) ?
  	  (islower(main[i]) ? main[i] : (char)tolower(main[i])) : main[i];
  lower_main[i] = '\0';

  for(i = 0; sub[i] && i < 64; i++)
  	lower_sub[i] = isalpha(sub[i]) ?
  	  (islower(sub[i]) ? sub[i] : (char)tolower(sub[i])) : sub[i];
  lower_sub[i] = '\0';

  return strstr(lower_main, lower_sub);
}

static int
is_playlist_selected(struct VerboseArgs *vargs)
{
  int i;

  for(i = 0; i < vargs->playlist->length; i++)
	if(vargs->playlist->meta[i].selected)
	  return 1;

  return 0;
}

static char*
get_abs_crt_path(struct VerboseArgs *vargs, char *temp)
{
  snprintf(temp, 512, "%s/%s",
		   vargs->dirlist->crt_dir,
		   vargs->dirlist->filename[vargs->dirlist->cursor - 1]);

  return temp;
}

static int
is_path_valid_format(char *path)
{
  #define MUSIC_FORMAT_NUM 6

  char valid_format[][MUSIC_FORMAT_NUM] = {
	"mp3", "flac", "wav", "wma", "ape", "ogg"
  };

  char *path_suffix = strrchr(path, '.');

  int i;
  
  if(path_suffix)
	{
	  for(i = 0; i < MUSIC_FORMAT_NUM; i++)
		if(is_substring_ignorecase(path_suffix, valid_format[i]))
		  return 1;
	}
  else
	return 0;

  return 0;
}

static char*
crt_mpd_relative_path(struct VerboseArgs *vargs)
{
  char *root, *crt;
  char temp[512];
  
  root = vargs->dirlist->root_dir;
  crt = get_abs_crt_path(vargs, temp);
  
  while(*root && *crt && *root++ == *crt++);

  if(!*root) // current directory is under the root directory
	return crt + 1;
  else
	return NULL;
}

static void
swap_playlist_items(struct VerboseArgs *vargs, int i, int j)
{
  struct MetaInfo temp;

  memmove(&temp, vargs->searchlist->plist->meta + i,
		  sizeof(struct MetaInfo) / sizeof(char));

  memmove(vargs->searchlist->plist->meta + i,
		  vargs->searchlist->plist->meta + j,
		  sizeof(struct MetaInfo) / sizeof(char));

  memmove(vargs->searchlist->plist->meta + j, &temp,
		  sizeof(struct MetaInfo) / sizeof(char));
}

static void
append_target(struct VerboseArgs *vargs, char *path)
{
	struct mpd_connection *conn = vargs->conn; 
	
	if (!mpd_command_list_begin(conn, false))
	  return;

	mpd_send_add(conn, path);

	if (!mpd_command_list_end(conn))
	  return;

	if (!mpd_response_finish(conn))
	  return;
}

/*************************************
 **       FUNCTIONS OF COMMANDS     **
 *************************************/
static void
cmd_forward(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;

  char *args = malloc(8 * sizeof(char));
  sprintf(args, "+%i%%", SEEK_UNIT);
  cmd_seek(1, &args, conn);
  free(args);
}

static void
cmd_backward(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;

  char *args = malloc(8 * sizeof(char));
  sprintf(args, "-%i%%", SEEK_UNIT);
  cmd_seek(1, &args, conn);
  free(args);
}

static int
cmd_volup(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;
  struct mpd_status *status;
  int volume;

  status = getStatus(conn);
  volume = mpd_status_get_volume(status);
  mpd_status_free(status);

  volume += VOLUME_UNIT;
  if(volume > 100)
	volume = 100;

  if (!mpd_send_set_volume(conn, volume))
	printErrorAndExit(conn);

  my_finishCommand(conn);
  
  return 1;
}
  
static int
cmd_voldown(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;
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

/* different from the old cmd_single, this new function
   turn on the Repeat command simultaneously */
static int
cmd_Single(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;
  struct mpd_status *status;
  
  status = getStatus(conn);  
  if(!mpd_status_get_repeat(status))
	cmd_repeat(0, NULL, conn);

  mpd_status_free(status);

  return cmd_single(0, NULL, conn);
}

static void
cmd_Toggle(struct VerboseArgs* vargs)
{
  cmd_toggle(0, NULL, vargs->conn);
}

static void
cmd_Playback(struct VerboseArgs* vargs)
{
  cmd_playback(0, NULL, vargs->conn);
}

static void
cmd_Repeat(struct VerboseArgs* vargs)
{
  cmd_repeat(0, NULL, vargs->conn);
}

static void
cmd_Random(struct VerboseArgs* vargs)
{
  cmd_random(0, NULL, vargs->conn);
}

// current cursor song move up
static void
song_in_cursor_move_to(struct VerboseArgs *vargs, int offset)
{
  int from = get_playlist_cursor_item_index(vargs);
  int to = from + offset;

  // check whether new position valid
  if(to < 0 || to >= vargs->playlist->length)
	return;
  
  // now check from
  if(from < 0 || from >= vargs->playlist->length)
	return;

  // no problem
  mpd_run_move(vargs->conn, vargs->playlist->meta[from].id - 1,
			   vargs->playlist->meta[to].id - 1);

  // inform them to update the playlist
  //vargs->playlist->update_signal = 1;

  swap_playlist_items(vargs, from, to);
}

static void playlist_scroll_up_line(struct VerboseArgs*);
static void playlist_scroll_down_line(struct VerboseArgs*);

static void
song_move_up(struct VerboseArgs * vargs)
{
  song_in_cursor_move_to(vargs, -1);

  // scroll up cursor also
  playlist_scroll_up_line(vargs);
}

static void
song_move_down(struct VerboseArgs * vargs)
{
  song_in_cursor_move_to(vargs, +1);

  // scroll down cursor also
  playlist_scroll_down_line(vargs);
}

static void
toggle_select_item(struct VerboseArgs *vargs, int id)
{
  if(id < 0 || id >= vargs->playlist->length)
	return;
  else
	vargs->playlist->meta[id].selected ^= 1;
}

static void
toggle_select(struct VerboseArgs *vargs)
{
  int id = get_playlist_cursor_item_index(vargs);

  toggle_select_item(vargs, id);

  // move cursor forward by one step
  playlist_scroll_down_line(vargs);
}

static void
playlist_scroll(struct VerboseArgs *vargs, int lines)
{
  static struct PlaylistArgs *pl;
  int height = wchain[PLAYLIST].win->_maxy + 1;
  // mid_pos is the relative position of cursor in the screen
  const int mid_pos = height / 2 - 1;

  pl = vargs->playlist;
  
  pl->cursor += lines;
	
  if(pl->cursor > pl->length)
	pl->cursor = pl->length;
  else if(pl->cursor < 1)
	pl->cursor = 1;

  pl->begin = pl->cursor - mid_pos;

  /* as mid_pos maight have round-off error, the right hand side
   * playlist_height - mid_pos compensates it.
   * always keep:
   *		begin = cursor - mid_pos    (previous instruction)
   * while:
   *		length - cursor = height - mid_pos - 1 (condition)
   *		begin = length - height + 1            (body)
   *
   * Basic Math :) */
  if(pl->length - pl->cursor < height - mid_pos)
	pl->begin = pl->length - height + 1;

  if(pl->cursor < mid_pos)
	pl->begin = 1;

  // this expression should always be false
  if(pl->begin < 1)
	pl->begin = 1;
}

static void
playlist_scroll_to(struct VerboseArgs *vargs, int line)
{
  vargs->playlist->cursor = 0;
  playlist_scroll(vargs, line);
}

static void
playlist_scroll_down_line(struct VerboseArgs *vargs)
{
  playlist_scroll(vargs, +1);
}

static void
playlist_scroll_up_line(struct VerboseArgs *vargs)
{
  playlist_scroll(vargs, -1);
}

static void
playlist_scroll_up_page(struct VerboseArgs *vargs)
{
  playlist_scroll(vargs, -15);
}

static void
playlist_scroll_down_page(struct VerboseArgs *vargs)
{
  playlist_scroll(vargs, +15);
}

static void
playlist_play_current(struct VerboseArgs *vargs)
{
  char *args = (char*)malloc(8 * sizeof(char));
  int i = vargs->playlist->cursor;
  snprintf(args, sizeof(args), "%i", vargs->playlist->meta[i-1].id);
  cmd_play(1, &args, vargs->conn);
  free(args);
}

static void
playlist_scroll_to_current(struct VerboseArgs *vargs)
{
  struct mpd_status *status;
  int song_id;
  status = getStatus(vargs->conn);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  playlist_scroll_to(vargs, song_id);
}

static void
playlist_cursor_hide(struct VerboseArgs* vargs)
{
  // because the minimum visible position is 1
  vargs->playlist->cursor = -1;
}

static void
enter_selected_dir(struct VerboseArgs* vargs)
{
  char temp[512];

  get_abs_crt_path(vargs, temp);
  
  if(is_dir_exist(temp))
	{
	  strcpy(vargs->dirlist->crt_dir, temp);

	  vargs->dirlist->begin = 1;
	  vargs->dirlist->cursor = 1;
	  vargs->dirlist->update_signal = 1;
	}
}

static void
exit_current_dir(struct VerboseArgs* vargs)
{
  char *p;

  p = strrchr(vargs->dirlist->crt_dir, '/');

  // prehibit exiting from root directory
  if(p - vargs->dirlist->crt_dir < (int)strlen(vargs->dirlist->root_dir))
	return;

  if(p && p != vargs->dirlist->crt_dir)
	{
	  *p = '\0';

	  vargs->dirlist->begin = 1;
	  vargs->dirlist->cursor = 1;
	  vargs->dirlist->update_signal = 1;
	}
}

static void
append_to_playlist(struct VerboseArgs *vargs)
{
  char *path, absp[512];

  /* check if path exist */
  get_abs_crt_path(vargs, absp);
  if(!is_path_exist(absp))
	return;

  /* check if a nondir file of valid format */
  if(!is_dir_exist(absp) && !is_path_valid_format(absp))
	return;
  
  path = crt_mpd_relative_path(vargs);

  if(path)
	append_target(vargs, path);
}

/* this funciton cloned from playlist_scroll */
static void
dirlist_scroll(struct VerboseArgs *vargs, int lines)
{
  static struct DirlistArgs *pl;
  int height = wchain[DIRLIST].win->_maxy + 1;
  // mid_pos is the relative position of cursor in the screen
  const int mid_pos = height / 2 - 1;

  pl = vargs->dirlist;
  
  pl->cursor += lines;
	
  if(pl->cursor > pl->length)
	pl->cursor = pl->length;
  else if(pl->cursor < 1)
	pl->cursor = 1;

  pl->begin = pl->cursor - mid_pos;

  if(pl->length - pl->cursor < height - mid_pos)
	pl->begin = pl->length - height + 1;

  if(pl->cursor < mid_pos)
	pl->begin = 1;

  // this expression should always be false
  if(pl->begin < 1)
	pl->begin = 1;
}

static void
dirlist_scroll_to(struct VerboseArgs *vargs, int line)
{
  vargs->dirlist->cursor = 0;
  dirlist_scroll(vargs, line);
}

static void
dirlist_scroll_down_line(struct VerboseArgs *vargs)
{
  dirlist_scroll(vargs, +1);
}

static void
dirlist_scroll_up_line(struct VerboseArgs *vargs)
{
  dirlist_scroll(vargs, -1);
}

static void
dirlist_scroll_up_page(struct VerboseArgs *vargs)
{
  dirlist_scroll(vargs, -15);
}

static void
dirlist_scroll_down_page(struct VerboseArgs *vargs)
{
  dirlist_scroll(vargs, +15);
}

/* this funciton cloned from playlist_scroll */
static void
tapelist_scroll(struct VerboseArgs *vargs, int lines)
{
  static struct TapelistArgs *pl;
  int height = wchain[TAPELIST].win->_maxy + 1;
  // mid_pos is the relative position of cursor in the screen
  const int mid_pos = height / 2 - 1;

  pl = vargs->tapelist;
  
  pl->cursor += lines;
	
  if(pl->cursor > pl->length)
	pl->cursor = pl->length;
  else if(pl->cursor < 1)
	pl->cursor = 1;

  pl->begin = pl->cursor - mid_pos;

  if(pl->length - pl->cursor < height - mid_pos)
	pl->begin = pl->length - height + 1;

  if(pl->cursor < mid_pos)
	pl->begin = 1;

  // this expression should always be false
  if(pl->begin < 1)
	pl->begin = 1;
}

static void
tapelist_scroll_to(struct VerboseArgs *vargs, int line)
{
  vargs->tapelist->cursor = 0;
  tapelist_scroll(vargs, line);
}

static void
tapelist_scroll_down_line(struct VerboseArgs *vargs)
{
  tapelist_scroll(vargs, +1);
}

static void
tapelist_scroll_up_line(struct VerboseArgs *vargs)
{
  tapelist_scroll(vargs, -1);
}

static void
tapelist_scroll_up_page(struct VerboseArgs *vargs)
{
  tapelist_scroll(vargs, -15);
}

static void
tapelist_scroll_down_page(struct VerboseArgs *vargs)
{
  tapelist_scroll(vargs, +15);
}

static void
cmd_nextsong(struct VerboseArgs* vargs)
{
  cmd_next(0, NULL, vargs->conn);

  playlist_scroll_to_current(vargs);
}

static void
cmd_prevsong(struct VerboseArgs* vargs)
{
  cmd_prev(0, NULL, vargs->conn);
  
  playlist_scroll_to_current(vargs);
}

static void
change_searching_scope(struct VerboseArgs* vargs)
{
  vargs->searchlist->crt_tag_id ++;
  vargs->searchlist->crt_tag_id %= 4;	  
}

static void
playlist_delete_song_in_cursor(struct VerboseArgs *vargs)
{
  int i = get_playlist_cursor_item_index(vargs);
  
  if(i < 0 || i >= vargs->playlist->length)
	return;
	
  mpd_send_delete(vargs->conn, vargs->playlist->meta[i].id - 1);

  my_finishCommand(vargs->conn);
}

// if any is deleted then return 1
static void
playlist_delete_song_in_batch(struct VerboseArgs *vargs)
{
  int i;

  // delete in descended order won't screw things up
  for(i = vargs->playlist->length - 1; i >= 0; i--)
	if(vargs->playlist->meta[i].selected)
	  {
		mpd_send_delete(vargs->conn, vargs->playlist->meta[i].id - 1);
		my_finishCommand(vargs->conn);
	  }
}

static void
playlist_delete_song(struct VerboseArgs *vargs)
{
  if(is_playlist_selected(vargs))
	playlist_delete_song_in_batch(vargs);
  else
	playlist_delete_song_in_cursor(vargs);
}

static void
switch_to_main_menu(struct VerboseArgs* vargs)
{
  clean_screen();

  winset_update(&vargs->wmode);
}

static void
switch_to_playlist_menu(struct VerboseArgs *vargs)
{
  clean_screen();
  
  winset_update(&vargs->playlist->wmode);
}

static void
switch_to_dirlist_menu(struct VerboseArgs *vargs)
{
  clean_screen();
  
  winset_update(&vargs->dirlist->wmode);
}

static void
switch_to_tapelist_menu(struct VerboseArgs *vargs)
{
  clean_screen();
  
  winset_update(&vargs->tapelist->wmode);
}

static void
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

/** Music Visualizer **/
static void
get_fifo_id(struct VerboseArgs *vargs)
{
  const char *fifo_path = vargs->visualizer->fifo_file;
  int id = -1;
  
  if((id = open(fifo_path , O_RDONLY | O_NONBLOCK)) < 0)
	debug("couldn't open the fifo file");

  vargs->visualizer->fifo_id = id;
}

static mpd_unused int
get_fifo_output_id(struct VerboseArgs *vargs)
{
	struct mpd_output *output;
	int ret = -1, id;
	const char *name;

	mpd_send_outputs(vargs->conn);

	while ((output = mpd_recv_output(vargs->conn)) != NULL) {
		id = mpd_output_get_id(output);
		name = mpd_output_get_name(output);

		if (mpd_output_get_enabled(output)
			&& !strstr(name, "FIFO"))
		  ret = id;

		mpd_output_free(output);
	}

	if(ret == -1)
	  debug("output disenabled.");
	
	mpd_response_finish(vargs->conn);

	return ret;
}


static mpd_unused void
fifo_output_update(struct VerboseArgs *vargs)
{
  int fifo_output_id = get_fifo_output_id(vargs);

  if(fifo_output_id != -1)
	{
	  mpd_run_disable_output(vargs->conn, fifo_output_id);
	  usleep(3e4);
	  mpd_run_enable_output(vargs->conn, fifo_output_id);
	}
}

static void
print_uv_meter(int bars, int max)
{
  int i, long_tail = 0;
  
  WINDOW *win = specific_win(VISUALIZER);

  int width = win->_maxx - 1;

  if(max == 0)
	bars = max = 1;
  
  while(bars > width)
	{
	  max = bars - width;
	  bars = width - max;
	  long_tail = 1;
	}
  
  if(max > width)
	max = width;

  wprintw(win, "_");

  if(long_tail)
	{
	  const char *trail = "-/|\\";
	  static int j = 0;

	  wattron(win, my_color_pairs[0]);
	  for(i = 0; i < bars; i++)
		wprintw(win, "%c", trail[j]);
	  wattron(win, my_color_pairs[0]);

	  wattron(win, my_color_pairs[2]);
	  for(i = 0; i < max; i++)
		wprintw(win, "%c", trail[j]);
	  wattroff(win, my_color_pairs[2]);	  
	  j = (j + 1) % 4;
	}
  else
	{
	  wattron(win, my_color_pairs[0]);
	  for(i = 0; i < bars; i++)
		wprintw(win, "|");
	  wattroff(win, my_color_pairs[0]);  
	  wmove(win, 0, max);
	  color_print(win, 3, "+");
	}

  mvwprintw(win, 0, win->_maxx, "_");
}

static void
draw_sound_wave(int16_t *buf)
{
  int i;
  double energy = .0, scope = 30.;
  const int size = BUFFER_SIZE;

  const int width = wchain[VISUALIZER].win->_maxx - 1;
  
  for(i = 0; i < size; i++)
	energy += buf[i] * buf[i];
	/* if(energy < buf[i] * buf[i]) */
	/*   energy = buf[i] * buf[i]; */

  /** filter the sound energy **/
  energy = pow(sqrt(energy / size), 1. / 3.);
  
  int bars = energy * width / scope;

  static int last_bars = 0, max = 0;
  const int brake = 1;

  if(bars - last_bars > brake)
	bars -= brake;
  else
	bars = last_bars - brake;

  last_bars = bars;
  
  if(max < bars || max > bars + 10)
	max = bars;
  if(bars < 2)
	max = bars;

  /* it seems that the mpd fifo a little percivably
	 forward than the sound card rendering, I use
	 cache to synchronize them */
#define DELAY 15
  static int bcache[DELAY], mcache[DELAY], id = 0;
  bcache[id] = bars, mcache[id] = max;
  id = (id + 1) % DELAY;
  print_uv_meter(bcache[id], mcache[id]);
  
  //print_uv_meter(bars, max);
}

static void
print_visualizer(struct VerboseArgs *vargs)
{
  int fifo_id = vargs->visualizer->fifo_id;
  int16_t *buf = vargs->visualizer->buff;

  if (fifo_id < 0)
	return;

  if(read(fifo_id, buf, BUFFER_SIZE *
		  sizeof(int16_t) / sizeof(char)) < 0)
	return;

  static long count = 0;
  // some delicate calculation of the refresh interval 
  if(count++ % 4000 == 0)
	{
	  fifo_output_update(vargs);
	  count = 1;
	}
  
  draw_sound_wave(buf);
  vargs->interval_level = 1;
}


/* the principle is that: if the keyboard events
 * occur frequently, then adjust the update rate
 * higher, if the keyboard is just idle, keep it
 * low. */
static void
smart_sleep(struct VerboseArgs* vargs)
{
  static int us = INTERVAL_MAX_UNIT;

  if(vargs->interval_level)
	{
	  us = INTERVAL_MIN_UNIT +
		(vargs->interval_level - 1) * 10000;
	  vargs->interval_level = 0;
	}
  else
	us = us < INTERVAL_MAX_UNIT ? us + INTERVAL_INCREMENT : us;

  usleep(us);
}

static void
basic_state_checking(struct VerboseArgs *vargs)
{
  static int repeat, randomm, single, queue_len, id,
	volume, bit_rate,
	play, rep, ran, sgl, que, idd, vol, ply, btr;
  struct mpd_status *status;

  status = getStatus(vargs->conn);
  rep = mpd_status_get_repeat(status);
  ran = mpd_status_get_random(status);
  sgl = mpd_status_get_single(status);
  que = mpd_status_get_queue_length(status);
  idd = mpd_status_get_song_id(status);
  vol = mpd_status_get_volume(status);
  ply = mpd_status_get_state(status);
  btr = mpd_status_get_kbit_rate(status);
  mpd_status_free(status);
  if(rep != repeat || ran != randomm || sgl != single
	 || que != queue_len || idd != id || vol != volume
	 || ply != play)
	{
	  repeat = rep;
	  randomm = ran;
	  single = sgl;
	  queue_len = que;
	  id = idd;
	  volume = vol;
	  play = ply;

	  signal_all_wins();
	}

  /* as many songs's bit rate varies while playing
	 we refresh the bit rate display only when the 
	 relevant change greater than 0.2 */
  if(abs(bit_rate - btr) / (float)(btr + 1) > 0.2)
	{
	  bit_rate = btr;
	  signal_win(BASIC_INFO);
	}
}

static void
print_basic_help(mpd_unused struct VerboseArgs *vargs)
{
  WINDOW *win = specific_win(HELPER);
  wprintw(win, "  [n] :\tNext song\t\t  [R] :\tToggle repeat\n");
  wprintw(win, "  [p] :\tPrevious song\t\t  [r] :\tToggle random\n");
  wprintw(win, "  [-] :\tVolume down\t\t  [s] :\tToggle single\n");
  wprintw(win, "  [=] :\tVolume Up\t\t  [b] :\tPlayback\n");
  wprintw(win, "  [t] :\tPlay / Pause\t\t  [l] :\tRedraw screen\n");
  wprintw(win, "  <L> :\tSeek backward\t\t  <R> :\tSeek forward\n");
  wprintw(win, "\n  [TAB] : New world\n");
  wprintw(win, "  [e/q] : Quit\n");
}

static void
print_extra_info(struct VerboseArgs *vargs)
{
  WINDOW *win = specific_win(EXTRA_INFO);

  struct mpd_status *status;
  status = init_mpd_status(vargs->conn);
  
  if (mpd_status_get_update_id(status) > 0)
	wprintw(win, "Updating DB (#%u) ...",
			mpd_status_get_update_id(status));

  if (mpd_status_get_volume(status) >= 0)
	wprintw(win, "Volume:%3i%c ", mpd_status_get_volume(status), '%');
  else {
	wprintw(win, "Volume: n/a   ");
  }

  mvwprintw(win, 0, 14, "Search: ");
  color_print(win, 6,
			  mpd_tag_name(vargs->searchlist->tags
						   [vargs->searchlist->crt_tag_id]));
  wprintw(win, "  ");

  if (mpd_status_get_error(status) != NULL)
	wprintw(win, "ERROR: %s\n",
			charset_from_utf8(mpd_status_get_error(status)));

  mpd_status_free(status);
  my_finishCommand(vargs->conn);
}

static void
print_basic_song_info(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;
  struct mpd_status *status;
  char format[16];
  int total_time, bit_rate;
  static int old_bit_rate = 111;

  WINDOW *win = specific_win(BASIC_INFO);
  
  status = init_mpd_status(conn);
  
  strncpy(format, "null", sizeof(format));
  
  if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	  mpd_status_get_state(status) == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  char buff[80];
	  /* int twid = 38; */
	  /* if(snprintf(buff, twid, "%s", */
	  /* 			  get_song_tag(song, MPD_TAG_TITLE)) >= twid) */
	  /* 	strcpy(buff + twid - 4, "..."); // in case of long title */
	  /* color_print(win, 1, buff); */

	  /* print artist
	  int awid = 28;
	  if(snprintf(buff, awid, "\t%s",
				  get_song_tag(song, MPD_TAG_ARTIST)) >= awid)
		strcpy(buff + awid - 4, "...");
	  color_print(win, 5, buff);
	  */

	  int title_len = win->_maxx < (int)sizeof(buff) ?
		win->_maxx : (int)sizeof(buff);
	  snprintf(buff, title_len, "%s", get_song_tag(song, MPD_TAG_TITLE));
	  color_print(win, 1,  buff);
	  //snprintf(format, 6, "%s%10c", get_song_format(song), ' ');
	  strncpy(format, get_song_format(song), sizeof(format));
	  
	  mpd_song_free(song);
	}

	mvwprintw(win, 1, 0, "[");
	if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  {
		color_print(win, 6, "playing");
		wprintw(win, "]");
	  }
	else
	  {
		color_print(win, 4, "paused");
		wprintw(win, "]");
	  }
  }

  /* mvwprintw(win, 1, 10, "[%i/%u]", */
  /* 		  mpd_status_get_song_pos(status) + 1, */
  /* 		  mpd_status_get_queue_length(status)); */
  
  // status modes [ors] : repeat, random, single
  mvwprintw(win, 1, 10, "["); 
  if (mpd_status_get_random(status))
	color_print(win, 1, "r");
  else wprintw(win, "-");

  if (mpd_status_get_single(status))
	color_print(win, 7, "s");
  else wprintw(win, "-");

  if (mpd_status_get_repeat(status))
	color_print(win, 5, "o");
  else wprintw(win, "-");
  wprintw(win, "]");

  mvwprintw(win, 1, 16, "[%s]", format); // music format

  total_time = mpd_status_get_total_time(status);
  mvwprintw(win, 1, 46, "%02i:%i", total_time / 60, total_time % 60);

  bit_rate = mpd_status_get_kbit_rate(status);
  if(abs(old_bit_rate - bit_rate) / (float)(bit_rate + 1) > 0.2
	 && bit_rate != 0)
	old_bit_rate = bit_rate;
  else
	bit_rate = old_bit_rate;
  mvwprintw(win, 1, 59, " %ikb/s", bit_rate);
  
  mpd_status_free(status);
  my_finishCommand(conn);
}

static void // VERBOSE_PROC_BAR
print_basic_bar(struct VerboseArgs *vargs)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i;
  struct mpd_status *status;
  struct mpd_connection *conn = vargs->conn;

  WINDOW *win = specific_win(VERBOSE_PROC_BAR);
  const int axis_length = win->_maxx - 7;
  
  status = getStatus(conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  mpd_status_free(status);

  crt_time_perc = (total_time == 0 ? 0 : 100 * crt_time / total_time);  
  fill_len = crt_time_perc * axis_length / 100;
  empty_len = axis_length - fill_len;
  
  wprintw(win, "[");
  wattron(win, my_color_pairs[2]);
  for(i = 0; i < fill_len - 1; wprintw(win, "="), i++);
  if(i == fill_len - 1) wprintw(win, "+");
  wprintw(win, ">");
  //for(i = 0; i <= fill_len; wprintw(win, "="), i++);
  for(i = 0; i < empty_len; wprintw(win, " "), i++);
  wattroff(win, my_color_pairs[2]);
  wprintw(win, "]");

  wprintw(win, "%3i%% ", crt_time_perc);
}

static void
playlist_simple_bar(struct VerboseArgs *vargs)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, rest_len, i;
  struct mpd_status *status;

  WINDOW *win = specific_win(SIMPLE_PROC_BAR);
  const int bar_length = win->_maxx + 1;
  
  status = getStatus(vargs->conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  mpd_status_free(status);

  crt_time_perc = (total_time == 0 ? 0 : 100 * crt_time / total_time);    
  fill_len = crt_time_perc * bar_length / 100;
  rest_len = bar_length - fill_len;

  wattron(win, my_color_pairs[2]);  
  for(i = 0; i < fill_len; wprintw(win, "*"), i++);
  wattroff(win, my_color_pairs[2]);  

  for(i = 0; i < rest_len; wprintw(win, "*"), i++);
}

static void
playlist_up_state_bar(struct VerboseArgs *vargs)
{
  WINDOW *win = specific_win(PLIST_UP_STATE_BAR);  

  if(vargs->playlist->begin > 1)
	wprintw(win, "%*s   %s", 3, " ", "^^^ ^^^");
}

static void
playlist_down_state_bar(struct VerboseArgs *vargs)
{
  static const char *bar =
	"_________________________________________________________/";
  WINDOW *win = specific_win(PLIST_DOWN_STATE_BAR);  

  int length = vargs->playlist->length,
	height = wchain[PLAYLIST].win->_maxy + 1,
	cursor = vargs->playlist->cursor;

  wprintw(win, "%*c %s", 5, ' ', bar);
  color_print(win, 8, "TED MADE");

  if(cursor + height / 2 < length)
  	mvwprintw(win, 0, 0, "%*s   %s", 3, " ", "... ...  ");
}

static void
print_list_item(WINDOW *win, int line, int color, int id,
					char *ltext, char *rtext)
{
  const int ltext_left = 6;
  const int rtext_left = 43;
  const int width = win->_maxx - 8;

  wattron(win, my_color_pairs[color - 1]);

  mvwprintw(win, line, 0, "%*c", width, ' ');
  id ? mvwprintw(win, line, 0, "%3i.", id) : 1;
  ltext ? mvwprintw(win, line, ltext_left, "%s", ltext) : 1;
  rtext ? mvwprintw(win, line, rtext_left, "%s", rtext) : 1;
  
  wattroff(win, my_color_pairs[color - 1]);
}

static void
playlist_redraw_screen(struct VerboseArgs *vargs)
{
  int line = 0, i, height = wchain[PLAYLIST].win->_maxy + 1;

  WINDOW *win = specific_win(PLAYLIST);  

  int id;
  char *title, *artist;
  for(i = vargs->playlist->begin - 1; i < vargs->playlist->begin
		+ height - 1 && i < vargs->playlist->length; i++)
	{
	  id = vargs->playlist->meta[i].id;
	  title = vargs->playlist->meta[i].pretty_title;
	  artist = vargs->playlist->meta[i].artist;

	  // cursor in
	  if(i + 1 == vargs->playlist->cursor)
		{
		  print_list_item(win, line++, 2, id, title, artist);
		  continue;
		}

	  // selected
	  if(vargs->playlist->meta[i].selected)
		{
		  print_list_item(win, line++, 9, id, title, artist);
		  continue;
		}
		
	  if(vargs->playlist->meta[i].id == vargs->playlist->current)
		{
		  print_list_item(win, line++, 1, id, title, artist);
		}
	  else
		{
		  print_list_item(win, line++, 0, id, title, artist);
		}
	}
}

static void
dirlist_redraw_screen(struct VerboseArgs *vargs)
{
  int line = 0, i, height = wchain[DIRLIST].win->_maxy + 1;

  WINDOW *win = specific_win(DIRLIST);  

  char *filename;
  for(i = vargs->dirlist->begin - 1; i < vargs->dirlist->begin
		+ height - 1 && i < vargs->dirlist->length; i++)
	{
	  filename = vargs->dirlist->prettyname[i];

	  if(i + 1 == vargs->dirlist->cursor)
		print_list_item(win, line++, 2, i + 1, filename, NULL);
	  else
		print_list_item(win, line++, 0, i + 1, filename, NULL);
	}
}

static void
tapelist_redraw_screen(struct VerboseArgs *vargs)
{
  int line = 0, i, height = wchain[TAPELIST].win->_maxy + 1;

  WINDOW *win = specific_win(TAPELIST);  

  char *filename;
  for(i = vargs->tapelist->begin - 1; i < vargs->tapelist->begin
		+ height - 1 && i < vargs->tapelist->length; i++)
	{
	  filename = vargs->tapelist->tapename[i];

	  if(i + 1 == vargs->tapelist->cursor)
		print_list_item(win, line++, 2, i + 1, filename, NULL);
	  else
		print_list_item(win, line++, 0, i + 1, filename, NULL);
	}
}

static void
pretty_copy(char *string, const char * tag, int size, int width)
{
  static int i, unic, asci;

  for(i = unic = asci = 0; tag[i] != '\0' && i < size - 1; i++)
	{
	  if(isascii((int)tag[i]))
		asci ++;
	  else
		unic ++;
	  string[i] = tag[i];
	}

  unic /= 3;

  if(width > 0)
	{
	  /* if the string too long, we interupt it
		 and profix ... to it */
	  if(width < 2*unic + asci)
		{
		  strncpy(string + width - 3, "...", 4);
		  return;
		}
	}

  string[i] = '\0';
}

void
playlist_update(struct VerboseArgs *vargs)
{
  struct mpd_song *song;
  
  if (!mpd_send_list_queue_meta(vargs->conn))
	printErrorAndExit(vargs->conn);

  int i = 0;
  while ((song = mpd_recv_song(vargs->conn)) != NULL
		 && i < MAX_PLAYLIST_STORE_LENGTH)
	{
	  pretty_copy(vargs->playlist->meta[i].title,
					get_song_tag(song, MPD_TAG_TITLE),
					512, -1);
	  pretty_copy(vargs->playlist->meta[i].pretty_title,
					get_song_tag(song, MPD_TAG_TITLE),
					128, 26);
	  pretty_copy(vargs->playlist->meta[i].artist,
					get_song_tag(song, MPD_TAG_ARTIST),
					128, 20);	  
	  pretty_copy(vargs->playlist->meta[i].album,
					get_song_tag(song, MPD_TAG_ALBUM),
					128, -1);
	  vargs->playlist->meta[i].id = i + 1;
	  vargs->playlist->meta[i].selected = 0;
	  ++i;
	  mpd_song_free(song);
	}

  if(i < MAX_PLAYLIST_STORE_LENGTH)
  	vargs->playlist->length = i;
  else
  	vargs->playlist->length = MAX_PLAYLIST_STORE_LENGTH;

  my_finishCommand(vargs->conn);
}

void
searchlist_update(struct VerboseArgs* vargs)
{
  char *tag_name, *key = vargs->searchlist->key;
  int i, j, ct = vargs->searchlist->crt_tag_id;

  /* we examine and update the playlist (datebase for searching)
   * every time, see if any modification (delete or add songs)
   * was made by other client during searching */
  playlist_update(vargs);

  int is_substr;
  
  for(i = j = 0; i < vargs->searchlist->plist->length; i++)
	{
	  // now plist is fresh
	  switch(vargs->searchlist->tags[ct])
		{
		case MPD_TAG_TITLE:
		  tag_name = vargs->searchlist->plist->meta[i].title; break;	  
		case MPD_TAG_ARTIST:
		  tag_name = vargs->searchlist->plist->meta[i].artist; break;
		case MPD_TAG_ALBUM:
		  tag_name = vargs->searchlist->plist->meta[i].album; break;
		default:
		  tag_name = NULL;
		}

	  if(tag_name != NULL)
		is_substr = is_substring_ignorecase
		  (tag_name, key) != NULL;
	  else
		{
		  is_substr = is_substring_ignorecase
			(vargs->searchlist->plist->meta[i].title, key) != NULL ||
			is_substring_ignorecase
			(vargs->searchlist->plist->meta[i].album, key) != NULL ||
			is_substring_ignorecase
			(vargs->searchlist->plist->meta[i].artist, key) != NULL;
		}

	  if(is_substr)
		{
		  memmove(vargs->searchlist->plist->meta + j,
				  vargs->searchlist->plist->meta + i,
				 sizeof(struct MetaInfo) / sizeof(char));
		  j ++;
		}
	}
  vargs->searchlist->plist->length = j;
  vargs->searchlist->plist->begin = 1;
}

void
dirlist_update(struct VerboseArgs* vargs)
{
  DIR *d;
  struct dirent *dir;

  d = opendir(vargs->dirlist->crt_dir);
  
  if(d)
	{

	  dir = readdir(d); // skip the directory itself

	  int i = 0;
  
	  while ((dir = readdir(d)) != NULL)
		{
		  if(dir->d_name[0] != '.')
			{
			  strncpy(vargs->dirlist->filename[i], dir->d_name, 128);
			  pretty_copy(vargs->dirlist->prettyname[i], dir->d_name, 128, 60);
			  i++;
			}
		}

	  vargs->dirlist->length = i;

	  closedir(d);
	}
}

void
tapelist_update(struct VerboseArgs* vargs)
{
	struct mpd_entity *entity;
	const struct mpd_playlist *playlist;

	int i = 0;

	if (!mpd_send_list_meta(vargs->conn, ""))
	  return;
	
	while((entity = mpd_recv_entity(vargs->conn)) != NULL && i < 128)
	  {
		enum mpd_entity_type type = mpd_entity_get_type(entity);
		if(type != MPD_ENTITY_TYPE_PLAYLIST)
		  continue;
		
		playlist = mpd_entity_get_playlist(entity);
		strncpy(vargs->tapelist->tapename[i],
				mpd_playlist_get_path(playlist), 512);

		mpd_entity_free(entity);

		i++;
	  }

	vargs->tapelist->length = i;
}

static void
playlist_update_checking(struct VerboseArgs *vargs)
{
  struct mpd_status *status;
  int queue_len, song_id;

  basic_state_checking(vargs);

  status = getStatus(vargs->conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(vargs->playlist->current != song_id)
	{
	  vargs->playlist->current = song_id;
	  signal_all_wins();
	}

  if(vargs->playlist->update_signal == 1 ||
	 vargs->playlist->length != queue_len)
	{
	  vargs->playlist->update_signal = 0;
	  playlist_update(vargs);
	  signal_all_wins();
	}

  /** for some unexcepted cases vargs->playlist->begin
	  may be greater than vargs->playlist->length;
	  if this happens, we reset the begin's value */
  if(vargs->playlist->begin > vargs->playlist->length)
	vargs->playlist->begin = 1;
}

static
void dirlist_update_checking(struct VerboseArgs *vargs)
{
  // TODO, when modification happened then update
  if(vargs->dirlist->update_signal)
	{
	  dirlist_update(vargs);
	  vargs->dirlist->update_signal = 0;
	  signal_all_wins();
	}
}

static
void tapelist_update_checking(struct VerboseArgs *vargs)
{
  if(vargs->tapelist->update_signal)
	{
	  tapelist_update(vargs);
	  vargs->tapelist->update_signal = 0;
	  signal_all_wins();
	}
}

static
void searchlist_update_checking(struct VerboseArgs*vargs)
{
  struct mpd_status *status;
  int queue_len, song_id;

  basic_state_checking(vargs);

  status = getStatus(vargs->conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(vargs->searchlist->update_signal)
	{
	  searchlist_update(vargs);
	  vargs->searchlist->update_signal = 0;
	  signal_all_wins();
	}
}

static void
search_prompt(struct VerboseArgs *vargs)
{
  char str[128];

  WINDOW *win = specific_win(SEARCH_INPUT);
  
  snprintf(str, sizeof(str), "%s█", vargs->searchlist->key);

  if(vargs->searchlist->picking_mode)
	color_print(win, 0, "Search: ");
  else
	color_print(win, 5, "Search: ");
  
  if(vargs->playlist->length == 0)
	color_print(win, 4, "no entries...");
  else if(vargs->searchlist->picking_mode)
	color_print(win, 0, str);
  else
	color_print(win, 6, str);

  wprintw(win, "%*s", 40, " ");
}

static
void fundamental_keymap_template(struct VerboseArgs *vargs, int key)
{
  switch(key)
	{
	case '+': ;
	case '=': ;
	case '0': 
	  cmd_volup(vargs); break;
	case KEY_RIGHT:
	  cmd_forward(vargs); break;
	case '-': ;
	case '9': 
	  cmd_voldown(vargs); break;
	case KEY_LEFT:
	  cmd_backward(vargs); break;
	case 'b':
	  cmd_Playback(vargs); break;
	case 'n':
	  cmd_nextsong(vargs); break;
	case 'p':
	  cmd_prevsong(vargs); break;
	case 't': ;
	case ' ':
	  cmd_Toggle(vargs); break;
	case 'r':
	  cmd_Random(vargs); break;
	case 's':
	  cmd_Single(vargs); break;
	case 'O':;
	case 'o':;
	case 'R':
	  cmd_Repeat(vargs); break;
	case 'L': // redraw screen
	  clean_screen();
	  break;
	case '/':
	  turnon_search_mode(vargs); break;
	case 'S':
	  change_searching_scope(vargs); break;
	case '\t':
	  switch_to_playlist_menu(vargs);
	  break;
	case '1':
	  switch_to_main_menu(vargs);
	  break;
	case '2':
	  switch_to_playlist_menu(vargs);
	  break;
	case '3':
	  switch_to_dirlist_menu(vargs);
	  break;
	case '4':
	  switch_to_tapelist_menu(vargs);
	  break;
	case 'v':
	  toggle_visualizer();
	  break;
	case 27: ;
	case 'e': ;
	case 'q':
	  vargs->quit_signal = 1; break;
	default:;
	}
}

static
void playlist_keymap_template(struct VerboseArgs *vargs, int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'v': break; // key be masked

	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  playlist_scroll_down_line(vargs);break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  playlist_scroll_up_line(vargs);break;
	case '\n':
	  playlist_play_current(vargs);break;
	case 'b':
	  playlist_scroll_up_page(vargs);break;
	case ' ':
	  playlist_scroll_down_page(vargs);break;
	case 'i':;
	case 12: ; // ctrl-l
	case 'l':  // cursor goto current playing place
	  playlist_scroll_to_current(vargs);
	  break;
	case 'g':  // cursor goto the beginning
	  playlist_scroll_to(vargs, 1);
	  break;
	case 'G':  // cursor goto the end
	  playlist_scroll_to(vargs, vargs->playlist->length);
	  break;
	case 'c':  // cursor goto the center
	  playlist_scroll_to(vargs, vargs->playlist->length / 2);
	  break;
	case 'D':
	  playlist_delete_song(vargs);
	  break;
	case '\t':
	  switch_to_main_menu(vargs);
	  break;
	case 'U':
	case 'K':
	  song_move_up(vargs);
	  break;
	case 'J':
	  song_move_down(vargs);
	  break;
	case 'm':
	  toggle_select(vargs);
	  break;
	  
	default:
	  fundamental_keymap_template(vargs, key);
	}
}

static void
dirlist_keymap_template(struct VerboseArgs *vargs, int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'U':;
	case 'J':;
	case 'K':;
	case 'm':;
	case 'v': break; // key be masked

	case '\n':
	  enter_selected_dir(vargs);break;
	case 127:
	  exit_current_dir(vargs);break;
	case 'a':
	  append_to_playlist(vargs);break;
	  
	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  dirlist_scroll_down_line(vargs);break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  dirlist_scroll_up_line(vargs);break;
	case 'b':
	  dirlist_scroll_up_page(vargs);break;
	case ' ':
	  dirlist_scroll_down_page(vargs);break;
	case 'g':  // cursor goto the beginning
	  dirlist_scroll_to(vargs, 1);
	  break;
	case 'G':  // cursor goto the end
	  dirlist_scroll_to(vargs, vargs->dirlist->length);
	  break;
	case 'c':  // cursor goto the center
	  dirlist_scroll_to(vargs, vargs->dirlist->length / 2);
	  break;
	case '\t':
	  switch_to_main_menu(vargs);
	  break;
	  
	default:
	  fundamental_keymap_template(vargs, key);
	}
}

static void
tapelist_keymap_template(struct VerboseArgs *vargs, int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'U':;
	case 'J':;
	case 'K':;
	case 'm':;
	case 'v': break; // key be masked

	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  tapelist_scroll_down_line(vargs);break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  tapelist_scroll_up_line(vargs);break;
	case 'b':
	  tapelist_scroll_up_page(vargs);break;
	case ' ':
	  tapelist_scroll_down_page(vargs);break;
	case 'g':  // cursor goto the beginning
	  tapelist_scroll_to(vargs, 1);
	  break;
	case 'G':  // cursor goto the end
	  tapelist_scroll_to(vargs, vargs->tapelist->length);
	  break;
	case 'c':  // cursor goto the center
	  tapelist_scroll_to(vargs, vargs->tapelist->length / 2);
	  break;
	case '\t':
	  switch_to_main_menu(vargs);
	  break;
	  
	default:
	  fundamental_keymap_template(vargs, key);
	}
}

// for picking the song from the searchlist
// corporate with searchlist_keymap()
static void
searchlist_picking_keymap(struct VerboseArgs *vargs)
{
  int key = getch();

  if(key != ERR)
	vargs->interval_level = 1;
  else
	return;

  switch(key)
	{
	case 'U':;
	case 'J':;
	case 'K':;
	case 'm':;
	case 'i':;
	case '\t':;
	case 'l': break; // these keys are masked
	  
	case '\n':
	  playlist_play_current(vargs);
	  turnoff_search_mode(vargs);
	  break;

	case 127:
	  // push the backspace back to delete the
	  // last character and force to update the
	  // searchlist by reemerge the keyboard event.
	  // and the rest works are the same with a
	  // normal "continue searching" invoked by '/'
	  ungetch(127);
	case '/':  // search on
	  // switch to the basic keymap for searching
	  // we only change being_mode->listen_keyboard instead of
	  // wchain[SEARCHLIST]->key_map, that's doesn't matter,
	  // the latter was fixed and any changes could lead to
	  // unpredictable behaviors.
	  being_mode->listen_keyboard = &searchlist_keymap; 
	  vargs->searchlist->picking_mode = 0;

	  playlist_cursor_hide(vargs);
	  signal_win(SEARCHLIST);
	  signal_win(SEARCH_INPUT);
	  break;

	case '\\':
	  turnoff_search_mode(vargs); break;
	default:
	  playlist_keymap_template(vargs, key);
	}

  signal_all_wins();
}

void
basic_keymap(struct VerboseArgs *vargs)
{
  int key = getch();

  if(key != ERR)
	vargs->interval_level = 1;
  else
	return;

  // get the key, let the template do the rest
  fundamental_keymap_template(vargs, key);

  signal_all_wins();
}

void
playlist_keymap(struct VerboseArgs* vargs)
{
  int key = getch();

  if(key != ERR)
	vargs->interval_level = 1;
  else
	return;

  playlist_keymap_template(vargs, key);

  signal_all_wins();  
}

void
dirlist_keymap(struct VerboseArgs* vargs)
{
  int key = getch();

  if(key != ERR)
	vargs->interval_level = 1;
  else
	return;

  dirlist_keymap_template(vargs, key);

  signal_all_wins();  
}

void
tapelist_keymap(struct VerboseArgs* vargs)
{
  int key = getch();

  if(key != ERR)
	vargs->interval_level = 1;
  else
	return;

  tapelist_keymap_template(vargs, key);

  signal_all_wins();  
}

// for getting the keyword
void
searchlist_keymap(struct VerboseArgs* vargs)
{
  int i, key = getch();

  if(key != ERR)
	vargs->interval_level = 1;
  else
	return;

  i = strlen(vargs->searchlist->key);
  
  switch(key)
	{
	case '\n':;
	case '\t':
	  if(i == 0)
		{
		  turnoff_search_mode(vargs);
		  break;
		}

	  being_mode->listen_keyboard = &searchlist_picking_keymap;
	  vargs->searchlist->picking_mode = 1;

	  playlist_scroll_to(vargs, 1);

	  vargs->searchlist->update_signal = 1;	  
	  signal_win(SEARCHLIST);
	  signal_win(SEARCH_INPUT);
	  break;
	case '\\':;
	case 27:
	  turnoff_search_mode(vargs);
	  break;
	case 127: // backspace is hitted
	  if(i == 0)
		{
		  turnoff_search_mode(vargs);
		  break;
		}
	  
	  if(!isascii(vargs->searchlist->key[i - 1]))
		vargs->searchlist->key[i - 3] = '\0';
	  else
		vargs->searchlist->key[i - 1] = '\0';
	  
	  vargs->searchlist->update_signal = 1;
	  signal_win(SEARCHLIST);
	  signal_win(SEARCH_INPUT);
	  break;
	default:
	  if(!isascii(key))
		{
		  vargs->searchlist->key[i++] = (char)key;
		  vargs->searchlist->key[i++] = getch();
		  vargs->searchlist->key[i++] = getch();
		}
	  else
		vargs->searchlist->key[i++] = (char)key;
	  vargs->searchlist->key[i] = '\0';
	  
	  vargs->searchlist->update_signal = 1;
	  signal_win(SEARCH_INPUT);
	  signal_win(SEARCHLIST);
	}
}

void
turnon_search_mode(struct VerboseArgs *vargs)
{
  vargs->searchlist->key[0] = '\0';
  vargs->searchlist->picking_mode = 0;
  vargs->searchlist->wmode.listen_keyboard = &searchlist_keymap;
  playlist_cursor_hide(vargs);

  clean_window(VERBOSE_PROC_BAR);
  clean_window(VISUALIZER);  
  winset_update(&vargs->searchlist->wmode);
}

void
turnoff_search_mode(struct VerboseArgs *vargs)
{
  vargs->searchlist->key[0] = '\0';
  playlist_update(vargs);
  playlist_scroll_to_current(vargs);

  clean_window(SEARCH_INPUT);
  winset_update(&vargs->playlist->wmode);
}

// basically sizes are what we are concerned
static void
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

static void
wchain_init(void)
{
  void (*func[WIN_NUM])(struct VerboseArgs*) =
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

static void
wchain_free(void)
{
  free(wchain);
}
	
static void
winset_init(struct VerboseArgs *vargs)
{
  // for basic mode (main menu)
  vargs->wmode.size = 5;
  vargs->wmode.wins = (struct WindowUnit**)
	malloc(vargs->wmode.size * sizeof(struct WindowUnit*));
  vargs->wmode.wins[0] = &wchain[BASIC_INFO];
  vargs->wmode.wins[1] = &wchain[EXTRA_INFO];  
  vargs->wmode.wins[2] = &wchain[VERBOSE_PROC_BAR];
  vargs->wmode.wins[3] = &wchain[VISUALIZER];
  vargs->wmode.wins[4] = &wchain[HELPER];
  vargs->wmode.update_checking = &basic_state_checking;
  vargs->wmode.listen_keyboard = &basic_keymap;

  // playlist wmode
  vargs->playlist->wmode.size = 6;
  vargs->playlist->wmode.wins = (struct WindowUnit**)
	malloc(vargs->playlist->wmode.size * sizeof(struct WindowUnit*));
  vargs->playlist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  vargs->playlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  vargs->playlist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  vargs->playlist->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  vargs->playlist->wmode.wins[4] = &wchain[BASIC_INFO];
  vargs->playlist->wmode.wins[5] = &wchain[PLAYLIST];
  vargs->playlist->wmode.update_checking = &playlist_update_checking;
  vargs->playlist->wmode.listen_keyboard = &playlist_keymap;

  // searchlist wmode
  vargs->searchlist->wmode.size = 7;
  vargs->searchlist->wmode.wins = (struct WindowUnit**)
	malloc(vargs->searchlist->wmode.size * sizeof(struct WindowUnit*));
  vargs->searchlist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  vargs->searchlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  vargs->searchlist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  vargs->searchlist->wmode.wins[3] = &wchain[SEARCH_INPUT];  
  vargs->searchlist->wmode.wins[4] = &wchain[SIMPLE_PROC_BAR];
  vargs->searchlist->wmode.wins[5] = &wchain[BASIC_INFO];
  vargs->searchlist->wmode.wins[6] = &wchain[SEARCHLIST];  
  vargs->searchlist->wmode.update_checking = &searchlist_update_checking;
  vargs->searchlist->wmode.listen_keyboard = &searchlist_keymap;

  // dirlist wmode
  vargs->dirlist->wmode.size = 6;
  vargs->dirlist->wmode.wins = (struct WindowUnit**)
	malloc(vargs->dirlist->wmode.size * sizeof(struct WindowUnit*));
  vargs->dirlist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  vargs->dirlist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  vargs->dirlist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  vargs->dirlist->wmode.wins[3] = &wchain[DIRLIST];  
  vargs->dirlist->wmode.wins[4] = &wchain[SIMPLE_PROC_BAR];
  vargs->dirlist->wmode.wins[5] = &wchain[BASIC_INFO];
  vargs->dirlist->wmode.update_checking = &dirlist_update_checking;
  vargs->dirlist->wmode.listen_keyboard = &dirlist_keymap;

  // tapelist wmode
  vargs->tapelist->wmode.size = 6;
  vargs->tapelist->wmode.wins = (struct WindowUnit**)
	malloc(vargs->tapelist->wmode.size * sizeof(struct WindowUnit*));
  vargs->tapelist->wmode.wins[0] = &wchain[PLIST_DOWN_STATE_BAR];
  vargs->tapelist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  vargs->tapelist->wmode.wins[2] = &wchain[PLIST_UP_STATE_BAR];
  vargs->tapelist->wmode.wins[3] = &wchain[TAPELIST];  
  vargs->tapelist->wmode.wins[4] = &wchain[SIMPLE_PROC_BAR];
  vargs->tapelist->wmode.wins[5] = &wchain[BASIC_INFO];
  vargs->tapelist->wmode.update_checking = &tapelist_update_checking;
  vargs->tapelist->wmode.listen_keyboard = &tapelist_keymap;
}

static void
winset_free(struct VerboseArgs *vargs)
{
  free(vargs->wmode.wins);
  free(vargs->playlist->wmode.wins);
  free(vargs->searchlist->wmode.wins);
  free(vargs->dirlist->wmode.wins);
  free(vargs->tapelist->wmode.wins);
}

static void
verbose_args_init(struct VerboseArgs *vargs, struct mpd_connection *conn)
{
  vargs->conn = conn;
  /* initialization require redraw too */
  vargs->interval_level = 1;
  vargs->quit_signal = 0;

  /* check the screen size */
  if(stdscr->_maxy < 3 || stdscr->_maxx < 68)
	{
	  endwin();
	  fprintf(stderr, "screen too small for normally displaying.\n");
	  exit(1);
	}
  
  /** playlist arguments */
  vargs->playlist =
	(struct PlaylistArgs*) malloc(sizeof(struct PlaylistArgs));
  vargs->playlist->begin = 1;
  vargs->playlist->length = 0;
  vargs->playlist->current = 0;
  vargs->playlist->cursor = 1;
  /** it's turn out to be good that updating
	  the playlist in the first place **/
  playlist_update(vargs);

  /** searchlist arguments */
  vargs->searchlist =
	(struct SearchlistArgs*) malloc(sizeof(struct SearchlistArgs));

  vargs->searchlist->tags[0] = MPD_TAG_NAME;
  vargs->searchlist->tags[1] = MPD_TAG_TITLE;
  vargs->searchlist->tags[2] = MPD_TAG_ARTIST;
  vargs->searchlist->tags[3] = MPD_TAG_ALBUM;
  vargs->searchlist->crt_tag_id = 0;

  vargs->searchlist->key[0] = '\0';
  vargs->searchlist->plist = vargs->playlist; // windows in this mode

  /** dirlist arguments **/
  // TODO add this automatically according to the mpd setting
  vargs->dirlist =
	(struct DirlistArgs*) malloc(sizeof(struct DirlistArgs));

  vargs->dirlist->begin = 1;
  vargs->dirlist->length = 0;
  vargs->dirlist->cursor = 1;
  strncpy(vargs->dirlist->root_dir, "/home/ted/Music", 128);
  strncpy(vargs->dirlist->crt_dir, vargs->dirlist->root_dir, 512);
  dirlist_update(vargs);

  /** tapelist arguments **/
  vargs->tapelist =
	(struct TapelistArgs*) malloc(sizeof(struct TapelistArgs));

  vargs->tapelist->begin = 1;
  vargs->tapelist->length = 0;
  vargs->tapelist->cursor = 1;
  tapelist_update(vargs);
  
  /** the visualizer **/
  vargs->visualizer =
	(struct VisualizerArgs*) malloc(sizeof(struct VisualizerArgs));
  // TODO add this automatically according to the mpd setting
  strncpy(vargs->visualizer->fifo_file, "/tmp/mpd.fifo", 64);
  get_fifo_id(vargs);
  
  /** windows set initialization **/
  winset_init(vargs);
  winset_update(&vargs->wmode);
}

static void
verbose_args_destroy(mpd_unused struct VerboseArgs *vargs)
{
  winset_free(vargs);
  free(vargs->playlist);
  free(vargs->searchlist);
  free(vargs->dirlist);
  free(vargs->tapelist);
  free(vargs->visualizer);
}

static void
color_init(void)
{
  if(has_colors() == FALSE)
	{
	  endwin();
	  fprintf(stderr, "your terminal does not support color\n");
	  exit(1);
	}
  start_color();
  init_pair(1, COLOR_YELLOW, COLOR_BLACK);
  my_color_pairs[0] = COLOR_PAIR(1) | A_BOLD;
  init_pair(2, COLOR_WHITE, COLOR_BLUE);
  my_color_pairs[1] = COLOR_PAIR(2) | A_BOLD;
  init_pair(3, COLOR_MAGENTA, COLOR_BLACK);  
  my_color_pairs[2] = COLOR_PAIR(3) | A_BOLD;
  init_pair(4, COLOR_RED, COLOR_BLACK);  
  my_color_pairs[3] = COLOR_PAIR(4) | A_BOLD;
  init_pair(5, COLOR_GREEN, COLOR_BLACK);  
  my_color_pairs[4] = COLOR_PAIR(5) | A_BOLD;
  init_pair(6, COLOR_CYAN, COLOR_BLACK);  
  my_color_pairs[5] = COLOR_PAIR(6) | A_BOLD;
  init_pair(7, COLOR_BLUE, COLOR_BLACK);
  my_color_pairs[6] = COLOR_PAIR(7) | A_BOLD;
  init_pair(8, 0, COLOR_BLACK);
  my_color_pairs[7] = COLOR_PAIR(8) | A_BOLD;
  init_pair(9, COLOR_YELLOW, COLOR_WHITE);
  my_color_pairs[8] = COLOR_PAIR(9) | A_BOLD;
  use_default_colors();
}

int
cmd_dynamic(mpd_unused int argc, mpd_unused char **argv,
			struct mpd_connection *conn)
{
  struct VerboseArgs vargs;

  // ncurses basic setting
  initscr();

  timeout(1); // enable the non block getch()
  curs_set(0); // cursor invisible
  noecho();
  keypad(stdscr, TRUE); // enable getch() get KEY_UP/DOWN
  color_init();

  // ncurses for unicode support
  setlocale(LC_ALL, "");

  /** windows chain initilization **/
  wchain_init();

  verbose_args_init(&vargs, conn);
  
  /** main loop for keyboard hit daemon */
  for(;;)
	{
	  being_mode->listen_keyboard(&vargs);

	  if(vargs.quit_signal) break;

	  being_mode->update_checking(&vargs);
	  wchain_size_update();
	  screen_redraw(&vargs);

	  smart_sleep(&vargs);
	}

  endwin();
  wchain_free();
  verbose_args_destroy(&vargs);

  return 0;
}

void
screen_redraw(struct VerboseArgs *vargs)
{
  int i;
  struct WindowUnit **wunit = being_mode->wins;
  for(i = 0; i < being_mode->size; i++)
	{
	  if(wunit[i]->visible && wunit[i]->redraw_signal)
		{
		  wunit[i]->redraw_routine(vargs);
		  wrefresh(wunit[i]->win);
		}
	  wunit[i]->redraw_signal = wunit[i]->flash | 0;
	}
}
