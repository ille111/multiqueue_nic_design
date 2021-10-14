import argparse
import os

standard_path = 'no_dos'

parser = argparse.ArgumentParser(description='Cleans experiments folder.')
parser.add_argument('-e', default=standard_path, help='Path to experiments folder')
args = parser.parse_args()

# Iterate though experiment folders.
root = args.e
for top, dirs, files in os.walk(root):
    if dirs == ['figures'] or dirs == []:
        os.system('rm ' + top + '/interrupt_trace.csv > /dev/null')
        os.system('rm ' + top + '/interrupt_trace.stats.csv > /dev/null')
        os.system('rm ' + top + '/sequence.csv > /dev/null')
        os.system('rm ' + top + '/trace.h > /dev/null')
        os.system('rm ' + top + '/rx_times.csv > /dev/null')
        os.system('rm -r ' + top + '/figures > /dev/null')
        # os.system('rm ' + top + '/packet_trace.csv > /dev/null')
