#include "dynamic.h"
//#include "command.h"

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
#include <assert.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); return -1; } while(0)

static struct WindowUnit *wchain; // array stores all subwindows
static struct WinMode *being_mode; // pointer to current used window set
static int my_color_pairs[8];


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
static void
printErrorAndExit(struct mpd_connection *conn)
{
	const char *message;

	assert(mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS);

	message = mpd_connection_get_error_message(conn);

	fprintf(stderr, "error: %s\n", message);
	mpd_connection_free(conn);
	exit(EXIT_FAILURE);
}

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

static mpd_unused void debug(const char *debug_info)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, debug_info);
  wrefresh(win);
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

  if (!mpd_run_set_volume(conn, volume))
	printErrorAndExit(conn);
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

int cmd_repeat ( int argc, char ** argv, struct mpd_connection *conn )
{
	int mode;

	if(argc==1) {
		mode = get_boolean(argv[0]);
		if (mode < 0)
			return -1;
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);
		mode = !mpd_status_get_repeat(status);
		mpd_status_free(status);
	}

	if (!mpd_run_repeat(conn, mode))
		printErrorAndExit(conn);

	return 1;
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
  snprintf(args, sizeof(args), "%i", vargs->playlist->id[i-1]);
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
playlist_delete_song(struct VerboseArgs *vargs)
{
  char *args = (char*)malloc(8 * sizeof(char));
  int i = vargs->playlist->cursor;
  
  if(i < 1 || i > vargs->playlist->length)
	return;
	
  snprintf(args, sizeof(args), "%i", vargs->playlist->id[i - 1]);
  cmd_del(1, &args, vargs->conn);
  free(args);
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
static
void smart_sleep(struct VerboseArgs* vargs)
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
	wprintw(win, "ERROR: %s\n", mpd_status_get_error(status));

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
print_playlist_item(WINDOW *win, int line, int color, int id,
					char *title, char *artist)
{
  const int title_left = 6;
  const int artist_left = 43;
  const int width = win->_maxx - 8;

  wattron(win, my_color_pairs[color - 1]);

  mvwprintw(win, line, 0, "%*c", width, ' ');
  mvwprintw(win, line, 0, "%3i.", id);
  mvwprintw(win, line, title_left, "%s", title);
  mvwprintw(win, line, artist_left, "%s", artist);
  
  wattroff(win, my_color_pairs[color - 1]);
}

static void
redraw_playlist_screen(struct VerboseArgs *vargs)
{
  int line = 0, i, height = wchain[PLAYLIST].win->_maxy + 1;

  WINDOW *win = specific_win(PLAYLIST);  

  int id;
  char *title, *artist;
  for(i = vargs->playlist->begin - 1; i < vargs->playlist->begin
		+ height - 1 && i < vargs->playlist->length; i++)
	{
	  id = vargs->playlist->id[i];
	  title = vargs->playlist->pretty_title[i];
	  artist = vargs->playlist->artist[i];

	  if(i + 1 == vargs->playlist->cursor)
		{
		  print_playlist_item(win, line++, 2, id, title, artist);
		  continue;
		}
	  
	  if(vargs->playlist->id[i] == vargs->playlist->current)
		{
		  print_playlist_item(win, line++, 1, id, title, artist);
		}
	  else
		{
		  print_playlist_item(win, line++, 0, id, title, artist);
		}
	}
}

static void
copy_song_tag(char *string, const char * tag, int size, int width)
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

static void
update_playlist(struct VerboseArgs *vargs)
{
  struct mpd_song *song;
  
  if (!mpd_send_list_queue_meta(vargs->conn))
	printErrorAndExit(vargs->conn);

  int i = 0;
  while ((song = mpd_recv_song(vargs->conn)) != NULL
		 && i < MAX_PLAYLIST_STORE_LENGTH)
	{
	  copy_song_tag(vargs->playlist->title[i],
					get_song_tag(song, MPD_TAG_TITLE),
					sizeof(vargs->playlist->title[0]), -1);
	  copy_song_tag(vargs->playlist->pretty_title[i],
					get_song_tag(song, MPD_TAG_TITLE),
					sizeof(vargs->playlist->pretty_title[0]), 26);
	  copy_song_tag(vargs->playlist->artist[i],
					get_song_tag(song, MPD_TAG_ARTIST),
					sizeof(vargs->playlist->title[0]), 20);	  
	  copy_song_tag(vargs->playlist->album[i],
					get_song_tag(song, MPD_TAG_ALBUM),
					sizeof(vargs->playlist->title[0]), -1);
	  vargs->playlist->id[i] = i + 1;
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
update_searchlist(struct VerboseArgs* vargs)
{
  char (*tag_name)[128], *key = vargs->searchlist->key;
  int i, j, ct = vargs->searchlist->crt_tag_id;

  /* we examine and update the playlist (datebase for searching)
   * every time, see if any modification (delete or add songs)
   * was made by other client during searching */
  update_playlist(vargs);

  // now plist is fresh
  switch(vargs->searchlist->tags[ct])
	{
	case MPD_TAG_TITLE:
	  tag_name = vargs->searchlist->plist->title; break;	  
	case MPD_TAG_ARTIST:
	  tag_name = vargs->searchlist->plist->artist; break;
	case MPD_TAG_ALBUM:
	  tag_name = vargs->searchlist->plist->album; break;
	default:
	  tag_name = NULL;
	}

  int is_substr;
  
  for(i = j = 0; i < vargs->searchlist->plist->length; i++)
	{
	  if(tag_name != NULL)
		is_substr = is_substring_ignorecase
		  (tag_name[i], key) != NULL;
	  else
		{
		  is_substr = is_substring_ignorecase
			(vargs->searchlist->plist->title[i], key) != NULL ||
			is_substring_ignorecase
			(vargs->searchlist->plist->album[i], key) != NULL ||
			is_substring_ignorecase
			(vargs->searchlist->plist->artist[i], key) != NULL;
		}

	  if(is_substr)
		{
		  strcpy(vargs->searchlist->plist->title[j],
				 vargs->searchlist->plist->title[i]);
		  strcpy(vargs->searchlist->plist->artist[j],
				 vargs->searchlist->plist->artist[i]);
		  strcpy(vargs->searchlist->plist->album[j],
				 vargs->searchlist->plist->album[i]);
		  strcpy(vargs->searchlist->plist->pretty_title[j], vargs->searchlist->plist->pretty_title[i]);
		  vargs->searchlist->plist->id[j] = vargs->searchlist->plist->id[i];
		  j ++;
		}
	}
  vargs->searchlist->plist->length = j;
  vargs->searchlist->plist->begin = 1;
}

static
void playlist_update_checking(struct VerboseArgs *vargs)
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
	  signal_win(BASIC_INFO);
	  signal_win(PLIST_UP_STATE_BAR);
	  signal_win(PLAYLIST);
	  signal_win(PLIST_DOWN_STATE_BAR);;
	}

  if(vargs->playlist->length != queue_len)
	{
	  update_playlist(vargs);
	  signal_win(BASIC_INFO);
	  signal_win(PLIST_UP_STATE_BAR);
	  signal_win(PLAYLIST);
	  signal_win(PLIST_DOWN_STATE_BAR);;
	}

  /** for some unexcepted cases vargs->playlist->begin
	  may be greater than vargs->playlist->length;
	  if this happens, we reset the begin's value */
  if(vargs->playlist->begin > vargs->playlist->length)
	vargs->playlist->begin = 1;
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
	  update_searchlist(vargs);
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
	  
	case KEY_DOWN:;
	case 'j':
	  playlist_scroll_down_line(vargs);break;
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
	  playlist_delete_song(vargs); break;
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
  update_playlist(vargs);
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
	  {height - 8, 73, 5, 0},	// PLAYLIST
	  {1, width, height - 3, 0},	// PLIST_DOWN_STATE_BAR
	  {height - 8, 72, 5, 0},	// SEARCHLIST
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
	  &redraw_playlist_screen,	 // PLAYLIST
	  &playlist_down_state_bar,  // PLIST_DOWN_STATE_BAR
	  &redraw_playlist_screen,	 // SEARCHLIST       
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
}

static void
winset_free(struct VerboseArgs *vargs)
{
  free(vargs->wmode.wins);
  free(vargs->playlist->wmode.wins);
  free(vargs->searchlist->wmode.wins);
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
  update_playlist(vargs);

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

  /** the visualizer **/
  vargs->visualizer =
	(struct VisualizerArgs*) malloc(sizeof(struct VisualizerArgs));
  strncpy(vargs->visualizer->fifo_file, "/tmp/mpd.fifo",
		  sizeof(vargs->visualizer->fifo_file));
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
