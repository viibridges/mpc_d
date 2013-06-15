#include "directory.h"
#include "windows.h"
#include "utils.h"
#include "keyboards.h"

int
is_path_valid_format(char *path)
{
  #define MUSIC_FORMAT_NUM 6

  char valid_format[][MUSIC_FORMAT_NUM] = {
	"mp3", "flac", "wav", "wma", "ape", "ogg"
  };

  char *path_suffix = strrchr(path, '.');

  int i;
  
  if(path_suffix)
	{
	  for(i = 0; i < MUSIC_FORMAT_NUM; i++)
		if(is_substring_ignorecase(path_suffix, valid_format[i]))
		  return 1;
	}
  else
	return 0;

  return 0;
}

char*
get_abs_crt_path(void)
{
  static char temp[512];

  snprintf(temp, 512, "%s/%s",
		   directory->crt_dir,
		   directory->filename[directory->cursor - 1]);

  return temp;
}

char*
get_mpd_crt_path(void)
{
  static char *root, *crt;
  
  root = directory->root_dir;
  crt = get_abs_crt_path();
  
  while(*root && *crt && *root++ == *crt++);

  if(!*root) // current directory is under the root directory
	return crt + 1;
  else
	return NULL;
}

void directory_redraw_screen(void)
{
  int line = 0, i, height = wchain[DIRECTORY].win->_maxy + 1;

  WINDOW *win = specific_win(DIRECTORY);  

  char *filename;
  for(i = directory->begin - 1; i < directory->begin
		+ height - 1 && i < directory->length; i++)
	{
	  filename = directory->prettyname[i];

	  if(i + 1 == directory->cursor)
		print_list_item(win, line++, 2, i + 1, filename, NULL);
	  else
		print_list_item(win, line++, 0, i + 1, filename, NULL);
	}
}

void
directory_helper(void)
{
  WINDOW *win = specific_win(DIRHELPER);

  color_print(win, 3, "\
Directories:");
  wprintw(win,  "\n\
----------------------------\n\n\
    < k > Move Cursor Up...\n\
    < j > Move Cursor Down.\n\
    <Ent> Enter Directory..\n\
    <Bck> Exit  Directory..");
  color_print(win, 5, "\n\n\
    [a] Append to Current..\n\
    [r] Replace Current....");
  wprintw(win, "\n\n\
    [c] Clear Current......");
}

void
directory_update(void)
{
  DIR *d;
  struct dirent *dir;

  d = opendir(directory->crt_dir);
  
  if(d)
	{

	  dir = readdir(d); // skip the directory itself

	  int i = 0;
  
	  while ((dir = readdir(d)) != NULL)
		{
		  if(dir->d_name[0] != '.')
			{
			  strncpy(directory->filename[i], dir->d_name, 128);
			  pretty_copy(directory->prettyname[i], dir->d_name, 128, 25);
			  i++;
			}
		}

	  directory->length = i;

	  closedir(d);
	}
}

void directory_update_checking(void)
{
  // TODO, when modification happened then update
  if(directory->update_signal)
	{
	  directory_update();
	  directory->update_signal = 0;
	  signal_all_wins();
	}
}

void
append_to_songlist(void)
{
  char *path, *absp;

  /* check if path exist */
  absp = get_abs_crt_path();
  if(!is_path_exist(absp))
	return;

  /* check if a nondir file of valid format */
  if(!is_dir_exist(absp) && !is_path_valid_format(absp))
	return;
  
  path = get_mpd_crt_path();

  if(path)
	{
	  mpd_run_add(conn, path);
	  char message[512];
	  snprintf(message, sizeof(message), "\"%s\" Has Been Appended.", path);
	  popup_simple_dialog(message);
	}	  
}

void replace_songlist(void)
{
  char *path, *absp;

  int choice =
	popup_confirm_dialog("Replacing Confirm:", 1);

  if(!choice) // action canceled
	return;

  /* check if path exist */
  absp = get_abs_crt_path();
  if(!is_path_exist(absp))
	return;

  /* check if a nondir file of valid format */
  if(!is_dir_exist(absp) && !is_path_valid_format(absp))
	return;
  
  path = get_mpd_crt_path();

  if(path)
	{
	  mpd_run_clear(conn);	  
	  mpd_run_add(conn, path);
	  char message[512];
	  snprintf(message, sizeof(message), "Replace With \"%s\".", path);
	  popup_simple_dialog(message);
	}	  
}

struct Directory* directory_setup(void)
{
  struct Directory *dir =
	(struct Directory*) malloc(sizeof(struct Directory));
  dir->update_signal = 0;
  
  dir->begin = 1;
  dir->length = 0;
  dir->cursor = 1;
  strncpy(dir->root_dir, "/home/ted/Music", 128);
  strncpy(dir->crt_dir, dir->root_dir, 512);

  // window mode setup
  dir->wmode.size = 5;
  dir->wmode.wins = (struct WindowUnit**)
	malloc(dir->wmode.size * sizeof(struct WindowUnit*));
  dir->wmode.wins[0] = &wchain[EXTRA_INFO];  
  dir->wmode.wins[1] = &wchain[DIRECTORY];  
  dir->wmode.wins[2] = &wchain[DIRHELPER];  
  dir->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  dir->wmode.wins[4] = &wchain[BASIC_INFO];
  dir->wmode.listen_keyboard = &directory_keymap;  

  return dir;
}

void directory_free(struct Directory *dir)
{
  free(dir->wmode.wins);
  free(dir);
}

void
enter_selected_dir(void)
{
  char *temp =  get_abs_crt_path();
  
  if(is_dir_exist(temp))
	{
	  strcpy(directory->crt_dir, temp);

	  directory->begin = 1;
	  directory->cursor = 1;
	  directory->update_signal = 1;
	}
}

void
exit_current_dir(void)
{
  char *p;

  p = strrchr(directory->crt_dir, '/');

  // prehibit exiting from root directory
  if(p - directory->crt_dir < (int)strlen(directory->root_dir))
	return;

  if(p && p != directory->crt_dir)
	{
	  *p = '\0';

	  directory->begin = 1;
	  directory->cursor = 1;
	  directory->update_signal = 1;
	}
}

void
directory_scroll_to(int line)
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  directory->cursor = 0;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, line);
}

void
directory_scroll_down_line(void)
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, +1);
}

void
directory_scroll_up_line(void)
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, -1);  
}

void
directory_scroll_up_page(void)
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, -15);  
}

void
directory_scroll_down_page(void)
{
  int height = wchain[DIRECTORY].win->_maxy + 1;
  scroll_line_shift_style(&directory->cursor, &directory->begin,
						  directory->length, height, +15);  
}
