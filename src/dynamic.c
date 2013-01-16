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
#include <pthread.h>
#include <curses.h>
#include <locale.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); return -1; } while(0)

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

enum my_menu_id
{
  MENU_MAIN,
  MENU_PLAYLIST,
  //	MENU_QUEUE,
  MENU_NUMBER
};

#define MAX_PLAYLIST_STORE_LENGTH 1000

struct PlaylistMenuArgs
{
  char **items;
  int length;
  int begin;
  int current; // current playing song id
  int cursor;  // marked as song id
};

struct VerboseArgs
{
  struct mpd_connection *conn;
  /** set to 1 once commands have been triggered by keyboad*/
  int new_command_signal;
  int menu_id;
  struct PlaylistMenuArgs *playlist;
};

static pthread_mutex_t conn_mutex;
#define LOCK_FRAGILE pthread_mutex_lock(&conn_mutex);
#define UNLOCK_FRAGILE pthread_mutex_unlock(&conn_mutex);

static void 
color_xyprint(int color_scheme, int x, int y, const char *str)
{
  if(color_scheme != 0)
	{
	  attron(COLOR_PAIR(1));
	  mvaddstr(x, y, str);
	  attroff(COLOR_PAIR(1));
	}
  else
	mvaddstr(x, y, str);	
}

static int
check_new_state(struct VerboseArgs *vargs)
{
  static int repeat, randomm, single, queue_len, id, volume, play,
	rep, ran, sin, que, idd, vol, ply;
  struct mpd_status *status;
  
  if(vargs->new_command_signal == 0)
	{
	  status = getStatus(vargs->conn);
	  rep = mpd_status_get_repeat(status);
	  ran = mpd_status_get_random(status);
	  sin = mpd_status_get_single(status);
	  que = mpd_status_get_queue_length(status);
	  idd = mpd_status_get_song_id(status);
	  vol = mpd_status_get_volume(status);
	  ply = mpd_status_get_state(status);
	  mpd_status_free(status);
	  if(rep != repeat || ran != randomm || sin != single
		 || que != queue_len || idd != id || vol != volume
		 || ply != play)
		{
		  repeat = rep;
		  randomm = ran;
		  single = sin;
		  queue_len = que;
		  id = idd;
		  volume = vol;
		  play = ply;
		  return 2;
		}
		
	  return 0;
	}
  else // new local command triggered by key strike
	vargs->new_command_signal = 0;
  
  return 1;
}

static void
print_basic_help(void)
{
  move(5, 0);
  printw("  [n] :\tNext song\t\t  [R] :\tToggle repeat\n");
  printw("  [p] :\tPrevious song\t\t  [r] :\tToggle random\n");
  printw("  [-] :\tVolume down\t\t  [s] :\tToggle single\n");
  printw("  [=] :\tVolume Up\t\t  [b] :\tPlayback\n");
  printw("  [t] :\tPlay / Pause\t\t  [l] :\tRedraw screen\n");
  printw("  <L> :\tSeek backward\t\t  <R> :\tSeek forward\n");
  printw("\n  [e/q] : Quit\n");
}

static void
print_basic_song_info(struct mpd_connection* conn)
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

  clear();
	  
  if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	  mpd_status_get_state(status) == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  // pretty_print_song with ncurses portability
	  printw("%s\n",
			 songToFormatedString(song, options.format, NULL));

	  mpd_song_free(song);
	}

	if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  printw("[playing]");
	else
	  printw("[paused] ");

	printw("     #%3i/%3u      %i:%02i\n",
		   mpd_status_get_song_pos(status) + 1,
		   mpd_status_get_queue_length(status),
		   mpd_status_get_total_time(status) / 60,
		   mpd_status_get_total_time(status) % 60
		   );
  }
	
  if (mpd_status_get_update_id(status) > 0)
	printw("Updating DB (#%u) ...\n",
		   mpd_status_get_update_id(status));

  if (mpd_status_get_volume(status) >= 0)
	printw("volume:%3i%c   ", mpd_status_get_volume(status), '%');
  else {
	printw("volume: n/a   ");
  }

  printw("Repeat: ");
  if (mpd_status_get_repeat(status))
	printw("on    ");
  else printw("off   ");

  printw("random: ");
  if (mpd_status_get_random(status))
	printw("on    ");
  else printw("off   ");

  printw("single: ");
  if (mpd_status_get_single(status))
	printw("on\n");
  else printw("off\n");

  if (mpd_status_get_error(status) != NULL)
	printw("ERROR: %s\n",
		   charset_from_utf8(mpd_status_get_error(status)));

  print_basic_help();
  refresh();

  mpd_status_free(status);
  my_finishCommand(conn);
}

