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

static pthread_mutex_t conn_mutex;
static int my_color_pairs[4];

#define LOCK_FRAGILE pthread_mutex_lock(&conn_mutex);
#define UNLOCK_FRAGILE pthread_mutex_unlock(&conn_mutex);

static void 
color_xyprint(int color_scheme, int x, int y, const char *str)
{
  if(color_scheme > 0)
	{
	  attron(my_color_pairs[color_scheme - 1]);
	  if(x >= 0 && y >= 0)
		mvprintw(x, y, "%s", str);
	  else
		printw("%s", str);
	  attroff(my_color_pairs[color_scheme - 1]);	  
	}
  else
	mvprintw(x, y, "%s", str);
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
  static char buff[55];

  if (!mpd_command_list_begin(conn, true) ||
	  !mpd_send_status(conn) ||
	  !mpd_send_current_song(conn) ||
	  !mpd_command_list_end(conn))
	printErrorAndExit(conn);

  status = mpd_recv_status(conn);
  if (status == NULL)
	printErrorAndExit(conn);

  if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	  mpd_status_get_state(status) == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  snprintf(buff, sizeof(buff), "《%s》 - %s\n",
			   mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
			   mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));
	  color_xyprint(1, -1, -1, buff);

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

  refresh();

  mpd_status_free(status);
  my_finishCommand(conn);
}

static void
print_basic_bar(struct mpd_connection *conn)
{
  static int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i, bit_rate, last_bit_rate = 0;
  static struct mpd_status *status;

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
  fill_len = crt_time_perc * AXIS_LENGTH / 100;
  empty_len = AXIS_LENGTH - fill_len;

  move(3, 0);
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
playlist_simple_bar(struct VerboseArgs *vargs)
{
  static int crt_time, crt_time_perc, total_time,
	fill_len, rest_len, i;
  static struct mpd_status *status;
  static const char *rot[4] = {"|", "\\", "—", "/"};
  static const int bar_length = 27, bar_begin = 40;

  status = getStatus(vargs->conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  mpd_status_free(status);

  crt_time_perc = (total_time == 0 ? 0 : 100 * crt_time / total_time);    
  fill_len = crt_time_perc * bar_length / 100;
  rest_len = bar_length - fill_len;

  move(4, bar_begin);

  attron(my_color_pairs[2]);  
  for(i = 0; i < fill_len; printw("*"), i++);
  attroff(my_color_pairs[2]);  

  for(i = 0; i < rest_len; printw("*"), i++);

  move(5 + PLAYLIST_HEIGHT, bar_begin + bar_length - 1);
  attron(my_color_pairs[3]);    
  printw(rot[crt_time % 4]);
  attroff(my_color_pairs[3]);  
  
  refresh();
}

static void
redraw_main_screen(struct mpd_connection *conn)
{
  clear();
  print_basic_song_info(conn);
  print_basic_bar(conn);
  print_basic_help();
}

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

static void
redraw_playlist_screen(struct VerboseArgs *vargs)
{
  static const int init_line = 4;
  static char buff[80];
  static const char *bar = "__________________________/ THE END";
  int i, j;

  clear();
  print_basic_song_info(vargs->conn);

  j = init_line + 1;
  for(i = vargs->playlist->begin - 1; i < vargs->playlist->begin
		+ PLAYLIST_HEIGHT - 1 && i < vargs->playlist->length; i++)
	{
	  snprintf(buff, sizeof(buff), "%3i.  %s        %s",
			   i + 1, vargs->playlist->pretty_title[i],
			   vargs->playlist->artist[i]);
	  if(i + 1 == vargs->playlist->cursor)
		{
		  color_xyprint(2, j++, 0, buff);
		  continue;
		}
	  
	  if(i + 1 == vargs->playlist->current)
		color_xyprint(1, j++, 0, buff);	  
	  else
		color_xyprint(0, j++, 0, buff);
	}

  if(i >= vargs->playlist->length)
	mvprintw(j, 0, "%*s   %s", 3, " ", bar);
  else
	mvprintw(j, 0, "%*s   %s", 3, " ", "... ...");

  if(vargs->playlist->begin > 1)
	mvprintw(init_line, 0, "%*s   %s", 3, " ", "^^^ ^^^");
  
  refresh();
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
	  else
		for(int j = 0; i < size - 1 && j < width - 2*unic - asci; i++, j++)
		  string[i] = ' ';
	}

  string[i] = '\0';
}

static void
playlist_items_update(struct VerboseArgs *vargs)
{
  struct mpd_song *song;
  
  if (!mpd_send_list_queue_meta(vargs->conn))
	printErrorAndExit(vargs->conn);

  int i = 0;
  while ((song = mpd_recv_song(vargs->conn)) != NULL
		 && i < MAX_PLAYLIST_STORE_LENGTH)
	{
	  copy_song_tag(vargs->playlist->title[i],
					mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
					sizeof(vargs->playlist->title[0]), -1);
	  copy_song_tag(vargs->playlist->pretty_title[i],
					mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
					sizeof(vargs->playlist->pretty_title[0]), 26);
	  copy_song_tag(vargs->playlist->artist[i],
					mpd_song_get_tag(song, MPD_TAG_ARTIST, 0),
					sizeof(vargs->playlist->title[0]), -1);	  
	  copy_song_tag(vargs->playlist->album[i],
					mpd_song_get_tag(song, MPD_TAG_ALBUM, 0),
					sizeof(vargs->playlist->title[0]), -1);	  
	  ++i;
	  mpd_song_free(song);
	}

  /* if(i < MAX_PLAYLIST_STORE_LENGTH) */
  /* 	vargs->playlist->length = i; */
  /* else */
  /* 	vargs->playlist->length = MAX_PLAYLIST_STORE_LENGTH; */

  my_finishCommand(vargs->conn);
}

static
int update_playlist_state(struct VerboseArgs *vargs)
{
  struct mpd_status *status;
  int queue_len, song_id, ret = 0;

  status = getStatus(vargs->conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(vargs->playlist->current != song_id)
	{
	  vargs->playlist->current = song_id;
	  ret = 1;
	}

  if(vargs->new_command_signal)
	{
	  vargs->new_command_signal = 0;
	  ret = 1;
	}
  
  if(vargs->playlist->length != queue_len)
	{
	  vargs->playlist->length = queue_len;
	  playlist_items_update(vargs);
	  ret = 1;
	}

  /** for some unexcepted cases vargs->playlist->begin
	  may be greater than vargs->playlist->length;
	  if this happens, we reset the begin's value */
  if(vargs->playlist->begin > vargs->playlist->length)
	vargs->playlist->begin = 1;

  return ret;
}

static void
menu_playlist_print_routine(struct VerboseArgs *vargs)
{
  static int rc;
  
  rc = update_playlist_state(vargs);
  if(rc)
	redraw_playlist_screen(vargs);

  playlist_simple_bar(vargs);
}

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

	  vargs->menu_print_routine(vargs);
	  
	  UNLOCK_FRAGILE

	  usleep(INTERVAL_UNIT);
	}

  return NULL;
}

