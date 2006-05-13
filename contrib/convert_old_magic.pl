#!/usr/bin/perl

# this utility can be used to convert old c-icap.magic files to new format
# The old line format was:
#  POS:LABEL:TYPE NAME:GROUP:COMMENT
# The new is :
#  POS:LABEL:TYPE NAME:COMMENT:GROUP1[:GROUP2[:GROUP3[...]]]


my $in  = shift;

if(!open(FILE,$in)){
    die "The file $in does not exists!\n\n";
}

while(<FILE>){
    $line=$_;


    if( $line =~ /^\#.*/  || $line =~ /^[\s*]$/){
	print $line;
    }
    else{
	chomp $line;
	split ':',$line;

	print $_[0].":".$_[1].":".$_[2].":".$_[4].":".$_[3]."\n";

    }

}

close(FILE);
