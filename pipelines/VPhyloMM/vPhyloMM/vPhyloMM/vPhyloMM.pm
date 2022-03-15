package vPhyloMM;

use File::Basename;
use File::Spec;

BEGIN{
	my $script_directory = &File::Basename::dirname( $0 );
	$script_directory = File::Spec->rel2abs( $script_directory );
	unshift @INC, File::Spec->rel2abs( $script_directory );
	my $package = __PACKAGE__;
	map { unshift @INC, join( '', File::Spec->rel2abs( (File::Spec->splitpath($_))[0..1] ) ) } grep { $_ =~ /$package/ } keys %INC;
	$INC{'vPhyloMM.pm'} = $0;
}

use strict;
use warnings;
use Bio::Seq;
use Bio::SeqIO;
use Getopt::Long qw(GetOptionsFromArray);
use Graph::Directed;
use ShortestPath;
use File::Basename;
use Statistics::Distributions;
use GraphViz;
use vPhyloMM_GUI;
use Tkx;
use Cwd;
use feature 'say';
use File::Copy;

my $sminpd_variables_template = qq`MINPD
	[input file] i"<FASTA>"
	[report all distances] d<REPORT_DISTANCES> [0:No, 1:Yes]
	[activate recombination detection] f<ACTIVATE_RECOMBINATION_DETECTION> [0:No, 1:Yes]
	[recombination detection option] r<RECOMBINATION_DETECTION_OPTION> [1:RIP, 2:Bootscan RIP, 3:Bootscan Standard, 4:Bootscan Standard+Dist]
	[crossover option] c<CROSSOVER_OPTION> [0:many, 1:only one]
	[PCC threshold] p<PCC_THRESHOLD> 
	[window size] w<WINDOW_SIZE>
	[step size] s<STEP_SIZE>
	[bootstrap recomb. tiebreaker option] t<BOOTSTRAP_TIEBREAKER> [0:No, 1:Yes]
	[bootscan seed] e<BOOTSCAN_SEED>
	[bootscan threshold] h<BOOTSCAN_THRESHOLD>
	[bootstrap:1/bootknife:0] b<BOOTSTRAP_BOOTKNIFE>
	[substitution model] v<SUBSITUTION_MODEL> [options: JC69, K2P, TN93] 
	[gamma shape - rate heterogeneity] a<GAMMA_SHAPE> [a0.5]
	[show bootstrap values] k<SHOW_BOOTSTRAP_VALUES> [0:No, 1:Yes]
	[clustered bootstrap] g"<CODON>" [Yes: add g and filename with codon positions, No: g]
	[clustering] j<CLUSTERING> [0: don't cluster by amino acid; 1: do cluster by amino acid]
	[output file] O"<SMINPD_OUTPUT_DIRECTORY>"
	[clustering distance threshold] T<CLUSTERING_THRESHOLD>
`;

my $run_matrix_script = qq`Arguments <- commandArgs()
	for(Argument in Arguments){
		Arg <- strsplit(Argument, split="=")
		if(length(Arg[[1]]) > 1){
			Key <- tolower(Arg[[1]][1])
			Value <- Arg[[1]][2]
			assign(Key, Value)
		}
	}
	
	
	if( !exists("outputfile") ){
		outputfile <- "PMatrix"
	}
	
	if( exists("times") ){
		times <- strsplit(times, split=",")[[1]]
		times <- as.numeric(times)
	} else{
		times <- c(1,8,16,20)
	}
	
	if( exists("wd") ){
		setwd(wd)
	}
	
	dat <- read.csv( inputfile, header=TRUE, sep=",", colClasses="character" )
	
	for(i in 5:ncol(dat)){
		dat[,i] <- as.numeric(dat[,i])
	}
	
	attach(dat)
	
	# Get the number of unique states
	size <- length(unique(c(ancestor.index,descendant.index)))
	
	# Index the rows corresponding to the same source / destination state 
	same <- ancestor.index==descendant.index
	
	# build up Q
	Q <- matrix(nrow=size, ncol=size)
	Q[cbind(ancestor.index, descendant.index)[!same,]] <- (quantity /(total.time.from.ancestor))[!same]
	
	# Set all NA values to 0
	Q[is.na(Q)]<-0
	
	diag(Q) <- - rowSums(Q)
	
	library(msm)
	for(i in 1:length(times)){
		m<-MatrixExp(Q, t=times[i])
		write(t(m),file=paste(outputfile,times[i],".txt"),append=FALSE,ncolumns=size,sep=",")
		for(j in 1:nrow(dat)){
			dat[j,paste("time=",times[i])]<-m[ancestor.index[j],descendant.index[j]]
		}
	}
	write.csv( dat, file=inputfile, append=FALSE, row.names=FALSE, quote=FALSE )
`;

my $amino_acid_to_codon = {
	'I' => ['ATT', 'ATC', 'ATA',],
	'L' => ['CTT', 'CTC', 'CTA', 'CTG', 'TTA', 'TTG', 'CT-',],
	'V' => ['GTT', 'GTC', 'GTA', 'GTG', 'GT-',],
	'F' => ['TTT', 'TTC',],
	'M' => ['ATG',],
	'C' => ['TGT', 'TGC',],
	'A' => ['GCT', 'GCC', 'GCA', 'GCG', 'GC-',],
	'G' => ['GGT', 'GGC', 'GGA', 'GGG', 'GG-',],
	'P' => ['CCT', 'CCC', 'CCA', 'CCG', 'CC-',],
	'T' => ['ACT', 'ACC', 'ACA', 'ACG', 'AC-',],
	'S' => ['TCT', 'TCC', 'TCA', 'TCG', 'AGT', 'AGC', 'TC-',],
	'Y' => ['TAT', 'TAC',],
	'W' => ['TGG',],
	'Q' => ['CAA', 'CAG',],
	'N' => ['AAT', 'AAC',],
	'H' => ['CAT', 'CAC',],
	'E' => ['GAA', 'GAG',],
	'D' => ['GAT', 'GAC',],
	'K' => ['AAA', 'AAG',],
	'R' => ['CGT', 'CGC', 'CGA', 'CGG', 'AGA', 'AGG', 'CG-',],
	'Z' => ['TAA', 'TAG', 'TGA',],
};

my $codon_to_amino_acid = {};

for my $amino_acid ( keys %$amino_acid_to_codon ){
	for my $codon ( @{$amino_acid_to_codon->{$amino_acid}} ){
		$codon_to_amino_acid->{$codon} = $amino_acid;
	}
}

my $script_directory = File::Spec->rel2abs( File::Basename::dirname( $0 ) );

our @ISA = qw(Exporter);
our @EXPORT_OK = qw(
	setup_variable
	setup_output
	setup_input
);

if( !caller() ){
	exit( &run( @ARGV ) );
}

sub run{
	
	my $log_file;
	my $log;
	my $help;
	my $verbose = 0;
	my $script_directory = &File::Basename::dirname( $0 );
	my $variables_file_input = File::Spec->catfile( $script_directory, 'vPhyloMM.vars.txt' );
	my %Pod2UsageParameters = (
		-message => "Try \"perldoc $0\" for more information.\n",
		-exitval => 0,
		-verbose => 1,
		-output  => \*STDERR,
	);
	
	# Parse arguments
	GetOptionsFromArray(
		\@_,
		"log=s" => \$log_file,
		"variables=s" => \$variables_file_input,
		"help" => \$help,
		"verbose=s" => \$verbose,
	) or pod2usage( \%Pod2UsageParameters );

	# print some help if needed
	if( 
		$help
	){
		pod2usage( \%Pod2UsageParameters );
	}
	
	# Figure out where the log output is going to
	if( $log_file ){
		$log_file = File::Spec->rel2abs( $log_file );
		if( !open $log, '>' . $log_file ){
			die "Error: The file \"$log_file\" could not be opened: $!";
		}
		else{
			print $log "Writing log to \"$log_file\"...\n" if $verbose;
		}
	}
	else{
		$log = \*STDERR;
		print $log "Writing log to standard error...\n" if $verbose;
	}
	
	$variables_file_input = File::Spec->rel2abs( $variables_file_input );
	say "using '$variables_file_input'" if $verbose;
	unless( -e $variables_file_input ){
		print $log "Error: vPhyloMM.pm can't find variables file '$variables_file_input'\n" if $verbose;
		return -1;
	}
	
	# create the gui object
	my $vPhyloMM_GUI = vPhyloMM_GUI->new(
		Variables_File_Input => $variables_file_input,
		Verbose => $verbose,
	);
	
	# run the gui
	my $gui_return_value = 0;
	if( ref( $vPhyloMM_GUI ) ne 'vPhyloMM_GUI' ){
		print $log "Error: vPhyloMM GUI could not be instantiated ($vPhyloMM_GUI)\n" if $verbose;
		return 1;
	}
	else{
		print $log "vPhyloMM_GUI instantiated\n" if $verbose > 1;
		$gui_return_value = $vPhyloMM_GUI->run( Log => $log, Verbose => $verbose );
	}
	
	print $log "Error: The GUI returned $gui_return_value\n" if $verbose and $gui_return_value;
	
	undef $log;
	
	return $gui_return_value;
	
}

sub new{
	
	my $class = shift;
	my %parameters = @_;
	my $self = {};
	
	bless $self, $class;
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		return -1;
	}; 
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log_File_Output}, \*STDERR ) and do{
		print STDERR "Error: vPhyloMM couldn't set up log output\n" if $verbose;
		return -1;
	};
	
	say $log "verbose is $verbose" if $verbose > 2;
	
	$self->setup_variable( \my $variables_file_input, $parameters{Variables_File_Input} );
	
	print $log "Instantiating new " . __PACKAGE__ . "...\n" if $verbose;
	
	if( $variables_file_input ){
		
		$self->set( 'Variables_File_Input', $variables_file_input );
		my $parse_variables_file_return = $self->parse_variables_file( Input => $variables_file_input, Verbose => $verbose, Log => $log );
		print $log "$parse_variables_file_return\n";
		
		if( $parse_variables_file_return ){
			print $log "Error: couldn't parse variables file '$variables_file_input' ($parse_variables_file_return)\n" if $verbose;
		}
		
		else{
			print $log "Finished parsing '$variables_file_input' ($parse_variables_file_return)\n" if $verbose;
		}
		
	}
	else{
		say $log "no variables file" if $verbose;
	}
	
	undef $log;
	
	return $self;
	
}

sub parse_variables_file{
	
	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_input( \my $input, $parameters{Input}, $self->{Variables_File_Input} );
	return -1 if $self->setup_log( \my $log, $parameters{Log}, $self->{Log_File_Output}, \*STDERR );

	say $log "parsing variables file ($verbose)" if $verbose;

	my %tree = (
		"Name" => "root",
		"Variables" => [],
		"Groups" => [],
	);
	my $current = \%tree;
	my @bread_crumbs;
	
	while ( my $line = <$input> ){
		chomp $line;
		$line =~ s/^\s*//;
		$line =~ s/\s*$//;
		next if !$line or $line =~ /^#/;
		if( $line =~ /<([^\/]+)>/ ) {
			my $name = $1;
			my %group = (
				"Name" => $name,
				"Variables" => [],
				"Groups" => [],
			);
			push @{$current->{"Groups"}}, \%group;
			push @bread_crumbs, $current;
			$current = \%group;
		}
		elsif( $line =~ /<\/(.+)>/ ){
			if( $1 ne $current->{"Name"} ){
				print $log "Error in variables file." if $verbose;
				return -1;
			}
			$current = pop @bread_crumbs;
		}
		elsif( $line =~ /^{/ ){
			my $hash_description = $line;
			while( $line !~ /}$/ and my $more = <$input> ){
				chomp $more;
				$more =~ s/^\s*//;
				$more =~ s/\s*$//;
				next if !$more or $more =~ /^#/;
				$hash_description .= $more;
				last if $more =~ /}$/;
			}
			my $variable_hash;
			$hash_description =~ s/\\/\\\\/g;
			eval( '$variable_hash = ' . $hash_description );
			my $name = $variable_hash->{"Name"};
			my $value = $variable_hash->{"Value"};
			my $path = "";
			for my $bread_crumb ( @bread_crumbs ){
				$path .= $bread_crumb->{"Name"} . "." ;
			}
			$path .= $current->{"Name"};
			$self->{$name} = $value;
			print $log "$name => $value\n" if $verbose > 1;
			push @{ $current->{"Variables"} }, $variable_hash;
		}
		else{
			print $log "Error in variables file." if $verbose;
			return -1;
		}
	}

	undef $input;
	
	$self->{Variables_Tree} = \%tree;
	
	print $log "done parsing variables file\n" if $verbose;
	
	undef $log;
	
	return 0;

}

sub print_variables{
	my $self = shift;
	my %parameters = @_;
	return -1 if $self->setup_output( \my $output, $parameters{Output}, \*STDOUT );
	map { print $output "$_ => " . $self->{$_} . "\n" ; } sort keys %$self;
	undef $output;
	return 0;
}

