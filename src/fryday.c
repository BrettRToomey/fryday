#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>

#include "assets.h"

#define BAD_REQUEST(x) http_response(req, 400, x, sizeof(x)-1)
#define Get(x, y) http_argument_get_string(req, x, y)

int init(int);

// MARK: @Endpoints
int	page(struct http_request *);
int nominations(struct http_request *);

static int createNomination(struct http_request *req);

// MARK: @Validators
int v_string(struct http_request *, char *);

int init(int state) {
    kore_pgsql_register("database", "postgresql://127.0.0.1/fryday");
    return KORE_RESULT_OK;
}

int createNomination(struct http_request *req) {
    char *nominee, *nominator, *subject;
    nominee = nominator = subject = NULL;

    http_populate_post(req);

    Get("nominee",   &nominee);
    Get("nominator", &nominator);
    Get("subject",   &subject);

    if (!nominee || !nominator || !subject) {
        BAD_REQUEST("Endpoint requires nominee, nominator and subject");
        return KORE_RESULT_OK;
    }

    struct kore_pgsql sql;

    kore_pgsql_init(&sql);

    if (!kore_pgsql_setup(&sql, "database", KORE_PGSQL_SYNC)) {
        kore_pgsql_logerror(&sql);
        goto out;
    }

    int ok = kore_pgsql_query_params(
        &sql,
        "INSERT INTO nominations (subject, nominee, nominator) VALUES ($1, $2, $3)",
        /*type:*/1, /*count:*/3,
        subject, 0, 0,
        nominee, 0, 0,
        nominator, 0, 0
    );

    if (!ok) {
        kore_pgsql_logerror(&sql);
        goto out;
    }

out:
    http_response_header(req, "Location", "/");
	http_response(req, 303, NULL, 0);

    kore_pgsql_cleanup(&sql);

    return KORE_RESULT_OK;
}

int page(struct http_request *req) {
    if (req->method == HTTP_METHOD_POST) {
        return createNomination(req);
    }

    http_response_header(req, "Content-Type", "text/html");
    http_response_header(req, "Access-Control-Allow-Origin", "*");
	http_response(req, 200, asset_index_html, asset_len_index_html);

	return KORE_RESULT_OK;
}

int nominations(struct http_request *req) {
    struct kore_pgsql sql;

    u_int8_t *content = NULL;
    size_t len = 0;

    kore_pgsql_init(&sql);

    if (!kore_pgsql_setup(&sql, "database", KORE_PGSQL_SYNC)) {
        kore_pgsql_logerror(&sql);
        goto out;
    }

    if (!kore_pgsql_query(&sql, "SELECT * FROM nominations ORDER BY votes DESC;")) {
        kore_pgsql_logerror(&sql);
        goto out;
    }

    char	*id, *subject, *nominee, *nominator, *votes;
	int		i, rows;

    struct kore_buf *b = kore_buf_alloc(2048);
    kore_buf_appendf(b, "[");

	rows = kore_pgsql_ntuples(&sql);
    for (i = 0; i < rows; i += 1) {
        if (i) {
            kore_buf_appendf(b, ",");
        }

        id        = kore_pgsql_getvalue(&sql, i, 0);
        subject   = kore_pgsql_getvalue(&sql, i, 1);
        nominee   = kore_pgsql_getvalue(&sql, i, 2);
        nominator = kore_pgsql_getvalue(&sql, i, 3);
        votes     = kore_pgsql_getvalue(&sql, i, 4);

        kore_buf_appendf(b, "{\"id\": %s, \"subject\": \"%s\", \"nominee\": \"%s\", \"nominator\": \"%s\", \"votes\": %s}", id, subject, nominee, nominator, votes);
    }


    kore_buf_appendf(b, "]");
    content = kore_buf_release(b, &len);

    http_response_header(req, "Content-Type", "application/json");

out:
    http_response(req, 200, content, len);
    kore_pgsql_cleanup(&sql);

    if (content)
        kore_free(content);

    return KORE_RESULT_OK;
}

int v_string(struct http_request *req, char *data) {
    return KORE_RESULT_OK;
}
