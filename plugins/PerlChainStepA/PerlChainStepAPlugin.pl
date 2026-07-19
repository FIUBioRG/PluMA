use lib '.';
use PerlPluMA;

my %params;
my $outputfilename;

sub input {
  my $inputfile = $_[0];
  open(PARAMFILE, '<', $inputfile) || die "File not found\n";
  while (<PARAMFILE>) {
     my ($key, $value) = split(/\t/, $_);
     chomp($value);
     $params{$key} = $value;
  }
  close(PARAMFILE);
}

sub run {
  $main::stepMarker = "StepA";
}

sub output {
  $outputfilename = $_[0];
  open(OUTFILE, '>', $outputfilename) || die "Can't open $outputfilename\n";
  print OUTFILE "plugin=StepA marker=$main::stepMarker value=".$params{"value"}."\n";
  close(OUTFILE);
}
