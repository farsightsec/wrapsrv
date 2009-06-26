/*
 * Copyright (c) 2009 by Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Import. */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>

#include <sys/wait.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <isc/list.h>

/* Data types. */

struct srv {
	LINK(struct srv)	link;
	char			*tname;
	uint16_t		weight;
	uint16_t		port;
};

struct srv_prio {
	LINK(struct srv_prio)	link;
	LIST(struct srv)	srv_list;
	uint16_t		prio;
};

typedef LIST(struct srv)	srv_list;
typedef LIST(struct srv_prio)	srv_prio_list;

/* Globals. */

static srv_prio_list prio_list;

/* Forward. */

static char *subst_cmd(struct srv *, const char *);
static char *target_name(const unsigned char *);
static int do_cmd(struct srv *, int, char **);
static struct srv *next_tuple(void);
static void free_tuples(void);
static void insert_tuple(char *, uint16_t, uint16_t, uint16_t);
static void parse_answer_section(ns_msg *);
static void usage(void);

#ifdef DEBUG
static void print_tuples(void);
#endif

/* Functions. */

static struct srv *
next_tuple(void) {
	struct srv_prio *pe;
	struct srv *se;
	uint16_t rnd;
	unsigned csum = 0;
	unsigned wsum = 0;

	pe = HEAD(prio_list);
	if (pe == NULL)
		return (NULL);

	for (se = HEAD(pe->srv_list);
	     se != NULL;
	     se = NEXT(se, link))
	{
		wsum += se->weight;
	}

	rnd = random() % (wsum + 1);

	for (se = HEAD(pe->srv_list);
	     se != NULL;
	     se = NEXT(se, link))
	{
		csum += se->weight;

		if (csum >= rnd) {
			UNLINK(pe->srv_list, se, link);
			break;
		}
	}

	if (se == NULL) {
		UNLINK(prio_list, pe, link);
		free(pe);
		return (next_tuple());
	}

#ifdef DEBUG
	fprintf(stderr, "rnd=%hu -> prio=%hu weight=%hu port=%hu tname=%s\n",
		rnd, pe->prio, se->weight, se->port, se->tname);
#endif

	return (se);
}

static void
free_tuples(void) {
	struct srv_prio *pe, *pe_next;
	struct srv *se, *se_next;

	pe = HEAD(prio_list);
	while (pe != NULL) {
		pe_next = NEXT(pe, link);
		UNLINK(prio_list, pe, link);

		se = HEAD(pe->srv_list);
		while (se != NULL) {
			se_next = NEXT(se, link);
			UNLINK(pe->srv_list, se, link);
			free(se->tname);
			free(se);
			se = se_next;
		}

		free(pe);
		pe = pe_next;
	}
}

static void
insert_tuple(char *tname, uint16_t prio, uint16_t weight, uint16_t port) {
	struct srv_prio *pe;
	struct srv *se;

	for (pe = HEAD(prio_list);
	     pe != NULL;
	     pe = NEXT(pe, link))
	{
		if (pe->prio == prio)
			break;
	}
	
	if (pe == NULL) {
		struct srv_prio *piter;

		pe = malloc(sizeof(*pe));
		assert(pe != NULL);

		INIT_LINK(pe, link);
		INIT_LIST(pe->srv_list);
		pe->prio = prio;

		for (piter = HEAD(prio_list);
		     piter != NULL;
		     piter = NEXT(piter, link))
		{
			assert(piter->prio != prio);

			if (piter->prio > prio) {
				INSERT_BEFORE(prio_list, piter, pe, link);
				break;
			}
		}

		if (piter == NULL)
			APPEND(prio_list, pe, link);
	}

	se = malloc(sizeof(*se));
	assert(se != NULL);

	INIT_LINK(se, link);
	se->tname = tname;
	se->weight = weight;
	se->port = port;

	APPEND(pe->srv_list, se, link);
}

#ifdef DEBUG
static void
print_tuples(void) {
	struct srv_prio *pe;
	struct srv *se;

	for (pe = HEAD(prio_list);
	     pe != NULL;
	     pe = NEXT(pe, link))
	{
		fprintf(stderr, "prio=%hu\n", pe->prio);
		for (se = HEAD(pe->srv_list);
		     se != NULL;
		     se = NEXT(se, link))
		{
			fprintf(stderr, "\tweight=%hu port=%hu tname=%s\n",
				se->weight, se->port, se->tname);
		}
	}
}
#endif

static char *
target_name(const unsigned char *target) {
	char buf[NS_MAXDNAME];

	if (ns_name_ntop(target, buf, sizeof buf) == -1) {
		perror("ns_name_ntop");
		exit(EXIT_FAILURE);
	}
	return (strdup(buf));
}

