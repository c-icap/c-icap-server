
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

CLEANFILES = $(manpages)


#c-icap.8: c-icap.8.in
#	$(do_subst) < c-icap.8.in > $@

$(manpages): %.8: %.8.in 
	$(do_subst) < $< > $@

man_MANS = $(manpages)

EXTRA_DIST = c-icap.8.in c-icap-client.8.in c-icap-config.8.in \
             c-icap-libicapapi-config.8.in c-icap-stretch.8.in
