#!/usr/local/bin/perl

use warnings;
use strict;
use Test::Simple tests => 1;

ok( do_call(\&callback1) == 44);
