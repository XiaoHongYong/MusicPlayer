################################################################################
#
# Build MusicPlayer
#
# Cmd line:
# build.py [action]
# action = /clean, /rebuild, or /build, default for /build
#
################################################################################

import sys
import os
import subprocess
import time
import shutil
import re


VERSION_FN = 'version.h'
WORK_DIR = os.path.abspath(os.path.dirname(__file__))

def err_exit(err):
	print(err)
	sys.exit(-1)

def execute(cmd):
	print(cmd)
	p = subprocess.Popen(cmd)
	p.wait()
	return p.returncode

def execute_expect_ok(cmd):
	print('Execute command:', cmd)
	p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=True)
	stdout, stderr = p.communicate()

	if p.returncode:
		print('Error occured, while execute the following command')
		print(stdout)
		print(stderr)
		sys.exit(-1)

	print('Successfully.')

	return stdout

def read_file(fn):
	with open(fn) as fp:
		return fp.read()

def write_file(fn, text):
	if os.path.exists(fn):
		old = read_file(fn)
		if old == text:
			return

	with open(fn, 'w') as fp:
		fp.write(text)

def get_feild_int_value_in_file(file_name, field_name):
	ftext = read_file(file_name)

	FIELD_PATTERN = r'(#define\s*{0}\s*)(\d+)'
	field_pattern = FIELD_PATTERN.format(field_name)
	ptn = re.compile(field_pattern)
	m = ptn.search(ftext)
	if m is None:
		err_exit(r'Can\'t find pattern ' + field_pattern + ' in file: ' + file_name)
	return int(m.groups()[1])

def replace_field_str_in_file(file_name, name, value):
	ftext = read_file(file_name)

	FIELD_PATTERN = r'(#define\s*{}\s*)"(.*?)"'.format(name)
	ptn = re.compile(FIELD_PATTERN)
	m = ptn.search(ftext)
	if m is None:
		err_exit(r'Can\'t find pattern ' + FIELD_PATTERN + ' in file: ' + file_name)

	data = ftext[:m.start()] + m.groups()[0] + '"' + value + '"' + ftext[m.end():]

	write_file(file_name, data)

def get_git_commit():
	return execute_expect_ok('git log -1 --format=%H').strip()[0:7]

def version_str(*args):
	a = []
	for i in args:
		a.append(str(i))
	return '.'.join(a)

def compress(input, output_fn):
	zip_exe = r'C:\Program Files\7-Zip\7z.exe'
	if not os.access(zip_exe, 0):
		zip_exe = r'D:\Program Files\7-Zip\7z.exe'
		if not os.access(zip_exe, 0):
			err_exit(r'Please install 7z at C:\Program Files\7-Zip\7z.exe.')

	compress([zip_exe, 'a', '-t7z', output_fn, input])

def build_player(build_action):
	major_ver = get_feild_int_value_in_file(VERSION_FN, 'MAJOR_VERSION')
	minor_ver = get_feild_int_value_in_file(VERSION_FN, 'MINOR_VERSION')
	build_num = get_git_commit()
	version = version_str(major_ver, minor_ver, build_num)

	replace_field_str_in_file(VERSION_FN, 'BUILD', build_num)
	replace_field_str_in_file(VERSION_FN, 'VERSION_STR', version)

	pack_release_dir = os.path.join(r'..\Release', version) + '\\'
	print(r'Version: ' + version)
	print(r'Release dir: ' + pack_release_dir)
	if not os.path.exists(pack_release_dir):
		os.makedirs(pack_release_dir)

	dev_env_exe = 'devenv.com'
	# cmd = [dev_env_exe, 'build/MusicPlayer.sln', build_action, '"Release|x64"', '/project', 'MusicPlayer']
	cmd = 'devenv.com build/MusicPlayer.sln /build "Release|x64" /project MusicPlayer'
	execute_expect_ok(cmd)

	if build_action == '/clean':
		sys.exit(0, 'All projects have been cleaned.')

	#
	# Create installation package
	#

	# Create Version.nsh
	with open(r'MusicPlayer\win32\version.nsh', 'w') as fp:
		fp.write('; This file was generated by build.py, don\'t modify it manually.\r\n')
		fp.write('!define VERSION	"' + version + '"')

	# make pack: MusicPlayer.exe
	execute_expect_ok(r'third-parties\NSIS\makensis.exe /X"SetCompressor /FINAL /SOLID lzma" MusicPlayer\win32\install.nsi')

	# backup files to release dir
	execute('cmd /c del ' + pack_release_dir + '*.* /q')

	shutil.move(r'MusicPlayer\win32\MusicPlayer.exe', pack_release_dir)

	# compress(r'build\release\MusicPlayer.pdb', pack_release_dir + r'MusicPlayer-pdb.7z')


if __name__ == '__main__':
	#
	# Build solution
	#
	build_action = '/build'

	if '/rebuild' in sys.argv: build_action = '/rebuild'
	elif '/clean' in sys.argv: build_action = '/clean'

	os.chdir(WORK_DIR)
	
	if not os.path.exists('build'):
		os.makedirs('build')

	execute_expect_ok('cd build && cmake -G "Visual Studio 17 2022" ..')

	try:
		# 备份 version.h
		version_backup = read_file(VERSION_FN)
		build_player(build_action)
	finally:
		write_file(VERSION_FN, version_backup)
