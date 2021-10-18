import os
import sys
import time
import subprocess

def run_folder(test_folder):
    test_case_list = os.listdir(test_folder)
    test_case_list.sort()
    os.chdir(test_folder)
    dir = os.getcwd()
    print(dir)
    not_ac = 0
    for test_case in test_case_list:
        if not os.path.isdir(test_case):
            continue;
        print(test_case)
        os.chdir(test_case)
        if not os.path.exists('debug.s'):
            os.chdir('..')
            print('CE')
            not_ac += 1
            continue
        os.system("gcc debug.s /home/pi/libsysy.a -o debug")
        if not os.path.exists('./debug'):
            os.chdir('..')
            print('CE')
            not_ac += 1
            continue
        input_file = None
        has_input = os.path.exists('in.txt')
        if has_input:
            input_file = open('in.txt', 'r')
        t0 = time.time()
        try:
            p = subprocess.run(['./debug'], stdin=input_file ,stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, timeout=60)
        except:
            print('TLE')
            not_ac += 1
        else:
            out_file = open('out.txt', 'r')
            std_out = out_file.read().strip()
            test_out = (str(p.stdout).strip() + '\n' + str(p.returncode)).strip()
            test_err = str(p.stderr).strip()
            if std_out == test_out:
                print('AC')
                print(test_err)
            else:
                print('right')
                print(std_out)
                print('wrong')
                print(test_out)
                print('WA')
                not_ac += 1
        finally:
            if has_input:
                input_file.close()
            os.chdir('..')
    os.chdir(dir)
    print('not_ac = %d' % not_ac)

if __name__ == '__main__':
    run_folder(sys.argv[1])