
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <event.h>
#include <evhttp.h>

#include "base64.h"
#include "sha1.h"

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

static unsigned long click;

static void
gen_handler(struct evhttp_request *req, void *arg)
{
	struct evbuffer *html;

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

// file lister handler.
static void
flst_handler(struct evhttp_request *req, void *arg)
{

}

static void
httpd_url_handler_init(struct evhttp *h)
{
	evhttp_set_gencb(h, gen_handler, gen_tpl);
	evhttp_set_cb(h, "/favicon.ico", icon_handler, NULL);
	//TODO add file lister handler.
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

	int ret = httpd_init(80);
	if (ret)
		return ret;

	event_dispatch();
	return 0;
}

/* wsserv.c */
