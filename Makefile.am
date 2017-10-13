AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS = -std=gnu11 -g -O3 -flto -fuse-linker-plugin -Isrc -Isrc/common
AM_LDFLAGS = -all-static

bin_PROGRAMS = bin/udp_client

UDP_CLIENT_SOURCE_FILES = \
src/udp_client/main.c

UDP_CLIENT_HEADER_FILES =

bin_udp_client_SOURCES = $(UDP_CLIENT_SOURCE_FILES) $(UDP_CLIENT_HEADER_FILES)
bin_udp_client_LDADD = -ljansson -ldynamic -lreactor -lts

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in docs/Makefile.in
maintainer-clean-local:; rm -rf autotools m4

CLEANFILES = {.,src}/*.{gcno,gcda,gcov}
