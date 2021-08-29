input_map = {
  '':      '',
  'West':  '61',
  'East':  '64',
  'r':     '72',
  'South': '73',
  'North': '77',
  'z':     '7a',
}
files = [
  ('1-0.dem',     135),
  ('1-1.dem',     0),
]
foo = [
  ('1-2.dem',     0),
  ('1-3.dem',     0),
  ('1-4.dem',     0),
  ('1-5.dem',     0),
  ('1-6.dem',     0),
  ('1-7.dem',     0),
  ('1-8.dem',     0),
  ('1-9.dem',     0),
  ('1-10.dem',    0),
  ('1-11.dem',    0),
  ('1-12.dem',    0),
  ('1-13.dem',    0),
  ('1-14.dem',    0),
  ('1-15.dem',    0),
  ('1-16.dem',    0),
  ('1-final.dem', 0),

  '2-1.dem',
  '2-2.dem',
  '2-3.dem',
  '2-4.dem',
  '2-5.dem',
  '2-6.dem',
  '2-7.dem',
  '2-8.dem',
  '2-9.dem',
  '2-10.dem',
  '2-final.dem',

  '3-1.dem',
  '3-2.dem',
  '3-3.dem',
  '3-4.dem',
  '3-5.dem',
  '3-6.dem',
  '3-7.dem',
  '3-8.dem',
  '3-9.dem',
  '3-10.dem',
  '3-11.dem',
  '3-12.dem',
  '3-13.dem',
  '3-14.dem',
  '3-final.dem',

  '4-1.dem',
  '4-2.dem',
  '4-3.dem',
  '4-4.dem',
  '4-5.dem',
  '4-6.dem',
  '4-final.dem',

  '5-1.dem',
  '5-2.dem',
  '5-3.dem',
  '5-4.dem',
  '5-5.dem',
  '5-6.dem',
  '5-7.dem',
  '5-8.dem',
  '5-9.dem',
  '5-10.dem',
  '5-11.dem',
  '5-12.dem',

  '6-1.dem',
  '6-2.dem',
  '6-2.5.dem',
  '6-3.dem',
  '6-3.5.dem',
  '6-4.dem',
  '6-5.dem',
  '6-6.dem',
  '6-7.dem',
  '6-8.dem',
  '6-8.5.dem',
  '6-9.dem',
  '6-9.5.dem',
  '6-10.dem',
  '6-10.5.dem',
  '6-11.dem',
  '6-11.5.dem',
  '6-12.dem',
  '6-13.dem',
  '6-14.dem',
  '6-15.dem',
  '6-15.5.dem',
  '6-16.dem',
  '6-17.dem',
  '6-17.5.dem',
  '6-18.dem',
  '6-18.5.dem',
  '6-19.dem',
  '6-19.5.dem',
  '6-20.dem',
  '6-20.5.dem',
  '6-21.dem',
  '6-22.dem',
  '6-22.5.dem',
  '6-23.dem',
  '6-23.5.dem',
  '6-24.dem',
  '6-24.5.dem',
  '6-25.dem',
  '6-25.5.dem',
  '6-26.dem',
  '6-26.5.dem',
  '6-27.dem',
  '6-27.5.dem',
  '6-28.dem',
  '6-final.dem',
]

output = ['|K|'] * 557
output[335] = '|K20|' # Play
output[404] = '|K20|' # Select file 1

# step_len = 166.667 # milliseconds
step_len = 170
frame_len = 1000 / 60 # millseconds
def add_inputs(inputs):
  until_next_step = 0
  for input in inputs:
    input = input.strip()
    if not input:
      continue
    key_code = '|K' + input_map[input] + '|'

    output.append(key_code)
    until_next_step += step_len
    while until_next_step > 0:
      output.append('|K|')
      # output.append(key_code)
      until_next_step -= frame_len

# Parse output from SSRDecompile
for file, after_file in files:
  with open('App/' + file, 'r') as f:
    add_inputs(f.read().split('\n'))

  output += ['|K|'] * after_file



# Bake output file
import tarfile
import os

t = tarfile.open('Sausage.x86_64.tar.gz', 'w:gz')
t.addfile(tarfile.TarInfo('annotations.txt'))
t.add('config.ini')
with open('inputs', 'w') as f:
  f.write('\n'.join(output))
t.add('inputs')
t.close()

os.rename('Sausage.x86_64.tar.gz', 'Sausage.x86_64.ltm')
