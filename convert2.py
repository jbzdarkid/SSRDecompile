input_map = {
  '':      '',
  'West':  '61',
  '2':     '61',
  'East':  '64',
  '3':     '64',
  'r':     '72',
  'South': '73',
  '1':     '73',
  'North': '77',
  '0':     '77',
  'z':     '7a',
}

"""
Game.Update
48 81 EC 70 03 00 00 48 8B F1

Game.DoPlayerInput Direction={rax}
48 83 C4 20 48 8B F8 83 FF 08

Game.DoUndo
55 48 8B EC 56 57 48 81 EC 90

LoaderSaver.Update() -> this.level.DoRestart() [Note, this is on a separate Update loop!]
55 48 8B EC 56 57 41 56 41 57 48 81 EC 00
"""

output = ['|K|'] * 557
output[335] = '|K20|' # Play
output[404] = '|K20|' # Select file 1

with open('x64_dbg_log.txt', 'r') as f:
    contents = f.read().split('\n')

for i in range(len(contents)):
    row = contents[i]
    if row == 'Game.Update':
        output.append('|K|')
    elif row.startswith('Game.DoPlayerInput'):
        direction = row.split('=')[1]
        key_code = '|K' + input_map[direction] + '|'
        output[-1] = key_code


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
