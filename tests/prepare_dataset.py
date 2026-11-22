
from pathlib import Path

source = Path("data")
target = Path("data_10000")

files = sorted(source.rglob("*.txt"))
needed = 10000 - len(files)

if needed <= 0:
    print("Документів уже достатньо")
    exit()

files.sort(key=lambda file: file.stat().st_size, reverse=True)
split_files = set(files[:needed])

for file in files:
    relative = file.relative_to(source)
    output = target / relative
    output.parent.mkdir(parents=True, exist_ok=True)

    if file in split_files:
        text = file.read_bytes()
        middle = len(text) // 2

        # Розділяємо за найближчим перенесенням рядка
        position = text.find(b"\n", middle)
        if position == -1:
            position = middle
        else:
            position += 1

        first = output.with_name(output.stem + "_part1.txt")
        second = output.with_name(output.stem + "_part2.txt")

        first.write_bytes(text[:position])
        second.write_bytes(text[position:])
    else:
        output.write_bytes(file.read_bytes())

print("Готово!")
print("Створено документів:", len(list(target.rglob("*.txt"))))
