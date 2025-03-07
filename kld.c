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
#include <string.h>
#include <sys/linker.h>
#include <sys/module.h>

#include "jep.h"

void
kld_ensure_load(const char *search)
{
	int rc, fileid, modid;
	const char *cp;
	struct module_stat mstat;

	assert(search != NULL);

	/* scan files in kernel */
	mstat.version = sizeof(struct module_stat);
	for (fileid = kldnext(0); fileid > 0; fileid = kldnext(fileid)) {
		/* scan modules in file */
		for (modid = kldfirstmod(fileid); modid > 0;
		     modid = modfnext(modid)) {
			if (modstat(modid, &mstat) < 0)
				continue;
			/* strip bus name if present */
			if ((cp = strchr(mstat.name, '/')) != NULL) {
				cp++;
			} else {
				cp = mstat.name;
			}

			/* found, already loaded */
			if (strcmp(search, cp) == 0)
				return;
		}
	}

	/*
	 * In theory user could have a jail that does not use epair that itself
	 * attempts to use this utility for a sub-jail. It could fail then as
	 * the kernel module would not necessarily be loaded and a jail isn't
	 * going to have permissions to do so.
	 *
	 * Best to fail now with a message that hopefully clues in user.
	 */
	if ((rc = kldload(search)) != 0) err(
		ERREXIT,
		"%s: unable to load kernel module \"%s\"",
		__func__,
		search
	);
}
