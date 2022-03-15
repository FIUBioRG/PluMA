#!/usr/bin/env perl
#
#       vPhyloMM_CLI.pl
#       
#       Copyright 2009 John Tyree <johntyree@gmail.com>
#       

=head1 NAME

Run_vPhyloMM.pl

=head1 SYNOPSIS

    perl Run_vPhyloMM.pl --new=<FILENAME> | --variables-file=<FILENAME> [--log=<FILENAME>] [--gui] [--verbose [--verbose ...]]

=head1 DESCRIPTION

An interface for the vPhyloMM library and GUI. Make a variables file with --new. Edit it using the gui or a text editor. Run it with the --variables-file flag.

=head1 DEPENDENCIES

=over

=item * Perl 5.10.0

=item * Getopt::Long

=item * Cwd

=item * vPhyloMM (included)

=item * vPhyloMM_GUI (included optional)

=item * GraphViz (GraphViz only)

=item * Graph::Directed (GraphViz only)

=back

See perldoc vPhyloMM and vPhyloMM_GUI for their respective dependencies.

=head1 OTHER DEPENDENCIES

=over

=item * Sliding MinPD (http://biorg.cis.fiu.edu/SlidingMinPD/)

=item * R (http://www.r-project.org/)

=over

=item * msm library for R (http://cran.r-project.org/web/packages/msm/)

=back

=item * GraphViz (Optional) (http://www.graphviz.org/)

=back

=head1 ARGUMENTS

=over

=item * new

Creates a new variables file with default values. Editing can be done in plain text or by loading the GUI, making changes, and selecting:

    File -> Save

=item * Variables file

Where to find a vPhyloMM variables file such as is described by "new".

=item * log

File name of the log file which is to be created or overwritten. If left unspecified, "log.txt" is used. Logs begin with the current time and end with the time that the program exits.

=item * gui

Run the vPhyloMM_GUI. It requires 'Tkx' which has known issues with many distributions of perl. This feature has been confirmed to work properly with ActiveState's ActivePerl distribution (http://www.activestate.com/activeperl) and confirmed NOT to work properly with perl.org's "perl 5.10.0", the default for many Linux distributions.

=item * verbose

Each appearance increases the verbosity of the log by 1. The default is 0. The current maximum is 2. If "verbose" is also specified in the variables file, the larger of the two values is used.

=back

=head1 HISTORY

=head2 Author

John Tyree <johntyree@gmail.com>

=head2 Creation Date

2009-07-08

=cut

use strict;
use warnings;
use vPhyloMM;
use Pod::Usage;
use Getopt::Long;
require 5.010_000;
use feature 'say';

my @revisionNumbers = (22072009,426);

my %Pod2UsageParameters = (
    -message => "Try \"perldoc $0\" for more information.\n",
    -verbose => 1,
    -exitval => 0,
    -output  => \*STDERR,
);

my $logFile = "log.txt";
my $variables_file;
my $gui = 0;
my $verbose;
my $new;
@ARGV or pod2usage(%Pod2UsageParameters);

my $OptionParser = new Getopt::Long::Parser;
    
$OptionParser->getoptions(
        "variables-file=s"  => \$variables_file,
        "verbose+"  => \$verbose,
        "log=s"     => \$logFile,
        "gui"       => \$gui,
        "new=s"       => \$new,
    );

exit(vPhyloMM->createVariablesFile($new)) if $new;

$variables_file = File::Spec->rel2abs( $variables_file ) if defined($variables_file);
$logFile = File::Spec->rel2abs( $logFile );

open(LOG, ">",$logFile) or die "Unable to open $logFile. $!";
close(LOG);

defined($variables_file) or pod2usage(
    -message => "No variables-file specified. Try perl $0 --new <FILENAME> to create a new one.\n",
    -verbose => 1,
    -exitval => 0,
    -output  => \*STDERR,
);
if ($gui) {
    eval {require vPhyloMM_GUI} or die "Error: $@";
    exit(runGUI($logFile, $variables_file, $verbose));
}

exit(runCLI($logFile, $variables_file, $verbose));


## Functions start here ##

sub runGUI{
    my ($logFile, $variables_file, $verbose) = @_;
   
    # create the gui object
    my $vPhyloMM_GUI = vPhyloMM_GUI->new(
        variables_file => $variables_file,
        Verbose => $verbose,
        Log => $logFile,
    );
    
    # run the gui
    my $guiReturnValue = 0;
    if( ref( $vPhyloMM_GUI ) ne 'vPhyloMM_GUI' ){
        print "ref(vPhyloMM_GUI) == ".ref($vPhyloMM_GUI)."\n";
        print STDERR "Error: vPhyloMM GUI could not be instantiated ($vPhyloMM_GUI)\n" if $verbose;
        return 1;
    }
    $guiReturnValue = $vPhyloMM_GUI->run();
    print STDERR "Error: The GUI returned $guiReturnValue\n" if $verbose and $guiReturnValue;    
    return $guiReturnValue;
}

sub runCLI {
    my ($log, $variables_file, $verbose) = @_;
    if (not -e $variables_file ) {
        print STDERR "Error: Can't find variables file '$variables_file'\n" if $verbose;
        return -1;
    }
    my $vPhyloMM = vPhyloMM->new(
            Log => $log,
            variables_file => $variables_file,
            Verbose => $verbose,
        );
    my $ref = ref $vPhyloMM;
    if( $ref ne "vPhyloMM" ){
        open(LOG, ">".$log) or print $!;
        print STDERR "Error: could not create a vPhyloMM object ($vPhyloMM)\n";
        print LOG "Error: could not create a vPhyloMM object ($vPhyloMM)\n";
        #~ undef $log;
        return -1;
    }
    my $CLI_Refresh_Function = sub { 
        printf("%.1f%% ", ($vPhyloMM->{currentJob} / $vPhyloMM->{numberOfJobs} * 100.0) ) if $vPhyloMM->{Verbose};
        return 0;
    };
    print "Generating reports ... " if $verbose;
    my $cliReturnValue = $vPhyloMM->generate_reports(Refresh_Function => $CLI_Refresh_Function);
    print "\n" if $verbose;
    printf(STDERR "Error: %s (%d)\n", $vPhyloMM->{errorMessage}, $cliReturnValue) if $cliReturnValue;    
    return 0;
}
