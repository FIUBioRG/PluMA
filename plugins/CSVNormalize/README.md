# CSVNormalize
# Language: Python
# Input: CSV (unnormalized matrix)
# Output: CSV (matrix normalized across rows)
# Tested with: PluMA 1.1, Python 3.6
# Dependency: numpy==1.16.0

PluMA plugin that takes a CSV file and normalizes each row such that its values sum to 1.
Expected input file is the CSV file to normalize, and the output file is the same CSV file normalized.
Row and column names remain the same.
