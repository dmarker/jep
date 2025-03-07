<!-- put reference links at top
    concepts:
      [FreeBSD][10]
      [netgraph(4)][11]
      [jails][12]
      [vnet(9)][13]
      [blog post][14]
      [Neighbor Discovery Protocol][15]
      [forum thread][16]
    commands
      [jexec(8)][20]
      [jail(8)][21]
      [rtsold(8)][22]
      [rtadvd(8)][23]
      [jep.sh][24]
    ports
      [net/dhcpcd][28]
    config
      [rc.conf(5)][30]
      [sysctl.conf(5)][31]
      [jail.conf(5)][32]
      [hosts(5)][33]
      [loader.conf(5)][34]
      [rtadvd.conf(5)][35]
    netgraph nodes:
      [ng_bridge(4)][40]
      [ng_eiface(4)][41]
    network:
      [if_bridge(4)][50]
      [epair(4)][51]
      [netlink(4)][52]
    code:
      [D49158][60]
      [2669fb7][61]

  -->
[10]: https://www.freebsd.org
[11]: https://man.freebsd.org/cgi/man.cgi?query=netgraph&sektion=4&manpath=FreeBSD+14.2-RELEASE+and+Ports
[12]: https://docs.freebsd.org/en/books/handbook/jails/
[13]: https://man.freebsd.org/cgi/man.cgi?query=vnet&manpath=FreeBSD+14.2-RELEASE+and+Ports
[14]: https://dan.langille.org/2023/08/14/changing-how-i-use-ip-address-with-freebsds-vnet-so-ipv6-works/
[15]: https://en.wikipedia.org/wiki/Neighbor_Discovery_Protocol
[16]: https://forums.freebsd.org/threads/ipv6-rtprefix-not-adding-routes-for-freebsd.95483/

[20]: https://man.freebsd.org/cgi/man.cgi?query=jexec&manpath=FreeBSD+14.2-RELEASE+and+Ports
[21]: https://man.freebsd.org/cgi/man.cgi?query=jail&manpath=FreeBSD+14.2-RELEASE+and+Ports
[22]: https://man.freebsd.org/cgi/man.cgi?query=rtsold&manpath=FreeBSD+14.2-RELEASE+and+Ports
[23]: https://man.freebsd.org/cgi/man.cgi?query=rtadvd&manpath=FreeBSD+14.2-RELEASE+and+Ports
[24]: https://raw.githubusercontent.com/dmarker/jep/refs/heads/main/jep.sh

[28]: https://www.freshports.org/net/dhcpcd

[30]: https://man.freebsd.org/cgi/man.cgi?query=rc.conf&manpath=FreeBSD+14.2-RELEASE+and+Ports
[31]: https://man.freebsd.org/cgi/man.cgi?query=sysctl.conf&manpath=FreeBSD+14.2-RELEASE+and+Ports
[32]: https://man.freebsd.org/cgi/man.cgi?query=jail.conf&manpath=FreeBSD+14.2-RELEASE+and+Ports
[33]: https://man.freebsd.org/cgi/man.cgi?query=hosts&manpath=FreeBSD+14.2-RELEASE+and+Ports
[34]: https://man.freebsd.org/cgi/man.cgi?query=loader.conf&manpath=FreeBSD+14.2-RELEASE+and+Ports
[35]: https://man.freebsd.org/cgi/man.cgi?query=rtadvd.conf&manpath=FreeBSD+14.2-RELEASE+and+Ports

[40]: https://man.freebsd.org/cgi/man.cgi?query=ng_bridge&manpath=FreeBSD+14.2-RELEASE+and+Ports
[41]: https://man.freebsd.org/cgi/man.cgi?query=ng_eiface&sektion=4&manpath=FreeBSD+14.2-RELEASE+and+Ports

[50]: https://man.freebsd.org/cgi/man.cgi?query=if_bridge&sektion=4&manpath=FreeBSD+14.2-RELEASE+and+Ports
[51]: https://man.freebsd.org/cgi/man.cgi?query=epair&sektion=4&manpath=FreeBSD+14.2-RELEASE+and+Ports
[52]: https://man.freebsd.org/cgi/man.cgi?query=netlink&manpath=FreeBSD+14.2-RELEASE+and+Ports

[60]: https://reviews.freebsd.org/D49158
[61]: https://github.com/NetworkConfiguration/dhcpcd/commit/2669fb715fadfd3cac931aec787c16c0d87be9a2

# JEP (jailed ethernet pairs)

## What?

This is a single utility for [FreeBSD][10] to simplify creating [epair(4)][51]
pairs, attaching one to an [if_bridge(4)][50] and gifting one to a [jail][12].

## Why?

But wait, doesn't `/usr/share/examples/jails/jib` do that? Yes of course, but
differently. Let's first understand the epair lifecycle for `jib` to contrast
what `jep` does.

`jib` relies upon 3 jail parameters in [jail.conf(5)][32]:
* `exec.prestart`
* `vnet.interface`
* `exec.poststop`

That is not the order you usually see in the config file, but its the order
things happen in. First `jib` must create the `epair` and rename them to
something you have _already_ placed in `vnet.interface`. Then `jail` will
create the the jail but before running any other process it will _push_
the interfaces listed in `vnet.interface` into the jail.

