import subprocess

out = subprocess.check_output(['tasklist'], text=True)
for line in out.splitlines():
    if 'conquer' in line.lower() or 'game' in line.lower() or 'ancient' in line.lower():
        print(line)
