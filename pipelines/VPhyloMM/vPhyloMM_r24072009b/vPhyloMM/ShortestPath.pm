#/usr/bin/env perl

=head1 NAME

ShortestPath.pm

=head1 SYNOPSIS

	perl ShortestPath.pm --help | --to=<name> --from=<name>
                         --weight=<name> [--max_edges=<number>]
                         [--k=<number>] [--input=<file>] [--log=<file>]
                         [--output=<file>] [--forcenames=<0,1>]
                         [--round=<number>] [--headers=<0,1>] [--verbose]

    or

	use ShortestPath;
	
	my $weights = {
		'a' => { 'b' => .1 },
		'a' => { 'c' => .4 },
		'c' => { 'd' => .1 },
		'a' => { 'e' => .5 },
		'b' => { 'a' => .9 },
	};
	
	$weights->{'a'}{'e'} = .2;
	
	my $SP = ShortestPath->new();
	$SP->write_shortest_paths(
		Weights => $weights,
		K => 3,
		Max_Edges => 5,
		Round => 7,
		Headers = 1,
		Output => \*STDOUT,
		Log => \*STDERR,
	);

=head1 DESCRIPTION

This module provides methods for listing the B<k> shortest paths of each length 1 to B<max_edges>. The underlying algorithm is brute force; all possible paths of each requested length are found (using matrix multiplication) and then sorted first by source and then by weight. Cyclic paths are identified as those having duplicate vertices and are not considered for output.

=head1 INPUT

If run as a script from the command line, the input should be CSV data (headers optional) each of whose rows provides data (source, destination, and weight) about one edge in a network graph. If used as a module, the main input should be a reference to a hash which provides that same data. See the SYNOPSIS for an example. 

=head1 OUTPUT

The output is CSV data describing the B<k> shortest paths from each vertex in the input for each length 1 edge to B<max_edges>.

=head1 ARGUMENTS

=over 4

=item * to, from, weight

The values of these required aguments should be either names or numbers. If a name is given then the CSV data must have a header which lists that name. If a number is given then that number will be used as the zero-based column index unless B<forcenames> is passed.

=over 4

=item * to

Specify the column which contains the source vertices for each edge. 

=item * from

Specify column which contains the destination vertices for each edge.

=item * weight

Specify the name of the column which contains the weights for each edge.

=back

=item * forcenames

If 1, the values of B<to>, B<from>, and B<weight> will be treated as column names rather than indices (for instance if "--to=1 --forcenames=1" were passed the script would look for the column named "1" rather than use the column whose index is 1 for the source vertices).

=item * max_edges

The maximum number of edges the longest path in the output should have. For each source vertex B<k> edges of each length 1 edge to B<max_edges> will be found (if they exist) and output.

=item * k

How many of the shortest paths to print for each length for each source vertex.

=item * round

How many decimal places to round a weight. If not given, no rounding will be done.

=item * headers

If 0 then it is assumed that the input data have no header and B<to>, B<from>, and B<weight> must be integers. If not given, the default 1 will be used to indicate headerless input data.

=item * input

Specify from what file to read the graph information. If not given, standard input is used as the default.

=item * output

Specify to what file to write the shortest paths information. If not given, standard output is used as the default.

=item * log

Specify to which file a log of all codon transitions will be written. If not given, standard error will be used as the default.

=item * verbose

Display progress messages.

=item * help

Display the usage information for this script and exit.

=back

=head1 HISTORY

=head2 Author

Brice Cadwallader

=head2 Creation Date

2/5/09

=cut

package ShortestPath;

use strict;
use warnings;

use Getopt::Long qw(GetOptionsFromArray);
use Pod::Usage;
use File::Spec::Functions qw(:ALL);

__PACKAGE__->new()->run( @ARGV ) unless caller();

sub new{
	my $package = shift;
	my $sp = {};
	return bless $sp, $package;
}

