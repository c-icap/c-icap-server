#!/usr/bin/perl

my(%args);
if($ENV{'REQUEST_METHOD'} eq "GET") {
   $query=$ENV{'QUERY_STRING'};
   my(@line_args)=split(/&/,$query);

   for my $arg (@line_args){
       $arg=~ s/\+/ /g; # replace + with spaces.....
       ($key,$val)=split(/=/,$arg);
       $val =~ s/%(..)/pack("c",hex($1))/ge;
       if($key eq "file" || $key eq "usename" || $key eq "content" ||$key eq "remove"  ){
	   $args{$key}=$val; 
       }
       
   }


}
else{
    print "Content-type: text/html\n\n";
    print "No Arguments.....";
    exit 1;
}


$filename="/srv/www/htdocs/downloads/".$args{"file"};
my(@stat)=stat $filename;
binmode(STDOUT);
if(open (F,"<$filename")){
    print "Connection: close\n";
    if($args{"content"}){
	print "Content-Type: ".$args{"content"}."\n\\";
    }
    else{
	print "Content-Type: application/octet-stream\n";
    }
    
    print "Content-Length: ".$stat[7]."\n";
    if($args{"usename"}){
	print "Content-Disposition: attachment; filename=".$args{"usename"}."\n\n";
    }
    else {
	print "Content-Disposition: attachment; filename=".$args{"file"}."\n\n";
    }

    while($len=sysread( F, $buf,512)){
	print $buf;
    }
    close F;
    if($args{"remove"}==1){
	unlink $filename;
    }
}
else {
    print "Connection: close\n";
    print "Content-Type: text/html\n\n";
    print "<H1>Error </H1>\n";
    print "The file ".$args{"file"}." does not exists in the server<br>\n";
    print "Please contact to the administrator for more info.\n\n";
}

