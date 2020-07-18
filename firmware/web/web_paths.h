#ifndef WEB_PATHS_H
#define WEB_PATHS_H

void web_paths_get(struct netconn *conn, char *url_buffer);

void web_paths_post(struct netconn *conn, char *url_buffer, char *postbody_buffer);

#endif /* WEB_PATHS_H */