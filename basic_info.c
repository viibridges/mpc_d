#include "basic_info.h"
#include "searchlist.h"
#include "utils.h"

extern struct mpd_connection *conn;

void
basic_state_checking(void)
{
  static int repeat, randomm, single, queue_len, id,
	volume, bit_rate,
	play, rep, ran, sgl, que, idd, vol, ply, btr;
  struct mpd_status *status;

  status = getStatus(conn);
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

void
print_basic_help(void)
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

void
print_extra_info(void)
{
  WINDOW *win = specific_win(EXTRA_INFO);
  
  struct mpd_status *status;
  status = init_mpd_status(conn);
  
  if (mpd_status_get_update_id(status) > 0)
	wprintw(win, "Updating DB (#%u) ...",
			mpd_status_get_update_id(status));

  if (mpd_status_get_volume(status) >= 0)
	wprintw(win, "Volume:%3i%c ", mpd_status_get_volume(status), '%');
  else {
	wprintw(win, "Volume: n/a   ");
  }

  extern struct Searchlist *searchlist;

  mvwprintw(win, 0, 14, "Search: ");
  color_print(win, 6,
			  mpd_tag_name(searchlist->tags
						   [searchlist->crt_tag_id]));
  wprintw(win, "  ");

  if (mpd_status_get_error(status) != NULL)
	wprintw(win, "ERROR: %s\n", mpd_status_get_error(status));

  mpd_status_free(status);
  my_finishCommand(conn);
}

void
print_basic_song_info(void)
{
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

void // VERBOSE_PROC_BAR
print_basic_bar(void)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i;
  struct mpd_status *status;

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

