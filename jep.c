/*-
 * The MIT License (MIT)
 * 
 * Copyright (c) 2025 David Marker
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/wait.h>
#include <jail.h>
#include <unistd.h>

#include "jep.h"

/* name of our utility */
#define	ME	"jep"


/*
 * The single purpose of this utility is to create and connect an epair(4) to an
 * if_bridge(4). The epair(4) is created in the jail so that it will
 * automatically be cleaned up when the jail is killed.
 *
 * Utilities like `jib` create the epair(4) and *push* one into the destination
 * jail. This is the opposite: create the epair(4) in the destination jail and
 * *pull* one out.
 *
 * And since we now don't need to track the names, lets make it easy to choose
 * whatever names make sense for the user. The only rules for the epair(4) end
 * that remains in the jail is that the name is a valid interface name and not
 * yet in use inside the jail. The host epair(4) end the name must neither exist
 * in the jail nor the host. This is because it must be renamed before it is
 * pulled from the jail.
 *
 * If this utility were only used by jail(8) as part of `exec.created` in your
 * jail.conf(5) I wouldn't have any cleanup, just ensure a bad exit code and
 * jail(8) will teardown the epair (if it was even created).
 *
 * But this utility can be run from outside of jail(8) against any running jail.
 * So we do have to try and clean up on errors.
 */

#define USAGE do { \
	(void) fprintf(stderr, \
		"USAGE: " ME " [-n] <jail> <if-host> <if-bridge> <if-jail> [mac]\n" \
		"\n" \
		"-n\tDisable automatic loading of network interface drivers.\n" \
		"<jail>\ta valid jail name or ID.\n" \
		"<if-*>\tparameters must all be valid interface names.\n" \
		"[mac]\tis optional but if provided will be assigned to\n" \
		"\t<if-jail>, the epair(4) that remains in <jail>.\n" \
		"\tThis can be useful for configuring DHCP.\n\n" \
		"epair(4) nodes are created in <jail> with one end remaining in the\n" \
		"jail and one pulled out from the jail to connect to an already\n" \
		"existing if_bridge(4).\n\n" \
		"RETURNS:\n" \
		"\t0 on success and prints the MAC address of <if-jail> to stdout\n" \
		"\t!0 on failure and error(s) will be sent to stderr.\n\n" \
		"EXAMPLE (assuming jail0br and lan0br are existing if_bridge(4)):\n"\
		"\t" ME " test jail0test jail0br jail0\n"\
		"\t" ME " test lan0test lan0br lan0\n"\
	); \
	exit(EX_USAGE); \
} while(0)


/* module global, so err_cleanup_* can find everything */
static struct {
	int		 ipc;		/* our side of socketpair */
	ifctx		 ifc;		/* needed by all if_* routines */
	int		 jid;		
	const char	*clean_if;	/* interface name to destroy on err */
	/* from argv */
	const char	*jail;		/* from argv in roundabout way */
	const char	*ifhost;
	const char	*ifbridge;
	const char	*ifjail;
	const char	*mac;
} G = {
	.ipc		= -1,
	.ifc		= -1,
	.jid		= -1,
	.clean_if	= NULL,
	.jail		= NULL,
	.ifhost		= NULL,
	.ifbridge	= NULL,
	.ifjail		= NULL,
	.mac		= NULL,
};

static void
err_cleanup_child(int _)
{
	/* may or may not be far enough to need to clean epair */
	if (G.clean_if) {
		(void) if_epair_destroy(G.ifc, G.clean_if);
	}
	/* by closing without writing mac parent knows error occurred */
	(void) shutdown(G.ipc, SHUT_RDWR);
	(void) close(G.ipc);
	(void) close(G.ifc);
}

static void
err_cleanup_parent(int _)
{
	int wc, status;

	/* ask child to handle cleanup */
	(void) write(G.ipc, "errout", sizeof("errout"));

	do { /* and now wait for child */
		wc = wait(&status);
	} while (wc == -1 && errno == EINTR);

	(void) shutdown(G.ipc, SHUT_RDWR);
	(void) close(G.ipc);
	(void) close(G.ifc);
}

/* just set up the socketpair and adjust G appropriately */
static int
gfork(process child, process parent)
{
	int fd[2];

	/*
	 * set up G.ifc now for parent, one less error path that requires
	 * coordination later.
	 */
	G.ifc = if_open_ctx(); /* exits on fail */
	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd) == -1) err(
		ERREXIT, "socketpair"
	);

	switch (fork()) {
	case -1:
		err(ERREXIT, "fork");
	case 0:
		err_set_exit(err_cleanup_child);
		/* switch into jail */
		if (jail_attach(G.jid) == -1) err(
			ERREXIT, "jail_attach(%d)", G.jid
		);
		(void) close(G.ifc);
		G.ifc = if_open_ctx(); /* must reopen in jail! */
		G.ipc = fd[0];
		(void) close(fd[1]);
		return child();
	default:	
		err_set_exit(err_cleanup_parent);
		G.ipc = fd[1];
		(void) close(fd[0]);
		return parent();
	}
}

