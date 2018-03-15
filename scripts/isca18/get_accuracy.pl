#!/usr/bin/perl
if($#ARGV != 0){
    print "ERROR : Usage : perl get_hitrate.pl <dut>\n";
    exit;
}

our $dut_stats_file = shift;

our $dut_accuracy=0.0;
$dut_accuracy = compute_accuracy($dut_stats_file);

print "$dut_accuracy\n";

sub compute_accuracy
{
    $stats_file = $_[0];   
    my $num_useful = 0;
    my $num_useless = 0;
    my $roi = 0;

    foreach (qx[cat $stats_file 2>/dev/null]) {
        $line = $_;

        if ($line =~ m/Region of Interest Statistics/)
        {
            $roi = 1;
        }
        if (($roi == 1) && ($line =~ m/USEFUL:[\s\t]+([\d]+)[\s\t]+USELESS:[\s\t]+([\d]+)/))
        {
            $useful = $1;
            $useless = $2;
            if ($line =~ m/^L1D/)
            {
#                print "$line\n";
                $num_useful = $num_useful + $useful;
                $num_useless = $num_useless + $useless;
            }
        }
    }

    $accuracy = 100* $num_useful/($num_useful + $num_useless);
    return $accuracy;
}