int
menu_main_keymap(struct VerboseArgs *vargs)
{
  switch(getch())
	{
	case '+': ;
	case '=': ;
	  cmd_volup(0, NULL, vargs->conn); break;
	case KEY_RIGHT:
	  cmd_forward(0, NULL, vargs->conn); break;
	case '-':
	  cmd_voldown(0, NULL, vargs->conn); break;
	case KEY_LEFT:
	  cmd_backward(0, NULL, vargs->conn); break;
	case 'b':
	  cmd_playback(0, NULL, vargs->conn); break;
	case 'n':
	  cmd_next(0, NULL, vargs->conn); break;
	case 'p':
	  cmd_prev(0, NULL, vargs->conn); break;
	case 't': ;
	case ' ':
	  cmd_toggle(0, NULL, vargs->conn); break;
	case 'r':
	  cmd_random(0, NULL, vargs->conn); break;
	case 's':
	  cmd_single(0, NULL, vargs->conn); break;
	case 'R':
	  cmd_repeat(0, NULL, vargs->conn); break;
	case 'L': // redraw screen
	  break;
	case '\t':
	  vargs->menu_id = MENU_PLAYLIST;
	  vargs->menu_print_routine = &menu_playlist_print_routine;
	  vargs->menu_keymap = &menu_playlist_keymap;	  
	  break;
	case 'e': ;
	case 'q':
	  return 1;
	default:
	  return 0;
	}

  vargs->new_command_signal = 1;

  return 0;
}

