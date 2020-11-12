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
#include <vector>
#include <string>

#ifdef _WIN32
//needed for LoadLibrary
#include <windows.h>

#else

#include <dlfcn.h>

#endif  



#ifdef _WIN32

#else

#endif

void Language::loadPlugin(std::string path, glob_t* globbuf, std::map<std::string, std::string>* pluginLanguages, bool list = false) {

#ifdef _WIN32

	WIN32_FIND_DATAA FindFileData;
	HANDLE hFindFile;

	std::string pathGlob = path + "\\*"; //adding wildcard to path to find all directories inside of plugin folder
	bool pluginFound = false;
	std::vector<std::string> pluginPaths;

	//call to glob fuction to retrieve all directories inside of plugin folder
	if (glob(pathGlob.c_str(), 0, NULL, globbuf) == 0)
	{
		for (unsigned int i = 0; i < globbuf->gl_pathc; i++)
		{
			std::string tempName = globbuf->gl_pathv[i];
			//FindFirstFile call to check which directory returned contains files of specific plugins based on their extension
			hFindFile = FindFirstFileA((tempName + "\\*Plugin." + extension).c_str(), &FindFileData);
			if (INVALID_HANDLE_VALUE == hFindFile)
			{
				/* std::cout << "Error in Finding File" << std::endl;
				 std::cout << "Error - " << GetLastError() << std::endl;*/
			}
			else
			{
				//std::cout << "File found" << std::endl;										Testing Purposes ************
				//std::wcout << "File Name - " << FindFileData.cFileName << std::endl;			Testing Purposes ************
				std::string tempFile = FindFileData.cFileName;
				std::string tempFileToString(tempFile.begin(), tempFile.end());
				std::string tempPath = tempName + "\\" + tempFileToString;
				//std::cout << "PRINTING PATH: " + tempPath << std::endl;						Testing Purposes ************
				pluginPaths.push_back(tempPath);
				pluginFound = true;
			}
		}
	}

#else // 
	std::string pathGlob = path + "/" + "*/*" + "Plugin." + extension;

# endif
	int ext_len = extension.length() + 2;
#ifdef _WIN32														// Changes made from STARTING here ***********
	if (pluginFound == true) {
		for (unsigned int i = 0; i < pluginPaths.size(); i++) {
			std::string filename = pluginPaths[i];
			std::cout << filename << std::endl;
#else
	if (glob(pathGlob.c_str(), 0, NULL, &(*globbuf)) == 0) {
		for (unsigned int i = 0; i < globbuf->gl_pathc; i++) {
			std::string filename = globbuf->gl_pathv[i];
#endif																// Changes made from ENDING here ***********
			std::string name;
#ifdef _WIN32

			std::string::size_type pos = filename.find_last_of("\\");

#else // _WIN32

			std::string::size_type pos = filename.find_last_of("/");
#endif
			if (pos != std::string::npos) {									// Changes to logic to find plugin name START here ***********
				name = filename.substr(pos + 1, filename.length() - pos - ext_len);
				std::cout << " Plugin Name: " << name << std::endl;			// Changes to logic to find plugin name END here ***********
			}
			else {
				name = filename.substr(prefix.length(), filename.length() - pos - ext_len);
			}
			if (name == "__init__") continue;
			//std::cout << "Plugin: " << name.substr(0, name.length()-6) << " Language: " << language << " Path: " << path << std::endl;
			if (!list) {
				if (extension == "dll") {
//Added for windows functionality
#ifdef _WIN32
					HMODULE handle = LoadLibraryA(filename.c_str());
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

#ifdef _WIN32
	pluginPaths.clear();
	pluginFound = false;
	std::cout << "***********************************************************************************" << std::endl;
#else
#endif

}
