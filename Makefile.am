AUTOMAKE_OPTIONS = gnu

AM_CFLAGS = -Wmissing-prototypes
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libinklevel.la
dist_doc_DATA = NEWS README AUTHORS COPYING ChangeLog
LIBTOOL_DEPS = @LIBTOOL_DEPS@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck


libinklevel_la_SOURCES = libinklevel.c canon.c epson_new.c hp_new.c bjnp-io.c \
                         bjnp-debug.c d4lib.c linux.c opensolaris.c util.c \
			 bjnp.h	config.h epson_new.h inklevel.h util.h canon.h \
			 d4lib.h hp_new.h platform_specific.h internal.h \
			 libinklevel.spec libinklevel.spec.in \
			 norpm rpmbuild

include_HEADERS = inklevel.h                         

libinklevel_la_LDFLAGS = -version-info @ABI_VERSION@

@rpmtarget@