The interface that was pushed into the jail automatically comes back out since
the epair end in the jail knows its "homevnet" is not the `vnet` of the `jail`
and it just returns to that when the `vnet` of the `jail` is torn down by the
kernel. Which is why `exec.poststop` must then go destroy the epair.

That is all fine. Many people use this. It may be all you want/need. But this
has a problem which is why I created `jep`. The naming of epair nodes happens
outside the newly created jail. This means no name can collide. To solve, you
use a naming convention and the interface a jail gets is named something like
`e0b_xxx` where `xxx` is often the jails name (since it is an available
variable in `jail.conf`).

`jep` users should also follow a naming convention, but for the names that
connect to the bridge _outside_ the jail. The name of the interfaces gifted
to jails can all be identical. The restriction is that you can't name two
interfaces the same only if they reside in the _same_ [vnet(9)][13]. With
`jep` it is possible (and I use it for this) to give every jail an interface
named `jail0`.

So this works because `jep` uses `exec.created` to create the `epair` _inside_
the jail. Both interfaces are cloned into existence in the jail `vnet`. This
means that we don't even have to clean up on jail shutdown. Remember that,
"homevnet" concept? Well because `jep` created the epair in the jail, when its
`vnet` is torn down it automatically removes the epair `jep` made beacuse
there is nowhere to return them.

To make this work, `jep` perform these steps, in this order:
* create epair inside jail
* optionally change mac of epair that will remain in jail
* rename both ends inside jail
* pull one end out of jail
* connect the end pulled out to an if_bridge
* bring that interface up

The interface left in the jail can now be configured with [rc.conf(5)][30]
however you like. Important to me is that all jails have well known names
that I configure the same.

# Prototyping

Provided you don't want to use `jep` anywhere but from inside [jail.conf(5)][32]
it is a pretty simple script. Mostly because we rely on the fact that the
[jail(8)][21] utility destroys the jail if any command in `exec.created` does
not exit without error. So just exit with an error on a problem and any
epairs created will go away with the jail! Very convenient and that prototype
is available as [jep.sh][24] in this repo.

But I often have jails that get their "jail network" (the "jail0" interface) and
no other. Yet sometimes (like for an upgrade) I want to give them a "lan0"
connected to the outside world. For that I wanted `jep` to run reliably from
outside the [jail(8)][21] utility. That means it must clean up on failure and
I'm just not that great at shell scripting. Which is why `jep` is written in C.

As a final note the C code always prints the MAC address of the [epair(4)][51]
that was created and remained in the jail. This is because I have plans to use
the MAC to add a newly created jail to DNS (possibly DHCP but mostly I switched
to IPv6) or maybe just add/update an entry in [hosts(5)][33]. Problem for another
day...

## Configuration

This is specific to [jail.conf(5)][32] and the [jail(8)][21] utility.

You need to configure your [if_bridge(4)][50]s before bringing up jails. `jep`
offers no assistence with these. But here is a skeleton of what you may want
in [rc.conf(5)][30]:
```
cloned_interfaces="bridge0 bridge1"
ifconfig_bridge0_name="lan0"
ifconfig_bridge1_name="jail0"
ifconfig_re0_name="lan0host"
ifconfig_lan0="addm lan0host up"
ifconfig_lan0host="-lro -tso4 -tso6 -rxcsum6 -txcsum6 -rxcsum -txcsum up"

# configure lan0 and jail0 however you usually do
```

That created two bridges (`lan0` and `jail0`), renamed `re0` to `lan0host` and
added it to `lan0`. You configure `lan0` and `jail0` as interfaces for the
system. I know that seems a little odd but you must do it this way if you
want IPv6 to work. Obviously you may have something besides `re0` to bridge
with.

You should probably add these to your [loader.conf(5)][34]:
```
if_bridge_load="YES"
if_epair_load="YES"
```
The reason being that we also have a MIB to adjust but it requires the module
to already be loaded. So next you need to add to your [sysctl.conf(5)][31]:
```
net.link.bridge.inherit_mac=1
net.link.bridge.pfil_onlyip=0
```
The first is needed for IPv6 the second for IPv4 (well DHCP specifically).

Now you are ready to use this in [jail.conf(5)][32]
```
# ... your jail prefs

# convenience variables for networks (can still add MAC to end if desired).
# adjust to where you installed `jep`.
# "$name" just expands to the jail name.
$netlan="/usr/local/bin/jep $name lan0$name lan0 lan0";
$netjail="/usr/local/bin/jep $name jail0$name jail0 jail0";

dev {
  vnet;
  exec.created = "$netlan 00:15:5d:01:11:31";
  exec.created += "$netjail";
  exec.start = "/bin/sh /etc/rc";
  exec.stop = "/bin/sh /etc/rc.shutdown jail";
}
```

## Standalone

