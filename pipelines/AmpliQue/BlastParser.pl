#!/usr/bin/perl -w
#  
# ==============================================================================
#
#									BioRG
#						Bioinformatics Research Group
#					   Florida International University
#
# The purpose of this script is to parse a BLAST report using one of the search
# modules of BioPerl.  The principal module(s) employed is SearchIO.  An output
# file with the extension ".sum" will be generated with the results.
#
# Note that this is a modified version of the original software by
# Qiandong Zeng ( Last revision: October 8, 2004. Zeng )
#
# Default Usage: BlastParser.pl SampleBlastReportFile.out -i
#
# NOTE: Default usage processes all hits with no filtering
#		The '-i' option can be omitted if no images
# 		 are desired.
#
# Alternate Usage:  blastParser.pl --images SampleBlastReportFile.out 1.0 1 1
#			NOTE: "Alternate Usage" filters results based on three parameters
#					1) the Evalue cutoff
#					2) the HSP Size cutoff
#					3) the identity cutoff
#
# Original Work: 	Qiandong Zeng, circa 2004
#
# Updated:		January 2008
# Created:		Camilo Valdes, BioRG.  December 2007
#
# Sample Command:
#        perl BlastParser.pl Output_file_from_Blast.out
#
# ==============================================================================

use strict;
use Getopt::Long;
use Bio::SearchIO;
use Bio::Graphics;
use Bio::SeqFeature::Generic;
use File::Path;
use Benchmark;
use Term::ANSIColor qw(:constants);


################################################################################
#
#	Main

# Program called with incorrect number of arguments
if( ! defined( $ARGV[ 0 ] ) )
{
	error();
}

# Command line option variable with default value (FALSE)
my $images = "";
# Get the '--images' command line option to see if we should draw the images
GetOptions('i' => \$images);


# BLast report output file to process
my $queryBlast = $ARGV[ 0 ];
    
# Number of query entries
my $count = 0;
    
# Default filtering parameters, used only if provided
my $eValueCutoff = "";
my $hspSizeCutoff = "";
my $identityCutoff = "";
    
my $filter = "No";
    
if( ( defined( $ARGV[ 1 ] ) ) && ( defined( $ARGV[ 2 ] ) ) && ( defined( $ARGV[ 3 ] ) ) )
{
	# E-value hits higher than this are ignored
    $eValueCutoff	= $ARGV[ 1 ];
    
    # HSPs shorter than this will be ignored
    $hspSizeCutoff	= $ARGV[ 2 ];
        
    # Alignment with lower identity than this are ignored
    $identityCutoff	= $ARGV[ 3 ];
		
    $filter = "Yes";
}

print( BOLD, GREEN, "\nCreating output files ..... ", RESET );
    
# Text-based Output file with Report parsed
my $outFile = "$ARGV[ 0 ]" . ".sum";
unless( open( OUTFILE, ">$outFile" ) )
{
	print ( BOLD, RED, "\nProblem creating output file $outFile. Please verify.\n\n", RESET );
	exit( 1 );
}


# Create a Directory to Store the Images.
# But skip it, if the -i was not set

if( ! $images )
{	
	goto SKIP_1;
}

my $DirPrefix = $ARGV[0];
my $FileName = $ARGV[0];

