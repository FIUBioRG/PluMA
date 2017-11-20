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

               Note: This particular file adapted from BasicUtils
               (C) 2003 Joseph Coffland under license from GNU GPL
\*********************************************************************************/





#ifndef PLUGINPROXY_H
#define PLUGINPROXY_H

#include "PluginMaker.h"
#include "PluginManager.h"

#include <string>

class Proxy {};

template<class T>
class PluginProxy : public Proxy
{
   public:
   PluginProxy(std::string keyword, PluginManager& mgr) {mgr.addMaker(keyword, new PluginMaker<T>());};
};

#endif
