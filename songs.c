#include "songs.h"
#include "basic_info.h"
#include "keyboards.h"
#include "commands.h"
#include "utils.h"

void
songlist_simple_bar(void)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, rest_len, i;
  struct mpd_status *status;

  WINDOW *win = specific_win(SIMPLE_PROC_BAR);
  const int bar_length = win->_maxx + 1;
  
  status = getStatus(conn);
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

void
songlist_up_state_bar(void)
{
  WINDOW *win = specific_win(SLIST_UP_STATE_BAR);  

  if(songlist->begin > 1)
	wprintw(win, "%*s   %s", 3, " ", "^^^ ^^^");
}

void
songlist_down_state_bar(void)
{
  static const char *bar =
	"_________________________________________________________/";
  WINDOW *win = specific_win(SLIST_DOWN_STATE_BAR);  

  int length = songlist->length,
	height = wchain[SONGLIST].win->_maxy + 1,
	cursor = songlist->cursor;

  wprintw(win, "%*c %s", 5, ' ', bar);
  color_print(win, 8, "TED MADE");

  if(cursor + height / 2 < length)
  	mvwprintw(win, 0, 0, "%*s   %s", 3, " ", "... ...  ");
}

void
songlist_redraw_screen(void)
{
  int line = 0, i, height = wchain[SONGLIST].win->_maxy + 1;

  WINDOW *win = specific_win(SONGLIST);  

  int id;
  char *title, *artist;
  for(i = songlist->begin - 1; i < songlist->begin
		+ height - 1 && i < songlist->length; i++)
	{
	  id = songlist->meta[i].id;
	  title = songlist->meta[i].pretty_title;
	  artist = songlist->meta[i].artist;

	  // cursor in
	  if(i + 1 == songlist->cursor)
		{
		  print_list_item(win, line++, 2, id, title, artist);
		  continue;
		}

	  // selected
	  if(songlist->meta[i].selected)
		{
		  print_list_item(win, line++, 9, id, title, artist);
		  continue;
		}
		
	  if(songlist->meta[i].id == songlist->current)
		{
		  print_list_item(win, line++, 1, id, title, artist);
		}
	  else
		{
		  print_list_item(win, line++, 0, id, title, artist);
		}
	}
}

void
songlist_update(void)
{
  struct mpd_song *song;
  
  if (!mpd_send_list_queue_meta(conn))
	printErrorAndExit(conn);

  int i = 0;
  while ((song = mpd_recv_song(conn)) != NULL
		 && i < MAX_SONGLIST_STORE_LENGTH)
	{
	  pretty_copy(songlist->meta[i].title,
					get_song_tag(song, MPD_TAG_TITLE),
					512, -1);
	  pretty_copy(songlist->meta[i].pretty_title,
					get_song_tag(song, MPD_TAG_TITLE),
					128, 26);
	  pretty_copy(songlist->meta[i].artist,
					get_song_tag(song, MPD_TAG_ARTIST),
					128, 20);	  
	  pretty_copy(songlist->meta[i].album,
					get_song_tag(song, MPD_TAG_ALBUM),
					128, -1);
	  songlist->meta[i].id = i + 1;
	  songlist->meta[i].selected = 0;
	  ++i;
	  mpd_song_free(song);
	}

  songlist->length = i;

  my_finishCommand(conn);
}

