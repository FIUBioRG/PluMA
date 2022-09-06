def readParameters(inputfile):
        infile = open(inputfile, 'r')
        parameters = dict()
        for line in infile:
            contents = line.strip().split('\t')
            parameters[contents[0]] = contents[1]
        return parameters

def readSequential(inputfile):
    retval = []
    infile = open(inputfile, 'r')
    for line in infile:
        retval.append(line.strip())
    return retval


