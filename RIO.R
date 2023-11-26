readParameters <- function(inputfile) {
  parameters <- read.table(inputfile, as.is=T);
  rownames(parameters) <- parameters[,1];
  return(parameters);
}

readSequential <- function(inputfile) {
   return(readLines(inputfile));
}
