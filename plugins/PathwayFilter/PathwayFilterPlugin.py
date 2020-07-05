import sys
#import numpy
#from plugins.CSV2GML.CSV2GMLPlugin import *
from CSV2GML.CSV2GMLPlugin import *



class PathwayFilterPlugin(CSV2GMLPlugin):
   def input(self, filename):
      # Format expected:
      # correlationfile <somefile.csv>
      # pathwayfile <somefile.txt>
      thefile = open(filename, 'r')
      for line in thefile:
         myline = line.strip()
         entries = myline.split('\t')
         if (entries[0] == 'correlationfile'):
            self.myfile = entries[1]
         elif (entries[0] == 'pathwayfile'):
            self.mypathways = entries[1]
         # Ignore everything else

   def run(self):
      # Read CSV file
      filestuff = open(self.myfile, 'r')
      self.firstline = filestuff.readline().strip()
      self.bacteria = self.firstline.split(',')
      if (self.bacteria.count('\"\"') != 0):
         self.bacteria.remove('\"\"')
      self.n = len(self.bacteria)
      self.ADJ = []
      self.ADJ2 = []
      i = 0
      for line in filestuff:
         contents = line.split(',')
         self.ADJ.append([])
         self.ADJ2.append([])
         for j in range(self.n):
            value = float(contents[j+1])
            self.ADJ[i].append(value)
            if (i == j):
               self.ADJ2[i].append(1)
            else:
               self.ADJ2[i].append(0)
         i += 1
      # Read Pathways file
      pathwaystuff = open(self.mypathways, 'r')
      for line in pathwaystuff:
         myline = line.strip()
         myline = myline[myline.find('INVOLVES:')+9:]
         elements = myline.split('\t')
         while (elements.count('') != 0):
            elements.remove('')
         bioelements = []
         for item in elements:
            for j in range(len(self.bacteria)):
               if (item[0] == 'X' and item[1].isdigit()):
                  if (self.bacteria[j] == item or 
                      self.bacteria[j] == '"'+item+'"'): #Exactly matching metabolite
                     bioelements.append(j)
               elif (self.bacteria[j].find(item) != -1): #Containing bateria name
                  bioelements.append(j)
                  #print "FOUND BACTERIA: ", item, " AND ", self.bacteria[j]
         
        
         for j in range(len(bioelements)):
            for k in range(len(bioelements)):
               self.ADJ2[bioelements[j]][bioelements[k]] = 1

      for i in range(self.n):
         for j in range(self.n):
            self.ADJ[i][j] *= self.ADJ2[i][j]


   def output(self, filename):
      filestuff2 = open(filename, 'w')
      filestuff2.write(self.firstline+"\n")

      for i in range(self.n):
         filestuff2.write(self.bacteria[i]+',')
         for j in range(self.n):
            filestuff2.write(str(self.ADJ[i][j]))
            if (j < self.n-1):
               filestuff2.write(",")
            else:
               filestuff2.write("\n")



