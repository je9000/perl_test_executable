#!/usr/local/bin/perl
 # testme.pl

 use warnings;
 use strict;

 my $I_LIKE_FRACTIONAL_TACOS = 0;
 my $cash;
 my $taco_price;
 
 sub how_many_tacos_can_i_afford {
     my ( $spend_me, $taco_price ) = @_;
     if ( $spend_me !~ /^\d+(?:\.\d+)?$/ ) { return undef; }
     if ( $taco_price !~ /^\d+(?:\.\d+)?$/ ) { return undef; }

     if ( $I_LIKE_FRACTIONAL_TACOS ) {
         return $spend_me / $taco_price;
     } else {
         return int( $spend_me / $taco_price );
     }
 }
 
 print "How much cash do you have: ";
 $cash = <>;
 print "How much does a taco cost: ";
 $taco_price = <>;

 chomp( $cash );
 chomp( $taco_price );

 print "\nYou can afford " . how_many_tacos_can_i_afford( $cash, $taco_price ) . " tacos! Yay!\n";

