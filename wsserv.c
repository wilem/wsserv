#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <event.h>
#include <evhttp.h>

#include "base64.h"
#include "sha1.h"
#include "indexer.h"

#define WS_MAX_FILE_LEN 4096 * 1024

static char *magic_key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// TODO not thread safe
// context
SHA1_CTX sha1_ctx;
// digest result
uint8_t  sha1_dig[SHA1_DIGEST_SIZE];

static void
sha1_digest(const uint8_t *data, const size_t len)
{
	SHA1_Init(&sha1_ctx);
	SHA1_Update(&sha1_ctx, data, len);
	SHA1_Final(&sha1_ctx, sha1_dig);
}

/**
 * cli_key + magic_key ==[sha-1]==> bin_key
 * bin_key ==[base64endcode]==> accept_key
 **/
static char *
accept_client_key(const char *cli_key, int key_len)
{
	int mk_len, bk_len, o_len;
	char *bk, *ok;

	mk_len = strlen(magic_key);
	bk_len = key_len + mk_len;
	bk = calloc(1, bk_len);
	memcpy(bk, cli_key, key_len);
	memcpy(bk + key_len, magic_key, mk_len);

	sha1_digest((uint8_t *)bk, (size_t)bk_len);

	o_len = Base64encode_len(SHA1_DIGEST_SIZE);
	ok = calloc(1, o_len);
	Base64encode(ok, (char *)sha1_dig, SHA1_DIGEST_SIZE);

	return ok;
}

int
test()
{
	char *txt = "hello\n";

	int olen = Base64encode_len(strlen(txt));

	char *ent = calloc(1, olen);

	Base64encode(ent, txt, strlen(txt));

	printf("src: \"%s\"\n", txt);
	printf("dst: \"%s\"\n", ent);

	char *cli_key, *acc_key;

	cli_key = "x3JJHMbDL1EzLkh9GBhXDw==";
	acc_key = accept_client_key(cli_key, strlen(cli_key));

	printf("cli: \"%s\"\n", cli_key);
	printf("acc: \"%s\"\n", acc_key);

	return 0;
}

static struct evhttp *httpd;

char *gen_tpl =	"<div id='footer' align='center'>"
		"<h1>404, Not Found.</h1></div>";
char *foot_tpl ="<hr /><div id='footer' align='right'>"
		"<b>Powered by Hayabusa HTTP Server v0.1 2013</b></div>";
char h_buf[1024];

char *h_html[] = {"Content-Type", "text/html; charset=UTF-8"};
char *h_xml[]  = {"Content-Type", "text/xml; charset=UTF-8"};
char *h_icon[] = {"Content-Type", "image/x-icon"};
char *h_cache[] = {"Cache-Control", "max-age=2592000"};

char file_uri[] = "/file";

static unsigned long click;

static void file_handler(struct evhttp_request *, void *);

// handle unregistered URIs
static int
dispatch_request(struct evhttp_request *req, void *arg)
{
fprintf(stderr, "+%s()\n", __func__);
	// match file\/.*
	if (0 == strncmp(req->uri, file_uri, strlen(file_uri))) {
		file_handler(req, arg);
fprintf(stderr, "-%s()\n", __func__);
		return 0;
	}
	// TODO other handler.

	// unknown URI, unhandled.
fprintf(stderr, "-%s()?\n", __func__);
	return -1;
}

static void
gen_handler(struct evhttp_request *req, void *arg)
{
	struct evbuffer *html;

	// dispatch URI
	fprintf(stderr, "> req.uri=\"%s\".\n", req->uri);
	if (0 != strcmp(req->uri, "/")) {
		if (dispatch_request(req, arg))
			return;
	}

	// index page.
	////////////////////// ALLOC MEM
	html = evbuffer_new();

	// add HTML page content
	evbuffer_add(html, gen_tpl, strlen(gen_tpl));

	evbuffer_add(html, foot_tpl, strlen(foot_tpl));
	evbuffer_add_printf(html,
		"<hr />site visit: %lu.", ++click);
	// add HTTP headers
	evhttp_add_header(req->output_headers, h_html[0], h_html[1]);
	// send response to client
	evhttp_send_reply(req, HTTP_OK, "OK", html);

	////////////////////// FREE MEM
	evbuffer_free(html);
}

