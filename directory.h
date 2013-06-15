#include "global.h"
#include "windows.h"

#ifndef LKAJSDOIFNAOCW9O8IFJ
#define LKAJSDOIFNAOCW9O8IFJ

struct Directory
{
  char root_dir[128];
  char crt_dir[512];
  char filename[MAX_SONGLIST_STORE_LENGTH][512]; // all items in current dir
  char prettyname[MAX_SONGLIST_STORE_LENGTH][128]; // all items in current dir

  struct WinMode wmode; // windows in this mode

  int update_signal;

  int length;
  int begin;
  int cursor;
};

struct Directory *directory;

int is_dir_exist(const char *path);
int is_nondir_exist(const char *path);
int is_path_exist(const char *path);
int is_path_valid_format(const char *path);
int is_path_visible(const char *path);
char* get_abs_path(const char *filename);
char* get_abs_crt_path(void);
char* get_mpd_path(char *abs_path);
char* get_mpd_crt_path(void);
void directory_redraw_screen(void);
void directory_display_icon(void);
void directory_helper(void);
void directory_update(void);
void directory_update_checking(void);

void append_to_songlist(void);
void replace_songlist(void);

struct Directory* directory_setup(void);
void directory_free(struct Directory *dir);

// list manipulation commands
void enter_selected_dir(void);
void exit_current_dir(void);
void directory_scroll_to(int line);
void directory_scroll_down_line(void);
void directory_scroll_up_line(void);
void directory_scroll_up_page(void);
void directory_scroll_down_page(void);

#endif
