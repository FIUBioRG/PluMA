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



#ifndef RR_H
#define RR_H

#include "Language.h"

#ifdef HAVE_R
#include "RInside.h"
#endif

namespace MiAMi {

class R : public Language {

   public:
      R(std::string language, std::string ext, std::string pp, int argc, char** argv);
      void executePlugin(std::string pluginname, std::string inputname, std::string outputname);
      void unload();
   private:
      RInside* myR;

};

}

#endif
