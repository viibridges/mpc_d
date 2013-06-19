#include "directory.h"
#include "windows.h"
#include "utils.h"
#include "keyboards.h"

int is_dir_exist(const char *path)
{
  struct stat s;

  if (!stat(path, &s) && (s.st_mode & S_IFDIR))
	return 1;
  else
	return 0;
}

int is_nondir_exist(const char *path)
{
  struct stat s;

  if (!stat(path, &s) && (s.st_mode & (S_IFREG | S_IFLNK)))
	return 1;
  else
	return 0;
}

int is_path_exist(const char *path)
{
  return is_dir_exist(path) || is_nondir_exist(path);
}

int is_path_valid_format(const char *path)
{
  const int music_format_num = 6;

  char valid_format[][12] = {
	"mp3", "flac", "wav", "wma", "ape", "ogg"
  };

  char *path_suffix = strrchr(path, '.');

  int i;
  
  if(path_suffix)
	{
	  for(i = 0; i < music_format_num; i++)
		if(is_substring_ignorecase(path_suffix, valid_format[i]))
		  return 1;
	}
  else
	return 0;

  return 0;
}

// whether the path should be in the list
int is_path_visible(const char *path)
{
  char *path_suffix = strrchr(path, '/');

  if(!path_suffix)
	return 0;
  
  return path_suffix[1] != '.' &&
	(is_dir_exist(path)
	 || (is_path_valid_format(path) && is_nondir_exist(path)));
}

char* get_abs_path(const char *filename)
{
  static char temp[512];

  char crt_dir[512];
  strncpy(crt_dir, directory->crt_dir, sizeof(crt_dir));

  if(*crt_dir)
	{
	  char *pt = crt_dir + strlen(crt_dir) - 1;
	  
	  while(*pt == '/') *pt = '\0'; // drop '/'
  
	  snprintf(temp, 512, "%s/%s", crt_dir, filename);
	}

  return temp;
}

char* get_abs_crt_path(void)
{
  return get_abs_path(directory->filename[directory->cursor - 1]);
}

char* get_mpd_path(char *abs_path)
{
  static char *root, *absp;
  
  root = directory->root_dir;
  absp = abs_path;
  
  while(*root && *absp && *root++ == *absp++);

  if(!*root) // current directory is under the root directory
	return absp + 1;
  else
	return NULL;
} 
 
char* get_mpd_crt_path(void)
{
  return get_mpd_path(get_abs_crt_path());
}

int get_last_dir_id(void)
{
  return directory->curs_history[directory->level];
}

void set_level_by(int offset)
{
  directory->level += offset;
  
  if(directory->level >= 64)
	directory->level = 64;
  
  if(directory->level < 0)
	directory->level = 0;
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

  if(directory->length < 1) // no item in the list
	print_list_item(win, line, 1, 0, "no item in the list", NULL);
}

void
directory_display_icon(void)
{
  WINDOW *win = specific_win(DIRICON);

  color_print(win, 1, "\
.----.______\n\
|DIR _______|___\n\
|   /          /\n\
|  /          /\n\
| /          /\n\
|/__________/");
}

void
directory_helper(void)
{
  WINDOW *win = specific_win(DIRHELPER);

  wmove(win, 2, 0);
  wprintw(win,  "\
   <k>\t  Move Cursor Up\n\
   <j>\t  Move Cursor Down\n\
   Ent\t  Enter Directory\n\
   Bck\t  Exit  Directory");
  color_print(win, 6, "\n\
   [a]\t  Append to Current\n\
   [r]\t  Replace Current");
  wprintw(win, "\n\
   [c]\t  Clear Current");

  box(win, ':', ' ');
  
  wmove(win, 0, 2);
  color_print(win, 3, "Instruction:");
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
		  char * absp = get_abs_path(dir->d_name);
		  if(is_path_visible(absp))
			{
			  strncpy(directory->filename[i], dir->d_name, 128);
			  pretty_copy(directory->prettyname[i], dir->d_name, 127, 24);
			  if(is_dir_exist(absp))
				{
				  char *pname = directory->prettyname[i];
				  int j = strlen(pname);
				  pname[j] = '/';
				  pname[j + 1] = '\0';
				}
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
	popup_confirm_dialog("Replacing Confirm:", 0);

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
  dir->level = 0;
  strncpy(dir->root_dir, "/home/ted/Music", 128);
  strncpy(dir->crt_dir, dir->root_dir, 512);

  // window mode setup
  dir->wmode.size = 6;
  dir->wmode.wins = (struct WindowUnit**)
	malloc(dir->wmode.size * sizeof(struct WindowUnit*));
  dir->wmode.wins[0] = &wchain[EXTRA_INFO];  
  dir->wmode.wins[1] = &wchain[DIRECTORY];  
  dir->wmode.wins[2] = &wchain[DIRHELPER];  
  dir->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  dir->wmode.wins[4] = &wchain[BASIC_INFO];
  dir->wmode.wins[5] = &wchain[DIRICON];
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

	  directory->curs_history[directory->level] = directory->cursor;
	  set_level_by(1); // level++

	  directory_scroll_to(1);
	  directory->update_signal = 1;
	}
}

int // if succeed return 0
exit_current_dir(void)
{
  char *p;

  p = strrchr(directory->crt_dir, '/');

  // prehibit exiting from root directory
  if(p - directory->crt_dir < (int)strlen(directory->root_dir))
	return 1;

  if(p && p != directory->crt_dir)
	{
	  *p = '\0';

	  set_level_by(-1); // level--

	  directory_update(); // must be done before scrolling
	  directory_scroll_to(get_last_dir_id());
	}

  return 0;
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
