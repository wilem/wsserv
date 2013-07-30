#ifndef _INDEXER_H_
#define _INDEXER_H_

#ifdef __cplusplus
extern "C" {
#endif

// put JSON format file list into a evbuffer.
struct evbuffer *list_dir(const char *dir_path);

#ifdef __cplusplus
}
#endif

#endif //_INDEXER_H_
