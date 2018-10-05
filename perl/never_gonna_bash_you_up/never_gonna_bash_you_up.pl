#!/usr/bin/perl

use strict;
use warnings;

my @lyrics = ("", "Never gonna give you up", "Never gonna let you down", "Never gonna run around", "And desert you", "", "Never gonna make you cry", "Never gonna say goodbye", "Never gonna tell a lie", 
"And 
hurt you");
my $contador = 0;

my $user = `echo \$USER`;
my $host = `echo \$HOSTNAME`;

chomp($user);
chomp($host);

system('clear');

while (1) {
  print( "[$user\@$host ~]\$ " );
  my $ignore = <STDIN>;
  if ( $lyrics[$contador] eq "" ) {
    system($ignore);
  }else{ 
    print( $lyrics[$contador], "\n" );
  }
  $contador = ($contador + 1) % scalar(@lyrics);
}
