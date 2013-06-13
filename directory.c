#include "directory.h"
#include "windows.h"
#include "utils.h"

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

void
directory_redraw_screen(void)
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

  wprintw(win,  "\n\
  Key Options:\n\
  ----------------------\n\
    [k] Move Cursor Up\n\
    [j] Move Cursor Down\n\
    [a] Append Current\n\
    [r] Rename Playlist");

  //wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);
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
			  pretty_copy(directory->prettyname[i], dir->d_name, 128, 60);
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

