#include "playlists.h"
#include "utils.h"

void
tapelist_redraw_screen(void)
{
  int line = 0, i, height = wchain[TAPELIST].win->_maxy + 1;

  WINDOW *win = specific_win(TAPELIST);  

  char *filename;
  for(i = tapelist->begin - 1; i < tapelist->begin
		+ height - 1 && i < tapelist->length; i++)
	{
	  filename = tapelist->tapename[i];

	  if(i + 1 == tapelist->cursor)
		print_list_item(win, line++, 2, i + 1, filename, NULL);
	  else
		print_list_item(win, line++, 0, i + 1, filename, NULL);
	}
}

void
tapelist_helper(void)
{
  WINDOW *win = specific_win(TAPEHELPER);

  wprintw(win,  "\n\
  Key Options:\n\
  ----------------------\n\
    [k] Move Cursor Up\n\
    [j] Move Cursor Down\n\
    [a] Append Current\n\
    [r] Rename Playlist");
}

void tapelist_update_checking(void)
{
  if(tapelist->update_signal)
	{
	  tapelist_update();
	  tapelist->update_signal = 0;
	  signal_all_wins();
	}
}

void
tapelist_update(void)
{
	struct mpd_entity *entity;
	const struct mpd_playlist *playlist;

	int i = 0;

	if (!mpd_send_list_meta(conn, ""))
	  return printErrorAndExit(conn);
	
	while((entity = mpd_recv_entity(conn)) != NULL && i < 128)
	  {
		enum mpd_entity_type type = mpd_entity_get_type(entity);
		if(type != MPD_ENTITY_TYPE_PLAYLIST)
		  continue;
		
		playlist = mpd_entity_get_playlist(entity);
		strncpy(tapelist->tapename[i],
				mpd_playlist_get_path(playlist), 512);

		mpd_entity_free(entity);

		i++;
	  }

	tapelist->length = i;
}

void tapelist_rename(void)
{
  char *to =
	popup_input_dialog("Enter a New Name Here:");

  int choice =
	popup_confirm_dialog("Do You Really Mean That?");

  if(!choice) // action canceled
	return;

  const char *from = tapelist->tapename[tapelist->cursor - 1];
	
  mpd_run_rename(conn, from, to);

  tapelist->update_signal = 1;
}

void tapelist_append(void)
{
  const char *name = tapelist->tapename[tapelist->cursor - 1];

  mpd_run_load(conn, name);
}
