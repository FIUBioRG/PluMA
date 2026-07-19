def readParameters(inputfile):
        parameters = dict()
        with open(inputfile, 'r') as infile:
            for line in infile:
                stripped = line.strip()
                if not stripped:
                    continue
                contents = stripped.split('\t')
                if len(contents) < 2:
                    print("Warning: skipping malformed parameter line in " + inputfile + ": " + repr(line))
                    continue
                parameters[contents[0]] = contents[1]
        return parameters

def readSequential(inputfile):
    retval = []
    with open(inputfile, 'r') as infile:
        for line in infile:
            retval.append(line.strip())
    return retval


