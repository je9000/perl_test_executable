#!/usr/local/bin/perl

use warnings;
use strict;

sub callback1 {
    warn "I'm called!";
    return 44;
}

sub do_call {
    return (shift)->();
}

warn do_call(\&callback1);
