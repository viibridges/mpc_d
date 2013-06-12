#include "global.h"
#include "windows.h"
#include "playlist.h"

#ifndef HC08FNCKJDSFP9FASKD
#define HC08FNCKJDSFP9FASKD

struct Searchlist
{
  enum mpd_tag_type tags[4]; // searching type
  char key[128];
  int crt_tag_id;
  int update_signal; // signal that activates searchlist_update(void)
  int picking_mode; // 1 when picking a song

  // share the same heap with the playlist, why not!  I don't want to waste memory; and some functions can be reused.
  struct Playlist *plist; // pointer to playlist

  struct WinMode wmode; // windows in this mode
};

void searchlist_update(void);
void searchlist_update_checking(void);
void search_prompt(void);

#endif