`jep` given no arguments will give you its usage:
```
USAGE: jep [-n] <jail> <if-host> <if-bridge> <if-jail> [mac]

-n      Disable automatic loading of network interface drivers.
<jail>  a valid jail name or ID.
<if-*>  parameters must all be valid interface names.
[mac]   is optional but if provided will be assigned to
        <if-jail>, the epair(4) that remains in <jail>.
        This can be useful for configuring DHCP.

epair(4) nodes are created in <jail> with one end remaining in the
jail and one pulled out from the jail to connect to an already
existing if_bridge(4).

RETURNS:
        0 on success and prints the MAC address of <if-jail> to stdout
        !0 on failure and error(s) will be sent to stderr.

EXAMPLE (assuming jail0br and lan0br are existing if_bridge(4)):
        jep test jail0test jail0br jail0
        jep test lan0test lan0br lan0
```

If I need to set up a private network for some number of jails:
```
# br=$(ifconfig bridge create)
# ifconfig $br name tmp0
# for j in dev bld db; do
> /usr/local/bin/jep $j tmp0$j tmp0 tmp0
> done
```

## Netgraph

`jib` has already been mentioned to contrast `jep`. But before using
[epair(4)][51] with [vnet(9)][13] I was using a [netgraph(4)][11]
[ng_eiface(4)][41] connected to an [ng_bridge(4)][40]. I had even gone so far as
to try and make something like this work and submitted [D49158][60], which is
when I found out [epair(4)][51] is the only _supported_ way to have two `vnets`
communicate (probably why they come created as a pair!).

I think it has been a worthwhile switch and don't regret it. If you still prefer
`netgraph` for this, be warned its on life support!

I recommend reading this [blog post][14] which definitely helped me switch.
But things seem to have changed since even that was written. I had to add
`net.link.bridge.pfil_onlyip=0` to [sysctl.conf(5)][31] so that DHCP (for IPv4)
worked. In the end there is no escaping reading the man pages.

## Issues

You can get yourself in trouble with interface names. Remember you can't create two
interfaces with the same name:

```
# ep=$(ifconfig epair create)
# ifconfig $ep name fu0
# ifconfig ${ep%a}b name fu0
ifconfig: ioctl SIOCSIFNAME (set name): File exists
```

The kernel won't allow it (which is good). But you can pull an interface out that
shares an existing name! This is bad. It means you can have multiple interfaces
sharing a name and that will cause a problem for you. Let's prove it:
```
# ep=$(ifconfig -j dev epair create)
# ifconfig -j dev $ep name fu0
# ifconifg fu0 -vnet dev
# ifconfig -j dev ${ep%a}b name fu0
```

You may think this is good. Both are named `fu0` so far not an issue but lets pull
the second one out:
```
# ifconfig fu0 -vnet dev
# ifconfig
...
fu0: flags=1008842<BROADCAST,RUNNING,SIMPLEX,MULTICAST,LOWER_UP> metric 0 mtu 1500
        options=8<VLAN_MTU>
        ether 02:59:9b:e2:cb:0a
        groups: epair
        media: Ethernet 10Gbase-T (10Gbase-T <full-duplex>)
        status: active
        nd6 options=29<PERFORMNUD,IFDISABLED,AUTO_LINKLOCAL>
fu0: flags=1008842<BROADCAST,RUNNING,SIMPLEX,MULTICAST,LOWER_UP> metric 0 mtu 1500
        options=8<VLAN_MTU>
        ether 02:59:9b:e2:cb:0b
        groups: epair
        media: Ethernet 10Gbase-T (10Gbase-T <full-duplex>)
        status: active
        nd6 options=29<PERFORMNUD,IFDISABLED,AUTO_LINKLOCAL>
```

Oops. Probably SIOCSIFRVNET (and SIOCSIFVNET) should check that the interface
name doesn't already exist in the vnet where its going.

In this case things went so bad the interfaces weren't removed by jail destruction
and a reboot was required!

## dhcpcd

[net/dhcpcd][28] is my prefered way of configuring interfaces with [FreeBSD][10].
It can do static and dynamic for both IPv4 and IPv6.

This is where the payoff of giving jails expected names shines, because
now you can share the same config file for all jails. Even if not every jail gets
a "lan0" interface at all times. This is because `dhcpcd` won't complain if an
interface in its config is missing. Even better, it will be notified if later the
interface arrives via [netlink(4)][52] and if its already in the config it will
configure it. That is exactly the behaviour I am after. Now I can add "lan0" to
jails I don't want near the internet to do updates then destroy "lan0".

Of course Version 10.2.2 has commit [2669fb7][61] which would allow you to use an
interface pattern like "jail0*" if you are worried about giving every jail a "jail0".
I had previously used a similar patch and had nothing to do with this commit. I
still like the same name so `ifconifg lan0` in any jail shows me internet
connectivity.

If you are using IPv6 seriously then [net/dhcpcd][28] is essential anyway. While
[rtadvd(8)][23] can send additional routes nothing in stock [FreeBSD][10] will
receive them. Neither the kernel nor [rtsold(8)][22] fully implements
[Neighbor Discovery Protocol][15], in particular they don't handle `rtprefix` option
if set in [rtadvd.conf(5)][35] for [rtadvd(8)][23]. For more you can read this
[forum thread][16] where I asked and eventually stumble into [net/dhcpcd][28].
