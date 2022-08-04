FILENAME_BUILDNO = 'BuildNumber'
FILENAME_VERSION_H = 'include/version.h'
FILENAME_BOARD_REVISION = 'BoardRevision'
version = 'v1.2.1.'

import datetime
import sys
Import("env")

# print(env.Dump())
my_flags = env.ParseFlags(env['BUILD_FLAGS'])
defines = {k: v for (k, v) in my_flags.get("CPPDEFINES")}
print(defines)
try:
    board_revision = defines["BOARD_REVISION"]
    print("Board revision from ENV: %s" % str(board_revision))
except IndexError:
    board_revision = 'revC-1g'

build_no = 0
try:
    with open(FILENAME_BUILDNO) as f:
        build_no = int(f.readline()) + 1
except:
    print('Starting build number from 1..')
    build_no = 1
with open(FILENAME_BUILDNO, 'w+') as f:
    f.write(str(build_no))
    print('Build number: {}'.format(build_no))

try:
    with open(FILENAME_BOARD_REVISION, 'w+') as f:
        f.write(board_revision)
        print('Board revision: {}'.format(board_revision))
except IOError:
    print('Could not write board revision to file')

hf = """#ifndef BUILD_NUMBER
  #define BUILD_NUMBER "{}"
#endif
#ifndef VERSION
  #define VERSION "{} - {}"
#endif
#ifndef VERSION_SHORT
  #define VERSION_SHORT "{}"
#endif
""".format(build_no, version+str(build_no), datetime.datetime.now(), version+str(build_no))
with open(FILENAME_VERSION_H, 'w+') as f:
    f.write(hf)
