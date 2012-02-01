use strict;
use warnings;
use IO::Socket;

$SIG{CHLD} = 'IGNORE';

sub ranking_add {
    my ($group_id, $player_id, $point) = @_;
    my $sock = IO::Socket::INET->new('127.0.0.1:6666');
    $sock->send(pack('llll', (1, 10, 10, 10)), 0);
    $sock->close();
}

sub ranking_search {
    my $page = int(rand()*10+1);
    my $sock = IO::Socket::INET->new('127.0.0.1:6667');
    $sock->send(pack('llll', (3, 10, 1, 5)), 0);
    my $mem;
    $sock->read($mem,40,0);
    $sock->close();
    my @result = unpack("llllllllll", $mem);
}

sub ranking_sum {
    my $group_id = shift;
    my $sock = IO::Socket::INET->new('127.0.0.1:6667');
    $sock->send(pack('ll', (4, 10)), 0);
    my $mem;
    $sock->read($mem,4,0);
    $sock->close();

    my @result = unpack("l", $mem);
    print "@result\n";
}

ranking_add(10,10,10);

if (1) {
if(fork() == 0) {
    for (1..1000) {
        ranking_sum(10);
        select undef, undef, undef, 0.01;
    }
    exit;
}
}

if (1) {
for (1..2000) {
    ranking_add(10,10,10);;
    select undef, undef, undef, 0.005;
}
}
