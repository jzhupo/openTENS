import os
import csv
from SCons.Script import Import

Import("env")

print("\n[Python-LittleFS]")

# 动态获取分区大小
def get_fs_size(env):
	fs_size = env.get("FS_SIZE")
	if fs_size:
		return int(fs_size)

	project_dir = env.get("PROJECT_DIR")
	for csv_name in ["tools/spiffs_4MB.csv", "no_ota.csv", "partitions.csv"]:
		csv_path = os.path.join(project_dir, csv_name)
		if os.path.isfile(csv_path):
			with open(csv_path, "r", encoding="utf-8") as f:
				for row in csv.reader(f):
					parts = [c.strip() for c in row if c.strip()]
					if len(parts) >= 5 and parts[1].lower() == "data" and parts[2].lower() in ["spiffs", "littlefs"]:
						size_text = parts[4]
						return int(size_text, 16) if size_text.lower().startswith("0x") else int(size_text, 10)
	return 0x140000

# 纯Python以UTF-8打包
def overwrite_littlefs(target, source, env):
	project_dir = env.get("PROJECT_DIR")
	data_dir = os.path.join(project_dir, "data")
	build_dir = env.subst("$BUILD_DIR")
	output_bin = os.path.join(build_dir, "littlefs.bin")
	fs_size = get_fs_size(env)

	print(f"Image: {output_bin}")
	print(f"Size: {hex(fs_size)} ({fs_size} bytes)")
	print(f"Folder: {data_dir}")

	try:
		from littlefs import LittleFS
	except ImportError:
		print("\n[Error] littlefs-python not installed! Run: & \"$env:USERPROFILE\\.platformio\\penv\\Scripts\\pip\" list | Select-String \"littlefs\"\n")
		raise

	block_size = 4096
	fs = LittleFS(block_size=block_size, block_count=fs_size // block_size)

	count = 0
	for root, _, files in os.walk(data_dir):
		for file in files:
			full_path = os.path.join(root, file)
			rel_path = "/" + os.path.relpath(full_path, data_dir).replace("\\", "/")
			print(f"  -> {rel_path}")
			parent_dir = os.path.dirname(rel_path)
			if parent_dir != "/":
				fs.makedirs(parent_dir, exist_ok=True)
			with open(full_path, "rb") as f:
				with fs.open(rel_path, "wb") as lfs_file:
					lfs_file.write(f.read())
			count += 1

	with open(output_bin, "wb") as f:
		f.write(fs.context.buffer)

	print(f"Python-LittleFS: Successfully with {count} files!")

# PostAction
target_bin = os.path.join(env.subst("$BUILD_DIR"), "littlefs.bin")
env.AddPostAction(target_bin, overwrite_littlefs)
