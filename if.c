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
#include <ifaddrs.h>
#include <net/if_bridgevar.h>
#include <net/if_dl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "jep.h"

/*
 * This file is just implementing a tiny subset of ifconfig(8), just enough
 * to do everything we need without resorting to system(3).
 * Most of these functions are just assert, runtime check, then ioctl.
 */


ifctx
if_open_ctx()
{
	int sd = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (sd == -1) err(
		ERREXIT, "%s: socket(AF_INET, SOCK_DGRAM, 0)", __func__
	);
	return sd;
}

char*
if_epair_create(ifctx ctx, char result[IFNAMSIZ])
{
	struct ifreq ifr = { .ifr_name = "epair" };

	assert(ctx >= 0);
	assert(result != NULL);
	
	if (ioctl(ctx, SIOCIFCREATE2, &ifr) != 0) {
		warn("%s: ioctl(...SIOCIFCREATE2...)", __func__);
		return (NULL);
	}
	strlcpy(result, ifr.ifr_name, IFNAMSIZ);
	return (result);
}

int
if_epair_destroy(ifctx ctx, const char *ifname)
{
	int rc = 0;
	struct ifreq ifr = {0};

	assert(ctx >= 0);
	assert(ifname != NULL);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (-1);
	}

	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if ((rc = ioctl(ctx, SIOCIFDESTROY, &ifr)) != 0) warn(
		"%s: ioctl(...SIOCIFDESTROY...)", __func__
	);
	return (rc);
}

/* This is a PULL operation! It uses SIOCSIFRVNET not SIOCSIFVNET */
int
if_vmove(ifctx ctx, const char *ifname, int jid)
{
	int rc = 0;
	struct ifreq ifr = {
		.ifr_jid = jid
	};

	assert(ifname != NULL && jid >= 0);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (-1);
	}

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if ((rc = ioctl(ctx, SIOCSIFRVNET, &ifr)) != 0) warn(
		"%s: ioctl(...SIOCSIFRVNET...)", __func__
	);
	return (rc);
}

int
if_rename(ifctx ctx, const char *ifname, const char *name)
{
	int rc = 0;
	struct ifreq ifr = {
		.ifr_data = (caddr_t)name
	};

	assert(ifname != NULL && name != NULL);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (strlen(name) >= IFNAMSIZ) warnc(
		EINVAL, "name=\"%s\" too long", name
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (-1);
	}

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if ((rc = ioctl(ctx, SIOCSIFNAME, &ifr)) != 0) warn(
		"%s: ioctl(...SIOCSIFNAME...)", __func__
	);
	return (rc);
}

/*
 * If there is a failure, this function warns and returns NULL.
 * If the MAC for `ifname` is not found `mac` is set to the empty string.
 * But found or not, without error `mac` is returned.
 */
char *
if_getmac(ifctx ctx, const char *ifname, char mac[LLNAMSIZ])
{
	int rc = 0;
	struct ifaddrs *ifap, *iter;
	struct sockaddr_dl *sdp;
	unsigned char *bmac;

	assert(ifname != NULL && mac != NULL);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (NULL);
	}

	if (getifaddrs(&ifap) == -1) {
		warn("%s: getifaddrs(%p)", __func__, &ifap);
		return (NULL);
	}
	mac[0] = '\0'; /* in case not found */
	for (iter = ifap; iter; iter = iter->ifa_next) {
		/* keep going until we find a mac for ifname */
		if ((iter->ifa_addr->sa_family != AF_LINK) ||
		    (strcmp(iter->ifa_name, ifname) != 0))
			continue;

		sdp = (struct sockaddr_dl*)iter->ifa_addr;
		bmac = (unsigned char *)LLADDR(sdp);
		rc = sprintf(
			mac,
			"%02x:%02x:%02x:%02x:%02x:%02x",
			bmac[0], bmac[1], bmac[2], bmac[3], bmac[4], bmac[5]
		);
		assert(rc == LLNAMLEN);
		break;
	}
	freeifaddrs(ifap);

	return (mac);
}

/*
 * mac is normalized to be the same output (lower-case hex separated by ':')
 * that ifconfig(8) returns same as if_getmac returns.
 * 
 * Planning to pipe to another utility...
 *
 * Originally I used link_addr(3) for the mac but really its just easier
 * to scanf(3) and check the right number of conversions occured.
 */
