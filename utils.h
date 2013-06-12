#include "global.h"

/*************************************
 **    SYSTEM  AND  MISCELLANY      **
 *************************************/
void printErrorAndExit(struct mpd_connection *conn);
void smart_sleep(void);
void my_finishCommand(struct mpd_connection *conn);
struct mpd_status * getStatus(struct mpd_connection *conn);
struct mpd_status* init_mpd_status(struct mpd_connection *conn);
int is_dir_exist(char *path);
int is_path_exist(char *path);
const char * get_song_format(const struct mpd_song *song);
const char * get_song_tag(const struct mpd_song *song, enum mpd_tag_type type);
char *is_substring_ignorecase(const char *main, char *sub);
void pretty_copy(char *string, const char * tag, int size, int width);
void scroll_line_shift_style
(int *cursor, int *begin, const int total, const int height, const int lines);