sub run_slidingminpd_serially{

	my $self = shift;
	my %parameters = @_;
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR ) and do { say STDERR "couldn't set up log"; return -1; };
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 1 ) and do { say $log "error: variable"; return -1; };
	$self->setup_variable( \my $DNA_directory, $parameters{DNA_Directory}, $self->{DNA_Directory} ) and do { say $log "error dna_directory"; return -1;};
	$self->setup_variable( \my $output_directory, $parameters{Output_Directory}, $self->{Output_Directory} ) and do { say $log "error output_directory"; return -1;};
	$self->setup_variable( \my $markers_file, $parameters{Markers_File_Input}, $self->{Markers_File_Input} );
	$self->setup_variable( \my $sminpd, $parameters{SlidingMinPD_File_Input}, $self->{SlidingMinPD_File_Input}  )  and do { say $log "error sminpd file input"; return -1;};
	$self->setup_variable( \my $refresh_function, $parameters{Refresh_Function}, );
	$DNA_directory = File::Spec->rel2abs( $DNA_directory );
	$output_directory = File::Spec->rel2abs( $output_directory );
	
	say $log "running Sliding MinPD in '$DNA_directory'..." if $verbose;
	
	if( ! -e $output_directory ){
		if( !mkdir $output_directory ){
			say $log "Error: couldn't create '$output_directory': $!";
			return -1;
		}
	}
	
	my $sminpd_output_directory = File::Spec->catfile( $output_directory, "Sliding MinPD" );
	
	if( ! -e $sminpd_output_directory ){
		if( !mkdir $sminpd_output_directory ){
			say $log "Error: couldn't create '$sminpd_output_directory': $!";
			return -1;
		}
		else{
			say $log "created $sminpd_output_directory" if $verbose;
		}
	}
	
	my %subdirectories = (
		clusters => '',
		transitions => '',
		distances => '',
		variables => '',
		logs => '',
	);
	
	map { $subdirectories{$_} = File::Spec->catfile( $sminpd_output_directory, $_ ); } keys %subdirectories;
	for my $subdirectory ( keys %subdirectories ){
		if( ! -e $subdirectories{$subdirectory} ){
			if( !mkdir $subdirectories{$subdirectory} ){
				say "Error: couldn't create directory '$subdirectories{$subdirectory}': $!" if $verbose;
				return -1;
			}
			else{
				say $log "created $subdirectories{$subdirectory} " if $verbose;
			}
		}
	}

	if( $markers_file ){
		$markers_file = File::Spec->rel2abs( $markers_file );
	}
	else{
		$markers_file = '';
	}
	
	my %substitution_table = (
		'<FASTA>' => '',
		'<REPORT_DISTANCES>' => $self->{Report_Distances},
		'<ACTIVATE_RECOMBINATION_DETECTION>' => $self->{Activate_Recombination},
		'<RECOMBINATION_DETECTION_OPTION>' => $self->{Recombination_Detection_Option},
		'<CROSSOVER_OPTION>' => $self->{Crossover_Option},
		'<PCC_THRESHOLD>' => $self->{PCC_Threshold},
		'<WINDOW_SIZE>' => $self->{Window_Size},
		'<STEP_SIZE>' => $self->{Step_Size},
		'<BOOTSTRAP_TIEBREAKER>' => $self->{Tie_Breaker},
		'<BOOTSCAN_SEED>' => $self->{Bootscan_Seed},
		'<BOOTSCAN_THRESHOLD>' => $self->{Bootscan_Threshold},
		'<BOOTSTRAP_BOOTKNIFE>' => $self->{Bootstrap_Bootknife},
		'<SUBSITUTION_MODEL>' => $self->{Substitution_Model},
		'<GAMMA_SHAPE>' => $self->{Gamma_Shape},
		'<SHOW_BOOTSTRAP_VALUES>' => $self->{ShowBootstrap_Values},
		'<CODON>' => $markers_file,
		'<CLUSTERING>' => $self->{Clustering},
		'<CLUSTERING_THRESHOLD>' => $self->{Clustering_Threshold},
		'<SMINPD_OUTPUT_DIRECTORY>' => $sminpd_output_directory,
	);
	opendir DNA_DIRECTORY, $DNA_directory or do{
		say $log "Error: could not open '$DNA_directory': $!" if $verbose;
		return -1;	
	};
	while( my $base = readdir DNA_DIRECTORY ){
		my $file = File::Spec->catfile( $DNA_directory, $base );
		next if -d $file || $base !~ /^([^\.]+)\./;
		say $log "running Sliding MinPD on $file" if $verbose;
		my $patient = $1;
		my $log_file = File::Spec->catfile( $subdirectories{logs}, "$patient.log.txt" );
		my $unique_variables_file = File::Spec->catfile( $subdirectories{variables}, "$patient.variables.txt" );
		$substitution_table{'<FASTA>'} = $file;
		open( UNIQUE_VARIABLES_FILE, '>'.$unique_variables_file ) or do{
			say $log "Error: Couldn't open \"$unique_variables_file\" : $!" if $verbose;
			return -1;
		};
		for my $line ( split /\n/, $sminpd_variables_template ){
			for my $PlaceMarker ( keys %substitution_table ){
				$line =~ s/${PlaceMarker}/$substitution_table{$PlaceMarker}/g;
			}
			say UNIQUE_VARIABLES_FILE $line ;
		}
		close UNIQUE_VARIABLES_FILE ;
		say $log "began patient: $patient File: \"$file\"" if $verbose;
		my $command = "\"$sminpd\" < \"$unique_variables_file\" >\"$log_file\" 2>&1";
		say $log "$command" if $verbose;
		if( $refresh_function ){
			return -1 if &$refresh_function();
		}
		`$command`;
		move( File::Spec->catfile( $sminpd_output_directory, $base . ".clust.txt" ), $subdirectories{clusters} );
		move( File::Spec->catfile( $sminpd_output_directory, $base . ".d.txt" ), $subdirectories{distances} );
		move( File::Spec->catfile( $sminpd_output_directory, $base . ".transitions.txt" ), $subdirectories{transitions} );
	}
	closedir DNA_DIRECTORY;
	say $log "done running Sliding MinPD on all patients" if $verbose;
	undef $log;
	return 0;
}

sub load_sequences{
	
	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_log( \my $log, $parameters{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $gap_character, $parameters{Gap_Character}, $self->{Gap_Character}, '-' );
	return -1 if $self->setup_variable( \my $dna_directory, $parameters{DNA_Directory}, $self->{DNA_Directory} );
	return -1 if $self->setup_variable( \my $format, $parameters{DNA_Input_Format}, $self->{DNA_Input_Format}, 'FASTA' );
	
	my $binaries = {};
	my $sequences = {};
	my $positions = {};
	
	$self->parse_markers_file( Verbose => $verbose, Log => $log ) if !$self->{Markers};
	my $markers = $self->{Markers};
	return -1 if !$markers;
	
	if( -d File::Spec->rel2abs( $dna_directory ) ){
		$dna_directory = File::Spec->rel2abs( $dna_directory );
		opendir DNA_Directory, $dna_directory or do{ print $log "Error: Couldn't open \"$dna_directory\" : $!\n"; return -1; };
		$dna_directory = [ map { File::Spec->catfile( $dna_directory, $_ ) } readdir DNA_Directory ];
		closedir DNA_Directory;
		$dna_directory = [ map { File::Spec->rel2abs($_); } @$dna_directory ];
		$dna_directory = [ grep { !( -d $_ )} @$dna_directory ];
	}
	else{
		$dna_directory = [ Tkx::SplitList($dna_directory) ];
		$dna_directory = [ map { File::Spec->rel2abs($_); } @$dna_directory ];
		$dna_directory = [ grep { !( -d $_ )} @$dna_directory ];
	}
	
	say $log "loading DNA sequences..." if $verbose;
	
	for my $dna_file ( @$dna_directory ) {
		next if -d $dna_file;
		my $basename = basename($dna_file);
		$basename =~ /^(\d+)\./;
		my $patient = $1;
		say $log "\t$dna_file" if $verbose;
		my %sequences = ();
		my $seq_in = Bio::SeqIO->new(
			-format => $format,
			-file   => $dna_file,
		);
		
		while ( my $seq = $seq_in->next_seq() ) {
			if ( $seq->alphabet ne 'dna' ) {
				print $log "Error: Found non-DNA sequence in file \"$dna_file\"\n";
				next;
			}
			my $display_id = $seq->display_id();
			my $sequence = $seq->seq;
			for my $i ( 0 .. $#{$markers} ){
				my $nucleotide_position = $markers->[$i]->{Position};
				my $amino_acid_position = $nucleotide_position / 3;
				my @codon;
				for my $j ( 0 .. 2 ){
					my $nucleotide = substr $sequence, $nucleotide_position + $j, 1;
					push @codon, $nucleotide ? $nucleotide : $gap_character;
				}
				$sequences->{$patient}{$display_id}{DNA}{$nucleotide_position} = join '', @codon;
				$sequences->{$patient}{$display_id}{Protein}{ $amino_acid_position } = $codon_to_amino_acid->{join '', @codon};
			}
			my $id = "$patient.$display_id";
			$binaries->{$patient}{$display_id} = [];
			$positions->{$patient}{$display_id} = [];
			$self->convert_amino_acid_sequence(
				AminoAcidSequence => $sequences->{$patient}{$display_id}{DNA},
				ID => $id,
				Binaries => $binaries->{$patient}{$display_id},
				Positions => $positions->{$patient}{$display_id},
			);
			say join ',', ($id, join('', @{$binaries->{$patient}{$display_id}}), join('', @{$positions->{$patient}{$display_id}})) if $verbose > 2;
		}
	}

	$self->{Binaries} = $binaries;
	$self->{Positions} = $positions;
	$self->{Sequences} = $sequences;

	say $log "done loading sequences" if $verbose;
	
	undef $log;
	return 0;
	
}

sub get_sequence_count{
	my $self = shift;
	my $sequences;
	my $count = 0;
	$self->load_sequences if !$self->{"Sequences"};
	$sequences = $self->{"Sequences"};
	die if !$sequences;
	for my $patient ( keys %$sequences ){
		$count += scalar keys %{$sequences->{$patient}};
	}
	return $count;
}

sub load_transitions{
	
	my $self = shift;
	my %parameters = @_;
	my $transitions = {};
	
	$self->setup_variable( \my $verbose, $self->{Verbose}, 0 ) and do{
		return -1;
	};
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR ) and do{
		say STDERR "Error: could not set up a log file for writing" if $verbose;
		return -1;
	};
	
	$self->setup_variable( \my $transitions_directory, $parameters{Transition_Directory}, $self->{Transitions_Directory}, File::Spec->catfile( $self->{SlidingMinPD_Directory}, "transitions" ) ) and do{
		say $log "Error: could not figure out which directory contains sminpd transitions" if $verbose;
		return -1;
	};
	
	say $log "loading transitions..." if $verbose;
	
	opendir TRANSITIONS_DIRECTORY, $transitions_directory or do{
		say $log "Error: couldn't open '$transitions_directory': $!" if $verbose;
		return -1;
	};
	
	while( my $base = readdir TRANSITIONS_DIRECTORY ){
		next if $base !~ /^(\d+)\./;
		my $patient_id = $1;
		my $file = File::Spec->catdir( $transitions_directory, $base );
		next if -d $file;
		open FILE, $file or return $!;
		while( my $line = <FILE> ){
			chomp $line;
			next if $line !~ /^\d+/;
			my( $descendant, $ancestor, $distance, $bootstrap ) = split /;/, $line;
			$transitions->{$patient_id}{$ancestor}{$descendant}{Bootstrap} = $bootstrap;
			#say join ',', ($patient_id, $ancestor, $descendant, $bootstrap );
		}
		close FILE;
	}
	close TRANSITIONS_DIRECTORY;

	$self->{Transitions} = $transitions;
	
	say $log "done loading transitions." if $verbose;
	
	undef $log;
	
	return 0;
	
}

sub load_distances{
	
	my $self = shift;
	my $verbose = $self->{verbose};
	my $distances_directory = $self->{Distances_Directory};
	my $distances;
	
	opendir DISTANCES_DIRECTORY, $distances_directory or die $!;
	
	while( my $base = readdir DISTANCES_DIRECTORY ){
		next if $base !~ /(\d+)\./;
		my $patient_id = $1;
		my $file = File::Spec->catdir( $distances_directory, $base );
		next if -d $file;
		open FILE, $file or die $!;
		while( my $line = <FILE> ){
			chomp $line;
			my( $descendant, $ancestor, $distance ) = split / /, $line;
			$distances->{$patient_id}{$ancestor}{$descendant} = $distance;
		}
		close FILE;
	}
	close DISTANCES_DIRECTORY;
	
	$self->{Distances} = $distances;
	
	return 0;
	
}

sub load_clusters{
	
	my $self = shift;
	my %parameters = @_;
	my $log;
	$self->setup_output( \$log, $parameters{Log}, \*STDERR );
	my $verbose;
	my $clusters_directory = File::Spec->catfile( $self->{SlidingMinPD_Directory}, "clusters");
	my $clusters;
	
	if( opendir CLUSTERS_DIRECTORY, $clusters_directory ){
		print $log "loading clusters...\n" if $verbose;
		while( my $base = readdir CLUSTERS_DIRECTORY ){
			next if $base !~ /^(\d+)\./;
			my $patient_id = $1;
			my $file = File::Spec->catfile( $clusters_directory, $base );
			open FILE, $file or die $!;
				while( my $line = <FILE> ){
					chomp $line;
					my @values = split /,/, $line;
					map { $_ =~ s/[\(\)\[\]]//g } @values;
					my $descendant = shift @values;
					$clusters->{$patient_id}{$descendant} = \@values;
				}
			close FILE;
		}
		closedir CLUSTERS_DIRECTORY;
	}
	if( !keys %$clusters ){
		say "loading clusters from transitions";
		my $transitions = $self->{Transitions};
		for my $patient ( keys %$transitions ){
			for my $ancestor ( keys %{$transitions->{$patient}} ){
				for my $descendant ( keys %{$transitions->{$patient}{$ancestor}} ){
					push @{$clusters->{$patient}{$descendant}}, [$ancestor,];
				}
			}
		}
	}
	
	$self->{Clusters} = $clusters;
	
	print $log "done loading clusters\n" if $verbose;
	
	undef $log;
	return 0;
	
}

sub convert_amino_acid_sequence{
	
	my $amino_acid_sequence;
	my $markers;
	my $offset;
	my $id;
	my $separator;
	my $binaries;
	my $positions;
	my $strict;
	my $verbose;
	my $self;
	my $ref = ref $_[0];
	my %parameters;

	if( $ref->isa( __PACKAGE__ ) ){
		$self = shift;
		$strict = $self->{"Strict"};
		$offset = $self->{"Offset"};
		$verbose = $self->{"Verbose"};
		$markers = $self->{"Markers"};
	}

	%parameters = @_;
	
	$amino_acid_sequence = $parameters{"AminoAcidSequence"} if $parameters{"AminoAcidSequence"};
	$markers = $parameters{"Markers"} if $parameters{"Markers"};
	$offset = 0;
	$id = $parameters{"ID"} if $parameters{"ID"};
	$binaries = $parameters{"Binaries"};
	$positions = $parameters{"Positions"};
	$strict = $parameters{"Strict"} if $parameters{"Strict"};
	$verbose = $parameters{"Verbose"} if $parameters{"Verbose"};
		
	if( !$markers ){
		$self->parse_markers_file();
		$markers = $self->{"Markers"};
	}
	
	for my $i ( 0 .. $#{$markers} ){
		
		my $amino_acid;

		if( ref $amino_acid_sequence eq 'HASH'){
			$amino_acid = $amino_acid_sequence->{  ( $markers->[$i]->{"Position"} + $offset ) };
			$amino_acid = $codon_to_amino_acid->{$amino_acid};
		}
		else{
			$amino_acid = substr( $amino_acid_sequence, $markers->[$i]->{"Position"} + $offset, 1 );
		}
		if( !$amino_acid ){
			die "There is no amino acid at " . $markers->[$i]->{"Position"} + $offset . " in sequence $id.\n" if $strict;
			push @$binaries, 0 if $binaries;
			next;
		}
		my $Needle = quotemeta( $amino_acid );
		if( grep /$Needle/, @{$markers->[$i]->{"AminoAcids"}} ){
			push @$binaries, 1 if $binaries;
			push @$positions, $i + 1 if $positions;
		}
		else{
			push @$binaries, 0 if $binaries;
		}
	}
	if( scalar @$positions == 0 and $positions ){
		push @$positions, 0;
	}
	return 1;
}

sub parse_markers_file{
	
	my $self = shift;
	my %parameters = @_;
	my $markers = [];
	my $positions = {};
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do {
		return -1;
	};
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR ) and do {
		say STDERR "Error: could not set up log" if $verbose;
		return -1;
	};
	
	$self->setup_variable( \my $markers_file, $parameters{Markers_File_Input}, $self->{Markers_File_Input}, \*STDERR ) and do {
		say $log "Error: could not set up variable" if $verbose;
		return -1;
	};

	$markers_file = File::Spec->rel2abs( $markers_file );
	if( !-e $markers_file ){
		say $log "Error: '$markers_file' doesn't exist" if $verbose;
		return -1;
	}
	
	say $log "Reading markers from '$markers_file'" if $verbose;
	
	open MARKERS_FILE, $markers_file or do{
		say $log "Error: Couldn't open \"$markers_file\": $!" if $verbose;
		return -1;
	};

	my $marker_index = 0;
	while ( my $line = <MARKERS_FILE> ) {
		chomp $line;
		$line =~ s/#.*//;
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;
		
		next if $line !~ /^(\d+),(.+)/;
		
		my $offset = $1;
		my @array_of_markers = split /,/, $2;
		for my $i ( 0 .. $#array_of_markers ){
			next if $array_of_markers[$i] !~ /^[^\d]?(\d+)([^\d]+)$/;
			my $marker_info = {
				Position => (( $1 + $offset ) * 3),
				AminoAcids => [ split( /\\/, $2 ) ],
			};
			push @$markers, $marker_info;
			push @{$positions->{$marker_info->{Position}}}, $marker_index + 1;
			my $amino_acids = join ',', @{ $marker_info->{AminoAcids} } ;
			my $position =  $marker_info->{Position};
			say $log "\tMarker " . ( $marker_index + 1 ) . ": $amino_acids at position $position";
			$marker_index++;
		}
		
	}
	map { $positions->{$_} = [ sort @{$positions->{$_}} ] } keys %$positions;

	close MARKERS_FILE;
	#die;
	$self->{Markers} = $markers;
	$self->{Marker_Positions} = $positions;
	
	say $log "done parsing markers file" if $verbose;
	undef $log;
	
	return 0;
	
}