static void
parse_answer_section(ns_msg *msg) {
	int rrnum, rrmax;
	ns_rr rr;
	uint16_t prio, weight, port, len;
	const unsigned char *rdata;
	char *tname;

	rrmax = ns_msg_count(*msg, ns_s_an);
	for (rrnum = 0; rrnum < rrmax; rrnum++) {
		if (ns_parserr(msg, ns_s_an, rrnum, &rr)) {
			perror("ns_parserr");
			exit(EXIT_FAILURE);
		}
		if (ns_rr_type(rr) == ns_t_srv) {
			len = ns_rr_rdlen(rr);
			rdata = ns_rr_rdata(rr);
			if (len > 3U * NS_INT16SZ) {
				NS_GET16(prio, rdata);
				NS_GET16(weight, rdata);
				NS_GET16(port, rdata);
				len -= 3U * NS_INT16SZ;
				tname = target_name(rdata);
				insert_tuple(tname, prio, weight, port);
			}
		}
	}
}

static char *
subst_cmd(struct srv *se, const char *cmd) {
	char *q, *str;
	const char *p = cmd;
	int ch;
	int n_host = 0;
	int n_port = 0;
	size_t bufsz;
	size_t len_tname;

	len_tname = strlen(se->tname);

	while ((ch = *p++) != '\0') {
		if (ch == '%' && *p == 'h')
			n_host++;
		if (ch == '%' && *p == 'p')
			n_port++;
	}

	bufsz = strlen(cmd) + 1;
	bufsz -= 2 * (n_host + n_port); /* '%h' and '%p' */
	bufsz += 5 * n_port; /* '%h' -> uint16_t */
	bufsz += (strlen(se->tname) + 1) * n_host;

	str = calloc(1, bufsz);
	assert(str != NULL);

	p = cmd;
	q = str;
	while ((ch = *p++) != '\0') {
		if (ch == '%' && *p == 'h') {
			strcpy(q, se->tname);
			q += len_tname;
			p++;
		} else if (ch == '%' && *p == 'p') {
			q += sprintf(q, "%hu", se->port);
			p++;
		} else {
			*q++ = ch;
		}
	}

	return (str);
}

static int
do_cmd(struct srv *se, int argc, char **argv) {
	char *cmd, *scmd, *p;
	int i, rc;
	size_t bufsz = 2;

	for (i = 2; i < argc; i++) {
		bufsz += 1;
		bufsz += strlen(argv[i]);
	}

	p = cmd = malloc(bufsz);
	assert(cmd != NULL);

	for (i = 2; i < argc; i++) {
		strcpy(p, argv[i]);
		p += strlen(argv[i]);
		if (i != argc - 1) {
			*p++ = ' ';
		}
	}

	scmd = subst_cmd(se, cmd);
	free(cmd);

#ifdef DEBUG
	fprintf(stderr, "scmd='%s'\n", scmd);
#endif

	rc = system(scmd);
	rc = WEXITSTATUS(rc);

	free(scmd);
	free(se->tname);
	free(se);

#ifdef DEBUG
	fprintf(stderr, "rc=%i\n", rc);
#endif
	return (rc);
}

static void
usage(void) {
	fprintf(stderr, "Usage: wrapsrv <SRVNAME> <COMMAND> [OPTION]...\n");
	fprintf(stderr, "%%h and %%p sequences will be converted to "
		"hostname and port.\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv) {
	char *qname;
	ns_msg msg;
	int len, rc = 0;
	unsigned char answer[NS_MAXMSG];
	unsigned rcode;
	struct srv *se;

	if (argc < 3)
		usage();

	INIT_LIST(prio_list);

	srandom(time(NULL));

	res_init();

	qname = argv[1];

	len = res_query(qname, ns_c_in, ns_t_srv, answer, sizeof answer);
	if (len < 0) {
		herror("res_query");
		return (EXIT_FAILURE);
	}

	if (ns_initparse(answer, len, &msg) < 0) {
		perror("ns_initparse");
		return (EXIT_FAILURE);
	}

	rcode = ns_msg_getflag(msg, ns_f_rcode);
	if (rcode != ns_r_noerror) {
		fprintf(stderr, "wrapsrv: query for %s returned rcode %u\n",
			qname, rcode);
		return (EXIT_FAILURE);
	}

	if (ns_msg_count(msg, ns_s_an) == 0) {
		fprintf(stderr, "wrapsrv: query for %s returned no answers\n",
			qname);
		return (EXIT_FAILURE);
	}

	parse_answer_section(&msg);
#ifdef DEBUG
	print_tuples();
	fprintf(stderr, "\n");
#endif

	while ((se = next_tuple()) != NULL) {
		if ((rc = do_cmd(se, argc, argv)) == 0)
			break;
#ifdef DEBUG
		fprintf(stderr, "\n");
#endif
	}

	free_tuples();

	return (rc);
}
