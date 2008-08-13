#ifndef __SCROBBLER_H__
#define __SCROBBLER_H__

#define SCROBBLER_TMP_FILE "/tmp/scrobbler_tmp"
#define SCROBBLER_USER "elmo2k3"
#define SCROBBLER_PASS_HASH "FILL_ME!"

#define SCROBBLER_HANDSHAKE_EXECUTE "wget -O %s \"http://post.audioscrobbler.com/?hs=true&p=1.2&c=tst&v=1.0&u=%s&t=%qd&a=%s\" >> /dev/null 2>&1"
#define SCROBBLER_MD5_EXECUTE "echo -n %s%qd | md5sum > /tmp/scrobbler_tmp"

#define SCROBBLER_NOW_PLAYING_EXECUTE "wget -O %s %s --post-data 's=%s&a=%s&t=%s&b=%s&l=%d&n=%s' >> /dev/null 2>&1" 
#define SCROBBLER_SUBMISSION_EXECUTE "wget -O %s %s --post-data 's=%s&a[0]=%s&t[0]=%s&b[0]=%s&l[0]=%d&n[0]=%s&i[0]=%qd&o[0]=P&r[0]=L&m[0]=' >> /dev/null 2>&1" 

extern int scrobblerHandshake(char *session_id, char *now_playing, char *submission);
extern int scrobblerNowPlaying(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track);
extern int scrobblerSubmitTrack(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track, time_t started_playing);

#endif

