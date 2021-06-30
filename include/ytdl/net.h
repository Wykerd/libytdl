#ifndef YTDL_NET_H
#define YTDL_NET_H

/**
 * Not very accurate but fast way to find youtube video url
 * 
 * May match invalid urls!
 * @returns 0 if successful. -1 if invalid url and 1 if not matched.
 */
int ytdl_net_get_id_from_url (const char *url_str, char id[12]);

#endif