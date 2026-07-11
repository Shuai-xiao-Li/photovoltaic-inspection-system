import os
import re
import glob

qml_files = glob.glob('qml/**/*.qml', recursive=True)

def replacer(m):
    prefix = m.group(1)
    r = int(m.group(2)) / 255.0
    g = int(m.group(3)) / 255.0
    b = int(m.group(4)) / 255.0
    a = m.group(5)
    return f"{prefix}Qt.rgba({r:g}, {g:g}, {b:g}, {a})"

for f in qml_files:
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    
    # Only target non-Canvas usages: color:, border.color:, return, or ternary operators
    pattern = r'(color:\s*|return\s*|[:?]\s*)"rgba\((\d+),\s*(\d+),\s*(\d+),\s*([0-9.]+)\)"'
    
    new_content = re.sub(pattern, replacer, content)
    
    if content != new_content:
        with open(f, 'w', encoding='utf-8') as file:
            file.write(new_content)
        print(f"Fixed colors in {f}")

print("Color fixes applied successfully!")
