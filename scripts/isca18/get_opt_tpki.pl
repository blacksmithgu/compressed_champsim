#!/usr/bin/perl
if($#ARGV != 1){
    print "ERROR : Usage : perl get_hitrate.pl <baseline> <dut>\n";
    exit;
}

our $baseline_stats_file = shift;
our $dut_stats_file = shift;

our $dut_tpki=0.0;
$dut_tpki = compute_tpki($baseline_stats_file, $dut_stats_file);

print "$dut_tpki\n";

sub compute_tpki
{
    $baseline_file = $_[0];   
    $stats_file = $_[1];   

    my $num_accesses = 0;
    my $roi = 0;

    foreach (qx[cat $baseline_file 2>/dev/null]) {
        $line = $_;

        if ($line =~ m/Region of Interest Statistics/)
        {
            $roi = 1;
        }
    
        if (($roi == 1) && ($line =~ m/LLC LOAD[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_accesses = $num_accesses + $1;
        }
        
        if (($roi == 1) && ($line =~ m/LLC RFO[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_accesses = $num_accesses + $1;
        }
    }
    
    #print "$num_accesses\n";

    $traffic_line=`grep "Traffic:" $stats_file`;
    my $tpa = 0.0;
    if ($traffic_line =~ m/Traffic:[\s\t]+[\d]+[\s\t]+([\d\.]+)/)
    {
        $tpa = $1/100;
    }
    else
    {
        print "Not found\n";
    }
    #print "$tpa\n";
    $tpki = ($num_accesses*$tpa)/1000000;
    return $tpki;
}

