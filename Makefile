#-
# The MIT License (MIT)
# 
# Copyright (c) 2025 David Marker
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 


CC=/usr/bin/clang
INSTALL=/usr/bin/install
RM=/bin/rm
TAR=/usr/bin/tar

CFLAGS=-std=c11 -g -Wall -Werror

all: jep

OBJ:=	jep.o		\
	kld.o		\
	if.o

jep.o : jep.c jep.h
kld.o : kld.c jep.h
if.o : if.c jep.h

jep: $(OBJ)
	$(CC) -o $@ $(OBJ) -ljail

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

install: jep
	$(INSTALL) -o root -g wheel -m 755 -d /usr/local/bin
	$(INSTALL) -o root -g wheel jep /usr/local/bin


ARCHIVE=LICENSE		\
	README.md	\
	jep.sh		\
	Makefile	\
	jep.h		\
	jep.c		\
	kld.c		\
	if.c

jep.tar: $(ARCHIVE)
	$(RM) -f $@
	$(TAR) cfv $@ -C .. $(ARCHIVE:C/^/jep\//g)

.PHONY:
tar: jep.tar

.PHONY:
clean:
	$(RM) -f *.o *.tar

.PHONY:
clobber: clean
	$(RM) -f jep
