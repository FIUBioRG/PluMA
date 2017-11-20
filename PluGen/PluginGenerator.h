/********************************************************************************\

              Plugin-based Microbiome Analysis (PluMA, formerly MiAMi)

              Copyright (C) 2016 Bioinformatics Research Group (BioRG)
                        Florida International University

          This program is free software; you can redistribute it and/or
           modify it under the terms of the GNU General Public License
          as published by the Free Software Foundation; either version 3
              of the License, or (at your option) any later version.

         This program is distributed in the hope that it will be useful,
         but WITHOUT ANY WARRANTY; without even the implied warranty of
         MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                GNU General Public License for more details.

         You should have received a copy of the GNU General Public License
            along with this program; if not, write to the Free Software
           Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
                             02110-1335, USA.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

\*********************************************************************************/


#ifndef PLUGINGENERATOR_H
#define PLUGINGENERATOR_H

#include <string>
#include <vector>

class PluginGenerator {
   public:
      PluginGenerator(std::string, bool literal);
      void generate(std::string, std::vector<std::string>&);   
   private:
      void makeDirectory(std::string);
      void makeHeaderFile(std::string);
      void makeSourceFile(std::string, std::vector<std::string>&);

      std::string myPath;
      bool myLiteral;
};


#endif