sub write_binaries{
	
	my $self = shift;
	my $binaries_directory = $self->{"Binaries_Directory"};
	my $binary_suffix = $self->{"Binary_Suffix"};
	my $binaries;
		
	if( !$self->{"Binaries"} ){
		$self->load_sequences();
	}

	$binaries = $self->{"Binaries"};

	mkdir $binaries_directory;
	-d $binaries_directory or die $!;
	
	for my $patient ( sort keys %$binaries ){
		my $file = File::Spec->catfile( $binaries_directory, "$patient.$binary_suffix" );
		open FILE, ">$file" or die $!;
		for my $display_id ( sort keys %{ $binaries->{$patient} } ){
			my $binary = join '', @{$binaries->{$patient}{$display_id}};
			print FILE "$display_id,$binary\n";
		}
		close FILE,
	}
	
	return 0;
	
}

sub load_binaries_from_directory{
	
	my $self = shift;
	my %parameters = @_;
	my %binaries;
	my %positions;
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 1 ) and do{
		return -1;
	};
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log_File_Output}, \*STDERR ) and do{
		say STDERR "Error: could not set up log" if $verbose;
		return -1;
	};
	
	$self->setup_variable( \my $binaries_directory, $parameters{Binaries_Directory}, $self->{Binaries_Directory} ) and do{
		say $log "Error: could not figure out which directory contains binary sequences" if $verbose;
		return -1;
	};
	
	$binaries_directory = File::Spec->rel2abs( $binaries_directory );
	
	say $log "loading binaries from '$binaries_directory'..." if $verbose;
	
	opendir BINARIES_DIRECTORY, $self->{Binaries_Directory} or do{
		say $log "Error: could not open directory '$binaries_directory':$!";
		return -1;
	};
	
	while( my $base = readdir BINARIES_DIRECTORY ){
		my $file = File::Spec->catfile( $self->{Binaries_Directory}, $base );
		next if -d $file;
		next if $base !~ /^(\d+)/;
		my $patient = $1;
		open FILE, $file or return $!;
		while( my $line = <FILE> ){
			chomp $line;
			next if $line !~ /(.+),(.+)/;
			my $display_id = $1;
			my $binary = $2;
			$binaries{$patient}{$display_id} = [ split //, $binary ];
			$positions{$patient}{$display_id} = $self->binary_to_positions( Binary => $binary );
		}
		close FILE;
	}
	closedir BINARIES_DIRECTORY;
	
	$self->{Binaries} = \%binaries;
	$self->{Positions} = \%positions;
	
	say $log "done loading binaries from '$binaries_directory'";
	
	undef $log;
	
	return 0;
	
}

sub write_transitions{

	my $self = shift;
	my %parameters = @_;
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		return -1;
	};
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR ) and do{
		say STDERR "Error: couldn't set up output" if $verbose;
		return -1;
	};
	
	$self->setup_output( \my $output, $parameters{Output}, $self->{Output}, \*STDOUT ) and do{
		say $log "Error: couldn't set up output" if $verbose;
		return -1;
	};
	
	$self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 ) and do{
		say $log "Error: couldn't set up variable" if $verbose;
		return -1;
	};
	
	$self->setup_variable( \my $bootstrap_threshold, $parameters{Bootstrap_Threshold}, $self->{Bootstrap_Threshold}, 0 ) and do{
		say $log "Error: couldn't set up variable" if $verbose;
		return -1;
	};
	

	$self->load_transitions( Verbose => $verbose, Log => $log ) if !$self->{Transitions};
	my $transitions = $self->{Transitions};
	if( !$transitions ){
		say $log "Error: Couldn't find transitions." if $verbose;
		return -1;
	}
	
	$self->load_sequences( Verbose => $verbose, Log => $log ) if !$self->{Binaries};
	my $binaries = $self->{Binaries};
	if( !$binaries ){
		say $log "Error: Couldn't find binaries." if $verbose;
		return -1;
	}
	
	say "the number of binary sequences is " . scalar keys %$binaries;
	for my $patient ( keys %$binaries ){
		say "patient $patient";
		for my $id ( keys %{$binaries->{$patient}} ){
			say "\t$id";
			say "\t\t" . join '', @{$binaries->{$patient}{$id}};
		}
	}
	
	$verbose = 5;
	$log = \*STDOUT;
	say $log "writing transitions..." if $verbose;
	say "writing transitions...";
	my @header_values = qw(
		descendant_binary
		ancestor_binary
		time_difference
		descendant_time
		ancestor_time
		patient_id
		ancestor
	);
	say $output join ',', @header_values if $headers;
	for my $patient ( sort keys %$transitions ){
		say $log "\tpatient $patient" if $verbose > 1;
		for my $ancestor ( sort keys %{$transitions->{$patient}} ){
			for my $descendant ( sort keys %{$transitions->{$patient}{$ancestor}} ){
				my $bootstrap = $transitions->{$patient}{$ancestor}{$descendant}{Bootstrap};
				if( $bootstrap < $bootstrap_threshold ){
					say $log "\t\tskipping because $bootstrap < $bootstrap_threshold" if $verbose > 1;
					next;
				}
				say "$patient $ancestor";
				my $ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
				my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
				say "\t$ancestor_binary $descendant_binary";
				my $ancestor_time = [split /\./, $ancestor]->[0];
				my $descendant_time = [split /\./, $descendant]->[0];
				my $time_difference = $descendant_time - $ancestor_time;
				my @row_values = (
					$descendant_binary,
					$ancestor_binary,
					$time_difference,
					$descendant_time,
					$ancestor_time,
					$patient,
					$ancestor,
				);
				say $output join ',', @row_values;
			}
		}
	}
	
	say $log "done writing transitions" if $verbose;
	
	undef $output;
	undef $log;
	
	return 0;
	
}

sub write_aggregates{
	
	my $self = shift;
	my %parameters = @_;
	
	return 1 if $self->setup_output( \my $output, $parameters{Output}, $self->{Aggregates_File_Output}, \*STDOUT );
	return 1 if $self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_variable( \my $bootstrap_threshold, $parameters{Bootstrap_Threshold}, $self->{Bootstrap_Threshold}, 0 );

	$self->load_transitions() if !$self->{Transitions};
	my $transitions = $self->{Transitions};
	return -1 if !$transitions;
	
	$self->load_sequences() if !$self->{Binaries};
	my $binaries = $self->{Binaries};
	return -1 if !$binaries;
	
	print $log "writing aggregates...\n" if $verbose;
	
	my @header_values = (
		'descendant binary',
		'ancestor binary',
		'time difference',
		'descendant time',
		'ancestor time',
		'patient id',
		'ancestor',
		'count',
	);
	print $output join( ',', @header_values ) . "\n" if $headers;
	
	for my $patient ( sort keys %$transitions ){
		print $log "\tpatient $patient\n" if $verbose > 1;
		my %aggregates;
		for my $ancestor ( sort keys %{$transitions->{$patient}} ){
			for my $descendant ( sort keys %{$transitions->{$patient}{$ancestor}} ){
				my $bootstrap = $transitions->{$patient}{$ancestor}{$descendant}{Bootstrap};
				if( defined( $bootstrap ) && defined( $bootstrap_threshold ) ){
					next if $bootstrap < $bootstrap_threshold;
				}
				my $ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
				my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
				my $ancestor_time = [split /\./, $ancestor]->[0];
				my $descendant_time = [split /\./, $descendant]->[0];
				my $time_difference = $descendant_time - $ancestor_time;
				my @row_values = (
					$descendant_binary,
					$ancestor_binary,
					$time_difference,
					$descendant_time,
					$ancestor_time,
					$patient,
					$ancestor,
				);
				$aggregates{ join ',', @row_values }++;
			}
		}
		
		for my $row ( sort keys %aggregates ){
			my $count = $aggregates{$row};
			print $output $row . ",$count\n";
		}
		
	}
	
	print $log "done writing aggregates\n";
	
	undef $output;
	undef $log;
	
	return 0;
	
}

sub calculate_summary{
	
	my $self = shift;
	my %parameters = @_;

	return -1 if $self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_variable( \my $bootstrap_threshold, $parameters{Bootstrap_Threshold}, $self->{Bootstrap_Threshold}, 0 );
	return -1 if $self->setup_variable( \my $aggregate, $parameters{Aggregate}, $self->{Aggregate}, 1 );
	
	my $transitions;
	my $binaries;
	my $summary = {};
	$self->load_transitions( Log => $log, Verbose => $verbose ) if !$self->{Transitions};
	$self->load_sequences( Log => $log, Verbose => $verbose ) if !$self->{Binaries};
	$transitions = $self->{Transitions};
	$binaries = $self->{Binaries};
	
	print $log "calculating summary...\n" if $verbose;
	
	for my $patient ( sort keys %$transitions ){
		my %aggregates;
		for my $ancestor ( sort keys %{$transitions->{$patient}} ){
			for my $descendant ( sort keys %{$transitions->{$patient}{$ancestor}} ){
				my $bootstrap = $transitions->{$patient}{$ancestor}{$descendant}{Bootstrap};
				if( $bootstrap < $bootstrap_threshold ){
					say $log "skipping $patient.$ancestor because $bootstrap < $bootstrap_threshold ($verbose)" if $verbose > 1;
					next;
				}
				else{
					say $log "not skipping $patient.$ancestor because $bootstrap >= $bootstrap_threshold ($verbose)" if $verbose > 1;
				}
				my $ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
				my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
				my $ancestor_time = (split /\./, $ancestor)[0];
				my $descendant_time = (split /\./, $descendant)[0];
				my $time_difference = $descendant_time - $ancestor_time;
				my @row_values = (
					$descendant_binary,
					$ancestor_binary,
					$time_difference,
					$descendant_time,
					$ancestor_time,
					$patient,
					$ancestor,
				);
				push @row_values, $descendant if !$aggregate;
				$aggregates{ join ',', @row_values }++;
			}
		}
		for my $row ( sort keys %aggregates ){
			my( $descendant_binary, $ancestor_binary, $time ) = split /,/, $row;
			$summary->{$ancestor_binary}{$descendant_binary}{Count}++;
			$summary->{$ancestor_binary}{$descendant_binary}{Time} += $time;
		}
	}
	
	$self->{Summary} = $summary;
	
	print $log "done calculating summary...\n" if $verbose;
	
	undef $log;
	return 0;
	
}

sub write_summary{
	
	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR );
	return -1 if $self->setup_output( \my $output, $parameters{Output}, $self->{Output}, \*STDOUT );
	return -1 if $self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_variable( \my $bootstrap_threshold, $parameters{Bootstrap_Threshold}, $self->{Bootstrap_Threshold}, 0 );
	return -1 if $self->setup_variable( \my $aggregate, $parameters{Aggregate}, $self->{Aggregate}, 1 );
	
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );

	if( !$summary ){
		$self->calculate_summary() if !$self->{Summary};
		$summary = $self->{Summary} if $self->{Summary};
	}
	return -1 if !$summary;
	
	print $log "writing summary...\n" if $verbose;

	my @header_values = (
		'descendant binary',
		'ancestor binary',
		'time',
		'count',
	);
	print $output join( ',', @header_values ) . "\n" if $headers;
	
	for my $ancestor_binary ( sort keys %$summary ){
		for my $descendant_binary ( sort keys %{$summary->{$ancestor_binary}} ){
			my $count = $summary->{$ancestor_binary}{$descendant_binary}{Count};
			my $time = $summary->{$ancestor_binary}{$descendant_binary}{Time};
			my @row_values = (
				$descendant_binary,
				$ancestor_binary,
				$time,
				$count,
			);
			print $output join( ',', @row_values ) . "\n";
		}
	}
	
	print $log "done writing summary\n" if $verbose;
	
	undef $log;
	undef $output;
	
	return 0;
	
}

sub setup_output{
	my $self = shift;
	unless( ref( $self ) eq __PACKAGE__ || $self eq __PACKAGE__ ){
		unshift @_, $self;
	}
	my $old_output = shift or return -1;
	my $new_output = "";
	for my $arg ( @_ ){
		if( $arg ){
			$new_output = $arg;
			last;
		}
	}
	return -1 if !$new_output;

	if( ref( $new_output ) eq 'GLOB' ){
		$$old_output = $new_output;
	}
	else{
		open $$old_output, ">" . $new_output or die $!;
	}
	
	return 0;
	
}

