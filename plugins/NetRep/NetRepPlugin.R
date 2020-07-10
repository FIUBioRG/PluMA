library("NetRep")

input <- function(inputfile) {
  parameters <<- read.table(inputfile, as.is=T);
  rownames(parameters) <<- parameters[,1];

  discovery_data <- read.csv(toString(parameters["data1", 2]));
  rownames(discovery_data) <- discovery_data[,1]
  discovery_data <- discovery_data[,-1]
  discovery_data <- as.matrix(discovery_data)

  test_data <- read.csv(toString(parameters["data2", 2]));
  rownames(test_data) <- test_data[,1]
  test_data <- test_data[,-1]
  test_data <- as.matrix(test_data)

  discovery_correlation <- read.csv(toString(parameters["corr1", 2]));
  rownames(discovery_correlation) <- discovery_correlation[,1]
  discovery_correlation <- discovery_correlation[,-1]
  discovery_correlation <- as.matrix(discovery_correlation)

  test_correlation <- read.csv(toString(parameters["corr2", 2]));
  rownames(test_correlation) <- test_correlation[,1]
  test_correlation <- test_correlation[,-1]
  test_correlation <- as.matrix(test_correlation)

  discovery_network <- read.csv(toString(parameters["network1", 2]));
  rownames(discovery_network) <- discovery_network[,1]
  discovery_network <- discovery_network[,-1]
  discovery_network <- as.matrix(discovery_network)

  test_network <- read.csv(toString(parameters["network2", 2]));
  rownames(test_network) <- test_network[,1]
  test_network <- test_network[,-1]
  test_network <- as.matrix(test_network)

  module_labels <<- read.csv(toString(parameters["modulelabels", 2]));
  tmp <<- module_labels[,1]
  module_labels <<- module_labels[,-1]
  module_labels <<- as.vector(module_labels)
  names(module_labels) <<- tmp

  data_list <<- list(cohort1=discovery_data, cohort2=test_data)
correlation_list <<- list(cohort1=discovery_correlation, cohort2=test_correlation)
network_list <<- list(cohort1=discovery_network, cohort2=test_network)
}

run <- function() {
  print(data_list)
  preservation <<- modulePreservation(
 network=network_list, data=data_list, correlation=correlation_list, 
 moduleAssignments=module_labels, discovery="cohort1", test="cohort2", 
 nPerm=10000, nThreads=2
)
}

output <- function(outputfile) {
   write.csv(preservation$observed, paste(outputfile, "observed.csv", sep=".")) 
   write.csv(preservation$p.value, paste(outputfile, "pvalue.csv", sep="."))
}
