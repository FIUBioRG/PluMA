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



#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <string>
#include <map>
#include <glob.h>


class Language {
  
   public:
      Language(std::string lang, std::string ext, std::string pp, std::string pre="") {language = lang; extension = ext; pluginpath = pp; prefix = pre;}
      virtual void loadPlugin(std::string path, glob_t* globbuf, std::map<std::string, std::string>* pluginLanguages, bool list);
      virtual void executePlugin(std::string pluginname, std::string inputfile, std::string outputfile)=0;
      virtual void unload()=0;
      virtual std::string ext() {return extension;}
      virtual std::string lang() {return language;}
      virtual std::string pre() {return prefix;}
 
   protected:
      std::string language;
      std::string extension;
      std::string prefix;
      std::string pluginpath;
};

#endif
