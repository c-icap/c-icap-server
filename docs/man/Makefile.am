
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
LOGDIR=$(localstatedir)/log/
SOCKDIR=/var/run/c-icap

do_subst=sed -e 's%[@]SYSCONFDIR[@]%$(CONFIGDIR)%g' \
	     -e 's%[@]PACKAGE_STRING[@]%$(PACKAGE_STRING)%g'


manpages = c-icap.8 c-icap-client.8 c-icap-config.8 c-icap-libicapapi-config.8 \
           c-icap-stretch.8 c-icap-mkbdb.8
manpages_src = $(manpages:.8=.8.in)

CLEANFILES = $(manpages)

SUFFIXES =  .8.in .8

.8.in.8:
	$(do_subst) < $< > $@

man_MANS = $(manpages)

EXTRA_DIST = $(manpages_src)
