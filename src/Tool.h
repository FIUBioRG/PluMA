/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018-2020 Bioinformatics Research Group (BioRG)
                       Florida International University


     Permission is hereby granted, free of charge, to any person obtaining
          a copy of this software and associated documentation files
        (the "Software"), to deal in the Software without restriction,
      including without limitation the rights to use, copy, modify, merge,
      publish, distribute, sublicense, and/or sell copies of the Software,
       and to permit persons to whom the Software is furnished to do so,
                    subject to the following conditions:

    The above copyright notice and this permission notice shall be included
            in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
     IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
     CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
           SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

       Note: This particular file adapted from CompuCell, a software framework
              for multimodel simulations of biocomplexity problems
           (C) 2003 University of Notre Dame under license from GNU GPL

\*********************************************************************************/


#ifndef TOOL_H
#define TOOL_H

#include <string>
#include <fstream>
#include <map>

class Tool {
public:
    Tool(){myCommand = "";}
    //virtual std::string toString(){return "Plugin";}
    virtual ~Tool(){}
    virtual void addRequiredParameterNoFlag(std::string parameter) {
           myCommand += " " + myParameters[parameter] + " ";
	   }
    virtual void addOptionalParameterNoFlag(std::string parameter) {
    if (myParameters.count(parameter) != 0)
           myCommand += " "+myParameters[parameter]+" ";
    }
    virtual void addRequiredParameter(std::string flag, std::string parameter) {
           myCommand += " "+flag+" "+myParameters[parameter]+" ";
	   }
    virtual void addOptionalParameter(std::string flag, std::string parameter) {
    if (myParameters.count(parameter) != 0)
           myCommand += " "+flag+" "+myParameters[parameter]+" ";
    }
    virtual void readParameterFile (std::string inputfile)
    { 
    std::ifstream ifile(inputfile.c_str(), std::ios::in);
    while (!ifile.eof()) {
   std::string key, value;
   ifile >> key;
   ifile >> value;
   myParameters[key] = value;
 }
 }
    protected:
std::map<std::string, std::string> myParameters;
std::string myCommand;
};

#endif
