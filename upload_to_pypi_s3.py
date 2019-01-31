from __future__ import print_function
import argparse
import os
import pip
import sys
import glob
import subprocess
import time
import traceback
import requests

this_file_dir = os.path.dirname(os.path.realpath(__file__))

def upload_to_pypi_s3_main(whl_folder,waitforserver_secs):

    assert(os.path.exists(whl_folder))
    whl_files = glob.glob(os.path.join(whl_folder,'*.whl'))
    if len(whl_files) <= 0:
        print('Nothing to do')
        return

    config_file = os.path.join(this_file_dir, 'local_pypi_server.ini')
    assert(os.path.exists(config_file))

    # Workaround pypi.s33d.cloud brittleness
    # pypi.s33d.cloud can cut the HTTPS for transfer that are too long
    # Daniel Lundin blames EA's network. This causes twine to restart
    # upload from zero. For large files this can happen often enough to ultimately fail.
    # On the other hand pypicloud[server] uses the AWS boto3 s3transfer API
    # and never fails. Our workaround is to start a local pypicloud[server]
    # the twine upload is very fast,
    pserve_process = subprocess.Popen(['pserve', config_file])

    try:
        twine_env = os.environ.copy()
        twine_env['TWINE_USERNAME'] = 'admin'
        twine_env['TWINE_PASSWORD'] = 's33d'
        time.sleep(waitforserver_secs) # sleep just to give the local server a head start - twine also retries when server is not there

        success = False
        for retry in range(1,3):
            try:
                for whl_file in whl_files:
                    subprocess.check_call(['twine','upload','--repository-url','http://localhost:6543/simple/',
                                           whl_file, '--skip-existing'], env=twine_env)
                success = True
                break
            except subprocess.CalledProcessError:
                time.sleep(retry*10)

        if not success:
            print('twine returned error code')

    finally:
        pserve_process.terminate()
        pserve_process.wait()

    try:
        # use pypi cloud GET API to rebuild the main server package list
        # i.e. the official pypi.s33d.cloud server
        print('Rebuild pypi.s33d.cloud package list')
        with requests.Session() as s:
            s.get('https://admin:s33d@pypi.s33d.cloud/admin/rebuild',verify=True)
    finally:
        pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Upload a whl file to s3.')
    parser.add_argument('whl_folder', type=str, help='Returns failure code - to test CI runner')
    parser.add_argument('--waitforserver', type=int, default=60, help='Returns failure code - to test CI runner')

    args = parser.parse_args()

    try:
        upload_to_pypi_s3_main(args.whl_folder, args.waitforserver)
    except:
        traceback.print_exc(file=sys.stdout)
        sys.exit(-1)
