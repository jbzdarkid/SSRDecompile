import tarfile
import os

def main():
  # Parse timings, generate map of frame: input
  offset = None
  output = {
    334: 'Space', # Play
    404: 'Space', # Select file 1
  }

  with open('App/timings.txt', 'r') as f:
    for line in f:
      idx, frame, action = line.strip().split('\t')
      idx, frame = int(idx), int(frame)

      if offset == None:
        # First input is on frame 557, so adjust to match the first frame from the game.
        offset = idx - 557
      elif (frame - offset) in output: # There is already an input on this frame
        offset += 1 # Wait until next frame

      output[frame - offset] = action


  # Convert inputs into libtas format
  input_map = {
    'West':  '|K61|',
    'East':  '|K64|',
    'South': '|K73|',
    'North': '|K77|',
    'Undo':  '|K7A|',
    'Space': '|K20|',
  }
  input_list = ['|K|'] * (max(output.keys()) + 1)
  for key, value in output.items():
    input_list[key] = input_map[value]

  generate_output_file(input_list)


def generate_output_file(input_list):
  '''Generate libtas output file from a list of libtas keycodes'''

  t = tarfile.open('Sausage.x86_64.ltm', 'w:gz')
  t.addfile(tarfile.TarInfo('annotations.txt'))
  t.add('config.ini')
  t.add('editor.ini')
  with open('inputs', 'w') as f:
    f.write('\n'.join(input_list))
  t.add('inputs')
  t.close()


if __name__ == '__main__':
  main()
