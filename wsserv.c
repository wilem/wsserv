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

char *h_html[]  = {"Content-Type", "text/html; charset=UTF-8"};
char *h_xml[]   = {"Content-Type", "text/xml; charset=UTF-8"};
char *h_js[]    = {"Content-Type", "text/javascript; charset=UTF-8"};
char *h_json[]  = {"Content-Type", "application/json; charset=UTF-8"};
char *h_icon[]  = {"Content-Type", "image/x-icon"};
char *h_cache[] = {"Cache-Control", "max-age=2592000"};

char f_prefix[] = "/f"; // file CONTROLLER prefix.
char m_prefix[] = "/m"; // file MODEL/json data prefix.
char v_prefix[] = "/v"; // file info VIEW/template prefix.
			// /v link to /f/app/view

static unsigned long click;

static void ctrler_handler(struct evhttp_request *, void *);
static void  model_handler(struct evhttp_request *, void *);

// handle prefix URIs
static int
dispatch_request(struct evhttp_request *req, void *arg)
{
fprintf(stderr, "+%s()\n", __func__);
	// match /f.*
	if (0 == strncmp(req->uri, f_prefix, strlen(f_prefix))) {
		ctrler_handler(req, arg);
	} else if (0 == strncmp(req->uri, m_prefix, strlen(m_prefix))) {
		model_handler(req, arg);
	} else { // TODO other handler.
	// unknown URI, unhandled.
fprintf(stderr, "-%s()?\n", __func__);
		return -1;
	}

fprintf(stderr, "-%s()\n", __func__);
	return 0;
}

static void
gen_handler(struct evhttp_request *req, void *arg)
{
	struct evbuffer *html;

	// dispatch URI
	fprintf(stderr, "> req.uri=\"%s\".\n", req->uri);
	if (0 != strcmp(req->uri, "/")) {
		if (0 == dispatch_request(req, arg))
			return;
	}
	//TODO handle "/"

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

////////////////// ALLOC MEM
// return:
//  0: ok
//  1: not found
// -1: error
static int
gen_file_data(const char *path, struct evbuffer **datap)
{
	int fd = open(path, O_RDONLY);
	if (-1 == fd) { //error
		perror("open");
		return -1;
	} else {
		////////////////// ALLOC MEM
		*datap = evbuffer_new();
		//XXX large file not support.
		evbuffer_read(*datap, fd, WS_MAX_FILE_LEN);
		return 0;
	}
}

// generate file index or return file itself.
// return:
//  1: NOT FOUND.
//  0: read OK.
// -1: read ERROR.
////////////////// ALLOC MEM
static int
gen_path_data(const char *uri, struct evbuffer **datap)
{
fprintf(stderr, "+%s() path=\"%s\"\n", __func__, uri);
	char path[512] = {0};

	// current dir from getcwd,
	// TODO chdir is not supported.
	// check uri for path or file name;
	// form a proper path relative to cwd
	snprintf(path, sizeof(path), "./%s", uri); //XXX relative path
	fprintf(stderr, "list_dir: %s\n", path);
	////////////////// ALLOC MEM
	*datap = list_dir(path);	// relpath
	if (-1 == (int)(*datap)) {	// not found
fprintf(stderr, "-%s() uri=\"%s\"\n", __func__, uri);
		return 1;
	} else if (NULL != *datap) {	// dir ok
fprintf(stderr, "-%s() uri=\"%s\"\n", __func__, uri);
		return 0;
	}
	//else if *datap == NULL: not dir
	// regular file.
	fprintf(stderr, "read_file: %s\n", path);
fprintf(stderr, "-%s() uri=\"%s\"\n", __func__, uri);
	return gen_file_data(path, datap);
}

static void
// static file handler: /f.* ;; CONTROLLER for file.
ctrler_handler(struct evhttp_request *req, void *arg)
{
fprintf(stderr, "+%s() uri=\"%s\"\n", __func__, req->uri);
	struct evbuffer *resp = NULL;
	char *fpath = req->uri + strlen(f_prefix);
	int ret, plen = strlen(fpath);

fprintf(stderr, "fpath=\"%s\"\n", fpath);
	// "" or "/" are root dir.
	char end = fpath[plen - 1];
	if (0 == plen || '/' == end) {
		//XXX redirect to ./v/index.html
		ret = gen_file_data("v/index.html", &resp);
fprintf(stderr, "index=\"%s\", ret=%d\n", fpath, ret);
	} else {
		ret = gen_path_data(fpath + 1, &resp); // del "/"
	}

	// header
	evhttp_add_header(req->output_headers, h_html[0], h_html[1]);
	evhttp_add_header(req->output_headers, h_cache[0], h_cache[1]);
	// msg body
	if (0 == ret) {
		evhttp_send_reply(req, HTTP_OK, "OK", resp);
	} else if (1 == ret) {
		////////////////// ALLOC MEM
		resp = evbuffer_new();
		evbuffer_add_printf(resp, "<h1>404, Not Found.</h1>");
		evhttp_send_reply(req, HTTP_NOTFOUND, "NOT FOUND", resp);
	} else {
		////////////////// ALLOC MEM
		resp = evbuffer_new();
		evbuffer_add_printf(resp, "<h1>400, Bad Request.</h1>");
		evhttp_send_reply(req, HTTP_BADREQUEST, "BAD REQUEST", resp);
	}

	////////////////// FREE MEM
	evbuffer_free(resp);
fprintf(stderr, "-%s() path=\"%s\"\n", __func__, fpath);
}

// JSON handler: /m.* ;; MODEL
static void
model_handler(struct evhttp_request *req, void *arg)
{
fprintf(stderr, "+%s() uri=\"%s\"\n", __func__, req->uri);
	struct evbuffer *resp = NULL;
	const char *mpath = req->uri + strlen(m_prefix);
	int ret, plen = strlen(mpath);

fprintf(stderr, "mpath=\"%s\"\n", mpath);
	// "" or "/" are root dir.
	if (0 == plen || (1 == plen && '/' == mpath[0])) { // doc root
		ret = gen_path_data(".", &resp);
	} else { // other dir
		ret = gen_path_data(mpath, &resp);
	}

	// header
	evhttp_add_header(req->output_headers, h_json[0], h_json[1]);
	evhttp_add_header(req->output_headers, h_cache[0], h_cache[1]);
	// msg body
	if (0 == ret) {
		evhttp_send_reply(req, HTTP_OK, "OK", resp);
	} else if (1 == ret) {
		////////////////// ALLOC MEM
		resp = evbuffer_new();
		evbuffer_add_printf(resp, "<h1>404, Not Found.</h1>");
		evhttp_send_reply(req, HTTP_NOTFOUND, "NOT FOUND", resp);
	} else {
		////////////////// ALLOC MEM
		resp = evbuffer_new();
		evbuffer_add_printf(resp, "<h1>400, Bad Request.</h1>");
		evhttp_send_reply(req, HTTP_BADREQUEST, "BAD REQUEST", resp);
	}

	////////////////// FREE MEM
	evbuffer_free(resp);
fprintf(stderr, "-%s() path=\"%s\"\n", __func__, mpath);
}

static void
httpd_url_handler_init(struct evhttp *h)
{
	evhttp_set_cb(h, "/f", ctrler_handler, NULL);
	evhttp_set_cb(h, "/m", model_handler, NULL);
	evhttp_set_cb(h, "/favicon.ico", icon_handler, NULL);
	evhttp_set_gencb(h, gen_handler, gen_tpl);
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