sub setup_log{
	my $self = shift;
	unless( ref( $self ) eq __PACKAGE__ || $self eq __PACKAGE__ ){
		unshift @_, $self;
	}
	my $old_output = shift or return -1;
	my $new_output = "";
	for my $arg ( @_ ){
		if( $arg ){
			$new_output = $arg;
			last;
		}
	}
	return -1 if !$new_output;

	if( ref( $new_output ) eq 'GLOB' ){
		$$old_output = $new_output;
	}
	else{
		open $$old_output, ">>" . $new_output or die $!;
	}
	
	return 0;
	
}

sub setup_input{
	
	my $self = shift;

	unless( ref( $self ) eq __PACKAGE__ || $self eq __PACKAGE__ ){
		unshift @_, $self;
	}

	my $old_input = shift or return -1;
	my $new_input = "";
	for my $arg ( @_ ){
		if( $arg ){
			$new_input = $arg;
			last;
		}
	}
	return -1 if !$new_input;

	if( ref( $new_input ) eq 'GLOB' ){
		$$old_input = $new_input;
	}
	else{
		open $$old_input, "<" . $new_input or die $!;
	}
	
	return 0;
	
}

sub summary_from_transitions{
	
	my $self = shift;
	my %parameters = @_;
	my $input;
	my $output;
	my $log;
	my %aggregates;
	my %summary;
	my $headers = exists $self->{Headers} ? $self->{Headers} : 1;
	
	return -1 if $self->setup_input( \$input, $parameters{Input}, \*STDIN );
	return -1 if $self->setup_output( \$output, $parameters{Output}, \*STDOUT );
	return -1 if $self->setup_output( \$log, $parameters{Log}, \*STDERR );
	
	my @header_values = (
		'descendant binary',
		'ancestor binary',
		'time difference',
		'descendant time',
		'ancestor time',
		'patient id',
		'ancestor',
		'count',
	);
	print $output join( ',', @header_values ) . "\n" if $headers;
	
	my $header = <$input> if $headers;
	
	while( my $line = <$input> ){
		chomp $line;
		$aggregates{$line}++;
	}
	
	for my $line ( keys %aggregates ){
		my( $descendant, $ancestor, $time ) = split /,/, $line;
		my $count = $aggregates{$line};
		print $output "$line,$count\n";
		$summary{$ancestor}{$descendant}{Time} += $time;
		$summary{$ancestor}{$descendant}{Count}++;
	}
	
	$self->{Summary} = \%summary;
	
	undef $log;
	undef $input;
	undef $output;
	
	return 0;
	
}

sub summary_from_aggregates{
	
	my $self = shift;
	my %parameters = @_;
	my $input;
	my $output;
	my $log;
	my %aggregates;
	my %summary;
	my $headers = exists $self->{Headers} ? $self->{Headers} : 1;
	
	return -1 if $self->setup_input( \$input, $parameters{Input}, \*STDIN );
	return -1 if $self->setup_output( \$output, $parameters{Output}, \*STDOUT );
	return -1 if $self->setup_log( \$log, $parameters{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $positions, $parameters{Positions}, 0 );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, 0 );
	
	say $log "reading in aggregates file" if $verbose;
	
	my $header = <$input> if $headers;
	
	while( my $line = <$input> ){
		chomp $line;
		my( $descendant, $ancestor, $time ) = split /,/, $line;
		$summary{$ancestor}{$descendant}{Time} += $time;
		$summary{$ancestor}{$descendant}{Count}++;
	}

	if( $headers ){
		if( $positions ){
			say $output join ',', qw(
				descendant.positions
				ancestor.positions
				time
				count
			);
		}
		else{
			say $output join ',', qw(
				descendant.binary
				ancestor.binary
				time
				count
			);
		}
	}
	
	for my $ancestor_binary ( sort keys %summary ){
		my $ancestor_positions = join '', @{$self->binary_to_positions( Binary => $ancestor_binary )};
		for my $descendant_binary ( sort keys %{$summary{$ancestor_binary}} ){
			my $descendant_positions = join '', @{$self->binary_to_positions( Binary => $descendant_binary )};
			my $count = $summary{$ancestor_binary}{$descendant_binary}{Count};
			my $time = $summary{$ancestor_binary}{$descendant_binary}{Time};
			if( $positions ){
				say $output join ',', (
					$descendant_positions,
					$ancestor_positions,
					$time,
					$count
				);
			}
			else{
				say $output join ',', (
					$descendant_binary,
					$ancestor_binary,
					$time,
					$count,
				);
			}
		}
	}
	
	$self->{Summary} = \%summary;
	
	undef $log;
	undef $input;
	undef $output;
	
	return 0;
	
}

sub summary_from_summary{
	
	my $self = shift;
	my %parameters = @_;
	my $input;
	my $output;
	my $log;
	my %summary;
	my $headers = exists $self->{Headers} ? $self->{Headers} : 1;
	
	return -1 if $self->setup_input( \$input, $parameters{Input}, \*STDIN );
	return -1 if $self->setup_output( \$output, $parameters{Output}, \*STDOUT );
	return -1 if $self->setup_log( \$log, $parameters{Log}, \*STDERR );
	

	my $header = <$input> if $headers;
	
	while( my $line = <$input> ){
		chomp $line;
		my( $descendant_binary, $ancestor_binary, $time, $count ) = split /,/, $line;
		$summary{$ancestor_binary}{$descendant_binary}{Time} = $time;
		$summary{$ancestor_binary}{$descendant_binary}{Count} = $count;
	}
	
	$self->{Summary} = \%summary;
	
	undef $log;
	undef $input;
	undef $output;
	
	return 0;
	
}

sub get_pruned_r_data{
	my $self = shift;
	$self->prune_summary() if !$self->{Pruned_Summary};
	my %rdata = ();
	my $return = $self->get_r_data(
		Summary => $self->{Pruned_Summary},
		RData => \%rdata,
	);
	if( $return ){
		die "return $return";	
	}
	if( keys %rdata ){
		$self->{Pruned_R_Data} = \%rdata;
		say scalar keys %rdata;
		return 0;	
	}
	else{
		return -1;	
	}
}

sub get_r_data{
	
	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_log( \my $log, $parameters{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 );
	return -1 if $self->setup_variable( \my $times, $parameters{Times}, $self->{Times}, "1,8,16,20" );
	return -1 if $self->setup_variable( \my $r_path, $parameters{R_Path_File_Input}, $self->{R_Path_File_Input}, 'R' );
	return -1 if $self->setup_variable( \my $directory, $parameters{Output_Directory}, $self->{Output_Directory}, File::Spec->rel2abs( $script_directory ) );
	#return -1 if $self->setup_variable( \my $r_script, $parameters{R_Script_File_Input}, $self->{R_Script_File_Input} );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_variable( \my $pmatrix, $parameters{PMatrix}, $self->{PMatrix}, 'PMatrix' );
	return -1 if $self->setup_variable( \my $pruned, $parameters{Pruned}, 0 );
	
	$self->setup_variable( \my $rdata, $parameters{RData}, $self->{RData} );
	$self->setup_variable( \my $p, $parameters{P}, $self->{P} );
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );
	
	$directory = File::Spec->catfile($directory, "MM");
	$directory = File::Spec->rel2abs( $directory );
	if( !-e $directory ){
		if( !mkdir $directory ){
			say "couldn't create directory '$directory'";
			return -1;
		}
	}
	my $r_script = File::Spec->catfile( $directory, "run_matrix.r" );
	open TEMP_R, ">$r_script" or do{ say "couldn't open temp"; return -1; };
	say TEMP_R $run_matrix_script;
	close TEMP_R;
	
	print $log "getting data from R...\n" if $verbose;
	say $log "using '$r_script'" if $verbose;

	$directory = File::Spec->rel2abs( $directory );
	$pmatrix = File::Spec->catfile( $directory, $pmatrix );
	
	%$rdata = ();
	%$p = ();
	
	if( !$summary || 'HASH' ne ref( $summary ) || !keys( %$summary ) ){
		if( !$self->calculate_summary( Log => $log, Verbose => $verbose ) || !$self->{Summary} || 'HASH' ne ref( $self->{Summary} ) || !keys( %{$self->{Summary}} ) ){
			say $log "Error: could not get summary";
			return -1;
		}
		$summary = $self->{Summary};
	}
	
	my $time_from_ancestor = {};
	my $index = {};
	my %index_to_binary;
	
	for my $ancestor_binary ( sort keys %$summary ){
		$index->{$ancestor_binary} = 1 + scalar keys %$index if !exists( $index->{$ancestor_binary} );
		for my $descendant_binary ( sort keys %{$summary->{$ancestor_binary}} ){
			$index->{$descendant_binary} = 1 + scalar keys %$index if !exists( $index->{$descendant_binary} );
			$time_from_ancestor->{$ancestor_binary} += $summary->{$ancestor_binary}{$descendant_binary}{Time};
		}
	}
	
	for my $binary ( keys %$index ){
		$index_to_binary{$index->{$binary}} = $binary;
	}
	
	my $temp_output;
	my $intermediate_file = File::Spec->rel2abs( "r.temp" );
	open $temp_output, ">" . $intermediate_file or return -1;
	
	my @header_values = (
		'ancestor.binary',
		'descendant.binary',
		'ancestor.positions',
		'descendant.positions',
		'ancestor.index',
		'descendant.index',
		'quantity',
		'total.time.from.ancestor',
	);
	print $temp_output join( ',', @header_values ) . "\n" if $headers;
	for my $ancestor_binary ( sort keys %$summary ){
		my $time = $time_from_ancestor->{$ancestor_binary};
		my $ancestor_index = $index->{$ancestor_binary};
		my $ancestor_positions = join '', @{$self->binary_to_positions( Binary => $ancestor_binary )};
		for my $descendant_binary ( sort keys %{$summary->{$ancestor_binary}} ){
			my $descendant_index = $index->{$descendant_binary};
			my $descendant_positions = join "", @{$self->binary_to_positions( Binary => $descendant_binary )};
			my $count = $summary->{$ancestor_binary}{$descendant_binary}{"Count"};
			my @row_values = (
				$ancestor_binary,
				$descendant_binary,
				$ancestor_positions,
				$descendant_positions,
				$ancestor_index,
				$descendant_index,
				$count,
				$time,
			);
			print $temp_output join( ',', @row_values ) . "\n" ;
		}
	}
	close $temp_output;
	
	my $r_command = qq("$r_path" --vanilla --slave --args inputfile="$intermediate_file" outputfile="$pmatrix" times=$times < "$r_script");
	`$r_command`;
	my $temp_input;
	open $temp_input, '<' . $intermediate_file or return -1;
	
	my $header = <$temp_input>;
	while( my $line = <$temp_input> ){
		say $log $line if $verbose > 2;
		chomp $line;
		my @values = split /,/, $line;
		my $ancestor_binary = $values[0];
		my $descendant_binary = $values[1];
		my @times = split /,/, $times;
		for my $i ( 0 .. $#times ){
			if( !exists($values[ 8 + $i ]) || !defined($values[ 8 + $i ]) ){
				say $log "Error: couldn't get R data (check the path to R)." if $verbose;
				return -1;
			}
			$rdata->{$ancestor_binary}{$descendant_binary}{Times}{$times[$i]} = $values[ 8 + $i ];
		}
	}
	
	close $temp_input;
	
	unlink $intermediate_file;
	
	for my $time ( split /,/, $times ){
		my $pmatrix_file = "$pmatrix $time .txt";
		if( !open PMATRIX, "<".$pmatrix_file ){
			print $log "Error: Couldn't open pmatrix file '$pmatrix_file' : $! \n" if $verbose;
		}
		my $ancestor_key = 1;
		while( my $line = <PMATRIX> ){
			chomp $line;
			my @probabilities = split /,/, $line;
			for my $i ( 0 .. $#probabilities ){
				if( !exists( $probabilities[$i] ) || !defined( $probabilities[$i] ) ){
					say $log "Error: couldn't get data from R. Check the path to R." if $verbose;
					return -1;
				}
				$p->{$time}{$index_to_binary{$ancestor_key}}{$index_to_binary{$i + 1}} = $probabilities[$i];
			}
			$ancestor_key++;
		}
		close PMATRIX;
	}
	
	unlink $r_script;
	
	print $log "done getting data from R\n" if $verbose;
	
	undef $log;
	
	return 0;
	
}

