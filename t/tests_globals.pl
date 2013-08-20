 #!/usr/local/bin/perl
 # tests_globals.pl

 my $tested_globals = $_;

 use warnings;
 use strict;
 use Test::Simple tests => 1;

 ${ $tested_globals->{'$I_LIKE_FRACTIONAL_TACOS'} } = 1;
 
 ok( how_many_tacos_can_i_afford( 100, 40 ) == 2.5, 'Fractional tacos' );

