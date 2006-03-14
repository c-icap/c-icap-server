!include <win32.mak>
!include "c_icap.mak"

all: c_icap.Dll

LIBICAPOBJS =  header.obj body.obj  net_io.obj  simple_api.obj base64.obj  cfg_lib.obj filetype.obj debug.obj
LIBUTIL=os\win32\shared_mem.obj os\win32\proc_mutex.obj os\win32\net_io.obj os\win32\threads.obj os\win32\utilfunc.obj

.c.obj:
	$(cc) -Iinclude  $(cdebug) $(cflags) $(cvarsdll) $(CI_DEFS) -I. -DCI_BUILD_LIB  -DUNICODE $*.c /Fo$*.obj
#	$(cc) -Iinclude  $(cdebug) $(cflags) $(cvarsmt) -I. -DCI_BUILD_LIB  -DUNICODE $*.c

c_icap.Dll: $(LIBICAPOBJS) $(LIBUTIL) $(DLL_ENTRY)
	$(link) $(ldebug) $(dlllflags) -def:c_icap.def -out:$*.Dll $**  Ws2_32.lib kernel32.lib $(DLL_ENTRY)  $(EXTRA_LIBS)