sub write_r{
	
	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR );
	return -1 if $self->setup_output( \my $output, $parameters{Output}, $self->{R_File_Output}, \*STDOUT );
	return -1 if $self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_variable( \my $show_counts, $parameters{Show_Counts}, $self->{Show_Counts}, 1 );
	return -1 if $self->setup_variable( \my $print_all, $parameters{Print_All}, 0 );
	$self->setup_variable( \my $times, $parameters{Times}, $self->{Times}, "1,8,16,20" );
	$self->setup_variable( \my $offset, $parameters{Offset}, $self->{Offset}, 0 );
	$self->setup_variable( \my $position_separator, $parameters{Position_Separator}, $self->{Position_Separator}, '' );
	$self->setup_variable( \my $show_codon_frequencies, $parameters{Show_Codon_Frequencies}, $self->{Show_Codon_Frequencies}, 1 );
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );
	
	
	print $log "writing r ($print_all)...\n";
	
	if( !keys %$summary ){
		$self->calculate_summary();
		$summary = $self->{Summary};
	}
	if( !keys %$summary ){
		say "couldn't get summary";
	}
	say "got summary";
	my $rdata;
	%$rdata = ();

	if( $self->get_r_data(
		RData => $rdata,
		Summary => $summary,
		Verbose => $verbose,
		Log => $log,
	)){
		say "error getting data from r";
		return -1;
	}
	say $log "got matrices from r" if $verbose;
	my @header_values = qw(
		ancestor.positions
		descendant.positions
		quantity
		total.time.from.ancestor
	);
	
	for my $time ( split /,/, $times ){
		push @header_values, "time=$time";
	}
	
	my %grouped_markers;
	my $codon_transitions;
	my $marker_info;
	
	if( $show_codon_frequencies ){
		
		$self->calculate_codon_transitions() if !$self->{Codon_Transitions} or !$self->{Marker_Info};
	
		$codon_transitions = $self->{Codon_Transitions};
		$marker_info = $self->{Marker_Info};
	
		return -1 if !$codon_transitions or !$marker_info;
	
		$self->parse_markers_file() if !$self->{Markers};
		return -1 if !$self->{Markers};
		my $markers = $self->{Markers};
		for my $i ( 0 .. $#{$markers} ){
			my $position = $markers->[$i]->{Position};
			push @{$grouped_markers{$position}}, $i + 1;
		}
		
		for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys( %grouped_markers ) ) ){
			my $heading = join $position_separator, sort @{$grouped_markers{$position}};
			my $most_prevalent = [ sort { $marker_info->{$position}{ancestors}{$b} <=> $marker_info->{$position}{ancestors}{$a} } keys %{$marker_info->{$position}{ancestors}} ]->[0];
			my $most_prevalent_count = $marker_info->{$position}{ancestors}{$most_prevalent};
			my $count = $marker_info->{$position}{count};
			my $statistic = $show_counts ? $most_prevalent_count : sprintf( "%.2f", ( $most_prevalent_count / $count ) * 100 ) . '%';
			$heading = "pos:" . $heading . "($most_prevalent ($statistic))";
			push @header_values, $heading;
		}
	}
	
	print $output join( ",", @header_values ) . "\n" if $headers;
	
	for my $a ( sort keys %$rdata ){
		for my $d ( sort keys %{$rdata->{$a}} ){
			say "$a->$d";
		}
	}

	for my $ancestor_binary ( sort keys %$rdata ){
		my $ancestor_positions = join "", @{$self->binary_to_positions( Binary => $ancestor_binary )};
		for my $descendant_binary ( sort keys %{$rdata->{$ancestor_binary}} ){
			if( !$print_all && !exists $summary->{$ancestor_binary}{$descendant_binary} ){
				next;
			}
			my $descendant_positions = join '', @{$self->binary_to_positions( Binary => $descendant_binary )};
			my $quantity = 0;
			my $time = 0;
			if( exists $summary->{$ancestor_binary}{$descendant_binary} ){
				$quantity = $summary->{$ancestor_binary}{$descendant_binary}{Count};
				$time = $summary->{$ancestor_binary}{$descendant_binary}{Time};
			}
			my @row_values = (
				$ancestor_positions,
				$descendant_positions,
				$quantity,
				$time,
			);
			for my $time ( split /,/, $times ){
				push @row_values, $rdata->{$ancestor_binary}{$descendant_binary}{Times}{$time};
			}
			if( $show_codon_frequencies && exists $summary->{$ancestor_binary}{$descendant_binary} ){
				for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys %grouped_markers ) ){
					my $count = $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"count"};
					my @position_values;
					my %ancestor_codons;
					my $most_prevalent = $marker_info->{$position}{"Most_Prevalent"};
					for my $transition ( keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}} ){
						my $ancestor_codon = [split /->/, $transition]->[0];
						$ancestor_codons{$ancestor_codon} += $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}{$transition};
					}
					for my $ancestor_codon ( sort { $ancestor_codons{$b} <=> $ancestor_codons{$a} } keys %ancestor_codons ){
						my $ancestor_count = $ancestor_codons{$ancestor_codon};
						my $statistic = $show_counts ? $ancestor_count : sprintf( "%.2f", ($ancestor_count / $count) * 100 );
						my $percent = '';
						$percent = '%' if !$show_counts;
						push @position_values, "$ancestor_codon($statistic$percent)";
						#push @position_values, "$ancestor_codon($statistic$percent)" if $statistic > 1 and $ancestor_codon;
					}
					push @row_values, join ';', @position_values;
				}
			}
			print $output join( ',', @row_values ) . "\n";
		}
	}

	my $extra = 0; 
	if( $extra ){
		say $output "\n";
		my $transitions = $self->{Transitions};
		my $binaries = $self->{Binaries};
		my $positions = $self->{Positions};
		my %actual_transitions;
		for my $patient( keys %$transitions ){
			for my $ancestor ( keys %{$transitions->{$patient}} ){
				my $ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
				for my $descendant ( keys %{$transitions->{$patient}{$ancestor}} ){
					my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
					my $transition = $ancestor_binary . '->' . $descendant_binary;
					$actual_transitions{$transition} = 0;
				}
			}
		}		
		for my $transition ( sort keys %actual_transitions ){
			my( $ancestor_binary, $descendant_binary ) = split '->', $transition;
			my $descendant_positions = join '', @{$self->binary_to_positions( Binary => $descendant_binary )};
			my $ancestor_positions = join '', @{$self->binary_to_positions( Binary => $ancestor_binary )};
			my $quantity = 0;
			my $time = 0;
			if( exists $summary->{$ancestor_binary}{$descendant_binary} ){
				$quantity = $summary->{$ancestor_binary}{$descendant_binary}{Count};
				$time = $summary->{$ancestor_binary}{$descendant_binary}{Time};
			}
			my @row_values = (
				$ancestor_positions,
				$descendant_positions,
				$quantity,
				$time,
			);
			for my $time ( split /,/, $times ){
				if( exists $rdata->{$ancestor_binary}{$descendant_binary} ){
					push @row_values, $rdata->{$ancestor_binary}{$descendant_binary}{Times}{$time};
				}
				else{
					push @row_values, 0;
				}
			}
			if( $show_codon_frequencies ){
				for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys %grouped_markers ) ){
					my $count = $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"count"};
					my @position_values;
					my %ancestor_codons;
					my $most_prevalent = $marker_info->{$position}{"Most_Prevalent"};
					for my $transition ( keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}} ){
						my $ancestor_codon = [split /->/, $transition]->[0];
						$ancestor_codons{$ancestor_codon} += $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}{$transition};
					}
					for my $ancestor_codon ( sort { $ancestor_codons{$b} <=> $ancestor_codons{$a} } keys %ancestor_codons ){
						my $ancestor_count = $ancestor_codons{$ancestor_codon};
						my $statistic = $show_counts ? $ancestor_count : sprintf( "%.2f", ($ancestor_count / $count) * 100 );
						my $percent = '';
						$percent = '%' if !$show_counts;
						push @position_values, "$ancestor_codon($statistic$percent)";
					}
					push @row_values, join ';', @position_values;
				}
			}
			say $output join ',', @row_values;
		}
	}
	print $log "done writing r...\n" if $verbose;
	
	undef $output;
	undef $log;
	
	return 0;
	
}

sub write_most_prevalent{
	my $self = shift;
	my %parameters = @_;
	$self->calculate_codon_transitions() if !$self->{Marker_Info} || !$self->{Codon_Transitions};
	$self->calculate_summary() if !$self->{Summary};
	$self->parse_markers_file() if !$self->{Markers};
	print "something is missing\n" if !$self->{Markers} or !$self->{Summary} or !$self->{Marker_Info} or !$self->{Codon_Transitions};
	my $markers = $self->{Markers};
	my $summary = $self->{Summary};
	my $codon_transitions = $self->{Codon_Transitions};
	my $marker_info = $self->{Marker_Info};
	my $output;
	my $log;
	return 1 if $self->setup_output( \$output, $parameters{Output}, $self->{Most_Prevalent_File_Output}, \*STDOUT );
	return 1 if $self->setup_output( \$log, $parameters{Log}, \*STDERR );
	return 1 if $self->setup_variable( \my $offset, $parameters{Offset}, $self->{Offset}, 98 );
	return 1 if $self->setup_variable( \my $position_separator, $parameters{Position_Separator}, '' );
	return 1 if $self->setup_variable( \my $show_counts, $parameters{Show_Counts}, $self->{Show_Counts}, 1 );
	return 1 if $self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 );
	return 1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 1 );
	return 1 if $self->setup_variable( \my $all, $parameters{All}, $self->{All}, 0 );
	
	print $log "writing most prevalent codons for each state (Show_Counts = $show_counts) \n" if $verbose;
	
	my @header_values = (
		'ancestor',
		'total.from',
	);
	
	my %grouped_markers;
	for my $i ( 0 .. $#{$markers} ){
		my $position = 3 * ($markers->[$i]->{Position} + $offset);
		push @{$grouped_markers{$position}}, $i + 1;
	}
		
	for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys( %grouped_markers ) ) ){
		my $heading = "pos:" . join $position_separator, sort @{$grouped_markers{$position}};
		my $count = $marker_info->{$position}{count};
		my $most_prevalent = [ sort { $marker_info->{$position}{ancestors}{$b} <=> $marker_info->{$position}{ancestors}{$a} } keys %{$marker_info->{$position}{ancestors}} ]->[0];
		for my $codon ( sort { $marker_info->{$position}{ancestors}{$b} <=> $marker_info->{$position}{ancestors}{$a} } keys %{$marker_info->{$position}{ancestors}} ){
			if( !$all ){
				next if $codon ne $most_prevalent;
			}
			my $statistic = sprintf( "%.2f", ( $marker_info->{$position}{ancestors}{$codon} / $count ) * 100 ) . '%';
			$heading .= "($codon ($statistic))";
		}
		push @header_values, $heading;
	}

	print $output join( ',', @header_values ) . "\n" if $headers;
	
	for my $ancestor_binary ( keys %$summary ){
		my @row_values = (
			join( '', @{$self->binary_to_positions( Binary => $ancestor_binary )} ),
		);
		my $count = 0;
		for my $descendant_binary ( keys %{$codon_transitions->{$ancestor_binary}} ){
			for my $transition ( keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}{[keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}}]->[0]}{transitions}} ){
				$count += $codon_transitions->{$ancestor_binary}{$descendant_binary}{[keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}}]->[0]}{transitions}{$transition};
			}
		}
		push @row_values, $count;
		for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys( %grouped_markers ) ) ){
			my %ancestor_codons;
			for my $descendant_binary ( keys %{$codon_transitions->{$ancestor_binary}} ){
				for my $transition ( keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}} ){
					$ancestor_codons{[split /->/, $transition]->[0]} += $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}{$transition};
				}
			}
			my @most_prevalents = sort { $ancestor_codons{$b} <=> $ancestor_codons{$a} } keys %ancestor_codons;
			my $most_prevalent_count = $ancestor_codons{$most_prevalents[0]};
			if( !$all ){
				@most_prevalents = grep { $ancestor_codons{$_} == $most_prevalent_count } @most_prevalents;
			}
			my @column_values;
			
			for my $most_prevalent ( @most_prevalents ){
				my $statistic;
				$statistic = sprintf( "%.2f", ( $ancestor_codons{$most_prevalent} / $count ) * 100 ) . '%';
				push @column_values, "$most_prevalent($statistic)";
			}
			push @row_values, join ';', @column_values;
		}
		print $output join( ',', @row_values ) . "\n";
	}
	undef $output;
	undef $log;
	return 0;
}

sub prune_summary{
	
	my $self = shift;
	my %parameters = @_;
	my %pruned_summary;
	my $graph = Graph::Directed->new;
	my $pruned_graph = Graph::Directed->new;
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		return -1;
	};
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log_File_Input}, \*STDERR ) and do{
		say STDERR "Error: could not set up log for output" if $verbose;
		return -1;
	};
	
	$self->setup_variable( \my $markers, $parameters{Markers}, $self->{Markers} );
	
	if( !$markers ){
		$self->parse_markers_file( Log => $log, Verbose => $verbose );
		$markers = $self->{Markers};
	}
	
	if( !$markers ){
		say $log "Error: can't find markers" if $verbose;
		return -1;
	}
	
	$self->setup_variable( \my $reconnect, $parameters{Reconnect}, $self->{Reconnect}, 1 ) and do{ return -1; };
	$self->setup_variable( \my $preferred_ancestor, $parameters{Preferred_Ancestor}, $self->{Preferred_Ancestor}, '0' x scalar @$markers ) and do{ return -1; };
	$self->setup_variable( \my $minimum_count, $parameters{Prune_Minimum_Quantity}, $self->{Prune_Minimum_Quantity}, 2 ) and do{ return -1; };
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );
	
	if( !$summary ){
		$self->calculate_summary( Log => $log, Verbose => $verbose );
		$summary = $self->{Summary};
	}
	
	if( !$summary ){
		say $log "Error: can't prune summary because it can't be found" if $verbose;
		return -1;
	}
		
	say $log "pruning transitions with quantity less than $minimum_count" if $verbose;
	$minimum_count++;
	for my $ancestor_binary ( sort keys %$summary ){
		my $ancestor_positions = join '', @{$self->binary_to_positions( Binary => $ancestor_binary )};
		for my $descendant_binary ( sort keys %{$summary->{$ancestor_binary}} ){
			my $descendant_positions = join "", @{$self->binary_to_positions( Binary => $descendant_binary )};
			my $count = $summary->{$ancestor_binary}{$descendant_binary}{Count};
			my $time = $summary->{$ancestor_binary}{$descendant_binary}{Time};
			if ( $count >= $minimum_count ){
				$pruned_summary{$ancestor_binary}{$descendant_binary}{Count} = $count;
				$pruned_summary{$ancestor_binary}{$descendant_binary}{Time} = $time;
				$pruned_graph->add_weighted_edge( $ancestor_binary, $descendant_binary, -$count );
			}
			else{
				say $log "pruning $ancestor_positions -$count-> $descendant_positions";
			}
			$graph->add_weighted_edge( $ancestor_binary, $descendant_binary, -$count );
		}
	}
	
	
	if( $reconnect ){
		say $log "ancestorless states will be reconnected" if $verbose;
		for my $vertex ( $pruned_graph->unique_vertices() ){
			next if $vertex eq $preferred_ancestor;
			my @predecessors = $pruned_graph->predecessors( $vertex );
			next unless @predecessors == 0 || ( @predecessors == 1 && $predecessors[0] eq $vertex );
			my $vertex_positions = join '', @{$self->binary_to_positions( Binary => $vertex )};
			say $log "$vertex_positions remains without ancestor." if $verbose;  
			if( $summary->{$preferred_ancestor}{$vertex}{Count} ){
				my $count = $summary->{$preferred_ancestor}{$vertex}{Count};
				my $time = $summary->{$preferred_ancestor}{$vertex}{Time};
				$pruned_graph->add_weighted_edge( $preferred_ancestor, $vertex, -$count );
				$pruned_summary{$preferred_ancestor}{$vertex}{Count} = $count;
				$pruned_summary{$preferred_ancestor}{$vertex}{Time} = $time;
				say $log "\t$vertex_positions was reconnected by unpruning $preferred_ancestor-$count->$vertex.\n" if $verbose;  
				next;
			}
		}
		for my $vertex ( $graph->unique_vertices() ){
			if( !$pruned_graph->has_vertex( $vertex ) ){
				$graph->delete_vertex( $vertex ) while $graph->has_vertex( $vertex );
			}
		}
		my $APSP = $graph->APSP_Floyd_Warshall();
		my $TCG = $graph->TransitiveClosure_Floyd_Warshall();
		for my $vertex ( $pruned_graph->unique_vertices() ){
			next if $vertex eq $preferred_ancestor;
			my @predecessors = $pruned_graph->predecessors( $vertex );
			next unless @predecessors == 0 || ( @predecessors == 1 && $predecessors[0] eq $vertex );
			my $vertex_positions = join '', @{$self->binary_to_positions( Binary => $vertex )};
			if( !$TCG->is_reachable( $preferred_ancestor, $vertex ) ){
				say $log "$vertex_positions cannot be reconnected with $preferred_ancestor" if $verbose;
				next;
			}
			my @sp_vertices = $APSP->path_vertices( $preferred_ancestor, $vertex );
			say $log "reconnecting $vertex_positions to $preferred_ancestor via " . join( '->', map { $self->binary_to_positions( Binary => $_ ); } @sp_vertices ) if $verbose;
			for my $i ( 1 .. $#sp_vertices ){
				my $start = $sp_vertices[ $i - 1 ];
				my $stop = $sp_vertices[ $i ];
				$pruned_summary{$start}{$stop}{Count} = $summary->{$start}{$stop}{Count};
				$pruned_summary{$start}{$stop}{Time} = $summary->{$start}{$stop}{Time};
			}
		}
	}
	else{
		say $log "ancestorless states will not be reconnected" if $verbose;
	}
	
	say $log "done pruning summary" if $verbose;
	
	$self->{Pruned_Summary} = \%pruned_summary;
	
	undef $log;
	
	return 0;
	
}

