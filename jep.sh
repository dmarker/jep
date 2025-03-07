#!/bin/sh
#-
# Copyright (c) 2025 David Marker
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
############################################################ IDENT(1)
#
# $Title: epair(4) management script for vnet jails $
#
############################################################ INFORMATION
#
# Use this tool manually or with jail.conf(5) to add a connected epair that
# automatically removes itself when the jail is shutdown.
# Unlike `jib` this creates the epair inside the jail. That is why it auto-
# matically is removed with the jail.
#
# This is my *prototype* to verify that creating epair(4) inside the jail
# means I don't have to clean it up. It does not cleanup if you use it
# outside of `exec.created` in jail.conf(5). It is relying upon jail(8) to
# destroy the jail on any failure of `exec.created` which will automatically
# remove the epair(4) no matter how far along we did or did not get. But it
# is so nice and clean I had to include it to demonstrate what we are going
# for.
#
# You can't `kldload from the jail` so that has to be done outside it.
#
# Also this will not help you create any if_bridge(4) to use, it is assumed
# that is done elsewhere like rc.conf(5).
#
#
# In jail.conf(5) format:
#
# ### BEGIN EXCERPT ###
#
# xxx {
# 	host.hostname = "xxx.yyy";
# 	path = "/vm/xxx";
# 
# 	#
# 	# NB: Below line required
# 	#
# 	vnet;
# 
# 	exec.clean;
# 	exec.system_user = "root";
# 	exec.jail_user = "root";
# 
# 	#
# 	# NB: Below 2-lines or similar required (any number of interface)
# 	# NB: You can assign a MAC address here too for DHCP.
# 	# NB: I name the epair in the host system using the jail name + bridge number
# 	#
# 	exec.created += "jep $name br0${name} br0 lan0";
# 	exec.created += "jei $name br1${name} br1 jail0 02:30:c2:1c:ac:0a";
# 
# 	# Standard recipe
# 	exec.start += "/bin/sh /etc/rc";
# 	exec.stop = "/bin/sh /etc/rc.shutdown jail";
#
# 	exec.consolelog = "/var/log/jail_xxx_console.log";
# 	mount.devfs;
#
# 	# Optional (default off)
# 	#allow.mount;
# 	#allow.set_hostname = 1;
# 	#allow.sysvipc = 1;
# 	#devfs_ruleset = "11"; # rule to unhide bpf for DHCP
# }
#
# ### END EXCERPT ###
#
# ASIDE: dhclient(8) inside a vnet jail...
#
# To allow dhclient(8) to work inside a vnet jail, make sure the following
# appears in /etc/devfs.rules (which should be created if it doesn't exist):
#
# 	[devfsrules_jail=11]
# 	add include $devfsrules_hide_all
# 	add include $devfsrules_unhide_basic
# 	add include $devfsrules_unhide_login
# 	add path 'bpf*' unhide
#
# And set ether devfs.ruleset="11" (jail.conf(5)) or
# jail_{name}_devfs_ruleset="11" (rc.conf(5)).
#
############################################################ GLOBALS

pgm="${0##*/}" # Program basename

#
# Global exit status
#
SUCCESS=0
FAILURE=1

############################################################ FUNCTIONS

usage()
{
	exec >&2
	echo "Usage: $pgm <jail> <host-ifname> <if_bridge> <jail-ifname> [mac]"
	echo ""
	echo "<jail> must be an existing jail name or ID."
	echo "<jail-ifname> must be a valid name for an interface and be"
	echo "available (not taken) in the jail."
	echo "<if_bridge> must already exist on the host system, it will"
	echo "not be created for you."
	echo "<host-ifname> must be a valid name for an interface and be"
	echo "available (not taken) in jail and system."
	echo ""
	echo "Example:"
	echo "	$pgm myjail lan0 br0 myjail0"
	echo ""

	exit $FAILURE
}

# The nice thing about creating inside the jail is that at any point if we fail
# we have no cleanup. The jail will be torn down and that will automatically
# clean the epair(4) up. Of course this only works when jail(8) is calling `jep`
# from `exec.created` in jail.conf(5).
#
# A serious limitation, but for the main use case... script > C
xerr()
{
	exec >&2
	echo $1
	exit $FAILURE
}

mustberoot_to_continue()
{
	if [ "$( id -u )" -ne 0 ]; then
		echo "Must run as root!" >&2
		exit $FAILURE
	fi
}


# check args
jail=$1
host_ifname=$2
bridge_name=$3
jail_ifname=$4
mac=$5

case $# in
	4)	;;
	5)	;;
	*)	usage
		;;
esac

mustberoot_to_continue

jail_end=$(ifconfig -j $jail epair create)
if [ $? -ne 0 ]; then
	exec >&2
	echo "ERROR: unable to create epair inside jail"
	echo "       jail may not exist or if_epair may not be loaded"
	echo ""
	echo "SUGGESTION: kldload if_epair"
	exit $FAILURE
fi
if [ ! -z $mac ]; then
	ifconfig -j $jail $jail_end ether $mac ||
		xerr "failed to change MAC of \"${jail_end}\" to \"${mac}\""
fi
host_end="${jail_end%a}b"	# strip 'a' replace with 'b'
ifconfig -j $jail $jail_end name $jail_ifname ||
	xerr "failed to rename \"${jail_end}\" to \"${jail_ifname}\""
ifconfig -j $jail $host_end name $host_ifname ||
	xerr "failed to rename \"${host_end}\" to \"${host_ifname}\""

ifconfig $host_ifname -vnet $jail ||
	xerr "failed to move \"${host_ifname}\" out of \"${jail}\""
ifconfig $bridge_name addm $host_ifname ||
	xerr "failed to connect \"${host_ifname}\" to \"${bridge_name}\""
ifconfig $host_ifname up ||
	xerr "failed to bring up \"${host_ifname}\""

exit $SUCCESS
