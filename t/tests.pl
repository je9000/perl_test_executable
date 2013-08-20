#!/usr/local/bin/perl -w

use strict;
use Test::Simple tests => 3;

ok( how_many_tacos_can_i_afford( 100, 20 ) == 5, 'Good data' );
ok( !defined how_many_tacos_can_i_afford( 'invalid', 20 ), 'Invalid cash' );
ok( !defined how_many_tacos_can_i_afford( 100, 'invalid' ), 'Invalid price' );