sub write_pruned_summary{
	
	my $self = shift;
	my %parameters = @_;
	$self->setup_output( \my $log, $parameters{Log}, \*STDERR ) and do { return -1; };
	$self->setup_output( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{ return -1; };
	$self->setup_variable( \my $markers, $parameters{Markers}, $self->{Markers} );	
	if( !$markers ){
		$self->parse_markers_file( Log => $log, Verbose => $verbose );
		$markers = $self->{Markers};
	}

	$self->setup_output( \my $output, $parameters{Output}, $self->{Pruned_Summary_File_Output}, \*STDOUT ) and do{ return -1; };
	$self->setup_variable( \my $pruned_summary, $parameters{Pruned_Summary}, $self->{Pruned_Summary} );
	$self->setup_variable( \my $summary, $parameters{Summary} );
	$self->setup_variable( \my $reconnect, $parameters{Reconnect}, $self->{Reconnect}, 1 ) and do{ return -1; };
	$self->setup_variable( \my $preferred_ancestor, $parameters{Preferred_Ancestor}, $self->{Preferred_Ancestor}, $markers ? '0' x scalar @$markers : '0000000' ) and do{ return -1; };
	$self->setup_variable( \my $minimum_count, $parameters{Prune_Minimum_Quantity}, $self->{Prune_Minimum_Quantity}, 2 ) and do{ return -1; };
	$self->setup_variable( \my $headers, $parameters{Headers}, $self->{Headers}, 1 ) and do{ return -1 };
	
	if( !$pruned_summary ){
		$self->prune_summary(
			Verbose => $verbose,
			Log => $log,
			Reconnect => $reconnect,
			Prune_Minimum_Quantity => $minimum_count,
			Summary => $summary,
		) or do{
			say $log "could not create a pruned summary" if $verbose;
			return -1;
		};
		$pruned_summary = $self->{Pruned_Summary};
	}
	
	return -1 if !$pruned_summary;

	$self->write_summary(
		Output => $output,
		Summary => $pruned_summary,
		Verbose => $verbose,
		Log => $log,
	);
	
	undef $log;
	undef $output;
	
	return 0;

}

sub write_pruned_r{
	
	my $self = shift;
	my $variable_name = "Pruned_R_File_Output";
	my %parameters = @_;
	my $headers = exists $self->{Headers} ? $self->{Headers} : 1;
	my $verbose = $self->{Verbose} ? $self->{Verbose} : 0;
	
	my $output;
	my $log;
	return 1 if $self->setup_output( \$output, $parameters{Output}, $self->{$variable_name}, \*STDOUT );
	return 1 if $self->setup_output( \$log, $parameters{Log}, \*STDERR );
	
	my $summary = "";
	$summary = $self->{"Pruned_Summary"} if $self->{"Pruned_Summary"};
	$summary = $parameters{"Summary"} if $parameters{"Summary"};
	if( !$summary ){
		$self->prune_summary() if !$self->{"Pruned_Summary"};
		$summary = $self->{"Pruned_Summary"};
	}
	return -1 if !$summary;
	
	$self->write_r( Output => $output, Summary => $summary );
	
	undef $output;
	undef $log;
	
	return 0;
	
}

sub setup_variable{
	
	my $self = shift;
	unless( ref( $self ) eq __PACKAGE__ || $self eq __PACKAGE__ ){
		unshift @_, $self;
	}
	
	my $ref = shift or return -1;
	
	for my $arg ( @_ ){
		if( defined $arg ){
			$$ref = $arg;
			return 0;
		}
	}
	
	return -1;
	
}

sub calculate_codon_transitions{
	
	my $self = shift;
	my %parameters = @_;
	#$self->setup_output( \my $output, $parameters{Output}, \*STDOUT );
	$self->setup_log( \my $log, $parameters{Log}, \*STDERR );
	$self->load_sequences if !$self->{"Sequences"};
	$self->load_transitions if !$self->{"Transitions"};
	$self->load_clusters if !$self->{"Clusters"};
	$self->parse_markers_file if !$self->{"Markers"};
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	$self->setup_variable( \my $use_clusters, $parameters{Use_Clusters}, $self->{Use_Clusters}, 1 );
	$self->setup_variable( \my $bootstrap_threshold, $parameters{Codon_Bootstrap_Threshold}, $self->{Codon_Bootstrap_Threshold}, 0 );
	
	my $sequences = $self->{"Sequences"};
	my $binaries = $self->{"Binaries"};
	my $transitions = $self->{"Transitions"};
	my $markers = $self->{"Markers"};
	my $offset = 0;
	my $clusters = $self->{"Clusters"};
	my $codon_transitions = {};
	my $marker_info = {};
	my %grouped_markers;
	$verbose = 1;
	print $log "Calculating codon transition frequencies...\n" if $verbose;
	
	for my $i ( 0 .. $#{$markers} ){
		my $position = $markers->[$i]->{Position};
		push @{$grouped_markers{$position}}, $i + 1;
	}

	for my $patient ( sort keys %$transitions ){
		for my $ancestor ( sort keys %{$transitions->{$patient}} ){
			my $ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
			for my $descendant ( sort keys %{$transitions->{$patient}{$ancestor}} ){
				my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
				if( $use_clusters ){
					print $log "\tlooking at clusters\n" if $verbose > 1;
					my $bootstrap = $transitions->{$patient}{$ancestor}{$descendant}{Bootstrap};
					if( defined $bootstrap_threshold ){
						if( $bootstrap < $bootstrap_threshold ){
							print $log "\t\tcodon count skipping because $bootstrap < $bootstrap_threshold\n" if $verbose > 1;
							next;
						}
						else{
							print $log "\t\tcodon count not skipping because $bootstrap >= $bootstrap_threshold\n" if $verbose > 1;
						}
					}
					else{
						print $log "\t\tcodon count not skipping because \$bootstrap_threshold is undefined\n" if $verbose > 1;
					}
					for my $candidate_ancestor ( @{$clusters->{$patient}{$descendant}} ){
						#say "$patient $candidate_ancestor";
						my $candidate_ancestor_binary = join '', @{$binaries->{$patient}{$candidate_ancestor}}; # this should be the same as $ancestor_binary but what if we change clustering?
						for my $position ( sort keys %grouped_markers ){
							die unless exists $sequences->{$patient}{$candidate_ancestor}{"DNA"}{$position};
							my $transition = $sequences->{$patient}{$candidate_ancestor}{"DNA"}{$position} . "->" . $sequences->{$patient}{$descendant}{"DNA"}{$position};
							$codon_transitions->{$candidate_ancestor_binary}{$descendant_binary}{$position}{"count"}++;
							$codon_transitions->{$candidate_ancestor_binary}{$descendant_binary}{$position}{"transitions"}{$transition}++;
							$marker_info->{$position}{Ancestor_States}{$candidate_ancestor_binary}{ $sequences->{$patient}{$candidate_ancestor}{DNA}{$position} }++;
							$marker_info->{$position}{"count"}++;
							$marker_info->{$position}{"ancestors"}{ $sequences->{$patient}{$candidate_ancestor}{DNA}{$position} }++;
						}
					}
				}
				else{
					my $bootstrap = $transitions->{$patient}{$ancestor}{$descendant}{Bootstrap};
					print $log "\tnot looking at clusters\n" if $verbose > 1;
					if( defined $bootstrap_threshold ){
						if( $bootstrap < $bootstrap_threshold ){
							print $log "\t\tcodon count skipping because $bootstrap < $bootstrap_threshold\n" if $verbose > 1;
							next;
						}
						else{
							print $log "\t\tcodon count not skipping because $bootstrap >= $bootstrap_threshold\n" if $verbose > 1;
						}
					}
					else{
						print $log "\t\tcodon count not skipping because \$bootstrap_threshold is undefined\n" if $verbose > 1;
					}
					for my $position ( sort keys %grouped_markers ){
						my $transition = $sequences->{$patient}{$ancestor}{DNA}{$position} . '->' . $sequences->{$patient}{$descendant}{DNA}{$position};
						$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"count"}++;
						$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"transitions"}{$transition}++;
						$marker_info->{$position}{Ancestor_States}{$ancestor_binary}{ $sequences->{$patient}{$ancestor}{DNA}{$position} }++;
						$marker_info->{$position}{"count"}++;
						$marker_info->{$position}{"ancestors"}{ $sequences->{$patient}{$ancestor}{DNA}{$position} }++;
					}
				}
			}
		}
	}
	
	for my $position ( keys %$marker_info ){
		$marker_info->{$position}{"Most_Prevalent"} = [ sort { $marker_info->{$position}{"ancestors"}{$b} <=> $marker_info->{$position}{"ancestors"}{$a} } keys %{$marker_info->{$position}{"ancestors"}} ]->[0];
	}
	
	$self->{"Codon_Transitions"} = $codon_transitions;
	$self->{"Marker_Info"} = $marker_info;
	
	print $log "done calculating codon transition frequencies\n" if $verbose;
	#undef $output;
	undef $log;
	
	return 0;
	
}

sub write_codon_transitions{

	my $self = shift;
	my $file = shift;
	$file = File::Spec->rel2abs( $file );
	$self->calculate_codon_transitions if !$self->{"Codon_Transitions"};
	my $codon_transitions = $self->{"Codon_Transitions"};
	my %grouped_markers;
	my $markers = $self->{"Markers"};
	my $position_separator = $self->{"Position_Separator"} ? $self->{"Position_Separator"} : ';';
	my @header_values;
	my $backup_suffix = $self->{"Backup_Suffix"} ? $self->{"Backup_Suffix"} : ".backup";
	my $headers = $self->{"Headers"};
	my $offset = $self->{"Offset"};
	my $show_counts = $self->{"Show_Counts"};
	my $marker_info = $self->{"Marker_Info"};
	
	for my $i ( 0 .. $#{$markers} ){
		my $position = 3 * ($markers->[$i]->{"Position"} + $offset);
		push @{$grouped_markers{$position}}, $i + 1;
	}
	
	for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys( %grouped_markers ) ) ){
		my $heading = join $position_separator, sort @{$grouped_markers{$position}};
		my $most_prevalent = [ sort { $marker_info->{$position}{"ancestors"}{$b} <=> $marker_info->{$position}{"ancestors"}{$a} } keys %{$marker_info->{$position}{"ancestors"}} ]->[0];
		$heading = "pos:" . $heading . "($most_prevalent)";
		push @header_values, $heading;
	}
	
	{
		$^I = $backup_suffix;
		@ARGV = ( $file );
		while( <> ){
			chomp $_;
			if( $_ =~ /^([01]+),([01]+),/ ){
				my $ancestor_binary = $1;
				my $descendant_binary = $2;
				my @row_values;
				push @row_values, $_;
				for my $position ( sort( { [sort @{$grouped_markers{$a}} ]->[0] <=> [sort @{$grouped_markers{$b}} ]->[0] } keys %grouped_markers ) ){
					my $count = $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"count"};
					my @position_values;
					my %ancestor_codons;
					my $most_prevalent = $marker_info->{$position}{"Most_Prevalent"};
					for my $transition ( sort{ $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"transitions"}{$b} <=> $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"transitions"}{$a} } keys %{$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"transitions"}} ){
						my $ancestor_codon = [split /->/, $transition]->[0];
						$ancestor_codons{$ancestor_codon} += $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"transitions"}{$transition};
					}
					for my $ancestor_codon ( sort { $ancestor_codons{$b} <=> $ancestor_codons{$a} } keys %ancestor_codons ){
						my $ancestor_count = $ancestor_codons{$ancestor_codon};
						my $statistic = $show_counts ? $ancestor_count : round( ( ($ancestor_count * 1.0 / $count * 1.0) ) * 100.0 );
						push @position_values, "$ancestor_codon($statistic%)"; 
					}
	
					@position_values = grep {$_ !~ /^$most_prevalent/ and $_ !~ /...\(0%\)$/ } @position_values;
					push @position_values, '*' if !@position_values;
					push @row_values, join ';', @position_values;
				}
				print join( ',', @row_values ) . "\n";
			}
			else{
				print $_ . "," . join( ',', @header_values ) . "\n" if $headers;
			}
		}
		unlink( $file.$^I );
	}

	return 0;

}

sub write_r_output{

	my $self = shift;
	my %parameters = @_;
	my $r_file = File::Spec->rel2abs( $parameters{Output_File} );
	my $r_path = $self->{"R_Path_File_Input"};
	my $directory = File::Spec->rel2abs( $self->{"Output_Directory"} );
	my $r_script = File::Spec->rel2abs( $self->{"R_Script_File_Input"} );
	my $times = $self->{"Times"};
	my $pmatrix = $self->{"PMatrix"};
	my $verbose = $self->{"Verbose"} ? $self->{"Verbose"} : 0;
	my $r_command = qq("$r_path" --vanilla --slave --args wd="$directory" inputfile="$r_file" outputfile="$pmatrix" times=$times < "$r_script");
	print STDERR "$r_command\n" if $verbose;
	`$r_command`;
	
	return 0;
	
}

