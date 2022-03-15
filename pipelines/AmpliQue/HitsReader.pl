#!/usr/bin/perl -w
#  
# =====================================================================
# Gisela Gonzalez
# Bioinformatics Research Group (BioRG)
# Florida International University
# August 2008
# =======================================================================
# This script reads a file that has been processed by BlastParser.pl;
# it finds if the forward and reverse primer thatwere found in the same target
# sequence, and output the description, and length of PCR results.
#
# Sample Command:
#
#    perl HitsReader.pl inputfile.txt outputfile.txt forwardPrimer ReversePrimer
#					
# ==============================================================================


use strict;


# Program called with incorrect number of arguments
if( ! defined( $ARGV[ 0 ] ) )
{
	error();
}

# file names
my $fin = shift @ARGV; chomp $fin;    # name of input file
my $fout = shift @ARGV; chomp $fout;  # name of output file
my $fprimer = shift @ARGV; chomp $fprimer; #name of the forward primer
my $rprimer = shift @ARGV; chomp $rprimer; #name of the reverse primer


#open output file
open(OUTPUT, ">$fout");

my $line;
my $rline;
my $arr;
my %fhash = ();
my %rhash = ();
my %duplicateFhash = ();

#open input file
open (MYFILE, "<$fin");


while( $line = <MYFILE> )
{
	chomp $line;
	my @arr = split(/\t/,$line);
	my $subject = $arr[13];
	if ($arr[0] eq $fprimer)
	{
	   
	    #find if there are duplicates in the forward primer hits
		if (exists($fhash{$subject}))
		{
			$duplicateFhash{$subject} = $line;
		}
		# create hash with subjects from forward primer
		$fhash{$subject} = $line;
	}
	#check for hits of reverse primer on same subject
	else
	{
		if ($arr[0] eq $rprimer && exists($fhash{$subject}) ||
		    ($arr[0] eq $rprimer && exists($duplicateFhash{$subject})))
		{
			#if hits are found create an array of the element from the forward primer hits
			my $FHitLines = $fhash{$subject};
			my @hits = split(/\t/,$FHitLines);
			#create an array of the elements from the reverse primer hits to compare with forward
			$rhash{$subject} = $line;
			my $RHitLines = $rhash{$subject};
			my @rhits = split (/\t/,$RHitLines);
			#find length of found sequence
			my $length = $rhits[7] - $hits[7] + 1;
			#find description
			my $domain = "";
			if ($hits[14] =~ "archae")
			{
				$domain = "Arch ";
			}
			elsif ($hits[14] =~ "prokaryote")
			{
				$domain = "? ";
			}
			else
			{
				$domain = "B ";
			}
			#initialize variables for other information
			my $genus = "-";
			my $species = "-";
			my $strain = "-";
			my $gid = "-";
    
			# find other info by using ; as a separator and creating an array
			 my @DA = split(/;/, $hits[14]);
	 
			if ($#DA >= 2)
			{
				$strain = $DA[1];
				#$strain =~ s/ //g; # Do I really need this? The output does not change when commented out
				$gid = $DA[$#DA];
				#split the first element of the arrays using blank space
				my @parts = split(/ /, $DA[0]);
				if ($#parts >= 0)
				{
					if (($parts[0] ne "uncultured") && ($parts[0] ne "unidentified"))
					{
						$genus = $parts[0];
						$species = $parts[1];
					}
					elsif (($parts[1] ne "bacterium") && ($parts[1] ne "archaeon") &&
					($parts[1] ne "eubacterium") && ($parts[1] ne "prokaryote"))
					{
						$genus = $parts[1];
					}
				}
			}
			elsif ($#DA == 1)
			{
				$gid = $DA[1];
				my @parts = split(/ /, $DA[0]);
				if ($#parts >= 0)
				{
					if (($parts[0] ne "uncultured") && ($parts[0] ne "unidentified"))
					{
						$genus = $parts[0];
						if ($#parts >= 1)
						{
							if (($parts[1] ne "sp.") && ($parts[1] ne "str.")
								&& ($parts[1] ne "bacterium"))
							{
								$species = $parts[1];
								if ($#parts >= 2)
								{
									$strain = $parts[2];
									if ($#parts >= 3)
									{
										$strain = $strain.$parts[3]; # what if there is more
									}
								}
							}
							else
							{
								if ($#parts >= 2)
								{
									$strain = $parts[2];
									if ($#parts >= 3)
									{
										$strain = $strain.$parts[3]; # what if there is more
									}
								}
							}				
						}
					}
					elsif (($parts[1] ne "bacterium") && ($parts[1] ne "archaeon") &&
					($parts[1] ne "eubacterium") && ($parts[1] ne "prokaryote"))
					{
						$genus = $parts[1];
						if ($#parts >= 2)
						{
							if ($parts[2] ne "bacterium")
							{
								$strain = $parts[2]; # what if there is more
							}
							elsif ($#parts >= 3)
							{
								$strain = $parts[3]; # what if there is more
							}
						}
					}
				}
			}
			else
			{
				if (($DA[0] ne "uncultured") && ($DA[0] ne "unidentified"))
				{
					$genus = $DA[0];
				}
			}	
			# report length of PCR product and the name of the organism
			print OUTPUT "$hits[13]\t$domain\t$length\t$genus\t$species\t$strain\t$gid\n";
			# print "$l\t$d\n";
		} 
		else
		{
			print "\n";
		}
	} 
}
    
close (MYFILE);
