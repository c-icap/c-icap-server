
!include <win32.mak>


all: c_icap.Dll

LIBICAPOBJS =  header.obj body.obj base64.obj   debug.obj

.c.obj:
	$(cc) -Iinclude  $(cdebug) $(cflags) $(cvarsdll) -I. -DCI_BUILD_LIB  -DUNICODE $*.c
#	$(cc) -Iinclude  $(cdebug) $(cflags) $(cvarsmt) -I. -DCI_BUILD_LIB  -DUNICODE $*.c

c_icap.Dll: $(LIBICAPOBJS)
	$(link) $(ldebug) $(dlllflags)  -def:c_icap.def -out:$*.Dll $** $(DLL_ENTRY)  $(EXTRA_LIBS)