sub binary_to_positions{
	
	my $self = shift;
	my $positions_separator;
	my %parameters;
	
	if( ref $self ne __PACKAGE__ ){
		unshift @_, $self;
		%parameters = @_;
	}
	else{
		%parameters = @_;
	}
	
	die if !exists $parameters{Binary};
	
	my @binary = split //, $parameters{Binary};
	my @positions;
	
	for( my $i = 0; $i < @binary; $i++ ){
		if( $binary[$i] == 1 ){
			push @positions, $i + 1;
		}
	}
	
	if( @positions == 0 ){
		push @positions, 0;
	}
	
	return [sort { $a <=> $b } @positions];
	
}

sub trim{
	my $string = shift;
	$string =~ s/^\s+//;
	$string =~ s/\s+$//;
	return $string;
}

sub write_shortest_paths{
	
	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_log( \my $log, $parameters{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $output_directory, $parameters{Shortest_Path_Directory}, $self->{Shortest_Path_Directory}, File::Spec->rel2abs( &getcwd() ) );
	return -1 if $self->setup_variable( \my $times, $parameters{Times}, $self->{Times}, "1,8,16,20" );
	return -1 if $self->setup_variable( \my $prefix, $parameters{Prefix}, $parameters{Shortest_Path_Prefix}, $self->{Shortest_Path_Prefix}, "shortest_path" );
	$self->setup_variable( \my $rdata, $parameters{RData}, $self->{RData} );
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Versboe}, 0 );
	return -1 if $self->setup_variable( \my $k, $parameters{K}, $self->{K}, 5 );
	return -1 if $self->setup_variable( \my $max_edges, $parameters{Max_Edges}, $self->{Max_Edges}, 5 );
	if( !$summary || 'HASH' ne ref( $summary ) || !keys( %$summary ) ){
		if( $self->calculate_summary() || !($self->{Summary}) || 'HASH' ne ref( $self->{Summary} ) || !keys( %{$self->{Summary}} ) ){
			say $log "Error: couldn't get summary" if $verbose;
			return -1;
		}
		$summary = $self->{Summary};
	}
	
	if( !$rdata || 'HASH' ne ref( $rdata ) || !keys( %$rdata ) ){
		$rdata = {};
		if( $self->get_r_data(
			RData => $rdata,
			Summary => $summary,
		) || !$rdata || 'HASH' ne ref( $rdata) || !keys( %$rdata ) ) {
			say $log "Error: couldn't get r data" if $verbose;
			return -1;
		}
	}

	$output_directory = File::Spec->rel2abs( $output_directory );
	
	my %weights;
	
	for my $ancestor_binary ( sort keys %$rdata ){
		my $ancestor_positions = join "", @{$self->binary_to_positions( Binary => $ancestor_binary)};
		for my $descendant_binary( sort keys %{$rdata->{$ancestor_binary}} ){
			my $descendant_positions = join "", @{$self->binary_to_positions( Binary => $descendant_binary)};
			for my $time ( split /,/, $times ){
				$weights{Times}{$time}{$ancestor_positions}{$descendant_positions} = $rdata->{$ancestor_binary}{$descendant_binary}{Times}{$time};
				if( !exists( $weights{Times}{$time}{$ancestor_positions}{$descendant_positions} ) || !defined( $weights{Times}{$time}{$ancestor_positions}{$descendant_positions} ) ){
					say $log "Error: no mm data for $ancestor_positions -> $descendant_positions at time $time" if $verbose;
				 	$weights{Times}{$time}{$ancestor_positions}{$descendant_positions} = 0;	
				}
			}
		}
	}
	
	for my $time ( split /,/, $times ){
		my $output_file = File::Spec->catfile( File::Spec->rel2abs( $output_directory ), '' . $prefix . $k . " - t$time.csv" );
		print "$output_file";
		my $output;
		open $output, ">" . $output_file or return -1;
		my $sp = ShortestPath->new();
		$sp->write_shortest_paths(
			"Max_Edges" => $max_edges,
			"K" => $k,
			"From" => "ancestor.positions",
			"To" => "descendant.positions",
			"Weights" => $weights{Times}{$time},
			"Output" => \$output,
			"Round" => 3,
		);
		close $output;
	}
	
	undef $log;
	
	return 0;
	
}

sub write_pruned_shortest_paths{
	
	my $self = shift;
	my %parameters = @_;
	my $rdata = {};
	my $return = '';
	my $output_directory = '';
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR );
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	$output_directory = $self->{Pruned_Shortest_Path_Directory} if exists $self->{Pruned_Shortest_Path_Directory};
	$output_directory = $parameters{Pruned_Shortest_Path_Directory} if exists $parameters{Pruned_Shortest_Path_Directory};
	my $prefix = "pruned - SP";
	
	if( $self->get_pruned_r_data() || !($self->{Pruned_R_Data}) || 'HASH' ne ref( $self->{Pruned_R_Data} ) || !keys(%{$self->{Pruned_R_Data}}) ){
		say $log "Error: couldn't get pruned r data" if $verbose;
		return -1;		
	}

	$rdata = $self->{Pruned_R_Data};
	
	return $self->write_shortest_paths(
		Shortest_Path_Directory => $output_directory,
		RData => $rdata,
		Prefix => $prefix,
	);
	
}

sub write_probability_graphs{

	my $self = shift;
	my %parameters = @_;
	
	return -1 if $self->setup_log( \my $log, $parameters{Log}, $self->{Log}, \*STDERR );
	return -1 if $self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	return -1 if $self->setup_variable( \my $times, $parameters{Times}, $self->{Times}, "1,8,16,20" );
	return -1 if $self->setup_variable( \my $round, $parameters{Graph_Round}, $self->{Graph_Round}, "3" );
	$self->setup_variable( \my $prefix, $parameters{Graph_Prefix}, $self->{Graph_Prefix}, 'graph' );
	$self->setup_variable( \my $output_directory, $parameters{Graph_Directory}, $self->{Graph_Directory}, File::Spec->rel2abs( &getcwd() ) );
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );
	$self->setup_variable( \my $rdata, $parameters{RData} );
	print $log "writing probability graphs...\n" if $verbose;
	my $return = '';
	
	if( !$rdata ){
		%$rdata = ();
		return -1 if $self->get_r_data(
			RData => $rdata,
			Summary => $summary,
		);
	}

	my %weights;
	my %graphs = ();
	
	for my $ancestor_binary ( sort keys %$rdata ){
		my $ancestor_positions = join '', @{$self->binary_to_positions( Binary => $ancestor_binary)};
		for my $descendant_binary( sort keys %{$rdata->{$ancestor_binary}} ){
			my $descendant_positions = join '', @{$self->binary_to_positions( Binary => $descendant_binary)};
			my %weights;
			for my $time ( split /,/, $times ){
				$graphs{$time} = GraphViz->new(
					directed => 1,
					layout => 'dot',
				) if !exists $graphs{$time};
				$graphs{$time}->add_edge(
					$ancestor_positions => $descendant_positions,
					label => sprintf( "%.${round}f", $rdata->{$ancestor_binary}{$descendant_binary}{Times}{$time} ),
				);
			}
		}
	}
	
	for my $time ( split /,/, $times ){
		my $output_file = File::Spec->catfile( File::Spec->rel2abs( $output_directory ), $prefix . " - t$time.png" );
		$graphs{$time}->as_png( $output_file );
	}

	print $log "done writing probability graphs\n" if $verbose;

	undef $log;;

	return 0;

}

sub write_pruned_probability_graphs{
	
	my $self = shift;
	my %parameters = @_;
	my $rdata = '';
	my $return = '';
	my $prefix = '';
	$prefix = $self->{Pruned_Graph_Prefix} if exists $self->{Pruned_Graph_Prefix};
	$prefix = $parameters{Graph_Prefix} if exists $parameters{Graph_Prefix};
	$prefix = "graph" if !$prefix;
	my $output_directory = '';
	$output_directory = $self->{Pruned_Graphs_Directory} if exists $self->{Pruned_Graphs_Directory};
	$output_directory = $parameters{Pruned_Graphs_Directory} if exists $parameters{Pruned_Graphs_Directory};
	$rdata = $parameters{RData} if exists $parameters{RData} and ref( $parameters{RData} ) eq 'HASH';
	if( !$rdata ){
		if( $self->get_pruned_r_data() ){
			die "Error:1";
		}
		if( !($self->{Pruned_R_Data}) ){
			die "Error:2";
		}
		if( 'HASH' ne ref $self->{Pruned_R_Data} ){
			die "Error:3";
		}
	}
	$rdata = $self->{Pruned_R_Data};
	return $self->write_probability_graphs(
		RData => $rdata,
		Graph_Prefix => $prefix,
		Graph_Directory => $output_directory,
	);
	
}

sub set{
	my $self = shift;
	my $parameter_name = shift;
	my $value = shift;
	$self->{$parameter_name} = $value;
	return $value;
}

sub count_zero_to_six{
	print "running\n";
	my $self = shift;
	my $transitions = $self->{"Transitions"};
	my $binaries = $self->{"Binaries"};
	my $sequences = $self->{"Sequences"};
	my $wild_type = '0000000';
	my $six = '0000010';
	my %table;
	for my $patient ( keys %$transitions ){
		for my $ancestor ( keys( %{$transitions->{$patient}} ) ){
			my $ancestor_binary = join "", @{$binaries->{$patient}{$ancestor}};
			next if $ancestor_binary ne $wild_type;
			for my $descendant ( keys( %{$transitions->{$patient}{$ancestor}} ) ){
				my $codon_at_190 = $sequences->{$patient}{$ancestor}{"DNA"}{ (190 + 98) * 3 };
				my $descendant_binary = join "", @{$binaries->{$patient}{$descendant}};
				print "$codon_at_190\n" if $codon_at_190 ne 'GGC';
				$table{a}++ if $descendant_binary eq $six and $codon_at_190 eq 'GGC';
				$table{b}++ if $descendant_binary eq $six and $codon_at_190 ne 'GGC';
				$table{c}++ if $descendant_binary ne $six and $codon_at_190 eq 'GGC';
				$table{d}++ if $descendant_binary ne $six and $codon_at_190 ne 'GGC';
			}
		}
	}
	map {print "$_ => $table{$_}\n";} sort keys %table;
	return 0;
}

sub read_codons{
	my $self = shift;
	my $codon_file = shift;
	open CODON_FILE, $codon_file or die $!;
	while( my $line = <CODON_FILE> ){
		my %check;
		chomp $line;
		($check{codon}, $check{position}, $check{begin}, $check{end}) = split /,/, $line;
		push @{$self->{"Codons_Of_Interest"}}, \%check;
	}
	close CODON_FILE;
}

sub generate_xor_contingency_tables{
	
	my $self = shift;
	$self->load_transitions if !exists $self->{Transitions};
	$self->load_clusters if !exists $self->{Clusters};
	$self->load_sequences if !exists $self->{Sequences};
	my $clusters = $self->{Clusters};
	my $transitions = $self->{Transitions};
	my $positions = $self->{Positions};
	my $sequences = $self->{Sequences};
	my $codons_of_interest = $self->{Codons_Of_Interest};
	my $binaries = $self->{Binaries};
	
	for my $patient ( keys %$transitions ){
		for my $ancestor ( keys %{$transitions->{$patient}} ){
			for my $descendant ( keys %{$transitions->{$patient}{$ancestor}} ){
				my $descendant_positions = join '', @{$positions->{$patient}{$descendant}};
				my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
				for my $candidate_ancestor ( @{$clusters->{$patient}{$descendant}} ){
					my $candidate_ancestor_positions = join '', @{$positions->{$patient}{$ancestor}};
					my $candidate_ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
					my $xor = $self->xor( Ancestor => $candidate_ancestor_binary, Descendant => $descendant_binary );
					for my $check ( @$codons_of_interest ){
						my $position = $check->{Position};
						$check->{a}++ if $xor eq $check->{XOR} and $sequences->{$patient}{$ancestor}{DNA}{$position} eq $check->{Codon};
						$check->{b}++ if $xor eq $check->{XOR} and $sequences->{$patient}{$ancestor}{DNA}{$position} ne $check->{Codon};
						$check->{c}++ if $xor ne $check->{XOR} and $sequences->{$patient}{$ancestor}{DNA}{$position} eq $check->{Codon};
						$check->{d}++ if $xor ne $check->{XOR} and $sequences->{$patient}{$ancestor}{DNA}{$position} ne $check->{Codon};
					}
				}
			}
		}
	}
	
	return 0;
	
}

sub xor{
	my $self = shift;
	my %parameters = @_;
	return undef if !$parameters{Ancestor} or !$parameters{Descendant};
	my $separator = '';
	$separator = $parameters{Separator} if exists $parameters{Separator};
	my @result;
	my @ancestor = split $separator, $parameters{Ancestor};
	my @descendant = split $separator, $parameters{Descendant};
	for my $i ( 0 .. $#ancestor ){
		if( $ancestor[$i] eq $descendant[$i] ){
			push @result, 0;
		}
		else{
			push @result, 1;
		}
	}
	return join $separator, @result;
}

