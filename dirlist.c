#include "dirlist.h"
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
		   dirlist->crt_dir,
		   dirlist->filename[dirlist->cursor - 1]);

  return temp;
}

char*
get_mpd_crt_path(void)
{
  static char *root, *crt;
  
  root = dirlist->root_dir;
  crt = get_abs_crt_path();
  
  while(*root && *crt && *root++ == *crt++);

  if(!*root) // current directory is under the root directory
	return crt + 1;
  else
	return NULL;
}

void
dirlist_redraw_screen(void)
{
  int line = 0, i, height = wchain[DIRLIST].win->_maxy + 1;

  WINDOW *win = specific_win(DIRLIST);  

  char *filename;
  for(i = dirlist->begin - 1; i < dirlist->begin
		+ height - 1 && i < dirlist->length; i++)
	{
	  filename = dirlist->prettyname[i];

	  if(i + 1 == dirlist->cursor)
		print_list_item(win, line++, 2, i + 1, filename, NULL);
	  else
		print_list_item(win, line++, 0, i + 1, filename, NULL);
	}
}

void
dirlist_update(void)
{
  DIR *d;
  struct dirent *dir;

  d = opendir(dirlist->crt_dir);
  
  if(d)
	{

	  dir = readdir(d); // skip the directory itself

	  int i = 0;
  
	  while ((dir = readdir(d)) != NULL)
		{
		  if(dir->d_name[0] != '.')
			{
			  strncpy(dirlist->filename[i], dir->d_name, 128);
			  pretty_copy(dirlist->prettyname[i], dir->d_name, 128, 60);
			  i++;
			}
		}

	  dirlist->length = i;

	  closedir(d);
	}
}

void dirlist_update_checking(void)
{
  // TODO, when modification happened then update
  if(dirlist->update_signal)
	{
	  dirlist_update();
	  dirlist->update_signal = 0;
	  signal_all_wins();
	}
}