if( $DirPrefix =~ m/\// )
{
	$DirPrefix =~ s/\/.*$/\//;
	$FileName =~ s/.out//;
	$FileName =~ s/^.*\///;
}
else
{
	$DirPrefix = "";
	$FileName =~ s/.out//;
}

my $ImgDir = $DirPrefix . $FileName . "_Images";

unless( mkdir("$ImgDir", 0777) )
{
	# Directory Allready exists
	print( BOLD, RED, "\nProblem creating Output Directory for Images. Please verify.\n\n", RESET );
	exit( 1 );
}

SKIP_1:

print( BOLD, GREEN, "Complete !\n", RESET );
    
# Header for OUTPUT file
print OUTFILE ( "Q-name\tQ-length\tS-length\tOverlap\tQ-start\tQ-end\tQ-strand\t" );
print OUTFILE ( "S-start\tS-end\tS-strand\tE-value\tScore\tPercent_id\tSubj\tDescription\n" );

# Benchmark the running time
my $start = new Benchmark;

print( BOLD, GREEN, "Loading BLAST report ..... ", RESET );
    
# Create a new BioSearchIO object from the input BLAST file
my $in = new Bio::SearchIO(-format => 'blast', -file => $queryBlast);

print( BOLD, GREEN, "Complete !\n", RESET );

print( BOLD, GREEN, "Parsing: ", RESET );

# Get the 'next' Report from the BLAST file object
while( my $searchObj = $in->next_result ) 
{
	#Dissable screen buffer
	$| = 1;
	#print( BOLD, YELLOW, ".", RESET );
	
	# Standard-issued BLAST report attributes
    my $query		= $searchObj->query_name;
	my $queryName 	= $searchObj->query_description;
	$queryName 	=~ s/\| //;
    my $queryLength	= $searchObj->query_length;
    
    # Again, skip the image-creation stuff if '-i' was not set
    if( ! $images )
	{	
		goto SKIP_2;
	}
	
    # Image Output files
	my $ImageOutFile = $ImgDir . "/" . $query . ".png";
	
	unless( open( ImgOUT, ">$ImageOutFile" ) )
	{
		print( BOLD, RED, "\nProblem creating Image file $ImageOutFile. Please verify.\n\n", RESET );
		exit( 1 );
	} 
	
	SKIP_2:
    
    # Graphics
    my $panel = Bio::Graphics::Panel->new(
    										-length => $searchObj->query_length,
    										-width => 900,
    										-pad_left => 10,
    										-pad_right => 10,	
    										);
    										
    # Graphics, image scale
    my $full_length = Bio::SeqFeature::Generic->new(
    													-start => 1,
    													-end => $searchObj->query_length,
    													-display_name => $searchObj->query_name,
    												);
    												
    # Graphics, drawing panel
    $panel->add_track( 
    					$full_length,
    					-glyph => 'arrow',
    					-tick => 2,
    					-fgcolor => 'black',
    					-double => 1,
    					-label => 1,
    					);
    
    my $track = $panel->add_track(
    								-glyph => 'graded_segments',
    								-label => 1,
    								-connector => 'dashed',
    								-bgcolor => 'blue',
    								-font2color => 'red',
    								-sort_order => 'high_score',
    								-description => sub{
    														my $feature = shift;
    														return unless $feature->has_tag('description');
    														my( $description ) = $feature->each_tag_value('description');
    														my $score = $feature->score;
    														"score = $score";
    													},
    								);
    
	# Process the individual Hits for the current Report
    while( my $hit = $searchObj->next_hit ) 
    {
    	my $subj = $hit->name;
	    my $sDesc = $hit->description;
	    	
	    $sDesc 	=~ s/\| //;
	    	
	    if ( $subj eq $query ) 
	    { 
	    	print( "\tEQUALS, MOVING ONTO NEXT\n\n" );
	    	next;
	    }
	    	
	    my $subjLength	= $hit->length;
	    
	    # Graphics
	    my $feature = Bio::SeqFeature::Generic->new(
	    											-score => $hit->raw_score,
	    											-display_name => $hit->name,
	    											-tag => { 
	    														description => ""
	    													},
	    											);
            
        # Process the individual HSPs from the current hit
        while( my $hsp = $hit->next_hsp ) 
        {
           	# Graphics
           	$feature->add_sub_SeqFeature( $hsp, 'EXPAND' );
           	
           	# Length of HSP, including gaps
			my $overlap 		= $hsp->length('total');
           	
            my $queryStart 		= $hsp->query->start;
            my $queryEnd 		= $hsp->query->end;
            my $subjStart 		= $hsp->hit->start;
            my $subjEnd	 		= $hsp->hit->end;
            my $hspEvalue 		= $hsp->evalue();
            my $subjStrand 		= $hsp->hit->strand;
			my $queryStrand		= $hsp->query->strand;
		    my $score 			= $hsp->score;
		    
			if( $hspEvalue	=~ /^e/ ) 
			{
				$hspEvalue	= "1" . $hspEvalue;
			}
				
			# Clean up the HSP Evalue, NCBI format change
			$hspEvalue =~ s/,//;
		            
		    if( $subjStrand < 0 ) 
		    {
				my $tmp = $subjEnd; 
		        $subjEnd = $subjStart;
		        $subjStart = $tmp;
		    }
            	
            # Get the fraction of identical positions within the given HSP
            my $hspIdentity = "";
            	
            if( $hsp->frac_identical() eq "1" ) 
            {
	        	$hspIdentity = "100";
	        }
	        else
	        {
	        	$hspIdentity = sprintf( "%3.1f", $hsp->frac_identical() * 100 );
	        }
                   
            # If filtering parameters are provided, then we use them
            if( ( defined( $ARGV[ 1 ] ) ) && ( defined( $ARGV[ 2 ] ) ) && ( defined( $ARGV[ 3 ] ) ) )
           	{
            	if ( ( $hspEvalue > $eValueCutoff ) || ( $overlap < $hspSizeCutoff ) || ( $hspIdentity < $identityCutoff ) ) 
                	{ next; }
            }
                                
            # Print everything to OUTPUT file
            print OUTFILE ( "$query\t$queryLength\t$subjLength\t$overlap\t$queryStart\t$queryEnd\t$queryStrand" );
            print OUTFILE ( "\t$subjStart\t$subjEnd\t$subjStrand\t$hspEvalue\t$score\t$hspIdentity\t$subj\t$sDesc\n" );
           
        } # End HSP's while loop
        
        #Graphics
        $track->add_feature($feature);
       
    } # End HIT while loop
    
    if( ! $images )
    {	
    	goto SKIP_3;
   	}
    # For Windows platforms, ImgOUT needs to be put into 'binary' mode so that the
    # PNG file does dot go through Window's carriage return/linefeed transformations
    binmode(ImgOUT);
    
    # Print the Image to file
    print ImgOUT $panel->png;
    
    close(ImgOUT);
    
    SKIP_3:
    $count++;

} # End Report Loop

close(OUTFILE);

print( BOLD, GREEN, " Complete !\n", RESET );
   
# Objects needed to compute running time
my $end = new Benchmark;
my $diff = timediff( $end, $start );
my $result = timestr( $diff );

#print( "\nProcessed $count queries with hits at E < $eValueCutoff and HSP size >= $hspSizeCutoff.\n" );
print( BOLD, BLUE, "\n*****************************************************************************\n", RESET );
print( "\nProcessed $count Queries" );
print( "\nOutput Report File: $outFile" );

if( $images )
{
	print( "\nOutput Images: $ImgDir" );
}
elsif( ! $images )
{
	print( "\nOutput Images: NO IMAGES WERE DRAWN." );
}

print( "\nFilter Option: $filter");
print( "\nRunning Time: $result\n" );
print( BOLD, BLUE, "\n*****************************************************************************\n\n", RESET );


################################################################################
#
# Subroutines & Methods
#
#

# 	Error Subroutine
#	Called only when the program is invoked with an incorrect number of arguments
#
sub error
{
	print( "\n*****************************************************************************\n");
	print( BOLD, RED, "Warning! Incorrect Number of Arguments\n\n", RESET );
	print( "Script to parse NCBI BLAST report output files and generate a summary\n\n" );
    print( "USAGE:     $0  <blastn output> <p-value cutoff> <hsp size cutoff> <identity cutoff>\n" );
    print( "\nExamples:\n" );
    print( "\t$0  --images SampleBlastReportFile.out\n" );
    print( "\t$0  --images SampleBlastReportFile.out  1.0  1  1\n" );
    print( "\n*****************************************************************************\n\n");
    
    exit( 1 );
}


#
# 	End Subroutines & Methods
#
################################################################################

# Exit the program with normal condition

exit( 0 );