static void
playlist_scroll(struct VerboseArgs *vargs, int lines)
{
  static struct PlaylistMenuArgs *pl;

  pl = vargs->playlist;
  
  pl->cursor += lines;
	
  if(pl->cursor > pl->length)
	pl->cursor = pl->length;
  else if(pl->cursor < 1)
	pl->cursor = 1;

  pl->begin = pl->cursor - PLAYLIST_HEIGHT / 2;

  if(pl->length - pl->cursor < PLAYLIST_HEIGHT / 2)
	pl->begin = pl->length - PLAYLIST_HEIGHT + 1;

  if(pl->cursor < PLAYLIST_HEIGHT / 2)
	pl->begin = 1;

  // this expression should always be false
  if(pl->begin < 1)
	pl->begin = 1;
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
  snprintf(args, sizeof(args), "%i", vargs->playlist->cursor);
  cmd_play(1, &args, vargs->conn);
  free(args);
}

static void
cmd_nextsong(struct VerboseArgs* vargs)
{
  struct mpd_status *status;
  int song_id;
  
  cmd_next(0, NULL, vargs->conn);

  status = getStatus(vargs->conn);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  playlist_scroll(vargs, song_id - vargs->playlist->cursor);
}

static void
cmd_prevsong(struct VerboseArgs* vargs)
{
  struct mpd_status *status;
  int song_id;
  
  cmd_prev(0, NULL, vargs->conn);

  status = getStatus(vargs->conn);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  playlist_scroll(vargs, song_id - vargs->playlist->cursor);
}

int
menu_playlist_keymap(struct VerboseArgs* vargs)
{
  switch(getch())
	{
	case KEY_DOWN:;
	case 'j':
	  playlist_scroll_down_line(vargs);break;
	case KEY_UP:;
	case 'k':
	  playlist_scroll_up_line(vargs);break;
	case 'b':
	  playlist_scroll_up_page(vargs);break;
	case ' ':
	  playlist_scroll_down_page(vargs);break;
	case 'n':
	  cmd_nextsong(vargs); break;
	case 'p':
	  cmd_prevsong(vargs); break;
	case '\n':
	  playlist_play_current(vargs);break;

	case '+': ;
	case '=': ;
	  cmd_volup(0, NULL, vargs->conn); break;
	case KEY_RIGHT:
	  cmd_forward(0, NULL, vargs->conn); break;
	case '-':
	  cmd_voldown(0, NULL, vargs->conn); break;
	case KEY_LEFT:
	  cmd_backward(0, NULL, vargs->conn); break;
	case 't':
	  cmd_toggle(0, NULL, vargs->conn); break;
	case 'r':
	  cmd_random(0, NULL, vargs->conn); break;
	case 's':
	  cmd_single(0, NULL, vargs->conn); break;
	case 'R':
	  cmd_repeat(0, NULL, vargs->conn); break;
	case 'L':
	  break;

	case 'i':;
	case 'l':  // cursor goto current playing place
	  playlist_scroll(vargs, vargs->playlist->current
					  - vargs->playlist->cursor);
	  break;
	case 'g':  // cursor goto the beginning
	  playlist_scroll(vargs, 1 - vargs->playlist->cursor);
	  break;
	case 'G':  // cursor goto the end
	  playlist_scroll(vargs, vargs->playlist->length
					  - vargs->playlist->cursor);
	  break;
	case 'c':  // cursor goto the center
	  playlist_scroll(vargs, vargs->playlist->length / 2
					  - vargs->playlist->cursor);
	  break;
	case '\t':
	  vargs->menu_id = MENU_MAIN;
	  vargs->menu_print_routine = &menu_main_print_routine;
	  vargs->menu_keymap = &menu_main_keymap;
	  break;
	case 'e': ;
	case 'q':
	  return 1;
	default:
	  return 0;
	}

  vargs->new_command_signal = 1;	  
  return 0;
}

static void
verbose_args_init(struct VerboseArgs *vargs, struct mpd_connection *conn)
{
  vargs->conn = conn;
  vargs->menu_id = MENU_MAIN;
  vargs->menu_print_routine = &menu_main_print_routine;
  vargs->menu_keymap = &menu_main_keymap;
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
  my_color_pairs[0] = COLOR_PAIR(1) | A_BOLD;
  init_pair(2, COLOR_WHITE, COLOR_BLUE);
  my_color_pairs[1] = COLOR_PAIR(2) | A_BOLD;
  init_pair(3, COLOR_MAGENTA, COLOR_BLACK);  
  my_color_pairs[2] = COLOR_PAIR(3) | A_BOLD;
  init_pair(4, COLOR_GREEN, COLOR_BLACK);  
  my_color_pairs[3] = COLOR_PAIR(4) | A_BOLD;
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
  timeout(1); // enable the non block getch()
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
	  LOCK_FRAGILE
	  is_quit = vargs.menu_keymap(&vargs);
	  UNLOCK_FRAGILE
	  
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