static void
print_basic_bar(struct mpd_connection *conn)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i, bit_rate;
  static int last_bit_rate = 0;
  const int axis_len = 40;
  struct mpd_status *status;

  status = getStatus(conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  bit_rate = mpd_status_get_kbit_rate(status);
  mpd_status_free(status);

  /* as many songs's bit rate varies while playing
	 we refresh the bit rate display only when the 
	 relevant change greater than 0.2 */
  if(abs(bit_rate - last_bit_rate) / (float)(last_bit_rate + 1) > 0.2)
	last_bit_rate = bit_rate;
  else
	bit_rate = last_bit_rate;

  crt_time_perc = (total_time == 0 ? 0 : 100 * crt_time / total_time);  
  fill_len = crt_time_perc * axis_len / 100;
  empty_len = axis_len - fill_len;

  move(3, 0);
  printw("\r");
  printw("[");
  for(i = 0; i < fill_len; printw("="), i++);
  printw(">");
  for(i = 0; i < empty_len; printw(" "), i++);
  printw("]");
  
  printw("%3i%% %5ik/s %3i:%02i/%i:%02i%*s",
		 crt_time_perc,
		 bit_rate,
		 crt_time / 60,
		 crt_time % 60,
		 total_time / 60,
		 total_time % 60,
		 8, " ");

  refresh();
}

static void
redraw_main_screen(struct mpd_connection *conn)
{
  print_basic_song_info(conn);
  print_basic_bar(conn);
}

#define INTERVAL_UNIT 30000

static void
menu_main_print_routine(struct VerboseArgs *vargs)
{
  if(check_new_state(vargs))
	{
	  // refresh the status display
	  redraw_main_screen(vargs->conn);
	}

  print_basic_bar(vargs->conn);
}

#define PLAYLIST_HEIGHT 18

static void
redraw_playlist_screen(struct VerboseArgs *vargs)
{
  clear();

  int j = 4;
  for(int i = vargs->playlist->begin - 1; i < vargs->playlist->begin
		+ PLAYLIST_HEIGHT - 1 && i < vargs->playlist->length; i++)
	{
	  if(i + 1 == vargs->playlist->cursor)
		{
		  color_xyprint(2, j++, 0, vargs->playlist->items[i]);
		  continue;
		}
	  
	  if(i + 1 == vargs->playlist->current)
		color_xyprint(1, j++, 0, vargs->playlist->items[i]);
	  else
		color_xyprint(0, j++, 0, vargs->playlist->items[i]);
	}
}

static void
playlist_items_update(struct VerboseArgs *vargs)
{
  struct mpd_song *song;
  
  if (!mpd_send_list_queue_meta(vargs->conn))
	printErrorAndExit(vargs->conn);

  int i = 0;
  while ((song = mpd_recv_song(vargs->conn)) != NULL)
	{
	  snprintf(vargs->playlist->items[i], 127, "%s",
			   songToFormatedString(song, options.format, NULL));
	  ++i;
	  mpd_song_free(song);
	}

  if(i < MAX_PLAYLIST_STORE_LENGTH)
	vargs->playlist->length = i;
  else
	vargs->playlist->length = MAX_PLAYLIST_STORE_LENGTH;

  struct mpd_status *status = getStatus(vargs->conn);
  vargs->playlist->current = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);
 
  my_finishCommand(vargs->conn);
}

