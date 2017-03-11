# prefer astyle for formating:
find . -name "*.[ch]" -exec astyle --mode=c -s4 --convert-tabs --keep-one-line-blocks --style=kr --lineend=linux --pad-header \{\} \;

# mostly k&r style with some exceptions
#find . -name "*.[ch]" -exec indent -nbad -bap -nbc -br -brs -c33 -cd33 -cdb -ce -ci4 -cli0 -cp33 -d0 -di1 -nfc1 -nfca -i4 -ip0 -l180 -ncs -npcs -nprs -npsl -saf -sai -saw -nsc -nsob -nss -nut \{\} \;

# Old formating command:
# find . -name "*.h" -o -name "*.c" -exec indent -kr -i4 -ts8 -di1 -l80 -nce -nut -nfca \{\} \;


