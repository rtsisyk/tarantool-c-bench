#include "bench.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "box/box.h"
#include "request.h"
#include "tuple.h"
#include "port.h"
#include "fiber.h"

#if defined(MSGPUCK_H_INCLUDED)
#define MASTER 1
#include "box/index.h"
#endif

enum { REQUEST_BODY_MAXLEN = 1024 };

/* {{{ Utils */

#if !defined(MASTER)
void
assert_fail(const char *assertion, const char *file, unsigned int line,
	    const char *function)
{
	(void)function;
	fflush(NULL);
	printf("assert: %s:%d %s\n", file, line, assertion);
	exit(1);
}
#endif /* STABLE */

double
nowtime(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec + t.tv_usec * 1e-6;
}

void
randstr(char *out, size_t len)
{
	static const char alphanum[] =
			"0123456789-_"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
	static const size_t alen = sizeof(alphanum);
	for (size_t i = 0; i < len; ++i)
		out[i] = alphanum[rand() % (alen - 1)];
	out[len] = '\0';
}

/* }}} */

/* {{{ Generators */

char *
gen_num(char *r, const struct keygen_params *params)
{
	(void) params;
#if defined(MASTER)
	r = mp_encode_array(r, 1);
	r = mp_encode_uint(r, rand());
#else /* STABLE */
	r = pack_u32(r, 1);
	r = pack_varint32(r, sizeof(uint32_t));
	r = pack_u32(r, rand());
#endif
	return r;
}

API_EXPORT char *
gen_str(char *r, const struct keygen_params *params)
{
	char buf[params->len + 1];
#if defined(MASTER)
	r = mp_encode_array(r, 1);
	randstr(buf, params->len);
	r = mp_encode_str(r, buf, params->len);
#else /* STABLE */
	r = pack_u32(r, 1);
	randstr(buf, params->len);
	r = pack_lstr(r, buf, params->len);
#endif
	return r;
}

char *
gen_num_num(char *r, const struct keygen_params *params)
{
	(void) params;
#if defined(MASTER)
	r = mp_encode_array(r, 2);
	r = mp_encode_uint(r, rand());
	r = mp_encode_uint(r, rand());
#else /* STABLE */
	r = pack_u32(r, 2);
	r = pack_varint32(r, sizeof(uint32_t));
	r = pack_u32(r, rand());
	r = pack_varint32(r, sizeof(uint32_t));
	r = pack_u32(r, rand());
#endif
	return r;
}

API_EXPORT char *
gen_str_str(char *r, const struct keygen_params *params)
{
	char buf[params->len + 1];
#if defined(MASTER)
	r = mp_encode_array(r, 2);
	randstr(buf, params->len);
	r = mp_encode_str(r, buf, params->len);
	randstr(buf, params->len);
	r = mp_encode_str(r, buf, params->len);
#else /* STABLE */
	r = pack_u32(r, 2);
	randstr(buf, params->len);
	r = pack_lstr(r, buf, params->len);
	randstr(buf, params->len);
	r = pack_lstr(r, buf, params->len);
#endif
	return r;
}

char *
gen_num_str(char *r, const struct keygen_params *params)
{
	char buf[params->len + 1];
#if defined(MASTER)
	r = mp_encode_array(r, 2);
	r = mp_encode_uint(r, rand());
	randstr(buf, params->len);
	r = mp_encode_str(r, buf, params->len);
#else /* STABLE */
	r = pack_u32(r, 2);
	r = pack_varint32(r, sizeof(uint32_t));
	r = pack_u32(r, rand());
	randstr(buf, params->len);
	r = pack_lstr(r, buf, params->len);
#endif
	return r;
}

char *
gen_str_num(char *r, const struct keygen_params *params)
{
	char buf[params->len + 1];
#if defined(MASTER)
	r = mp_encode_array(r, 2);
	randstr(buf, params->len);
	r = mp_encode_str(r, buf, params->len);
	r = mp_encode_uint(r, rand());
#else /* STABLE */
	r = pack_u32(r, 2);
	randstr(buf, params->len);
	r = pack_lstr(r, buf, params->len);
	r = pack_varint32(r, sizeof(uint32_t));
	r = pack_u32(r, rand());
#endif
	return r;
}

/* }}} */

/* {{{ Tests */

struct port_bench
{
	struct port_vtab *vtab;
	size_t count;
};

#if !defined(MASTER)
static void
port_bench_add_tuple(struct port *port, struct tuple *tuple, uint32_t flags)
{
	(void) tuple;
	(void) flags;
	struct port_bench *port_bench = (struct port_bench *) port;
	port_bench->count++;
}

static struct port_vtab port_bench_vtab = {
	port_bench_add_tuple,
	null_port_eof,
};
#endif

void
test_keys(const struct test_params *params)
{
	char reqdata[REQUEST_BODY_MAXLEN];

	for (uint32_t i = 0; i < params->count; i++) {
		char *r = reqdata;
		params->keygen(r, params->keygen_params);
	}
}

