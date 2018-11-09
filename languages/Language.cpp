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


#include "Language.h"
#include <iostream>


#ifdef _WIN32
//needed for LoadLibrary
	#include <windows.h>

#else

#include <dlfcn.h>

#endif  



#ifdef _WIN32

#else

#endif

void Language::loadPlugin(std::string path, glob_t* globbuf, std::map<std::string, std::string>* pluginLanguages, bool list=false) {

#ifdef _WIN32

	std::string pathGlob = path + "\\" + "**\\*" + "Plugin." + extension;
	std::cout << pathGlob << std::endl;

#else // 


   std::string pathGlob = path + "/" + "*/*" + "Plugin." + extension;
# endif
   int ext_len = extension.length()+2;
   if (glob(pathGlob.c_str(), 0, NULL, &(*globbuf)) == 0) {
      for (unsigned int i = 0; i < globbuf->gl_pathc; i++) {

		  //ISSUE
        std::string filename = globbuf->gl_pathv[i];
		 //ISSUE

        std::string name;
#ifdef _WIN32
		
		std::string::size_type pos = filename.find_last_of("\\");

#else // _WIN32

        std::string::size_type pos = filename.find_last_of("/");
#endif
        if (pos != std::string::npos) name = filename.substr(pos + prefix.length()+1, filename.length()-pos-prefix.length()-ext_len);
        else name = filename.substr(prefix.length(), filename.length()-pos-ext_len);
        if (name == "__init__") continue;
        //std::cout << "Plugin: " << name.substr(0, name.length()-6) << " Language: " << language << " Path: " << path << std::endl;
        if (!list) {
           if (extension == "so") {
//Added for windows functionality
#ifdef _WIN32
			   HMODULE handle = LoadLibrary(filename.c_str());
			   if (handle == NULL) 
			   {
				   std::cout << "Warning: Null Handle" << std::endl;
				   std::cout << GetLastError() << std::endl;
			   }
#else

              void* handle = dlopen(filename.c_str(), RTLD_LAZY | RTLD_GLOBAL);
              if (!handle) {
                 std::cout << "Warning: Null Handle" << std::endl;
                 std::cout << dlerror() << std::endl;
              }
#endif
              else
                 (*pluginLanguages)[name] = language;
           }
           else
                (*pluginLanguages)[name] = language;
        }
      }
   }
   else {
      std::cout << "Found no " << language << " plugins" << std::endl;
   }
}
