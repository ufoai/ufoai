#!/usr/bin/env python
"""
Crossplatform helper to compile *.map files to *.bsp with ufo2map

Features:
  * only changed .map files are compiled (checks timestamp)

License: Public domain
Authors:
  anatoly techtonik <techtonik@gmail.com>
"""

import os
import subprocess
import sys
from os.path import abspath, dirname, exists, getmtime


__version__ = '0.1'

# UFOAI source checkout
UFOROOT = abspath(abspath(dirname(__file__)) + '/../../') + '/'
# base directory with ending slash
BASEDIR = 'base/'
# maps directory, relative to 'base/' directory for some reason
# see bug #3441: https://sourceforge.net/p/ufoai/bugs/3441/
MAPSDIR = 'maps/'
FLAGS = '-soft'
# ufo2map path
UFO2MAP = UFOROOT + 'ufo2map'

# --- functions library --- 

def get_maps(path, verbose=True):
  ''' Return three lists (all, not compiled, updated) of .map
      files from the given path '''
  all, notcomp, updated = [], [], []
  for root, dirs, files in os.walk(path):
    # print(root.replace(path, '')[1:])
    for x in files:
      if x.endswith('.bsp'):
        continue
      if x.endswith('.ump'):
        continue
      elif x.endswith('.map'):
        bspname = x[:-4] + '.bsp'
        bspfile = root + '/' + bspname
        mapfile = root + '/' + x
        all.append(mapfile)
        if not exists(bspfile):
          # print('  ' + x + ' (not compiled)')
          notcomp.append(mapfile)
        elif getmtime(mapfile) > getmtime(bspfile):
          # print('  ' + x + ' (updated)')
          updated.append(mapfile)
        else:
          # print('  ' + x)
          pass
      else:
        if verbose:
          print('warning: unknown file - ' + x)
  return all, notcomp, updated


def runret(command):
  """ Run command through shell, return ret code """
  print(command)
  return subprocess.call(command, shell=True)

# --- /functions ---

"""
[ ] check dirs exist
[ ] check ufo2map exists
[ ] detect features .bsp was compiled with to compare with given settings
"""

if __name__ == '__main__':
  all, notcomp, updated = get_maps(UFOROOT + BASEDIR + MAPSDIR)
  print('Total: %s  Not compiled: %s  Updated: %s'
            % (len(all), len(notcomp), len(updated)))
  for i,m in enumerate(notcomp + updated):
    name = m.replace(UFOROOT + BASEDIR + MAPSDIR, '')[1:]
    print('--- Compiling %s/%s - %s' % (i+1, len(notcomp + updated), name))
    res = runret(UFO2MAP + ' ' + FLAGS + ' ' + m.replace(UFOROOT + BASEDIR, ''))
    if res != 0:
      print('Error: ufo2map returned bad result - %s' % res)
      sys.exit(res)