/* child is in the jail */
static int
child(void)
{
	int idx;
	const char *mac;
	char macbuf[LLNAMSIZ] = { '\0', };
	char epair[IFNAMSIZ] = { '\0' };

	if (if_epair_create(G.ifc, epair) == NULL) errx(
		ERREXIT, "unable to create epair in jail \"%s\"", G.jail
	);
	G.clean_if = epair;

	/* set or retrieve mac of epair in jail we report it later */
	if (G.mac != NULL) {
		strlcpy(macbuf, G.mac, sizeof(macbuf));
		if ((mac = if_setmac(G.ifc, epair, macbuf)) == NULL) errx(
			ERREXIT, "unable to set mac=\"%s\"", G.mac
		);
	} else {
		if ((mac = if_getmac(G.ifc, epair, macbuf)) == NULL) errx(
			ERREXIT, "unable to retrieve mac for \"%s\"", epair
		);
	}
	
	if (if_rename(G.ifc, epair, G.ifjail) < 0) errx(
		ERREXIT, "unable to rename \"%s\" -> \"%s\"", epair, G.ifjail
	);
	G.clean_if = G.ifjail; /* in case of err, epair name has changed */

	/* We know it is `epairXa` and X must be at least 1 digit. */
	for (idx = sizeof("epair"); epair[idx] != '\0'; idx++)
		; /* just advancing idx */
	epair[--idx] = 'b';

	if (if_rename(G.ifc, epair, G.ifhost) < 0) errx(
		ERREXIT, "unable to rename \"%s\" -> \"%s\"", epair, G.ifhost
	);

	/* inform our parent of the mac address */
	if (write(G.ipc, mac, LLNAMSIZ) != LLNAMSIZ) errx(
		ERREXIT, "unable to report mac to parent"
	);

	/* parent sending data is signal to cleanup */
	if (read(G.ipc, macbuf, LLNAMSIZ) != 0) {
		err_cleanup_child(0);
		return (0); /* not used by parent if they told child to cleanup */
	}

	(void) shutdown(G.ipc, SHUT_RDWR);
	(void) close(G.ipc);
	(void) close(G.ifc);
	return (0);
}

static int
parent(void)
{
	int rc, wc, status;
	char macbuf[LLNAMSIZ] = { '\0', };
	G.mac = macbuf;

	/*
	 * If child process has any failure it is just going to close its side
	 * which gives us a short read and we know it has already cleaned up.
	 * But this is one case where something bad happened on the jail side
	 * and we need its exit code to pass on.
	 */
	if ((rc = read(G.ipc, macbuf, LLNAMSIZ)) != LLNAMSIZ)
		goto out;

	if (if_vmove(G.ifc, G.ifhost, G.jid) == -1) errx(
		ERREXIT, "unable to retrieve \"%s\" from \"%s\"", G.ifhost, G.jail
	);
	if (if_addm(G.ifc, G.ifhost, G.ifbridge) == -1) errx(
		ERREXIT, "unable to addm \"%s\" to \"%s\"", G.ifhost, G.ifbridge
	);
	if (if_up(G.ifc, G.ifhost) != 0) errx(
		ERREXIT, "unable to bring \"%s\" up", G.ifhost
	);

	/* Important to shutdown G.ipc so child exits clean */
	(void) shutdown(G.ipc, SHUT_RDWR);
	(void) close(G.ipc);
	(void) close(G.ifc);

	/* XXX this may change when I write something to consume it ... */
	(void) fprintf(stdout, "{\"%s\": \"%s\"}\n", G.jail, G.mac);

out:
	do {
		wc = wait(&status);
	} while (wc == -1 && errno == EINTR);

	if (wc == -1)
		return EX_OSERR;

	assert(WIFEXITED(status));
	return WEXITSTATUS(status);
}


int
main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	/* enough args to check for '-n' */
	if (argc < 2) USAGE;

	/*
	 * Unless told not to, check for kernel modules and attempt to add them
	 * if not already present.
	 */
	if (strcmp(argv[1], "-n") == 0) {
		/* adjust arguments so we can ignore "-n" */
		argv++; argc--;
	} else {
		kld_ensure_load("if_epair");
		kld_ensure_load("if_bridge");
	}
	if (argc < 5 || argc > 6) USAGE;

	G.ifhost = argv[2];
	G.ifbridge = argv[3];
	G.ifjail = argv[4];
	G.mac = (argc == 6) ? argv[5] : NULL;

	/* need the jail id */
	if ((G.jid = jail_getid(argv[1])) == -1) errx(
		ERREXIT, "%s", jail_errmsg
	);

	/*
	 * User may have given us numeric ID so ensure we have name for later.
	 * This name is malloc()ed but not worth worrying about free()ing.
	 *
	 * XXX still not sure I want to print jail name, we have that in
	 *     jail.conf(5) anyway so any utility that uses mac probably
	 *     doesn't need this...
	 */
	if ((G.jail = jail_getname(G.jid)) == NULL) errx(
		ERREXIT, "%s", jail_errmsg
	);

	return gfork(child, parent);
}
