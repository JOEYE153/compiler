import os
import sys
import subprocess

def run_folder(test_folder, optimize_level):
    test_case_list = os.listdir(test_folder)
    test_case_list.sort()
    os.chdir(test_folder)
    dir = os.getcwd()
    print(dir)
    for test_case in test_case_list:
        if not os.path.isdir(test_case):
            continue;
        print(test_case)
        os.chdir(test_case)
        os.system("gcc -O%d -march=armv7ve -S test.c -o gcc_O%d.s -Wno-implicit-function-declaration" % (optimize_level, optimize_level))
        if not os.path.exists('gcc_O%d.s' % optimize_level):
            os.chdir('..')
            continue
        os.system("gcc gcc_O%d.s /home/pi/libsysy.a -o gcc_O%d" % (optimize_level, optimize_level))      
        input_file = None
        has_input = os.path.exists('in.txt')
        if has_input:
            input_file = open('in.txt', 'r')
        p = subprocess.run(['./gcc_O%d' % optimize_level], stdin=input_file ,stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        if has_input:
            input_file.close()
        out_file = open('out.txt', 'r')
        std_out = out_file.read().strip()
        gcc_out = (str(p.stdout).strip() + '\n' + str(p.returncode)).strip()
        print(std_out)
        print(gcc_out)
        print(std_out == gcc_out)
        os.chdir('..')
    os.chdir(dir)

if __name__ == '__main__':
    if len(sys.argv) >= 3:
        run_folder(sys.argv[1], sys.argv[2])
    else:
        for optimize_level in range(4):
            print('----------------------------------O%d----------------------------------')
            run_folder(sys.argv[1], optimize_level)