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

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); return -1; } while(0)

static struct WindowUnit wchain[WIN_NUM]; // array stores all subwindows
static struct WinMode *being_mode; // pointer to current used window set

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
clean_screen(void)
{
  int i;
  for(i = 0; i < being_mode->size; i++)
	{
	  werase(being_mode->wins[i]->win);
	  wrefresh(being_mode->wins[i]->win);
	}
}

static mpd_unused void debug(const char *debug_info)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, debug_info);
  wrefresh(win);
}

static mpd_unused void debug_int(const int num)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, "%d", num);
  wrefresh(win);
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

static void
winset_update(struct WinMode *wmode)
{
  being_mode = wmode;
  clean_screen();
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

/* the principle is that: if the keyboard events
 * occur frequently, then adjust the update rate
 * higher, if the keyboard is just idle, keep it
 * low. */
static
void smart_sleep(struct VerboseArgs* vargs)
{
  static int us = INTERVAL_MAX_UNIT;

  if(vargs->key_hit)
	{
	  us = INTERVAL_MIN_UNIT;
	  vargs->key_hit = 0;
	}
  else
	us = us < INTERVAL_MAX_UNIT ? us + INTERVAL_INCREMENT : us;

  usleep(us);
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

static void
basic_state_checking(struct VerboseArgs *vargs)
{
  static int repeat, randomm, single, queue_len, id, volume, play,
	rep, ran, sin, que, idd, vol, ply;
  struct mpd_status *status;
  
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

	  signal_all_wins();
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
  wrefresh(win);
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

static void
print_basic_song_info(struct VerboseArgs *vargs)
{
  struct mpd_connection *conn = vargs->conn;
  struct mpd_status *status;
  static char buff[80], format[20];

  WINDOW *win = specific_win(BASIC_INFO);
  
  status = init_mpd_status(conn);
  if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	  mpd_status_get_state(status) == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  static int twid = 38, awid = 28;
	  color_print(win, 1, "《 ");
	  if(snprintf(buff, twid, "%s",
				  get_song_tag(song, MPD_TAG_TITLE)) >= twid)
		strcpy(buff + twid - 4, "..."); // in case of long title
	  color_print(win, 1, buff);
	  color_print(win, 1, " 》  by  ");

	  if(snprintf(buff, awid, "%s",
				  get_song_tag(song, MPD_TAG_ARTIST)) >= awid)
		strcpy(buff + awid - 4, "...");
	  color_print(win, 5, buff);

	  snprintf(format, sizeof(format), "* %s *", get_song_format(song));

	  mpd_song_free(song);
	}

	if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  wprintw(win, "\n[playing]");
	else
	  wprintw(win, "\n[paused] ");

	wprintw(win, "     #%3i/%3u      %i:%02i",
			mpd_status_get_song_pos(status) + 1,
			mpd_status_get_queue_length(status),
			mpd_status_get_total_time(status) / 60,
			mpd_status_get_total_time(status) % 60
			);
  }

  if(format[0] != '\0')
	wprintw(win, "\t  %s\n", format);
  else
	wprintw(win, "\n");
	
  if (mpd_status_get_update_id(status) > 0)
	wprintw(win, "Updating DB (#%u) ...\n",
			mpd_status_get_update_id(status));

  if (mpd_status_get_volume(status) >= 0)
	wprintw(win, "volume:%3i%c   ", mpd_status_get_volume(status), '%');
  else {
	wprintw(win, "volume: n/a   ");
  }

  wprintw(win, "Repeat: ");
  if (mpd_status_get_repeat(status))
	wprintw(win, "on    ");
  else wprintw(win, "off   ");

  wprintw(win, "random: ");
  if (mpd_status_get_random(status))
	wprintw(win, "on    ");
  else wprintw(win, "off   ");

  wprintw(win, "single: ");
  if (mpd_status_get_single(status))
	wprintw(win, "on    ");
  else wprintw(win, "off   ");

  wprintw(win, "Search: ");
  color_print(win, 6,
			  mpd_tag_name(vargs->searchlist->tags
						   [vargs->searchlist->crt_tag_id]));
  wprintw(win, "  ");

  if (mpd_status_get_error(status) != NULL)
	wprintw(win, "ERROR: %s\n",
			charset_from_utf8(mpd_status_get_error(status)));

  mpd_status_free(status);
  my_finishCommand(conn);

  wrefresh(win);
}

static void // VERBOSE_PROC_BAR
print_basic_bar(struct VerboseArgs *vargs)
{
  static int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i, bit_rate, last_bit_rate = 0;
  static struct mpd_status *status;
  struct mpd_connection *conn = vargs->conn;

  WINDOW *win = specific_win(VERBOSE_PROC_BAR);
  
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
  wprintw(win, "[");
  wattron(win, my_color_pairs[2]);
  for(i = 0; i < fill_len; wprintw(win, "="), i++);
  wprintw(win, ">");
  for(i = 0; i < empty_len; wprintw(win, " "), i++);
  wattroff(win, my_color_pairs[2]);
  wprintw(win, "]");
  
  wprintw(win, "%3i%% %5ik/s %3i:%02i%*s",
		  crt_time_perc,
		  bit_rate,
		  crt_time / 60,
		  crt_time % 60,
		  8, " ");

  wrefresh(win);
}

static void
playlist_simple_bar(struct VerboseArgs *vargs)
{
  static int crt_time, crt_time_perc, total_time,
	fill_len, rest_len, i;
  static struct mpd_status *status;
  static const int bar_length = 27, bar_begin = 43;

  WINDOW *win = specific_win(SIMPLE_PROC_BAR);
  
  status = getStatus(vargs->conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  mpd_status_free(status);

  crt_time_perc = (total_time == 0 ? 0 : 100 * crt_time / total_time);    
  fill_len = crt_time_perc * bar_length / 100;
  rest_len = bar_length - fill_len;

  move(4, bar_begin);

  wattron(win, my_color_pairs[2]);  
  for(i = 0; i < fill_len; wprintw(win, "*"), i++);
  wattroff(win, my_color_pairs[2]);  

  for(i = 0; i < rest_len; wprintw(win, "*"), i++);

  wrefresh(win);
}

static void
playlist_up_state_bar(struct VerboseArgs *vargs)
{
  WINDOW *win = specific_win(PLIST_UP_STATE_BAR);  

  if(vargs->playlist->begin > 1)
	wprintw(win, "%*s   %s", 3, " ", "^^^ ^^^");

  wrefresh(win);  
}

static void
playlist_down_state_bar(struct VerboseArgs *vargs)
{
  static const char *bar =
	"______________________________________________/̅END LINE";
  WINDOW *win = specific_win(PLIST_DOWN_STATE_BAR);  

  int length = vargs->playlist->length,
	height = wchain[PLAYLIST].win->_maxy + 1,
	cursor = vargs->playlist->cursor;

  wprintw(win, "%*s %s", 5, " ", bar);

  if(cursor + height / 2 < length)
  	mvwprintw(win, 0, 0, "%*s   %s", 3, " ", "... ...  ");

  wrefresh(win);
}

static void
redraw_playlist_screen(struct VerboseArgs *vargs)
{
  static char buff[80];
  int i, height = wchain[PLAYLIST].win->_maxy + 1;

  WINDOW *win = specific_win(PLAYLIST);  
  
  for(i = vargs->playlist->begin - 1; i < vargs->playlist->begin
		+ height - 1 && i < vargs->playlist->length; i++)
	{
	  snprintf(buff, sizeof(buff), "%3i.  %s        %s",
			   vargs->playlist->id[i], vargs->playlist->pretty_title[i],
			   vargs->playlist->artist[i]);

	  if(i + 1 == vargs->playlist->cursor)
		{
		  color_print(win, 2, buff);
		  wprintw(win, "\n");
		  continue;
		}
	  
	  if(vargs->playlist->id[i] == vargs->playlist->current)
		{
		  color_print(win, 1, buff);	  
		  wprintw(win, "\n");
		}
	  else
		{
		  color_print(win, 0, buff);
		  wprintw(win, "\n");
		}
	}

  wrefresh(win);
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
playlist_list_update(struct VerboseArgs *vargs)
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
					sizeof(vargs->playlist->title[0]), 30);	  
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
	  playlist_list_update(vargs);
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

  if(vargs->searchlist->search_mode == SEARCHING)
	{
	  update_searchlist(vargs);
	  vargs->searchlist->search_mode = TYPING;
	  signal_win(BASIC_INFO);
	  signal_win(PLIST_UP_STATE_BAR);
	  signal_win(PLAYLIST);
	  signal_win(PLIST_DOWN_STATE_BAR);;
	}
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

void
update_searchlist(struct VerboseArgs* vargs)
{
  char (*tag_name)[128], *key = vargs->searchlist->key;
  int i, j, ct = vargs->searchlist->crt_tag_id;

  switch(vargs->searchlist->tags[ct])
	{
	case MPD_TAG_TITLE:
	  tag_name = vargs->playlist->title; break;	  
	case MPD_TAG_ARTIST:
	  tag_name = vargs->playlist->artist; break;
	case MPD_TAG_ALBUM:
	  tag_name = vargs->playlist->album; break;
	default:
	  tag_name = NULL;
	}

  int is_substr;
  
  for(i = j = 0; i < vargs->playlist->length; i++)
	{
	  if(tag_name != NULL)
		is_substr = is_substring_ignorecase
		  (tag_name[i], key) != NULL;
	  else
		{
		  is_substr = is_substring_ignorecase
			(vargs->playlist->title[i], key) != NULL ||
			is_substring_ignorecase
			(vargs->playlist->album[i], key) != NULL ||
			is_substring_ignorecase
			(vargs->playlist->artist[i], key) != NULL;
		}

	  if(is_substr)
		{
		  strcpy(vargs->playlist->title[j],
				 vargs->playlist->title[i]);
		  strcpy(vargs->playlist->artist[j],
				 vargs->playlist->artist[i]);
		  strcpy(vargs->playlist->album[j],
				 vargs->playlist->album[i]);
		  strcpy(vargs->playlist->pretty_title[j],
				 vargs->playlist->pretty_title[i]);
		  vargs->playlist->id[j] = vargs->playlist->id[i];
		  j ++;
		}
	}
  vargs->playlist->length = j;
}

static void
search_prompt(struct VerboseArgs *vargs)
{
  char str[128];

  WINDOW *win = specific_win(SEARCH_INPUT);
  
  snprintf(str, sizeof(str), "%s█", vargs->searchlist->key);

  if(vargs->searchlist->search_mode == TYPING)
	color_print(win, 5, "Search: ");
  else
	color_print(win, 0, "Search: ");
  
  if(vargs->playlist->length == 0)
	color_print(win, 4, "no entries...");
  else if(vargs->searchlist->search_mode == TYPING)
	color_print(win, 6, str);
  else
	color_print(win, 0, str);

  wprintw(win, "%*s", 40, " ");

  wrefresh(win);
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

/* different from the old cmd_single, this new function
   turn on the Repeat command simultaneously */
static int
cmd_Single(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
  struct mpd_status *status;
  
  status = getStatus(conn);  
  if(!mpd_status_get_repeat(status))
	cmd_repeat(argc, argv, conn);

  mpd_status_free(status);

  return cmd_single(argc, argv, conn);
}

static void
switch_to_playlist_menu(struct VerboseArgs *vargs)
{
  clean_screen();
  
  winset_update(&vargs->playlist->wmode);
}

static void
resize_term_window(const int x, const int y)
{
  char vx[128];
  int sys_ret;

  snprintf(vx, sizeof(vx),
		   "[[ $TERM == 'linux' ]] || resize -s %d %d >/dev/null",
		   y, x);
  sys_ret = system(vx);

  if(sys_ret == -1)
	{
	  endwin();
	  fprintf(stderr, "terminal window resize failed.\n");
	  exit(1);
	}
}

static void
toggle_mini_window(struct VerboseArgs *vargs)
{
  static int enable = 1;
  int x, y;

  if(enable)
	{
	  x = 70, y = 15;
	  enable = 0;
	}
  else
	{
	  x = vargs->org_screen_x;
	  y = vargs->org_screen_y;
	  enable = 1;
	}
  
  resize_term_window(x, y);

  signal_all_wins();
}

void
basic_keymap(struct VerboseArgs *vargs)
{
  int k = getch();

  if(k != ERR)
	vargs->key_hit = 1;

  switch(k)
	{
	case '+': ;
	case '=': ;
	case '0': 
	  cmd_volup(0, NULL, vargs->conn); break;
	case KEY_RIGHT:
	  cmd_forward(0, NULL, vargs->conn); break;
	case '-': ;
	case '9': 
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
	  cmd_Single(0, NULL, vargs->conn); break;
	case 'R':
	  cmd_repeat(0, NULL, vargs->conn); break;
	case 'L': // redraw screen
	  break;
	case '/':
	  turnon_search_mode(vargs); break;
	case 'S':
	  vargs->searchlist->crt_tag_id ++;
	  vargs->searchlist->crt_tag_id %= 4;	  
	  break;
	case '\t':
	  switch_to_playlist_menu(vargs);
	  break;
	case 'w':
	  toggle_mini_window(vargs);
	  break;
	case 27: ;
	case 'e': ;
	case 'q':
	  vargs->quit_signal = 1;
	default:;
	}

  signal_all_wins();
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

  if(vargs->searchlist->search_mode != DISABLE)
	turnoff_search_mode(vargs);

}

static void
playlist_scroll_to_current(struct VerboseArgs *vargs)
{
  struct mpd_status *status;
  int song_id;
  status = getStatus(vargs->conn);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(vargs->searchlist->search_mode == DISABLE)  
	playlist_scroll_to(vargs, song_id);
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
switch_to_main_menu(struct VerboseArgs* vargs)
{
  clean_screen();

  winset_update(&vargs->wmode);
}

void
playlist_keymap(struct VerboseArgs* vargs)
{
  int k = getch();

  if(k != ERR)
	vargs->key_hit = 1;
	
  switch(k)
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
	case '0': 
	  cmd_volup(0, NULL, vargs->conn); break;
	case KEY_RIGHT:
	  cmd_forward(0, NULL, vargs->conn); break;
	case '-': ;
	case '9': 
	  cmd_voldown(0, NULL, vargs->conn); break;
	case KEY_LEFT:
	  cmd_backward(0, NULL, vargs->conn); break;
	case 't':
	  cmd_toggle(0, NULL, vargs->conn); break;
	case 'r':
	  cmd_random(0, NULL, vargs->conn); break;
	case 's':
	  cmd_Single(0, NULL, vargs->conn); break;
	case 'R':
	  cmd_repeat(0, NULL, vargs->conn); break;
	case 'L':
	  break;
	case 'S':
	  vargs->searchlist->crt_tag_id ++;
	  vargs->searchlist->crt_tag_id %= 4;	  
	  break;

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
	case '/':
	  turnon_search_mode(vargs);
	  break;
	case 'w':
	  toggle_mini_window(vargs);
	  break;
	case 127:
	  if(vargs->searchlist->search_mode != DISABLE)
		{
		  ungetch(127);
		  vargs->menu_keymap = &searchlist_keymap;
		}
	  break;
	case '\\':
	  turnoff_search_mode(vargs); break;
	case 27: ;
	case 'e': ;
	case 'q':
	  vargs->quit_signal = 1;
	default:;
	}

  signal_all_wins();
}

void
searchlist_keymap(struct VerboseArgs *vargs)
{
  int i, ch;
  /** toggle search mode from the beginning,
	  user need to choose the search tag type.
	  default is MPD_TAG_TITLE */
  
  vargs->playlist->cursor = 0;
  vargs->searchlist->search_mode = TYPING;

  ch = getch();

  i = strlen(vargs->searchlist->key);
  if(ch != ERR)
	{
	  vargs->key_hit = 1;
	  
	  vargs->searchlist->search_mode = SEARCHING;

	  if(ch == '\n' || ch == '\t')
		{
		  // no key word is specified
		  if(i == 0)
			{
			  turnoff_search_mode(vargs);
			}
			
		  vargs->searchlist->search_mode = PICKING;
		  vargs->menu_keymap = &playlist_keymap;
		  playlist_scroll_to(vargs, 1);
		  signal_win(SEARCHLIST);
		  signal_win(SEARCH_INPUT);
		  return 0;
		}
	  else if(ch == '\\' || ch == 27)
		{
		  turnoff_search_mode(vargs);
		}
	  else if(ch == 127)
		{
		  // backspace to the begining, exit searching mode
		  if(i == 0)
			{
			  turnoff_search_mode(vargs);
			}
		  
		  if(!isascii(vargs->searchlist->key[i - 1]))
			vargs->searchlist->key[i - 3] = '\0';
		  else
			vargs->searchlist->key[i - 1] = '\0';
		  
		  playlist_list_update(vargs);
		}
	  else if(!isascii(ch))
		{
		  vargs->searchlist->key[i++] = (char)ch;		  
		  vargs->searchlist->key[i++] = getch();
		  vargs->searchlist->key[i++] = getch();
		}
	  else
		vargs->searchlist->key[i++] = (char)ch;
	  
	  vargs->searchlist->key[i] = '\0';
	}
}

void
turnon_search_mode(struct VerboseArgs *vargs)
{
  clean_screen();

  winset_update(&vargs->searchlist->wmode);
}

void
turnoff_search_mode(struct VerboseArgs *vargs)
{
  if(vargs->searchlist->search_mode == DISABLE)
	return;
  
  vargs->searchlist->search_mode = DISABLE;
  vargs->searchlist->key[0] = '\0';

  playlist_list_update(vargs);
  playlist_scroll_to_current(vargs);
  
  clean_screen();
  winset_update(&vargs->playlist->wmode);
}

// basically sizes are what we are concerned
static void
wchain_size_update(void)
{
  int height = stdscr->_maxy + 1, width = stdscr->_maxx;
  int wparam[WIN_NUM][4] =
	{
	  {3, width, 0, 0},             // BASIC_INFO
	  {1, width, 3, 0},				// VERBOSE_PROC_BAR
	  {9, width, 5, 0},				// HELPER
	  {1, 27, 4, 43},				// SIMPLE_PROC_BAR
	  {1, 42, 4, 0},				// PLIST_UP_STATE_BAR
	  {height - 8, width, 5, 0},	// PLAYLIST
	  {1, width, height - 3, 0},	// PLIST_DOWN_STATE_BAR
	  {height - 8, width, 5, 0},	// SEARCHLIST
	  {1, width, height - 1, 0},	// SEARCH_INPUT
	  {1, width, height - 2, 0}		// DEBUG_INFO       
	}; 

  int i;
  for(i = 0; i < WIN_NUM; i++)
	{
	  wresize(wchain[i].win, wparam[i][0], wparam[i][1]);
	  mvwin(wchain[i].win, wparam[i][2], wparam[i][3]);
	}

  // acutally searchlist window is playlist window.
  wchain[SEARCHLIST].win = wchain[PLAYLIST].win;
}

static void
wchain_init(void)
{
  void (*func[WIN_NUM])(struct VerboseArgs*) =
	{
	  &print_basic_song_info,    // BASIC_INFO       
	  &print_basic_bar,			 // VERBOSE_PROC_BAR 
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
  for(i = 0; i < WIN_NUM; i++)
	{
	  wchain[i].win = newwin(0, 0, 0, 0);// we're gonna change soon
	  wchain[i].redraw_routine = func[i];
	  wchain[i].flash = 0;
	}

  // some windows need refresh frequently
  wchain[VERBOSE_PROC_BAR].flash = 1;
  wchain[SIMPLE_PROC_BAR].flash = 1;
  
  wchain_size_update();
}

static void
winset_init(struct VerboseArgs *vargs)
{
  // for basic mode (main menu)
  vargs->wmode.size = 3;
  vargs->wmode.wins = (struct WindowUnit**)
	malloc(vargs->wmode.size * sizeof(struct WindowUnit*));
  vargs->wmode.wins[2] = &wchain[BASIC_INFO];
  vargs->wmode.wins[1] = &wchain[VERBOSE_PROC_BAR];
  vargs->wmode.wins[0] = &wchain[HELPER];
  vargs->wmode.update_checking = &basic_state_checking;
  vargs->wmode.listen_keyboard = &basic_keymap;

  // playlist wmode
  vargs->playlist->wmode.size = 5;
  vargs->playlist->wmode.wins = (struct WindowUnit**)
	malloc(vargs->playlist->wmode.size * sizeof(struct WindowUnit*));
  vargs->playlist->wmode.wins[4] = &wchain[PLIST_DOWN_STATE_BAR];
  vargs->playlist->wmode.wins[3] = &wchain[PLIST_UP_STATE_BAR];
  vargs->playlist->wmode.wins[2] = &wchain[SIMPLE_PROC_BAR];
  vargs->playlist->wmode.wins[1] = &wchain[BASIC_INFO];
  vargs->playlist->wmode.wins[0] = &wchain[PLAYLIST];
  vargs->playlist->wmode.update_checking = &playlist_update_checking;
  vargs->playlist->wmode.listen_keyboard = &playlist_keymap;

  // searchlist wmode
  vargs->searchlist->wmode.size = 6;
  vargs->searchlist->wmode.wins = (struct WindowUnit**)
	malloc(vargs->searchlist->wmode.size * sizeof(struct WindowUnit*));
  vargs->searchlist->wmode.wins[5] = &wchain[PLIST_DOWN_STATE_BAR];
  vargs->searchlist->wmode.wins[4] = &wchain[PLIST_UP_STATE_BAR];
  vargs->searchlist->wmode.wins[3] = &wchain[SEARCH_INPUT];  
  vargs->searchlist->wmode.wins[2] = &wchain[SIMPLE_PROC_BAR];
  vargs->searchlist->wmode.wins[1] = &wchain[BASIC_INFO];
  vargs->searchlist->wmode.wins[0] = &wchain[SEARCHLIST];  
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
  vargs->menu_keymap = &basic_keymap;
  vargs->old_menu_keymap = NULL;
  /* initialization require redraw too */
  vargs->key_hit = 1;
  vargs->quit_signal = 0;
  vargs->org_screen_x = stdscr->_maxx + 1;
  vargs->org_screen_y = stdscr->_maxy + 1;

  /* check the screen size */
  if(stdscr->_maxy < 8 || stdscr->_maxx < 69)
	{
	  endwin();
	  fprintf(stderr, "screen too small for normally displaying.\n");
	  exit(1);
	}
  //toggle_mini_window(vargs); // toggle mini window mode
  
  /** playlist arguments */
  vargs->playlist =
	(struct PlaylistArgs*) malloc(sizeof(struct PlaylistArgs));
  vargs->playlist->begin = 1;
  vargs->playlist->length = 0;
  vargs->playlist->current = 0;
  vargs->playlist->cursor = 1;

  /** searchlist arguments */
  vargs->searchlist =
	(struct SearchlistArgs*) malloc(sizeof(struct SearchlistArgs));
  vargs->searchlist->search_mode = DISABLE;
  {
	vargs->searchlist->tags[0] = MPD_TAG_NAME;
    vargs->searchlist->tags[1] = MPD_TAG_TITLE;
	vargs->searchlist->tags[2] = MPD_TAG_ARTIST;
	vargs->searchlist->tags[3] = MPD_TAG_ALBUM;
	vargs->searchlist->crt_tag_id = 0;
  }
  vargs->searchlist->key[0] = '\0';

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
	  screen_redraw(&vargs);

	  smart_sleep(&vargs);
	}

  endwin();
  resize_term_window(vargs.org_screen_x, vargs.org_screen_y);
  
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
	  if(wunit[i]->redraw_signal)
		wunit[i]->redraw_routine(vargs);
	  wunit[i]->redraw_signal = wunit[i]->flash | 0;
	}
}
