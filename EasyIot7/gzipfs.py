Import( 'env', 'projenv' )

import os
import gzip
import shutil
import glob
from shutil import copy

def gzip_file( src_path, dst_path ):
    with open(src_path, 'rb') as src, gzip.open(dst_path, 'wb') as dst:
        for chunk in iter(lambda: src.read(4096), b""):
            dst.write(chunk)

def gzip_webfiles( source, target, env ):
    #suffixes to gzip & copy
    filetypes_to_gzip = ('css', 'html', 'js')

    data_dir_path = env.get('PROJECT_DIR') + os.path.sep + '..' + os.path.sep + 'html'
    gzip_dir_path = env.get('PROJECTDATA_DIR')

    print("Using HTML dir " + data_dir_path)
    print("Using Data dir " + gzip_dir_path)

    if not os.path.exists(data_dir_path):
        print('HTML directory missing: ' + data_dir_path)
        return
    
    if not os.path.exists(gzip_dir_path):
        print('Data directory missing: ' + gzip_dir_path)
        print('Attempting to create...')
        try:
            os.mkdir(gzip_dir_path)
        except Exception as e:
            print('Failed to create dir: ' + gzip_dir_path)
            return
    
    for root, subdirs, files in os.walk(os.path.join(data_dir_path)):
        for filename in files:
            if filename.startswith('.'):
                continue

            file_path = os.path.join(root, filename)
            dirAppend = root.replace(data_dir_path, '')
            targetDir = gzip_dir_path + dirAppend
            targetFile = filename + '.gz'
            targetFullPath = os.path.join(targetDir, targetFile)

            if not os.path.exists(targetDir):
                try:
                    os.mkdir(targetDir)
                except Exception as e:
                    print('Failed to create dir ' + targetDir + ' aborting execution')
                    return

            if os.path.exists(targetFullPath):
                os.remove(targetFullPath)

            if filename.lower().endswith(filetypes_to_gzip):
                print('Gzipping ' + file_path + ' to ' + targetFullPath)
                gzip_file(file_path, targetFullPath)
            else:
                print('Copying ' + file_path + ' to ' + targetDir)
                copy(file_path, targetDir)      
    return

env.AddPreAction('$BUILD_DIR/littlefs.bin', gzip_webfiles)