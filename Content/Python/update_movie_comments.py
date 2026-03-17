"""
Update MovieComments.txt from:
  KEY: Comment.
to:
  KEY: Human Readable Title.|Comment.

FALLBACK line is left unchanged.
"""

import csv

project_root = "C:/Users/ethan/repos/weirdplace2"
csv_path = f"{project_root}/Content/CSVs/vhs_covers.csv"
comments_path = f"{project_root}/Content/Dialogue/MovieComments.txt"

# Load ID -> Name from CSV
id_to_name = {}
with open(csv_path, newline='', encoding='utf-8') as f:
    reader = csv.DictReader(f)
    for row in reader:
        id_to_name[row['ID'].strip()] = row['Name'].strip()

# Read current comments
with open(comments_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

output_lines = []
missing_keys = []

for line in lines:
    stripped = line.rstrip('\n')
    if not stripped:
        output_lines.append(stripped)
        continue

    # Split on first ": "
    if ': ' not in stripped:
        print(f"WARNING: malformed line, skipping: {stripped!r}")
        output_lines.append(stripped)
        continue

    key, comment = stripped.split(': ', 1)
    key = key.strip()
    comment = comment.strip()

    if key == 'FALLBACK':
        output_lines.append(stripped)
        continue

    name = id_to_name.get(key)
    if name is None:
        missing_keys.append(key)
        # Keep comment-only fallback so file stays valid
        output_lines.append(f"{key}: {comment}")
    else:
        output_lines.append(f"{key}: {name}.|{comment}")

with open(comments_path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(output_lines))

print(f"Done. Updated {comments_path}")
if missing_keys:
    print(f"WARNING: {len(missing_keys)} keys not found in CSV (kept as comment-only):")
    for k in missing_keys:
        print(f"  {k}")
