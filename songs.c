#include "songs.h"
#include "basic_info.h"
#include "utils.h"

void
playlist_simple_bar(void)
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
playlist_up_state_bar(void)
{
  WINDOW *win = specific_win(PLIST_UP_STATE_BAR);  

  if(playlist->begin > 1)
	wprintw(win, "%*s   %s", 3, " ", "^^^ ^^^");
}

void
playlist_down_state_bar(void)
{
  static const char *bar =
	"_________________________________________________________/";
  WINDOW *win = specific_win(PLIST_DOWN_STATE_BAR);  

  int length = playlist->length,
	height = wchain[PLAYLIST].win->_maxy + 1,
	cursor = playlist->cursor;

  wprintw(win, "%*c %s", 5, ' ', bar);
  color_print(win, 8, "TED MADE");

  if(cursor + height / 2 < length)
  	mvwprintw(win, 0, 0, "%*s   %s", 3, " ", "... ...  ");
}

void
playlist_redraw_screen(void)
{
  int line = 0, i, height = wchain[PLAYLIST].win->_maxy + 1;

  WINDOW *win = specific_win(PLAYLIST);  

  int id;
  char *title, *artist;
  for(i = playlist->begin - 1; i < playlist->begin
		+ height - 1 && i < playlist->length; i++)
	{
	  id = playlist->meta[i].id;
	  title = playlist->meta[i].pretty_title;
	  artist = playlist->meta[i].artist;

	  // cursor in
	  if(i + 1 == playlist->cursor)
		{
		  print_list_item(win, line++, 2, id, title, artist);
		  continue;
		}

	  // selected
	  if(playlist->meta[i].selected)
		{
		  print_list_item(win, line++, 9, id, title, artist);
		  continue;
		}
		
	  if(playlist->meta[i].id == playlist->current)
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
playlist_update(void)
{
  struct mpd_song *song;
  
  if (!mpd_send_list_queue_meta(conn))
	printErrorAndExit(conn);

  int i = 0;
  while ((song = mpd_recv_song(conn)) != NULL
		 && i < MAX_PLAYLIST_STORE_LENGTH)
	{
	  pretty_copy(playlist->meta[i].title,
					get_song_tag(song, MPD_TAG_TITLE),
					512, -1);
	  pretty_copy(playlist->meta[i].pretty_title,
					get_song_tag(song, MPD_TAG_TITLE),
					128, 26);
	  pretty_copy(playlist->meta[i].artist,
					get_song_tag(song, MPD_TAG_ARTIST),
					128, 20);	  
	  pretty_copy(playlist->meta[i].album,
					get_song_tag(song, MPD_TAG_ALBUM),
					128, -1);
	  playlist->meta[i].id = i + 1;
	  playlist->meta[i].selected = 0;
	  ++i;
	  mpd_song_free(song);
	}

  if(i < MAX_PLAYLIST_STORE_LENGTH)
  	playlist->length = i;
  else
  	playlist->length = MAX_PLAYLIST_STORE_LENGTH;

  my_finishCommand(conn);
}

void
playlist_update_checking(void)
{
  struct mpd_status *status;
  int queue_len, song_id;

  basic_state_checking();

  status = getStatus(conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(playlist->current != song_id)
	{
	  playlist->current = song_id;
	  signal_all_wins();
	}

  if(playlist->update_signal == 1 ||
	 playlist->length != queue_len)
	{
	  playlist->update_signal = 0;
	  playlist_update();
	  signal_all_wins();
	}

  /** for some unexcepted cases playlist->begin
	  may be greater than playlist->length;
	  if this happens, we reset the begin's value */
  if(playlist->begin > playlist->length)
	playlist->begin = 1;
}

// return -1 when cursor is hiden
int get_playlist_cursor_item_index(void)
{
  if(playlist->cursor < 1 ||
	 playlist->cursor > playlist->length)
	return -1;
  
  return playlist->cursor - 1;
}

int
is_playlist_selected(void)
{
  int i;

  for(i = 0; i < playlist->length; i++)
	if(playlist->meta[i].selected)
	  return 1;

  return 0;
}

void
swap_playlist_items(int i, int j)
{
  struct MetaInfo temp;

  memmove(&temp, playlist->meta + i,
		  sizeof(struct MetaInfo) / sizeof(char));

  memmove(playlist->meta + i, playlist->meta + j,
		  sizeof(struct MetaInfo) / sizeof(char));

  memmove(playlist->meta + j, &temp,
		  sizeof(struct MetaInfo) / sizeof(char));
}


/** search mode staff **/
void
searchmode_update(void)
{
  char *tag_name, *key = playlist->key;
  int i, j, ct = playlist->crt_tag_id;

  /* we examine and update the playlist (datebase for searching)
   * every time, see if any modification (delete or add songs)
   * was made by other client during searching */
  playlist_update();

  int is_substr;
  
  for(i = j = 0; i < playlist->length; i++)
	{
	  // now plist is fresh
	  switch(playlist->tags[ct])
		{
		case MPD_TAG_TITLE:
		  tag_name = playlist->meta[i].title; break;	  
		case MPD_TAG_ARTIST:
		  tag_name = playlist->meta[i].artist; break;
		case MPD_TAG_ALBUM:
		  tag_name = playlist->meta[i].album; break;
		default:
		  tag_name = NULL;
		}

	  if(tag_name != NULL)
		is_substr = is_substring_ignorecase
		  (tag_name, key) != NULL;
	  else
		{
		  is_substr = is_substring_ignorecase
			(playlist->meta[i].title, key) != NULL ||
			is_substring_ignorecase
			(playlist->meta[i].album, key) != NULL ||
			is_substring_ignorecase
			(playlist->meta[i].artist, key) != NULL;
		}

	  if(is_substr)
		{
		  memmove(playlist->meta + j,
				  playlist->meta + i,
				 sizeof(struct MetaInfo) / sizeof(char));
		  j ++;
		}
	}
  playlist->length = j;
  playlist->begin = 1;
}

void searchmode_update_checking(void)
{
  basic_state_checking();

  if(playlist->update_signal)
	{
	  searchmode_update();
	  playlist->update_signal = 0;
	  signal_all_wins();
	}
}

void
search_prompt(void)
{
  char str[128];

  WINDOW *win = specific_win(SEARCH_INPUT);
  
  snprintf(str, sizeof(str), "%s█", playlist->key);

  if(playlist->picking_mode)
	color_print(win, 0, "Search: ");
  else
	color_print(win, 5, "Search: ");
  
  if(playlist->length == 0)
	color_print(win, 4, "no entries...");
  else if(playlist->picking_mode)
	color_print(win, 0, str);
  else
	color_print(win, 6, str);

  wprintw(win, "%*s", 40, " ");
}

