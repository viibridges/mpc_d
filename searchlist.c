#include "searchlist.h"
#include "utils.h"
#include "basic_info.h"

void
searchlist_update(void)
{
  char *tag_name, *key = searchlist->key;
  int i, j, ct = searchlist->crt_tag_id;

  /* we examine and update the playlist (datebase for searching)
   * every time, see if any modification (delete or add songs)
   * was made by other client during searching */
  playlist_update();

  int is_substr;
  
  for(i = j = 0; i < searchlist->plist->length; i++)
	{
	  // now plist is fresh
	  switch(searchlist->tags[ct])
		{
		case MPD_TAG_TITLE:
		  tag_name = searchlist->plist->meta[i].title; break;	  
		case MPD_TAG_ARTIST:
		  tag_name = searchlist->plist->meta[i].artist; break;
		case MPD_TAG_ALBUM:
		  tag_name = searchlist->plist->meta[i].album; break;
		default:
		  tag_name = NULL;
		}

	  if(tag_name != NULL)
		is_substr = is_substring_ignorecase
		  (tag_name, key) != NULL;
	  else
		{
		  is_substr = is_substring_ignorecase
			(searchlist->plist->meta[i].title, key) != NULL ||
			is_substring_ignorecase
			(searchlist->plist->meta[i].album, key) != NULL ||
			is_substring_ignorecase
			(searchlist->plist->meta[i].artist, key) != NULL;
		}

	  if(is_substr)
		{
		  memmove(searchlist->plist->meta + j,
				  searchlist->plist->meta + i,
				 sizeof(struct MetaInfo) / sizeof(char));
		  j ++;
		}
	}
  searchlist->plist->length = j;
  searchlist->plist->begin = 1;
}

void searchlist_update_checking(void)
{
  struct mpd_status *status;
  int queue_len, song_id;

  basic_state_checking();

  status = getStatus(conn);
  queue_len = mpd_status_get_queue_length(status);
  song_id = mpd_status_get_song_pos(status) + 1;
  mpd_status_free(status);

  if(searchlist->update_signal)
	{
	  searchlist_update();
	  searchlist->update_signal = 0;
	  signal_all_wins();
	}
}

void
search_prompt(void)
{
  char str[128];

  WINDOW *win = specific_win(SEARCH_INPUT);
  
  snprintf(str, sizeof(str), "%s█", searchlist->key);

  if(searchlist->picking_mode)
	color_print(win, 0, "Search: ");
  else
	color_print(win, 5, "Search: ");
  
  if(searchlist->plist->length == 0)
	color_print(win, 4, "no entries...");
  else if(searchlist->picking_mode)
	color_print(win, 0, str);
  else
	color_print(win, 6, str);

  wprintw(win, "%*s", 40, " ");
}