sub run{
	
	my $self = shift;
	my $input_file = '';
	my $output_file = '';
	my $log_file = '';
	my $input;
	my $output;
	my $log;
	my $from = '';
	my $to = '';
	my $weight = '';
	my $k = '4';
	my $max_edges = '4';
	my $help = '';
	my $verbose = '';
	my $headers = '1';
	my $forcenames = '0';
	my $to_index = '';
	my $from_index = '';
	my $weight_index = '';
	my $round = '';
	my $weights = {};
	my %Pod2UsageParameters = ( # Parameters for Pod2Usage
		-message => "Try \"perldoc $0\" for more information.\n",
	    -exitval => 0,  
	    -verbose => 1,  
	    -output  => \*STDERR,
	);
	@_ or pod2usage(\%Pod2UsageParameters);
	# Parse arguments
	GetOptionsFromArray(
		\@_,
		"input=s" => \$input_file,
		"output=s" => \$output_file,
		"log=s" => \$log_file,
		"headers=s" => \$headers,
		"from=s" => \$from,
		"to=s" => \$to,
		"round=s" => \$round,
		"forcenames=s" => \$forcenames,
		"weight=s" => \$weight,
		"k=s" => \$k,
		"max_edges=s" => \$max_edges,
		"help" => \$help,
		"verbose" => \$verbose,
	) or pod2usage( \%Pod2UsageParameters );
	pod2usage( \%Pod2UsageParameters ) if $help;
	
	# Figure out where the log output is going to
	if( $log_file ){
		$log_file = rel2abs $log_file;
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
	
	# Figure out where the main output is going to
	if( $output_file ){
		$output_file = rel2abs $output_file;
		if( !open $output, '>' . $output_file ){
			print $log "Error: The file \"$output_file\" could not be opened: $!";
			return $!;
		}
		else{
			print $log "Writing to \"$output_file\"...\n" if $verbose;
		}
	}
	else{
		$output = \*STDOUT;
		print $log "Writing to standard output...\n" if $verbose;
	}
	
	# Figure out where the input is coming from
	if( $input_file ){
		$input_file = rel2abs $input_file;
		if( !open $input, '<' . $input_file ){
			print $log "Error: The file \"$input_file\" could not be opened: $!";
			return $!;
		}
		else{
			print $log "Reading from \"$input_file\"...\n" if $verbose;
		}
	}
	else{
		$input = \*STDIN;
		print $log "Reading from standard input...\n" if $verbose;
	}
	
	# figure out the column indices of to, from, and weight
	if( $headers ){
		my $header = <$input>;
		chomp $header;
		my @header_values = split /,/, $header;
		for my $i ( 0 .. $#header_values ){
			$to_index = $i if( ( $to !~ /^\d+$/ or $forcenames ) and ( $to eq $header_values[$i] ) );
			$from_index = $i if( ( $to !~ /^\d+$/ or $forcenames ) and ( $from eq $header_values[$i] ) );
			$weight_index = $i if( ( $to !~ /^\d+$/ or $forcenames ) and ( $weight eq $header_values[$i] ) );
		}
	}
	
	$to_index = $to if $to =~ /^\d+$/ and !$forcenames;
	$from_index = $to if $from =~ /^\d+$/ and !$forcenames;
	$weight_index = $to if $weight =~ /^\d+$/ and !$forcenames;
	
	# make sure all indices are accounted for
	for my $index ( $to_index, $from_index, $weight_index ){
		if( $index eq '' or ( $index =~ /^\d+$/ and $index < 0 ) ){
			print $log "Error: invalid index for from, to, or weight\n";
			pod2usage( \%Pod2UsageParameters );
		}
	}
	
	# build hash of weights
	while( my $line = <$input> ){
		chomp $line;
		my @values = split ',', $line;
		$weights->{$values[$from_index]}{$values[$to_index]} = $values[$weight_index];
	}
	undef $input;
	
	# run the shortest paths algorithm
	$self->write_shortest_paths(
		K => $k,
		Max_Edges => $max_edges,
		Output => \$output,
		Log => $log,
		Weights => $weights,
		Round => $round,
	);
	
	return 0;
	
}

sub write_shortest_paths{
	
	my $self = shift;
	my %parameters = @_;
	my $k = $parameters{K};
	my $round = $parameters{Round};
	my $max_edges = $parameters{Max_Edges};
	my $output = ${$parameters{Output}};
	my $weight_function = $parameters{Weight_Function};
	my $log = defined $parameters{Log} ? $parameters{Log} : \*STDERR;
	my $verbose = defined $parameters{Verbose} ? $parameters{Verbose} : 0;
	my $headers = defined $parameters{Headers} ? $parameters{Headers} : 1;
	my $MaxVertices = $max_edges + 1;
	$self->{Weight} = $parameters{Weights};
	$self->{Info} = {};
	$weight_function = sub { return shift; } if !$weight_function;
	my %Map;
	my @UniqueVertices;
	my %unique_vertices;
	my $Adjacency;
	
    print $log "Running ShortestPath->write_shortest_paths() ...\n" if $verbose > 1;
    
	# Print the header for the CSV output file
	if( $headers ){
		my @header_values = (
			'probability',
			'ancestor',
			'descendant',
			'edges',
			'vertices',
		);
		for my $i ( 1 .. $MaxVertices ){
			push @header_values, "vertex$i";
		}
		print $output join( ',', @header_values ) . "\n";
	}
	
	for my $ancestor ( keys %{$self->{Weight}} ){
		$unique_vertices{$ancestor} = 1;
		for my $descendant ( keys %{$self->{Weight}->{$ancestor}} ){
			$unique_vertices{$descendant} = 1;
		}
	}
	
	@UniqueVertices = keys %unique_vertices;
	
	my @EmptyList;
	for my $i (0 .. $#UniqueVertices){
		push(@EmptyList, []);
	}

	for my $i (0 .. $#UniqueVertices){
		push(@$Adjacency, [@EmptyList]);
	}
	
	# Map all of the vertices to a number
	for my $Vertex (sort(@UniqueVertices)){
		$Map{$Vertex} = scalar(keys(%Map));
	}
	$self->{Reverse_Map} = { reverse(%Map) };

	# Build the adjacency matrix
	for my $Vertex (@UniqueVertices){
		for my $Successor (keys %{$self->{Weight}->{$Vertex}} ){
			$Adjacency->[$Map{$Vertex}][$Map{$Successor}] = [$Map{$Successor},];
		}
	}
	
	$self->{Dimension} = scalar @UniqueVertices; ## size of the graph
	my $Result = $Adjacency;
	for my $i ( 2 .. $max_edges + 1 ) {
		my %entry;
		for my $path ( $self->print_paths( $Result ) ){
			my @path_array = split /,/, $path;
			my $Source = $path_array[0];
			my $Weight = $self->Weight( @path_array );
			my %sub_entry;
			$sub_entry{Weight} = $Weight;
			$sub_entry{Path} = \@path_array;
			push @{$self->{Info}->{$i}{$Source}{Paths}}, \%sub_entry;
		}
	   $Result = $self->matrix_mult( $Result, $Adjacency );
	}

	for my $Length ( sort keys %{$self->{Info}} ){
		my %Entry = %{$self->{Info}->{$Length}};
		for my $Source (sort keys(%Entry)){
			my $Printed = 0;
			my %SubEntry = %{$Entry{$Source}};
			my @Paths = sort( { $b->{"Weight"} <=> $a->{"Weight"} } @{$SubEntry{"Paths"}});
			for my $i (0 .. $#Paths){
				my @Path = @{$Paths[$i]->{"Path"}};
				my %DuplicateVertexChecker;
				my $ContainsDuplicate = 0;
				for my $Vertex (@Path){
					if(exists($DuplicateVertexChecker{$Vertex})){
						$ContainsDuplicate = 1;
						last;
					}
					else{
						$DuplicateVertexChecker{$Vertex} = 1;
					}
				}
				if ($ContainsDuplicate == 1){
					last;
				}
				my $Weight = $Paths[$i]->{"Weight"};
				my $format_string = "%.${round}f" if $round ne '';
				$Weight = sprintf( $format_string, $Weight ) if $round ne '';
				my $Source = $Path[0];
				my $Destination = $Path[$#Path];
				my $Vertices = $Length;
				my $Edges = $Length - 1;
				my $P = join(",", @Path);
				if( $Printed < $k ){
					print $output "$Weight,$Source,$Destination,$Edges,$Vertices,$P\n";
					$Printed++;
				}
			}
		}
	}

	return 0;
	
}

# Subroutine to figure out the weight of a given path
# Input: path given as array of vertices
# Output: product of the weights along the edges from each vertex to the next
sub Weight {
	
	my $self = shift;
	my $PathWeight = $self->{Weight}->{$_[0]}{$_[1]};
	
	for my $i (1 .. ($#_ - 1)){
		$PathWeight *= $self->{Weight}->{$_[$i]}{$_[$i + 1]};
	}
	
	return $PathWeight;
	
}

## the i,j entry of the matrix is a list of all the paths from i to j, but
## without "i," at the beginning, so we must add it
sub print_paths {
	
	my $self = shift;
    my $M = shift;
    my @paths;
    my @ReturnPaths;
    
    for my $i ( 0 .. $#{$M} ) {
      for my $j ( 0 .. $#{$M} ) {
        push @paths, map { "$i,$_" } @{ $M->[$i][$j] };
      }
    }
	
	for my $i ( 0 .. $#paths ){
    	my %duplicates;
    	my @vertices = split /,/, $paths[$i];
    	for my $vertex ( @vertices ){
    		if( exists $duplicates{$vertex} ){
    			delete $paths[$i];
    			next;
    		}
    		$duplicates{$vertex} = 1;
    	}
    }
    
    for my $path ( @paths ){
    	push @ReturnPaths, join( ",", map( $self->{Reverse_Map}->{$_}, split( /,/, $path  ) ) ) if $path;
    }
    
    return @ReturnPaths;
    
}

## modified matrix multiplication. instead of multiplication, we
## combine paths from i->k and k->j to get paths from i->j (this is why
## we include the endpoint in the path, but not the starting point).
## then instead of addition, we union all these paths from i->j
sub matrix_mult {
	
	my $self = shift;
    my ($A, $B) = @_;
    my $return_array;
    
    for my $i (0 .. $self->{Dimension}) {
      for my $j (0 .. $self->{Dimension}) {
        my @result;
        for my $k (0 .. $self->{Dimension}) {
          push @result, $self->combine_paths( $A->[$i][$k], $B->[$k][$j] );
        }
        $return_array->[$i][$j] = \@result;
      }
    }
    
    return $return_array;
    
}

## the sub to combine i->k paths and k->j paths into i->j paths --
## we simply concatenate with a comma in between.
sub combine_paths {
	
	my $self = shift;
    my ($x, $y) = @_;
    my @result; 
    
    for my $i (@$x) {
      for my $j (@$y) {
        push @result, "$i,$j";
      } 
    }   
    
    return @result;
    
}

1;
