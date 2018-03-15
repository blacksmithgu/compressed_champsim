#!/usr/bin/perl
if($#ARGV != 0){
    print "ERROR : Usage : perl get_hitrate.pl <dut>\n";
    exit;
}

our $dut_stats_file = shift;

our $dut_mpki=0.0;
$dut_mpki = $dut_mpki + compute_mpki($dut_stats_file, 0);
$dut_mpki = $dut_mpki + compute_mpki($dut_stats_file, 1);
$dut_mpki = $dut_mpki + compute_mpki($dut_stats_file, 2);
$dut_mpki = $dut_mpki + compute_mpki($dut_stats_file, 3);

print "$dut_mpki\n";

sub compute_mpki
{
    $stats_file = $_[0];   
    $core = $_[1];
    my $num_misses = 0;
    my $roi = 0;
    my $core_stats = 0;

    foreach (qx[cat $stats_file 2>/dev/null]) {
        $line = $_;

        if ($line =~ m/Region of Interest Statistics/)
        {
            $roi = 1;
        }
    
        if ($line =~ m/CPU ([\d]+)/)
        {
            if($1 == $core)
            {
                $core_stats = 1;
            }
            else
            {
                $core_stats = 0;
            }
        }
    
        if (($roi == 1) && ($core_stats == 1) && ($line =~ m/LLC LOAD[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_misses = $num_misses + $3;
        }
        
        if (($roi == 1) && ($core_stats == 1) && ($line =~ m/LLC RFO[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_misses = $num_misses + $3;
        }
    }

    $mpki = $num_misses/1000000;
    return $mpki;
}