const char *
if_setmac(ifctx ctx, const char *ifname, char mac[LLNAMSIZ])
{
	int		rc = 0;
	struct ifreq	ifr = {
		.ifr_addr = {
			.sa_family = AF_LINK,
			.sa_len = ETHER_ADDR_LEN, /* binary mac size */
		}
	};
	unsigned char	*bmac = (unsigned char *)(&ifr.ifr_addr.sa_data);

#	define		BMAC_SCAN_ARGS \
			mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", \
			&bmac[0], &bmac[1], &bmac[2], \
			&bmac[3], &bmac[4], &bmac[5]
#	define		BMAC_PRINT_ARGS \
			mac, "%02x:%02x:%02x:%02x:%02x:%02x", \
			bmac[0], bmac[1], bmac[2], bmac[3], bmac[4], bmac[5]

	assert(ifname != NULL && mac != NULL);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (strlen(mac) >= LLNAMSIZ) warnc(
		EINVAL, "mac=\"%s\" too long", mac
	), rc++;
	if (sscanf(BMAC_SCAN_ARGS) != 6) warnc(
		EINVAL, "mac=\"%s\" invalid", mac
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (NULL);
	}

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	rc = ioctl(ctx, SIOCSIFLLADDR, &ifr);
	if (rc != 0) {
		warn("%s: ioctl(...SIOCSIFLLADDR...)", __func__);
		return (NULL);
	}
	rc = sprintf(BMAC_PRINT_ARGS); /* normalize */
	assert(rc == LLNAMLEN);
#	undef BMAC_SCAN_ARGS
#	undef BMAC_PRINT_ARGS

	return (mac);
}

int
if_addm(ifctx ctx, const char *ifname, const char *brname)
{
	int rc = 0;
	struct ifbreq req = {0};
	struct ifdrv ifd = {
		.ifd_cmd = BRDGADD,
		.ifd_len = sizeof(req),
		.ifd_data = &req
	};

	assert(ifname != NULL && brname != NULL);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (strlen(brname) >= IFNAMSIZ) warnc(
		EINVAL, "brname=\"%s\" too long", brname
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (-1);
	}

	strlcpy(req.ifbr_ifsname, ifname, sizeof(req.ifbr_ifsname));
	strlcpy(ifd.ifd_name, brname, sizeof(ifd.ifd_name));
	if ((rc = ioctl(ctx, SIOCSDRVSPEC, &ifd)) != 0) warn(
		"%s: ioctl(...SIOCSDRVSPEC...)", __func__
	);
	return (rc);
}

static int
getifflags(ifctx ctx, const char *ifname, uint32_t *flags)
{
	int rc = 0;
	struct ifreq ifr = {0};

	assert(ctx >= 0);
	assert(ifname != NULL);
	assert(strlen(ifname) < IFNAMSIZ);

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if ((rc = ioctl(ctx, SIOCGIFFLAGS, &ifr)) != 0) {
		warn("%s: ioctl(...SIOCGIFFLAGS...)", __func__);
		return (rc);
	}
	*flags = ((ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16));
	return (rc);
}

static int
setifflags(ifctx ctx, const char *ifname, uint32_t flags)
{
	int rc = 0;
	struct ifreq ifr = {
		.ifr_flags = flags & 0xffff,
		.ifr_flagshigh = flags >> 16
	};

	assert(ctx >= 0);
	assert(ifname != NULL);
	assert(strlen(ifname) < IFNAMSIZ);

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if ((rc = ioctl(ctx, SIOCSIFFLAGS, &ifr)) != 0) warn(
		"%s: ioctl(...SIOCSIFFLAGS...)", __func__
	);
	return (rc);
}

int
if_up(ifctx ctx, const char *ifname)
{
	int rc = 0;
	uint32_t flags;

	assert(ctx >= 0);
	assert(ifname != NULL);
	if (strlen(ifname) >= IFNAMSIZ) warnc(
		EINVAL, "ifname=\"%s\" too long", ifname
	), rc++;
	if (rc) {
		errno = EINVAL;
		return (-1);
	}

	if ((rc = getifflags(ctx, ifname, &flags)) != 0)
		return (rc);
	return setifflags(ctx, ifname, flags | IFF_UP);
}
