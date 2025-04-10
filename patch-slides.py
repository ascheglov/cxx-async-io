import os

script_dir = os.path.dirname(os.path.realpath(__file__))

def ReadLines(path):
    with open(path) as f:
        return f.readlines()

dst = []
for line in ReadLines(script_dir+'/slides.in.md'):
    if line.startswith('!!! include '):
        _, _, path, sec = line.strip().split(' ')
        inc = ReadLines(script_dir+'/'+path)
        '''
        Use # to work around the formatter: 
        {
          // comment
        #// empty preprocessor directive with comment
        }
        '''
        begin = inc.index('#// SECTION BEGIN: '+sec+'\n')
        end = inc.index('#// SECTION END: '+sec+'\n')
        dst += inc[begin+1:end]
    else:
        dst.append(line)

with open(script_dir+'/slides/slides.md', 'w') as f:
    f.writelines(dst)
