/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018-2020, 2026 Bioinformatics Research Group (BioRG)
                       Florida International University

\*********************************************************************************/

#ifndef RPLUGINGENERATOR_H
#define RPLUGINGENERATOR_H

#include <string>
#include <vector>

class RPluginGenerator {
public:
    RPluginGenerator(std::string path, bool literal);
    void generate(std::string pluginname, std::vector<std::string>& command);

private:
    void makeDirectory(std::string pluginname);
    void makeRFile(std::string pluginname, std::vector<std::string>& command);
    void makeReadme(std::string pluginname, std::vector<std::string>& command);

    std::string myPath;
    bool myLiteral;
};

#endif
