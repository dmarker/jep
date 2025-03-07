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

#ifndef _DMARKER_FREEDAVE_NET_JEP_H_
#define _DMARKER_FREEDAVE_NET_JEP_H_

#include <errno.h>
#include <net/if.h>
#include <sysexits.h>

#define	STRFY2(x)	#x
#define	STRFY(x)	STRFY2(x)

#define	LLNAMSIZ	18
#define	LLNAMLEN	(LLNAMSIZ - 1)

#define ERREXIT ((errno == EPERM) ? EX_NOPERM : EX_OSERR)


typedef int (*process)(void);

/* module loading: kld.c */
void	kld_ensure_load(const char *);

/* struct ifnet functions: if.c */
typedef int ifctx;

ifctx		 if_open_ctx();

char		*if_epair_create(ifctx, char[IFNAMSIZ]);
int		 if_epair_destroy(ifctx, const char *);
int		 if_rename(ifctx, const char *, const char *);
int		 if_vmove(ifctx, const char *, int);
int		 if_addm(ifctx, const char *, const char *);
const char	*if_setmac(ifctx, const char *, char[LLNAMSIZ]);
char		*if_getmac(ifctx, const char *, char[LLNAMSIZ]);
int		 if_up(ifctx, const char *);

#endif /* _DMARKER_FREEDAVE_NET_JEP_H_ */