static
int check_playlist_state(struct VerboseArgs *vargs)
{
  struct mpd_status *status;
  int queue_len, song_id;

  status = getStatus(vargs->conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(vargs->playlist->length != queue_len
	 || vargs->playlist->current != song_id)
	return 1;

  return 0;
}

static void
menu_playlist_print_routine(struct VerboseArgs *vargs)
{
  if(check_playlist_state(vargs))
	{
	  playlist_items_update(vargs);
	  redraw_playlist_screen(vargs);
	}

  if(check_new_state(vargs))
	redraw_playlist_screen(vargs);
}

#define SEEK_UNIT 3
#define VOLUME_UNIT 3

static void
cmd_forward(mpd_unused int argc, mpd_unused char **argv,
			struct mpd_connection  *conn)
{
  char *args = malloc(8 * sizeof(char));
  sprintf(args, "+%i%%", SEEK_UNIT);
  cmd_seek(1, &args, conn);
  free(args);
}

static void
cmd_backward(mpd_unused int argc, mpd_unused char **argv,
			 struct mpd_connection  *conn)
{
  char *args = malloc(8 * sizeof(char));
  sprintf(args, "-%i%%", SEEK_UNIT);
  cmd_seek(1, &args, conn);
  free(args);
}

static int
cmd_volup(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
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
cmd_voldown(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
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

static void*
menu_rendering(void *args)
{
  struct VerboseArgs *vargs;

  vargs = (struct VerboseArgs*)args;
  for(;;)
	{
	  LOCK_FRAGILE

	  if(vargs->menu_id == MENU_MAIN)
		menu_main_print_routine(vargs);
	  else if(vargs->menu_id == MENU_PLAYLIST)
		menu_playlist_print_routine(vargs);

	  UNLOCK_FRAGILE

	  usleep(INTERVAL_UNIT);
	}

  return NULL;
}

#define MAIN_CASE_EXECUTE(command_name)			\
  LOCK_FRAGILE									\
  command_name(0, NULL, vargs->conn);			\
  vargs->new_command_signal = 1;				\
  UNLOCK_FRAGILE								\
  break;

static int
menu_main_keyboard(struct VerboseArgs *vargs)
{
  switch(getch())
	{
	case '+': ;
	case '=': ;
	  MAIN_CASE_EXECUTE(cmd_volup)
	case KEY_RIGHT:
		MAIN_CASE_EXECUTE(cmd_forward)
	case '-':
		MAIN_CASE_EXECUTE(cmd_voldown)
	case KEY_LEFT:
		MAIN_CASE_EXECUTE(cmd_backward)
	case 'b':
		MAIN_CASE_EXECUTE(cmd_playback)
	case 'n':
		MAIN_CASE_EXECUTE(cmd_next)
	case 'p':
		MAIN_CASE_EXECUTE(cmd_prev)
	case 't': ;
	case ' ':
	  MAIN_CASE_EXECUTE(cmd_toggle)
	case 'r':
	  MAIN_CASE_EXECUTE(cmd_random)
	case 's':
	  MAIN_CASE_EXECUTE(cmd_single)
	case 'R':
	  MAIN_CASE_EXECUTE(cmd_repeat)
	case 'l':
	  LOCK_FRAGILE
	  vargs->new_command_signal = 1;
	  UNLOCK_FRAGILE
	  break;
	case 'c':
	  LOCK_FRAGILE
	  vargs->menu_id = MENU_PLAYLIST;
	  vargs->new_command_signal = 1;	  	  
	  UNLOCK_FRAGILE
	  break;
	case 'e': ;
	case 'q':
	  return 1;
	default: ;
	}
  
  return 0;
}

static void
playlist_scroll_single(struct VerboseArgs *vargs, int sign)
{  
  vargs->playlist->cursor += sign;
	
  if(vargs->playlist->cursor > vargs->playlist->length)
	vargs->playlist->cursor = vargs->playlist->length;
  else if(vargs->playlist->cursor < 1)
	vargs->playlist->cursor = 1;

  if(vargs->playlist->cursor > PLAYLIST_HEIGHT / 2 &&
	 vargs->playlist->length - vargs->playlist->cursor + 1 >= PLAYLIST_HEIGHT / 2)
	{
	  vargs->playlist->begin += sign;

	  if(vargs->playlist->begin > vargs->playlist->length - PLAYLIST_HEIGHT + 1)
		vargs->playlist->begin = vargs->playlist->length - PLAYLIST_HEIGHT + 1;

	  if(vargs->playlist->begin < 1)
		vargs->playlist->begin = 1;

	  vargs->playlist->cursor = vargs->playlist->begin + PLAYLIST_HEIGHT / 2;
	}
}

static void
playlist_scroll(struct VerboseArgs *vargs, int lines)
{
  int s = lines > 0 ? 1 : -1, l = abs(lines);

  for(int i = 0; i < l; i++)
	playlist_scroll_single(vargs, s);
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

#define PLAYLIST_CASE_EXECUTE(command_name)		\
  LOCK_FRAGILE									\
  command_name(vargs);							\
  vargs->new_command_signal = 1;				\
  UNLOCK_FRAGILE								\
  break;

static int
menu_playlist_keyboard(struct VerboseArgs* vargs)
{
  switch(getch())
	{
	case 'j':
	  PLAYLIST_CASE_EXECUTE(playlist_scroll_down_line)
	case 'k':
	  PLAYLIST_CASE_EXECUTE(playlist_scroll_up_line)
	case 'b':
	  PLAYLIST_CASE_EXECUTE(playlist_scroll_up_page)
	case ' ':
	  PLAYLIST_CASE_EXECUTE(playlist_scroll_down_page)
	case 'c':
	  LOCK_FRAGILE
	  vargs->menu_id = MENU_MAIN;
	  vargs->new_command_signal = 1;	  
	  UNLOCK_FRAGILE
	  break;
	case 'e': ;
	case 'q':
	  return 1;
	default: ;
	}

  return 0;
}

static void
verbose_args_init(struct VerboseArgs *vargs, struct mpd_connection *conn)
{
  vargs->conn = conn;
  vargs->menu_id = MENU_MAIN;
  /* initialization require redraw too */
  vargs->new_command_signal = 1;

  /** playlist arguments */
  vargs->playlist =
	(struct PlaylistMenuArgs*) malloc(sizeof(struct PlaylistMenuArgs));
  vargs->playlist->items =
	(char**) malloc(MAX_PLAYLIST_STORE_LENGTH * sizeof(char*));
  for(int i = 0; i < MAX_PLAYLIST_STORE_LENGTH; i++)
	vargs->playlist->items[i] =
	  (char*)malloc(128 * sizeof(char));
  vargs->playlist->begin = 1;
  vargs->playlist->length = 0;
  vargs->playlist->current = 0;
  vargs->playlist->cursor = 1;
}

static void
verbose_args_destroy(mpd_unused struct VerboseArgs *vargs)
{
  for(int i = 0; i < MAX_PLAYLIST_STORE_LENGTH; i++)
	  free(vargs->playlist->items[i]);
  free(vargs->playlist->items);

  free(vargs->playlist);
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
  init_pair(2, COLOR_WHITE, COLOR_BLUE);  
  use_default_colors();
}

int
cmd_dynamic(mpd_unused int argc, mpd_unused char **argv,
			struct mpd_connection *conn)
{
  pthread_t thread_menu_rendering;
  int rc, is_quit;
  struct VerboseArgs vargs;

  // ncurses basic setting
  initscr();
  timeout(-1); // no time out for keyboard stroke
  curs_set(0); // cursor invisible
  noecho();
  keypad(stdscr, TRUE); // enable getch() get KEY_UP/DOWN
  color_init();

  // ncurses for unicode support
  setlocale(LC_ALL, "");

  pthread_mutex_init(&conn_mutex, NULL);

  verbose_args_init(&vargs, conn);
  
  rc = pthread_create(&thread_menu_rendering, NULL,
					  menu_rendering, (void*)&vargs);
  if(rc)
	{
	  endwin();
	  DIE("thread init failed,\nexit...\n");
	}

  /** main loop for keyboard hit daemon */
  for(;;)
	{
	  if(vargs.menu_id == MENU_MAIN)
		is_quit = menu_main_keyboard(&vargs);
	  else if(vargs.menu_id == MENU_PLAYLIST)
	  	is_quit = menu_playlist_keyboard(&vargs);
	  else
		is_quit = 1;

	  if(is_quit)
		{
		  pthread_cancel(thread_menu_rendering);
		  break;
		}

	  usleep(INTERVAL_UNIT);	  
	}

  endwin();
  
  pthread_join(thread_menu_rendering, NULL);
  pthread_mutex_destroy(&conn_mutex);

  verbose_args_destroy(&vargs);

  return 0;
}
