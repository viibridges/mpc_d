#include "basic_info.h"
#include "songs.h"
#include "utils.h"
#include "keyboards.h"

void
basic_state_checking(void)
{
  int rep, ran, sgl, len, crt, vol, ply, btr;
  struct mpd_status *status;

  status = getStatus(conn);

  rep = mpd_status_get_repeat(status);
  ran = mpd_status_get_random(status);
  sgl = mpd_status_get_single(status);
  len = mpd_status_get_queue_length(status);
  crt = mpd_status_get_song_id(status);
  vol = mpd_status_get_volume(status);
  ply = mpd_status_get_state(status);
  btr = mpd_status_get_kbit_rate(status);

  basic_info->crt_time = mpd_status_get_elapsed_time(status);
  basic_info->total_time = mpd_status_get_total_time(status);
  
  mpd_status_free(status);

  if(rep != basic_info->repeat || ran != basic_info->random
	 || sgl != basic_info->single || len != basic_info->total
	 || crt != basic_info->current || vol != basic_info->volume
	 || ply != basic_info->state)
	{
	  basic_info->repeat = rep;
	  basic_info->random = ran;
	  basic_info->single = sgl;
	  basic_info->total = len;
	  basic_info->current = crt;
	  basic_info->volume = vol;
	  basic_info->state = ply;

	  signal_all_wins();
	}

  /* as many songs's bit rate varies while playing
	 we refresh the bit rate display only when the 
	 relevant change greater than 0.2 */
  if(abs(basic_info->bit_rate - btr) / (float)(btr + 1) > 0.2)
	{
	  basic_info->bit_rate = btr;
	  signal_win(BASIC_INFO);
	}

  
  /* get current song's name */
  status = init_mpd_status(conn);

  strncpy(basic_info->format, "null", 16);
  *basic_info->crt_name = '\0';
	
  if (basic_info->state == MPD_STATE_PLAY ||
	  basic_info->state == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  strncpy(basic_info->crt_name, get_song_tag(song, MPD_TAG_TITLE), 512);
	  strncpy(basic_info->format, get_song_format(song), 16);
	}

	mpd_song_free(song);
  }

  mpd_status_free(status);
  my_finishCommand(conn);	
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
  
  if (basic_info->volume >= 0)
	wprintw(win, "Volume:%3i%c ", basic_info->volume, '%');
  else {
	wprintw(win, "Volume: n/a   ");
  }

  mvwprintw(win, 0, 14, "Search: ");
  color_print(win, 6,
			  mpd_tag_name(songlist->tags
						   [songlist->crt_tag_id]));
  wprintw(win, "  ");
}

void
print_basic_song_info(void)
{
  WINDOW *win = specific_win(BASIC_INFO);
  
  if (basic_info->crt_name) {

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
	snprintf(buff, title_len, "%s", basic_info->crt_name);
	color_print(win, 1,  buff);
  }

  mvwprintw(win, 1, 0, "[");
  if (basic_info->state == MPD_STATE_PLAY)
	color_print(win, 6, "playing");
  else if(basic_info->state == MPD_STATE_PAUSE)
	color_print(win, 4, "paused");
  else
	color_print(win, 4, "stopped");
  wprintw(win, "]");

  /* mvwprintw(win, 1, 10, "[%i/%u]", */
  /* 		  mpd_status_get_song_pos(status) + 1, */
  /* 		  mpd_status_get_queue_length(status)); */
  
  // status modes [ors] : repeat, random, single
  mvwprintw(win, 1, 10, "["); 
  if (basic_info->random)
	color_print(win, 1, "r");
  else wprintw(win, "-");

  if (basic_info->single)
	color_print(win, 7, "s");
  else wprintw(win, "-");

  if (basic_info->repeat)
	color_print(win, 5, "o");
  else wprintw(win, "-");
  wprintw(win, "]");

  mvwprintw(win, 1, 16, "[%s]", basic_info->format); // music format

  mvwprintw(win, 1, 46, "%02i:%02i",
			basic_info->total_time / 60, basic_info->total_time % 60);

  static int old_bit_rate = 111;
  if(abs(old_bit_rate - basic_info->bit_rate)
	 / (float) (basic_info->bit_rate + 1) > 0.2 && basic_info->bit_rate != 0)
	old_bit_rate = basic_info->bit_rate;
  else
	basic_info->bit_rate = old_bit_rate;
  mvwprintw(win, 1, 59, " %ikb/s", basic_info->bit_rate);
}

void // VERBOSE_PROC_BAR
print_basic_bar(void)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i;

  WINDOW *win = specific_win(VERBOSE_PROC_BAR);
  const int axis_length = win->_maxx - 8;
  
  crt_time = basic_info->crt_time;
  total_time = basic_info->total_time;

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

  //wprintw(win, "%3i%% ", crt_time_perc);

  int left_time = total_time - crt_time;
  wprintw(win, " %02i:%02i", left_time / 60, left_time % 60);
}

struct BasicInfo* basic_info_setup(void)
{
  struct BasicInfo* binfo =
	(struct BasicInfo*) malloc(sizeof(struct BasicInfo));

  binfo->state = binfo->repeat
	= binfo->random = binfo->single
	= binfo->total = binfo->current
	= binfo->volume = binfo->bit_rate
	= binfo->crt_time = binfo->total_time = 0;

  // window mode setup
  binfo->wmode.size = 5;
  binfo->wmode.wins = (struct WindowUnit**)
	malloc(binfo->wmode.size * sizeof(struct WindowUnit*));
  binfo->wmode.wins[0] = &wchain[BASIC_INFO];
  binfo->wmode.wins[1] = &wchain[EXTRA_INFO];  
  binfo->wmode.wins[2] = &wchain[VERBOSE_PROC_BAR];
  binfo->wmode.wins[3] = &wchain[VISUALIZER];
  binfo->wmode.wins[4] = &wchain[HELPER];
  binfo->wmode.listen_keyboard = &basic_keymap;

  return binfo;
}
