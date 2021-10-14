'''
This script runs all experiments found in the provided folder. The provided folder needs to contain prepared packet
traces. with one folder per trace and experiment.
'''

from serial import Serial
import argparse
import os
import time


standard_port = '/dev/ttyUSB0'
standard_path = 'experiments/example_settings'
interrupt_trace = 'interrupt_trace.csv'
output_file = 'rx_times.csv'
project_path = 'esp_nic_evaluator'
trace2blob_path = 'trace2blob/trace2blob.sh'
packet_trace = 'packet_trace.csv'
nic_simulator = 'nic_simulator/main.py'
simulate = 1
run_on_esp = 1

# You can also customize that when invoking the app.
parser = argparse.ArgumentParser(description='Runs experiments.')
parser.add_argument('-p', default=standard_port, help='Sets the device port')
parser.add_argument('-e', default=standard_path, help='Path to experiments folder')
parser.add_argument('-s', default=simulate, help='Set 0 to skip NIC simulator')
parser.add_argument('-b', default=run_on_esp, help='Set 0 to skip build and run on esp32')
args = parser.parse_args()

# Export environment
os.system('. $HOME/esp/esp-idf/export.sh')

# Iterate though experiment folders.
root = args.e
for top, dirs, files in os.walk(root):

    # Run experiment in every folder containing a packet trace but no interrupt trace yet.
    if (packet_trace in files) and (output_file not in files):
        trace_file_path = top + '/' + interrupt_trace
        if args.s == 1:
            # Run NIC Simulator
            print('Running NIC simulator for ' + top)
            os.system('python ' + nic_simulator + ' ' + top + '/' + packet_trace + ' --config ' + top +
                      '/config.json --irqout ' + trace_file_path + ' --seqout ' + top + '/sequence.csv')

            # Create trace blob.
            print("Creating blob from interrupt trace.")
            os.system('cat ' + trace_file_path + ' | ' + trace2blob_path + ' > ' + top + '/trace.h')
        if args.b == 1:
            # Copy trace blob to project.
            print("Copying trace blob to project folder")
            os.system('cp ' + top + '/trace.h' + ' ' + project_path + '/main/')

            # Remove prior build.
            os.system('rm -r ' + project_path + '/build/')

            # Build and flash project.
            print('Building and flashing project '  + top)
            os.chdir(project_path)
            os.system('make flash > /dev/null')

            # Define output file.
            output = open(top + '/' + output_file, 'w', newline='')
            start_time = time.time()
            # Contact serial port.
            with Serial(args.p, 115200, timeout=60) as ser:
                while True:
                    try:
                        line = ser.readline().decode("utf-8")
                    except ValueError:
                        pass

                    # Remove usual output.
                    if line.startswith('END'):
                        output.close()
                        break

                    if line[:1].isdigit():
                        output.write(line)
                ser.__del__()
            print("--- %s seconds ---" % (time.time() - start_time))
print("All experiments run successfully.")
