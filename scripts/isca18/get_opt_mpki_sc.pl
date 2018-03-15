#!/usr/bin/perl
if($#ARGV != 1){
    print "ERROR : Usage : perl get_hitrate.pl <baseline> <dut>\n";
    exit;
}

our $baseline_stats_file = shift;
our $dut_stats_file = shift;

our $dut_mpki=0.0;
$dut_mpki = $dut_mpki + compute_mpki($baseline_stats_file, $dut_stats_file, 0);

print "$dut_mpki\n";

sub compute_mpki
{
    $baseline_file = $_[0];   
    $stats_file = $_[1];   
    $core = $_[2];

    my $num_accesses = 0;
    my $roi = 0;
    my $core_stats = 0;

    foreach (qx[cat $baseline_file 2>/dev/null]) {
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
            $num_accesses = $num_accesses + $1;
        }
        
        if (($roi == 1) && ($core_stats == 1) && ($line =~ m/LLC RFO[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_accesses = $num_accesses + $1;
        }
    }

    $hit_rate_line=`grep "OPTgen hit rate for core $core" $stats_file`;
    my $hit_rate = 0.0;
    if ($hit_rate_line =~ m/OPTgen hit rate for core $core:[\s\t]+([\d\.]+)/)
    {
        $hit_rate = $1;
    }
    else
    {
        print "Not found\n";
    }
    #print "$hit_rate\n";
    my $num_misses = (100-$hit_rate)*$num_accesses/100;

    $mpki = $num_misses/1000000;
    return $mpki;
}

