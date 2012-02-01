package Ranking;
use strict;
use warnings;

use Any::Moose;
use IO::Socket;

sub clean {
    my $sock = IO::Socket::INET->new('127.0.0.1:6666');
    if($sock) {
        $sock->send(pack('l', (5)), 0);
        $sock->close();
        return 1;
    }
    return 0;
}

sub add {
    my ($self, $group_id, $player_id, $point) = @_;
    my $sock = IO::Socket::INET->new('127.0.0.1:6666');
    if($sock) {
        $sock->send(pack('llll', (1, $group_id, $player_id, $point)), 0);
        $sock->close();
        return 1;
    }
    return 0;
}

sub sum {
    my ($self, $group_id) = @_;
    my $sock = IO::Socket::INET->new('127.0.0.1:6667');
    if($sock) {
        $sock->send(pack('ll', (4, $group_id)), 0);
        my $mem;
        $sock->read($mem,4,0);
        $sock->close();
        my @result = unpack("l", $mem);
        return $result[0];
    }
    return 0;
}

sub rank {
    my ($self, $group_id, $player_id) = @_;
    my $sock = IO::Socket::INET->new('127.0.0.1:6667');
    if($sock) {
        $sock->send(pack('llll', (2, $group_id, $player_id, 1)), 0);
        my $mem;
        $sock->read($mem,12,0);
        $sock->close();
        my @result = unpack("lll", $mem);
        return $result[2];
    }
    return 0;
}

sub ranks {
    my ($self, $group_id, $player_id, $page, $page_num) = @_;
    $page_num //= 5;
    my $sock = IO::Socket::INET->new('127.0.0.1:6667');
    if($sock) {
        if($page) {
            $sock->send(pack('llll', (3, $group_id, $page, $page_num)), 0);
        } else {
            $sock->send(pack('llll', (2, $group_id, $player_id, $page_num)), 0);
        }
        my $mem;
        $sock->read($mem,12*$page_num,0);
        my @result = unpack("l[$page_num]l[$page_num]l[$page_num]", $mem);
        return \@result;
    }
    return 0;
}

__PACKAGE__->meta->make_immutable;
