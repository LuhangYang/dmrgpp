#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(:config no_ignore_case);
use Ci;

my ($min,$max,$submit,$valgrind,$workdir,$restart,$n);
GetOptions(
'm=f' => \$min,
'M=f' => \$max,
'S' => \$submit, 
'valgrind=s' => \$valgrind,
'w=s' => \$workdir,
'r' => \$restart,
'n=f' => \$n);
defined($submit) or $submit = 0;
defined($valgrind) or $valgrind = "";
defined($workdir) or $workdir = "tests";
defined($restart) or $restart = 0;

if (defined($n)) {
	if (defined($min) or defined($max)) {
		die "$0: -n cannot be used with either -m or -M\n";
	}

	$min = $n;
	$max = $n;
}

my $templateBatch = "batchDollarized.pbs";
my @tests;
Ci::getTests(\@tests);

prepareDir();

my $total = scalar(@tests);
for (my $i = 0; $i < $total; ++$i) {
	my $n = $tests[$i];
	next if (defined($min) and $n < $min);
	next if (defined($max) and $n > $max);
	
	my $ir = isRestart("../inputs/input$n.inp",$n);
	if ($restart) {
		next if (!$ir);
	} else {
		next if ($ir);
	}

	procTest($n,$valgrind,$submit);
}

sub isRestart
{
	my ($file,$n) = @_;
	open(FILE, "$file") or return 0;
	my $so;
	while (<FILE>) {
		chomp;
		if (/SolverOptions=(.*$)/) {
			$so = $1;
			last;
		}
	}

	close(FILE);
	defined($so) or return 0;
	if ($so =~/restart/) { return 1; }
	return 0;
}

sub procTest
{
	my ($n,$tool,$submit) = @_;
	my $valgrind = ($tool eq "") ? "" : "valgrind --tool=$tool ";
	$valgrind .= " --callgrind-out-file=callgrind$n.out " if ($tool eq "callgrind");
	my $cmd = "$valgrind./dmrg -f ../inputs/input$n.inp &> output$n.txt";
	my $batch = createBatch($n,$cmd);
	submitBatch($batch) if ($submit);
}

sub createBatch
{
	my ($ind,$cmd) = @_;
	my $file = "Batch$ind.pbs";
	open(FOUT,">$file") or die "$0: Cannot write to $file: $!\n";

	open(FILE,"../$templateBatch") or die "$0: Cannot open ../$templateBatch: $!\n";

	while (<FILE>) {
		while (/\$\$([a-zA-Z0-9\[\]]+)/) {
			my $line = $_;
			my $name = $1;
			my $str = "\$".$name;
			my $val = eval "$str";
			defined($val) or die "$0: Undefined substitution for $name\n";
			$line =~ s/\$\$$name/$val/;
			$_ = $line;
		}

		print FOUT;
	}

	close(FILE);
	close(FOUT);

	print STDERR "$0: $file written\n";
	return $file;
}

sub submitBatch
{
	my ($batch,$extra) = @_;
	defined($extra) or $extra = "";
	sleep(1);
	print STDERR "$0: Submitted $batch $extra $batch\n";

	my $ret = `qsub $extra $batch`;
	chomp($ret);
	return $ret;
}

sub prepareDir
{
	my $b = (-r "$workdir");
	system("mkdir $workdir") if (!$b);
	my $cmd = "diff ../src/dmrg $workdir/dmrg &> /dev/null";
	my $ret = system($cmd);
	system("cp -a ../src/dmrg $workdir/") if ($ret != 0);
	chdir("$workdir/");
}
