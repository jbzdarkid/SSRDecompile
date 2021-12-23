# Set up the final output buffer
output = ['|K|'] * 556 # First input is on frame 557
output[334] = '|K20|' # Play
output[404] = '|K20|' # Select file 1

# Read all of the inputs in the completed demo

input_map = {
  '':      '',
  'West':  '|K61|',
  'East':  '|K64|',
  'South': '|K73|',
  'North': '|K77|',
  'Undo':  '|K7A|',
}
with open('App/all.dem', 'r') as f:
  inputs = [input_map[line.strip()] for line in f]

# Read the data for which instruction happens on which frame
bits = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80]
with open('input_frames.txt', 'rb') as f:
  for byte in iter(lambda: f.read(1), b''):
    for bitmask in bits:
      if byte & bitmask:
        output.append(inputs.pop(0))
      else:
        output.append('|K|')

# Generate output file
import tarfile
import os

t = tarfile.open('Sausage.x86_64.ltm', 'w:gz')
t.addfile(tarfile.TarInfo('annotations.txt'))
t.add('config.ini')
t.add('editor.ini')
with open('inputs', 'w') as f:
  f.write('\n'.join(output))
t.add('inputs')
t.close()

# os.rename('Sausage.x86_64.tar.gz', 'Sausage.x86_64.ltm')
