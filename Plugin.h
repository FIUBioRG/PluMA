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

       Note: This particular file adapted from CompuCell, a software framework
              for multimodel simulations of biocomplexity problems
           (C) 2003 University of Notre Dame under license from GNU GPL

\*********************************************************************************/


#ifndef PLUGIN_H
#define PLUGIN_H

#include <string>

class Plugin {
  public:
  Plugin(){}
  //virtual std::string toString(){return "Plugin";}
  virtual ~Plugin(){}
  virtual void input(std::string file) {}
  virtual void run() {}
  virtual void output(std::string file) {}
};

#endif