// map URI to file/path name
// file indexer
// char **
// index_files(char *root, size_t nfile)

// reply with a fake icon.
static void
icon_handler(struct evhttp_request *req, void *arg)
{
        u_char icon[4] = { 0xFF, 0, 0xFF, 0 };

	struct evbuffer *page = evbuffer_new();

	evbuffer_add(page, icon, sizeof icon);
	evhttp_add_header(req->output_headers, h_icon[0], h_icon[1]);
	evhttp_add_header(req->output_headers, h_cache[0], h_cache[1]);
	evhttp_send_reply(req, HTTP_OK, "OK", page);

	evbuffer_free(page);
}

// generate file index or file itself.
////////////////// ALLOC MEM
static struct evbuffer *
gen_file_data(const char *uri)
{
fprintf(stderr, "+%s() uri=\"%s\"\n", __func__, uri);
	char path[512] = {0};
	struct evbuffer *data = NULL;

	// current dir from getcwd,
	// TODO chdir is not supported.
	// check uri for path or file name;
	// form a proper path relative to cwd
	char pat[] = "/file";
	char *pstr = strstr(uri, pat);
	if (NULL == pstr)
		goto file_error;
	pstr += strlen(pat);
	fprintf(stderr, "path: \"%s\"\n", pstr);
	snprintf(path, sizeof(path), "./%s", pstr); //XXX relative path
	if (0 == strlen(path)) // ""
		path[0] = '.'; // "."
	fprintf(stderr, "list_dir: %s\n", path);
	////////////////// ALLOC MEM
	data = list_dir(path);
	if (-1 == (int)data) { // not found
		goto file_error;
	} else if (NULL != data) { // dir
		goto file_ok;
	}
	// regular file.
	fprintf(stderr, "read_path: %s\n", path);
	int fd = open(path, O_RDONLY);
	if (-1 == fd) { //error
		perror("open");
	} else {
		////////////////// ALLOC MEM
		data = evbuffer_new();
		//XXX large file not support.
		evbuffer_read(data, fd, WS_MAX_FILE_LEN);
	}

file_ok:
fprintf(stderr, "-%s() uri=\"%s\"\n", __func__, uri);
	return data;

file_error:
	////////////////// ALLOC MEM
	data = evbuffer_new();
	evbuffer_add_printf(data,
			"file: \"%s\" not found.", uri);
	return data;
}

// static file handler.
static void
file_handler(struct evhttp_request *req, void *arg)
{
fprintf(stderr, "+%s() uri=\"%s\"\n", __func__, req->uri);

	struct evbuffer *data, *resp = evbuffer_new();
	// alloc mem
	data = gen_file_data(req->uri);
	evbuffer_add_buffer(resp, data);
	evhttp_add_header(req->output_headers, h_html[0], h_html[1]);
	evhttp_add_header(req->output_headers, h_cache[0], h_cache[1]);
	evhttp_send_reply(req, HTTP_OK, "OK", resp);

	// free mem
	evbuffer_free(data);
	evbuffer_free(resp);
fprintf(stderr, "-%s() uri=\"%s\"\n", __func__, req->uri);
}

static void
httpd_url_handler_init(struct evhttp *h)
{
	evhttp_set_gencb(h, gen_handler, gen_tpl);
	evhttp_set_cb(h, "/file", file_handler, NULL);
	evhttp_set_cb(h, "/favicon.ico", icon_handler, NULL);
}

int
httpd_init(int port)
{
	char *host = "0.0.0.0";
	httpd = evhttp_start(host, port);
	if (httpd == NULL) {
		fprintf(stderr, "HTTP fail to start at %s:%d\n", host, port);
		return -1;
	}

	httpd_url_handler_init(httpd);

	return 0;
}

int
main()
{
	event_init();

	int ret = httpd_init(8080);
	if (ret)
		return ret;

	event_dispatch();
	return 0;
}

/* wsserv.c */
