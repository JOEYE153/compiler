import os
import sys

def run_folder(test_folder):
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
            os.system("ln -s " + dir + '/' + test_case + ' ' + prefix + '/test.c')
            print(test_case)
        elif suffix == 'out':
            os.system("ln -s " + dir + '/' + test_case + ' ' + prefix + '/out.txt')
            print(test_case)
        elif suffix == 'in':
            os.system("ln -s " + dir + '/' + test_case + ' ' + prefix + '/in.txt')
            print(test_case)

if __name__ == '__main__':
    run_folder(sys.argv[1])