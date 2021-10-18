import os
import sys

def run_folder(test_folder, exe_path, optimize_level):
    test_case_list = os.listdir(test_folder)
    test_case_list.sort()
    os.chdir(test_folder)
    dir = os.getcwd()
    print(dir)
    for test_case in test_case_list:
        test_case_name_split = test_case.split('.')
        if (len(test_case_name_split) != 2):
            continue;
        prefix = test_case_name_split[0]
        suffix = test_case_name_split[-1]
        if not os.path.exists(prefix):
            os.mkdir(prefix)
        if suffix == 'sy':
            print(test_case)
            os.system(exe_path + ' -S -o ' + prefix + '/debug.s' + ' ' + test_case + ' -O%d' % optimize_level)

if __name__ == '__main__':
    optimize_level = 2
    if len(sys.argv) >= 4:
        optimize_level = int(sys.argv[3])
    run_folder(sys.argv[1], sys.argv[2], optimize_level)