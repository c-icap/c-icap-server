#!/usr/bin/perl

$in=$ARGV[0];
$out=$ARGV[1];


if(!open(IN,"<$in")) {
    print "Can't open input file: $in\n";
    exit -1;
}


#if(!open(OUT,"<$out")){
#    print "Can't open input file: $out\n";
#    exit -1;
#}
my @TAGS;
my $CurrentTag = "";

$CORE = "c-icap's core";
my $HAS_CORE = 0;

my @MODS;
$MODS[0]->{"Name"} = $CORE;
my $CurrentModule = $CORE;
my $CurrentModuleTxt = "";
my $MODINDX = 0;

my $line = <IN>;
my $INDX = -1;

do {
 START_LOOP:
  if ( $line =~ /#\s*TAG:\s(.*)/ ) {
    my $t=$1;
    #        print "New Tag Found: ".$t."\n";
    $CurrentTag= $t;
    $INDX = $INDX + 1;
    $TAGS[$INDX]->{'Name'} = $CurrentTag;
    $TAGS[$INDX]->{'Module'} = $CurrentModule;
    $TAGS[$INDX]->{'Section'} = $CORE;

    if ($CurrentModule eq $CORE) {
	$HAS_CORE = 1;
    }
  }
  elsif ($CurrentTag ne "" &&  $line =~ /#\s*Format:\s(.*)/ ) {
    my $f = $1;
    $TAGS[$INDX]->{'Format'} = $f;
  }
  elsif ($CurrentTag ne "" &&  $line =~ /#\s*Description:\s*/ ) {
    # print "Description\n";
    $TAGS[$INDX]->{'Description'} = readLongTag(*IN{IO}, \$line);
    goto START_LOOP;
  }
  elsif ($CurrentTag ne "" &&  $line =~ /#\s*Default:\s*/ ) {
    # print "Default\n";
    $TAGS[$INDX]->{'Default'} = readLongTag(*IN{IO}, \$line);
    goto START_LOOP;
  }
  elsif ($CurrentTag ne "" &&  $line =~ /#\s*Example:\s*/ ) {
    # print "Example\n";
    $TAGS[$INDX]->{'Example'} = readLongTag(*IN{IO}, \$line);
    goto START_LOOP;
  }
  elsif ( $line =~ /#\s*Module:\s(.*)/ ) {
    my $m=$1;
    #        print "New Tag Found: ".$t."\n";
    $CurrentModule= $m;
    $CurrentModuleTxt = $m;
    $MODINDX = $MODINDX + 1;
    $MODS[$MODINDX]->{"Name"} = $CurrentModule;
  }
  elsif ($CurrentModuleTxt ne "" &&  $line =~ /#\s*Description:\s*/ ) {
    # print "Description module\n";
    $MODS[$MODINDX]->{'Description'} = readLongTag(*IN{IO}, \$line);
    goto START_LOOP;
  }
  elsif ($CurrentModuleTxt ne "" &&  $line =~ /#\s*Example:\s*/ ) {
    # print "Example module\n";
    $MODS[$MODINDX]->{'Example'} = readLongTag(*IN{IO}, \$line);
    goto START_LOOP;
  }
  elsif ( $line =~ /#\s*End module:\s(.*)/ ) {
    $CurrentModule= $CORE;
  }
  else {
    $CurrentModuleTxt = "";
    $CurrentTag = "";
  }
} while($line = <IN>);

close(IN);
# close(OUT);

print "<H2>Modules/Subsystems</H2>";
print "<ul>";
foreach (@MODS) {
  if (!($HAS_CORE == 0 && $_->{"Name"} eq $CORE)) {
    print_mod_index_html("", $_->{"Name"});
  }
}
print "</ul>";

print "<H2>Configuration parameters</H2>\n\n";
foreach (@MODS) {
  my $modname = $_->{'Name'};
  if (!($HAS_CORE == 0 && $_->{"Name"} eq $CORE)) {
    print_mod_html($_);
    print "<ul>";
    foreach (@TAGS) {
      if ($modname eq $_->{'Module'}) {
	print_tag_index_html("", $_->{'Name'});
      }
    }
    print "</ul>\n\n";
  }
}
print "<hr>\n\n";
print "<H2>Configuration parameters description</H2>\n\n";

foreach (@TAGS) {
  print_tag_html($_);
}



sub readLongTag {
  my $in = shift;
  my $tagline = shift;
  my $line;
  my $descr;
  my $putLN;

  $descr = "";
  $putLN = "";
  while ($line = <$in>) {
    if ($line !~ /#\ *\t(.*)/) {
      #print "Last line for Descr: ".$line;
      $$tagline = $line;
      last;
    }
    else {
      $descr = $descr.$putLN.$1;
      $putLN = "\n";
    }
  }
  $descr =~ s/</&#60;/g;
  $descr =~ s/>/&#62;/g;
  return $descr;
}

sub print_tag_index_html{
  my $indx = shift;
  my $tagname = shift;
  my $tn = $tagname;
  $tn =~ s/[\.|\s]/_/g;
  print "<li><A HREF=\"\#tag_".$tn."\">$indx  $tagname </A> </li>\n";
}

sub print_tag_html {
  my $tag = shift;
  my $tagname = $tag->{'Name'};
  $tagname =~ s/[\.|\s]/_/g;
  print "<DL>\n";
  print "<DT> <B><I> <A name=\"tag_$tagname\">".$tag->{'Name'}."</A></I></B></DT>\n";
  print "<DD>\n";
  print "<I>Format:</I> <blockquote> <pre>".$tag->{'Format'}."</pre> </blockquote>\n";
  print "<I>Description:</I> <blockquote> <pre>".$tag->{'Description'}."</pre> </blockquote> \n";
  print "<I>Default:</I> <blockquote>  <pre>".$tag->{'Default'}."</pre> </blockquote> \n";
  if (exists($tag->{'Example'})) {
    print "<I>Example:</I> <blockquote> <pre>".$tag->{'Example'}."</pre> </blockquote> \n";
  }
  print "</DD>";
  print "</DL>\n\n";
}


sub print_mod_index_html{
  my $indx = shift;
  my $modname = shift;
  my $mn = $modname;
  $mn =~ s/[\.|\s]/_/g;
  print "<li><A HREF=\"\#mod_".$mn."\">$indx  $modname </A> </li>\n";
}

sub print_mod_html {
  my $mod = shift;
  my $modname = $mod->{'Name'};
  $modname =~ s/[\.|\s]/_/g;
  print "<H3> <A name=\"mod_$modname\">".$mod->{'Name'}." configuration</A></I></B></H3>\n";
  if (exists($mod->{'Description'})) {
    print "<I>Description:</I> <blockquote> <pre>".$mod->{'Description'}."</pre> </blockquote> \n";
  }
  if (exists($mod->{'Example'})) {
    print "<I>Example:</I> <blockquote> <pre>".$mod->{'Example'}."</pre> </blockquote> \n";
  }
  print "\n\n";
}