void
test_selects(const struct test_params *params)
{
	char reqdata[REQUEST_BODY_MAXLEN];
#if !defined(MASTER)
	struct port_bench port = { &port_bench_vtab, 0 };
#endif

	for (uint32_t i = 0; i < params->count; i++) {
		char *r = reqdata;
#if defined(MASTER)
		char *key_end = params->keygen(r, params->keygen_params);
		box_tuple_t *result;
		box_index_get(params->space_id, 0, r, key_end, &result);
#else /* STABLE */
		port.count = 0;
		r = pack_u32(r, params->space_id); // space
		r = pack_u32(r, 0); // index
		r = pack_u32(r, 0); // offset
		r = pack_u32(r, 4294967295); // limit
		r = pack_u32(r, 1); // key count
		r = params->keygen(r, params->keygen_params);
		box_process((struct port *) &port, SELECT, reqdata, r - reqdata);
		assert(port.count == 1);
#endif
		fiber_gc();
	}
}

void
test_replaces(const struct test_params *params)
{
	char reqdata[REQUEST_BODY_MAXLEN];
#if !defined(MASTER)
	struct port_bench port = { &port_bench_vtab, 0 };
#endif

	for (uint32_t i = 0; i < params->count; i++) {
		char *r = reqdata;
#if defined(MASTER)
		char *tuple_end = params->keygen(r, params->keygen_params);
		box_replace(params->space_id, r, tuple_end, 0);

#else /* STABLE */
		port.count = 0;
		r = pack_u32(r, params->space_id); // space
		r = pack_u32(r, 0); // flags
		r = params->keygen(r, params->keygen_params);
		box_process((struct port *) &port, REPLACE, reqdata, r - reqdata);
#endif
		fiber_gc();
	}
}

void
test_selrepl(const struct test_params *params)
{
	char reqdata[REQUEST_BODY_MAXLEN];
#if !defined(MASTER)
	struct port_bench port = { &port_bench_vtab, 0 };
#endif

	for (uint32_t i = 0; i < params->count; i++) {
		char *r = reqdata;
#if defined(MASTER)
		char *rend = params->keygen(r, params->keygen_params);
		box_tuple_t *result;
		box_index_get(params->space_id, 0, r, rend, &result);
		box_replace(params->space_id, r, rend, 0);
#else /* STABLE */
		port.count = 0;
		r = pack_u32(r, params->space_id); // space
		r = pack_u32(r, 0); // index
		r = pack_u32(r, 0); // offset
		r = pack_u32(r, 4294967295); // limit
		r = pack_u32(r, 1); // key count
		r = params->keygen(r, params->keygen_params);
		box_process((struct port *) &port, SELECT, reqdata, r - reqdata);

		r = reqdata;
		r = pack_u32(r, params->space_id); // space
		r = pack_u32(r, 0); // flags
		r = params->keygen(r, params->keygen_params);
		box_process((struct port *) &port, REPLACE, reqdata, r - reqdata);
#endif
		fiber_gc();
	}
}

void
test_updates(const struct test_params *params)
{
	char reqdata[REQUEST_BODY_MAXLEN];
	char reqdata2[REQUEST_BODY_MAXLEN];
#if !defined(MASTER)
	struct port_bench port = { &port_bench_vtab, 0 };
#endif

	for (uint32_t i = 0; i < params->count; i++) {
		char *r = reqdata;
#if defined(MASTER)
		char *rend = params->keygen(r, params->keygen_params);
		char *r2 = reqdata2;
		r2 = mp_encode_array(r2, 1);
		r2 = mp_encode_array(r2, 3);
		r2 = mp_encode_str(r2, "!", 1);
		r2 = mp_encode_int(r2, -1);
		r2 = mp_encode_uint(r2, 0);
		box_update(params->space_id, 0, r, rend, reqdata2, r2, 0, 0);
#else /* STABLE */
		port.count = 0;
		r = pack_u32(r, params->space_id);
		r = pack_u32(r, 0); /* flags */
		r = params->keygen(r, params->keygen_params);
		r = pack_u32(r, 1); /* op_count */
		r = pack_u32(r, -1); /* field_no */
		r = pack_u8(r, 7); /* UPDATE_OP_INSERT */
		r = pack_varint32(r, sizeof(uint32_t));
		r = pack_u32(r, 0);
		box_process((struct port *) &port, UPDATE, reqdata, r - reqdata);
#endif
		fiber_gc();
	}

}

void
test_deletes(const struct test_params *params)
{
	char reqdata[REQUEST_BODY_MAXLEN];
#if !defined(MASTER)
	struct port_bench port = { &port_bench_vtab, 0 };
#endif

	for (uint32_t i = 0; i < params->count; i++) {
		char *r = reqdata;
#if defined(MASTER)
		const char *key_end = params->keygen(r, params->keygen_params);
		box_delete(params->space_id, 0, r, key_end, 0);
#else /* STABLE */
		port.count = 0;
		r = pack_u32(r, params->space_id);
		r = pack_u32(r, 0); /* flags */
		r = params->keygen(r, params->keygen_params);
		box_process((struct port *) &port, DELETE, reqdata, r - reqdata);
#endif
		fiber_gc();
	}
}

/* }}} */