sub find_codons{
	
	my $self = shift;
	my %parameters = @_;

	$self->setup_output( \my $output, $parameters{Output}, $self->{Find_Codons_Output_File}, \*STDOUT ) and return -1;
	$self->setup_log( \my $log, $parameters{Log}, \*STDERR ) and return -1;
	$self->setup_variable( \my $minimum, $parameters{Rare_Minimum}, $self->{Rare_Minimum}, 5 ) and return -1;
	$self->setup_variable( \my $min_freqency, $parameters{Minimum_Frequency}, $self->{Interesting_Codon_Minimum_Frequency}, 70 ) and return -1;
	$self->setup_variable( \my $r_path, $parameters{R_Path}, $self->{R_Path_File_Input}, "r" ) and return -1;
	$self->setup_variable( \my $r_script, $parameters{R_Script}, $self->{Find_Codons_R_Script_File_Input}, File::Spec->rel2abs( "Source/R/Fisher.R" ) ) and return -1;
	$self->setup_variable( \my $temp_file, $parameters{Temp_File}, "find_codons_temp.txt" ) and return -1;
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and return -1;
	
	$self->calculate_codon_transitions( Log => $log, Verbose => $verbose ) if !exists $self->{Codon_Transitions};
	$self->calculate_summary( Log => $log, Verbose => $verbose ) if !exists $self->{Summary};
	my $codon_transitions = $self->{Codon_Transitions};
	my $marker_info = $self->{Marker_Info};
	my $summary = $self->{Summary};
	$self->{Codons_Of_Interest} = [];

	my %rules;

	# figure out the interesting codons
	# should really do this from the summary (will filter later)
	for my $ancestor_binary ( keys %$codon_transitions ){
		my $ancestor_positions = join '', @{$self->binary_to_positions( Binary => $ancestor_binary )};
		for my $descendant_binary ( keys %{ $codon_transitions->{$ancestor_binary} } ){
			my $descendant_positions = join '', @{$self->binary_to_positions( Binary => $descendant_binary )};
			for my $position ( keys %{ $codon_transitions->{$ancestor_binary}{$descendant_binary} } ){
				my $marker = join '', @{ $self->{Marker_Positions}->{($position / 3) - $self->{Offset}} };
				my %ancestor_codon;
				$codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{codons} = [];
				my $ancestor_most_prevalent = [ sort {$marker_info->{$position}{Ancestor_States}{$ancestor_binary}{$b} <=> $marker_info->{$position}{Ancestor_States}{$ancestor_binary}{$a}} keys %{$marker_info->{$position}{Ancestor_States}{$ancestor_binary}} ]->[0];
				for my $transition ( keys %{ $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{"transitions"} } ){
					$ancestor_codon{[split /->/, $transition]->[0]} += $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{transitions}{$transition};
				}
				for my $codon ( keys %ancestor_codon ){
					next if $codon eq $ancestor_most_prevalent;
					next if $ancestor_codon{$codon} < $minimum;
					my $frequency = 100.0 *  $ancestor_codon{$codon} / $codon_transitions->{$ancestor_binary}{$descendant_binary}{$position}{count};
					if( $frequency  > $min_freqency ){
						my @others;
						my %frequencies;
						for my $other_descendant_binary ( keys %{$codon_transitions->{$ancestor_binary}} ){
							next if $other_descendant_binary eq $descendant_binary;
							my $other_descendant_positions = join '', @{$self->binary_to_positions( Binary => $other_descendant_binary )};
							my %other_count;
							for my $transition ( keys %{ $codon_transitions->{$ancestor_binary}{$other_descendant_binary}{$position}{transitions} } ){
								$other_count{[split /->/, $transition]->[0]} += $codon_transitions->{$ancestor_binary}{$other_descendant_binary}{$position}{transitions}{$transition};
							}
							next if !grep { $_ eq $codon } keys %other_count;
							next if $other_count{$codon} < $minimum;
							my $other_frequency = 100.0 * $other_count{$codon} / $codon_transitions->{$ancestor_binary}{$other_descendant_binary}{$position}{count};
							if( $other_frequency > $min_freqency && $self->has_marker_in_common( Sequence1 => $ancestor_binary, Sequence2 => $other_descendant_binary ) ){
									$frequencies{$other_descendant_positions} = $other_frequency;
									push @others, $other_descendant_binary;
							}
						}
						push @others, $descendant_binary;
						say $log "a,$ancestor_positions,$descendant_positions,$frequency,$codon,$marker" if $verbose;
						push @{$rules{$marker}{$codon}{$ancestor_positions}{$descendant_positions}}, 'a'; 
						for my $other_descendant_positions ( keys %frequencies ){
							my $other_frequency = $frequencies{$other_descendant_positions};
							say $log "c,$ancestor_positions,$descendant_positions,$other_frequency,$codon,$marker" if $verbose;
							push @{$rules{$marker}{$codon}{$ancestor_positions}{$other_descendant_positions}}, 'c';
						}
						push @{$self->{Codons_Of_Interest}}, {
							Begin => $ancestor_binary,
							End => [@others],
							Codon => $codon,
							Position => $position,
						};
						next; 
					}
					next if $frequency <= 1.0;
					my @others;
					for my $other_descendant_binary ( keys %{ $codon_transitions->{$ancestor_binary} } ){
						next if $other_descendant_binary eq $descendant_binary;
						my %other_count;
						for my $transition ( keys %{ $codon_transitions->{$ancestor_binary}{$other_descendant_binary}{$position}{transitions} } ){
							$other_count{[split /->/, $transition]->[0]} += $codon_transitions->{$ancestor_binary}{$other_descendant_binary}{$position}{transitions}{$transition};
						}
						next if !grep { $_ eq $codon } keys %other_count;
						my $other_frequency = 100.0 * $other_count{$codon} / $codon_transitions->{$ancestor_binary}{$other_descendant_binary}{$position}{count};
						if( $other_frequency > 1 ){
							push @others, $other_descendant_binary;
						}
					}
					next if @others;
					say $log "b,$ancestor_positions,$descendant_positions,$frequency,$codon,$marker";
					push @{$rules{$marker}{$codon}{$ancestor_positions}{$descendant_positions}}, 'b';
					push @{$self->{Codons_Of_Interest}}, {
						Begin => $ancestor_binary,
						End => [$descendant_binary],
						Codon => $codon,
						Position => $position,
					};
				}
			}
		}
	}

	$temp_file = File::Spec->rel2abs( $temp_file );
	open TEMP, ">$temp_file" or do{
		say $log "Error: couldn't open '$temp_file' for writing" if $verbose;
		return -1;
	};

	$self->generate_contingency_tables() and do {
		say $log "Error generating contingency tables" if $verbose;
		return -1;
	};
	my @header_values = (
		'rule',
		'ancestor',
		'descendant',
		'rare.codon',
		'position',
		'a',
		'b',
		'c',
		'd',
	);
	say TEMP join ',', @header_values;
	for my $data ( @{$self->{Codons_Of_Interest}} ){
		next if $data->{a} < $minimum;
		my $ancestor = join '', @{$self->binary_to_positions( Binary => $data->{Begin} )};
		my $descendant = join ';', map {join '', @{$self->binary_to_positions( Binary => $_ )}} @{$data->{End}};
		my $codon = $data->{Codon};
		my $position = $data->{Position};
		my $marker = join '', @{ $self->{Marker_Positions}->{($position / 3) - $self->{Offset}} };
		my @row_values = (
			join( ';', @{$rules{$marker}{$codon}{$ancestor}{$descendant}} ),
			$ancestor,
			$descendant,
			$codon,
			$marker,
			exists $data->{a} ? $data->{a} : 0,
			exists $data->{b} ? $data->{b} : 0,
			exists $data->{c} ? $data->{c} : 0,
			exists $data->{d} ? $data->{d} : 0,
		);
		say TEMP join ',', @row_values;
	}
	
	close TEMP;
	
	if( $r_script && $r_path ){
		my $r_command = qq("$r_path" --vanilla --slave --args input_file="$temp_file" < "$r_script");
		say $log $r_command if $verbose;
		`$r_command`;
	}
	
	open TEMP, $temp_file or do{
		say $log "Error: couldn't open '$temp_file' for reading";
		return -1;
	};
	while( my $line = <TEMP> ){
		print $output $line;
	}
	close TEMP;
	unlink $temp_file;
	
	undef $output;
	undef $log;
	
	return 0
	
}

sub get_summary_states{
	
	my $self = shift;
	my %parameters = @_;
	my %states;
	
	$self->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		return -1;
	};
	
	$self->setup_log( \my $log, $parameters{Log}, $self->{Log_File_Input}, \*STDERR ) and do{
		say STDERR "Error: could not set up log for output" if $verbose;
		return -1;
	};
	$self->setup_variable( \my $summary, $parameters{Summary}, $self->{Summary} );
	$self->setup_variable( \my $pruned, $parameters{Pruned}, $self->{Pruned}, 0 ) and return -1;
	if( !$summary ){
		if( $pruned ){
			$self->prune_summary( Log => $log, Verbose => $verbose );
			$summary = $self->{Pruned_Summary};
		}
		else{
			$self->calculate_summary( Log => $log, Verbose => $verbose );
			$summary = $self->{Summary};
		}
	}
	if( !$summary ){
		say $log "Error: can't find summary";
		return -1;
	}
	
	for my $ancestor_binary ( keys %$summary ){
		$states{$ancestor_binary}++;
		for my $descendant_binary( keys %{$summary->{$ancestor_binary}} ){
			$states{$descendant_binary}++;
		}
	}
	
	return \%states;
	
}

sub generate_contingency_tables{
	
	my $self = shift;
	$self->load_transitions if !exists $self->{Transitions};
	$self->load_clusters if !exists $self->{Clusters};
	$self->load_sequences if !exists $self->{Sequences};
	my $clusters = $self->{Clusters};
	my $transitions = $self->{Transitions};
	my $positions = $self->{Positions};
	my $sequences = $self->{Sequences};
	my $codons_of_interest = $self->{Codons_Of_Interest};
	my $binaries = $self->{Binaries};
	
	for my $patient ( keys %$transitions ){
		for my $ancestor ( keys %{$transitions->{$patient}} ){
			for my $descendant ( keys %{$transitions->{$patient}{$ancestor}} ){
				my $descendant_positions = join "", @{$positions->{$patient}{$descendant}};
				my $descendant_binary = join '', @{$binaries->{$patient}{$descendant}};
				for my $candidate_ancestor ( @{$clusters->{$patient}{$descendant}} ){
					my $candidate_ancestor_positions = join '', @{$positions->{$patient}{$ancestor}};
					my $candidate_ancestor_binary = join '', @{$binaries->{$patient}{$ancestor}};
					for my $check ( @$codons_of_interest ){
						$check->{a} = 0 unless $check->{a};
						$check->{b} = 0 unless $check->{b};
						$check->{c} = 0 unless $check->{c};
						$check->{d} = 0 unless $check->{d};
						next if $candidate_ancestor_binary ne $check->{Begin};
						my $position = $check->{Position};
						$check->{a}++ if grep( { $descendant_binary eq $_ } @{$check->{End}} ) && $sequences->{$patient}{$ancestor}{DNA}{$position} eq $check->{Codon};
						$check->{b}++ if grep( { $descendant_binary eq $_ } @{$check->{End}} ) && $sequences->{$patient}{$ancestor}{DNA}{$position} ne $check->{Codon};
						$check->{c}++ if !grep( { $descendant_binary eq $_ } @{$check->{End}} ) && $sequences->{$patient}{$ancestor}{DNA}{$position} eq $check->{Codon};
						$check->{d}++ if !grep( { $descendant_binary eq $_ } @{$check->{End}} ) && $sequences->{$patient}{$ancestor}{DNA}{$position} ne $check->{Codon};
					}
				}
			}
		}
	}
	
	return 0;
	
}

sub write_default_codons{
	my $self = shift;
	my %parameters = @_;
	my $output;
	my $log;
	my $variable_name = "Default_Codons_Output_File";
	my $headers = 1;
	return 1 if $self->setup_output( \$output, $parameters{Output}, $self->{$variable_name}, \*STDOUT );
	return 1 if $self->setup_output( \$log, $parameters{Log}, \*STDERR );
	my $marker_info = $self->{Marker_Info};
	my $summary = $self->{Summary};
	my $marker_positions = $self->{Marker_Positions};
	my @header_values = ( 'ancestor' );
	push @header_values, join '', @{$marker_positions->{$_}} foreach sort { $marker_positions->{$a}->[0] <=> $marker_positions->{$b}->[0] } keys %$marker_positions;
	print $output join( ',', @header_values ) . "\n" if $headers;
	for my $ancestor_binary ( sort keys %$summary ){
		my @row_values = ( join '', @{$self->binary_to_positions( Binary => $ancestor_binary )} );
		for my $position ( sort { $marker_positions->{$a}->[0] <=> $marker_positions->{$b}->[0] } keys %$marker_positions ){
			my $default_codon = [ sort {$marker_info->{$position}{Ancestor_States}{$ancestor_binary}{$b} <=> $marker_info->{$position}{Ancestor_States}{$ancestor_binary}{$a}} keys %{$marker_info->{$position}{Ancestor_States}{$ancestor_binary}} ]->[0];
			push @row_values, $default_codon;
		}
		print $output join( ',', @row_values ) . "\n";
	}
	return 0;
}

sub prune_codons_of_interest{
	my $self = shift;
	my $codons_of_interest =  $self->{"Codons_Of_Interest"};
	$self->{"Codons_Of_Interest"} = [];
	for my $i ( 0 .. $#{$codons_of_interest} ){
		my $codon = $codons_of_interest->[$i];
		for my $square ( qw(a b c d) ){
			if( !exists $codon->{$square} ){
				$codon->{$square} = 0;
			}
		}
		if(
			$codon->{a} + $codon->{b} == 0 or 
			$codon->{a} + $codon->{c} == 0 or 
			$codon->{b} + $codon->{d} == 0 or 
			$codon->{c} + $codon->{d} == 0
		){
			next;
		}
		push @{$self->{"Codons_Of_Interest"}}, $codon;
	}
	return 0;
}

sub count_recombinant_transitions{
	my $self = shift;
	my $transitions_directory = shift;
	my $positions = $self->{"Positions"};
	my %count;
	my $total = 0;
	my %not_count;
	my %total_count;
	opendir TRANSITIONS_DIRECTORY, $transitions_directory or die $!;
	while( my $base = readdir TRANSITIONS_DIRECTORY ){
		my $file = File::Spec->catfile( $transitions_directory, $base );
		next if $base !~ /^(\d+)\./;
		next if -d $file;
		my $patient = $1;
		open( FILE, $file ) or die $!;
		while( my $line = <FILE> ){
			chomp $line;
			next if !$line;
			next if $line =~ /[)(]/;
			$line =~ /^([^;]+);/;
			my $descendant = $1;
			my $is_recombinant = 0;
			$total++;
			if( $descendant =~ /^Recombinant:/ ){
				$is_recombinant = 1;
				$descendant =~ s/^Recombinant://;
				my $descendant_positions = join "", @{$positions->{$patient}{$descendant}};
				$count{$descendant_positions}++;
				$total_count{$descendant_positions}++;
			}
			else{
				my $descendant_positions = join "", @{$positions->{$patient}{$descendant}};
				$total_count{$descendant_positions}++;
				$not_count{$descendant_positions}++;
			}
		}
		close( FILE );
	}
	closedir TRANSITIONS_DIRECTORY;
	map {$count{$_} = 0 if !$count{$_}; $not_count{$_} = 0 if !$not_count{$_}; print "$_,$count{$_},$not_count{$_}\n";} sort keys %total_count ;
	print "$total\n";
	return 0;
}

sub get{
	my $self = shift;
	my $name = shift;
	my $value = exists $self->{$name} ? $self->{$name} : undef;
	return $value;
}

sub has_marker_in_common{
	my $self = shift;
	my %parameters = @_;
	my @a = split //, $parameters{Sequence1};
	my @b = split //, $parameters{Sequence2};
	for my $i ( 0 .. $#a ){
		if( $a[$i] eq $b[$i] ){
			return 1 unless $a[$i] eq '0';
		}
	}
	return 0;
}

1;

__END__
