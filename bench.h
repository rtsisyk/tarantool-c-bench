#if !defined(BENCH_H_INCLUDED)
#define BENCH_H_INCLUDED 1

#include <stddef.h>
#include <stdint.h>

#define API_EXPORT  extern "C" __attribute__ ((visibility ("default")))

/* {{{{ Utils */

API_EXPORT double
nowtime(void);

/* }}} */

/* {{{ Generators */

struct keygen_params {
	uint32_t len;
};

typedef char *(*keygen_t)(char *, const struct keygen_params *);

API_EXPORT char *
gen_uint(char *r, const struct keygen_params *params);

API_EXPORT char *
gen_str(char *r, const struct keygen_params *params);

API_EXPORT char *
gen_uint_uint(char *r, const struct keygen_params *params);

API_EXPORT char *
gen_str_str(char *r, const struct keygen_params *params);

API_EXPORT char *
gen_uint_str(char *r, const struct keygen_params *params);

API_EXPORT char *
gen_str_uint(char *r, const struct keygen_params *params);

/* }}} */

/* {{{ Tests */

struct test_params {
	keygen_t keygen;
	struct keygen_params *keygen_params;
	uint32_t space_id;
	uint32_t count;
};

typedef double (test_t)(const struct test_params *);

API_EXPORT void
test_keys(const struct test_params *params);

API_EXPORT void
test_selects(const struct test_params *params);

API_EXPORT void
test_replaces(const struct test_params *params);

API_EXPORT void
test_deletes(const struct test_params *params);

/* }}} */

#endif /* !defined(BENCH_H_INCLUDED) */