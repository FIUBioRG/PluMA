sub readParameters($$)
{
  my $inputfile = @_;
  my %retval;

  open (<DATA>, $inputfile) || die "File not found\n";
  while (<DATA>) {
     ($mykey, $myvalue) = split(/\t/, $_);
     $retval{$mykey} = $myvalue;
  }
  close(DATA);
  return($retval);
}

sub readSequential($$)
{
   my $inputfile = @_;
   my @retval;

   open (<DATA>, $inputfile) || die "File not found\n";
   chomp(@retval = <DATA>);
   close(DATA);
   return(@retval);
}

