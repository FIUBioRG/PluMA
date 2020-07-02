#library(SpiecEasi)
library(phyloseq)
library(ape)
library(philr)


input <- function(inputfile) {
  parameters <<- read.table(inputfile, as.is=T);
  rownames(parameters) <<- parameters[,1]; 
   # Need to get the three files
   otu.path <- parameters["otufile", 2]
   tree.path <- parameters["tree", 2]
   map.path <- parameters["mapping", 2]
   print(otu.path)
   print(tree.path)
   print(map.path)
   HMP <<- import_qiime(otu.path, map.path, tree.path, parseFunction = parse_taxonomy_qiime)
}


run <- function() {
   samples.to.keep <- sample_sums(HMP) >= 1000
   HMP <<- prune_samples(samples.to.keep, HMP)
   HMP <<- filter_taxa(HMP, function(x) sum(x >3) > (0.01*length(x)), TRUE)
   phy_tree(HMP) <- makeNodeLabel(phy_tree(HMP), method='number', prefix='n')
   trans_output <<- philr(t(otu_table(HMP)), phy_tree(HMP), part.weights='enorm.x.gm.counts', ilr.weights='blw.sqrt', return.all=FALSE)
}


output <- function(outputfile) {
  # This at the moment does not include any information about OTUs
  # For now I am only interested in the structure of the network.
  # This *needs* to change eventually.
  write.table(trans_output, file=outputfile, sep=",", append=FALSE, na="");  
}