void
songlist_update_checking(void)
{
  struct mpd_status *status;
  int queue_len, song_id;

  basic_state_checking();

  status = getStatus(conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(songlist->current != song_id)
	{
	  songlist->current = song_id;
	  signal_all_wins();
	}

  if(songlist->update_signal == 1 ||
  	 songlist->total != queue_len)
  	{
  	  songlist->update_signal = 0;
	  songlist->total = queue_len;
  	  songlist_update();
  	  signal_all_wins();
  	}

  /** for some unexcepted cases songlist->begin
	  may be greater than songlist->length;
	  if this happens, we reset the begin's value */
  if(songlist->begin > songlist->length)
	songlist->begin = 1;
}

// return -1 when cursor is hiden
int get_songlist_cursor_item_index(void)
{
  if(songlist->cursor < 1 ||
	 songlist->cursor > songlist->length)
	return -1;
  
  return songlist->cursor - 1;
}

int
is_songlist_selected(void)
{
  int i;

  for(i = 0; i < songlist->length; i++)
	if(songlist->meta[i].selected)
	  return 1;

  return 0;
}

void
swap_songlist_items(int i, int j)
{
  struct MetaInfo temp;

  memmove(&temp, songlist->meta + i,
		  sizeof(struct MetaInfo) / sizeof(char));

  memmove(songlist->meta + i, songlist->meta + j,
		  sizeof(struct MetaInfo) / sizeof(char));

  memmove(songlist->meta + j, &temp,
		  sizeof(struct MetaInfo) / sizeof(char));
}


/** search mode staff **/
void
turnon_search_mode(void)
{
  // show up the input window
  wchain[SEARCH_INPUT].visible = 1;
  
  songlist->key[0] = '\0';
  songlist->picking_mode = 0;
  //songlist->wmode.listen_keyboard = &searchmode_keymap;
  songlist_cursor_hide();

  clean_window(VERBOSE_PROC_BAR);
  clean_window(VISUALIZER);

  // switch the keyboard and update_checking() rountine
  songlist->wmode.listen_keyboard = &searchmode_keymap;
  wchain[SONGLIST].update_checking = &searchmode_update_checking;

  being_mode_update(&songlist->wmode);

  songlist->search_mode = 1;
}

void
turnoff_search_mode(void)
{
  // turn off the input window
  wchain[SEARCH_INPUT].visible = 0;
  
  songlist->key[0] = '\0';
  songlist_update();
  songlist_scroll_to_current();

  clean_window(SEARCH_INPUT);

  // switch the keboard and update_checking() back
  songlist->wmode.listen_keyboard = &songlist_keymap;
  wchain[SONGLIST].update_checking = &songlist_update_checking;

  songlist->update_signal = 1;

  songlist->search_mode = 0;  
}

void
searchmode_update(void)
{
  char *tag_name, *key = songlist->key;
  int i, j, ct = songlist->crt_tag_id;

  /* we examine and update the songlist (datebase for searching)
   * every time, see if any modification (delete or add songs)
   * was made by other client during searching */
  songlist_update();

  int is_substr;
  
  for(i = j = 0; i < songlist->length; i++)
	{
	  switch(songlist->tags[ct])
		{
		case MPD_TAG_TITLE:
		  tag_name = songlist->meta[i].title; break;	  
		case MPD_TAG_ARTIST:
		  tag_name = songlist->meta[i].artist; break;
		case MPD_TAG_ALBUM:
		  tag_name = songlist->meta[i].album; break;
		default:
		  tag_name = NULL;
		}

	  if(tag_name != NULL)
		is_substr = is_substring_ignorecase
		  (tag_name, key) != NULL;
	  else
		{
		  is_substr = is_substring_ignorecase
			(songlist->meta[i].title, key) != NULL ||
			is_substring_ignorecase
			(songlist->meta[i].album, key) != NULL ||
			is_substring_ignorecase
			(songlist->meta[i].artist, key) != NULL;
		}

	  if(is_substr)
		{
		  memmove(songlist->meta + j,
				  songlist->meta + i,
				 sizeof(struct MetaInfo) / sizeof(char));
		  j ++;
		}
	}
  songlist->length = j;
  songlist->begin = 1;
}

void searchmode_update_checking(void)
{
  basic_state_checking();

  if(songlist->update_signal)
	{
	  searchmode_update();
	  songlist->update_signal = 0;
	  signal_all_wins();
	}
}

void
search_prompt(void)
{
  char str[128];

  WINDOW *win = specific_win(SEARCH_INPUT);
  
  snprintf(str, sizeof(str), "%s█", songlist->key);

  if(songlist->picking_mode)
	color_print(win, 0, "Search: ");
  else
	color_print(win, 5, "Search: ");
  
  if(songlist->length == 0)
	color_print(win, 4, "no entries...");
  else if(songlist->picking_mode)
	color_print(win, 0, str);
  else
	color_print(win, 6, str);

  wprintw(win, "%*s", 40, " ");
}

void songlist_clear(void)
{
  int c =
	popup_confirm_dialog("Clear Confirm:", 0);

  if(!c) return;
  
  mpd_run_clear(conn);
}

struct Songlist* songlist_setup(void)
{
  struct Songlist *slist =
	(struct Songlist*) malloc(sizeof(struct Songlist));

  slist->update_signal = 0;
  slist->search_mode = 0;
  slist->total = 0;
  
  slist->begin = 1;
  slist->length = 0;
  slist->current = 0;
  slist->cursor = 1;

  slist->tags[0] = MPD_TAG_NAME;
  slist->tags[1] = MPD_TAG_TITLE;
  slist->tags[2] = MPD_TAG_ARTIST;
  slist->tags[3] = MPD_TAG_ALBUM;
  slist->crt_tag_id = 0;
  slist->key[0] = '\0';

  // window mode setup
  slist->wmode.size = 7;
  slist->wmode.wins = (struct WindowUnit**)
	malloc(slist->wmode.size * sizeof(struct WindowUnit*));
  slist->wmode.wins[0] = &wchain[SLIST_DOWN_STATE_BAR];
  slist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  slist->wmode.wins[2] = &wchain[SLIST_UP_STATE_BAR];
  slist->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  slist->wmode.wins[4] = &wchain[BASIC_INFO];
  slist->wmode.wins[5] = &wchain[SONGLIST];
  slist->wmode.wins[6] = &wchain[SEARCH_INPUT];  
  slist->wmode.listen_keyboard = &songlist_keymap;  

  return slist;
}
