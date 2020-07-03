# PathwayFilter
# Language: Python
# Input: TXT
# Output: TXT
# Tested with: PluMA 1.1, Python 3.6
# Dependencies: CSV2GML Plugin, Pathway Tools Databse

Take a CSV and remove edges between nodes not found in Pathway Tools Database, and not involving the human (this is useful to also catch possible host interactions)

The plugin accepts as input a TXT file of keyword-value tab-delimited pairs:
correlationfile: CSV file for network
pathwayfile: TXT file of pathways present in PathwayTools.

The output CSV file contains all data from the input CSV file, but with all edges not found in the database zeroed out
