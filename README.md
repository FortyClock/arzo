# Решение задач линейного программирования симплекс-методом

Тема № 9 индивидуального задания по дисциплине «Алгоритмы решения задач
оптимизации». Программа решает задачи максимизации и минимизации
`cᵀx` при `Ax ≤ b`, `x ≥ 0`, в том числе с отрицательными
правыми частями. Используется двухфазный табличный симплекс-метод.

## Сборка и проверка

Понадобятся CMake 3.20+ и компилятор с поддержкой C++20.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/simplex_cli experiments/data/input/worked-example.lp
```


При сборке Visual Studio исполняемый файл обычно находится в
`build/Release/simplex_cli.exe`. Пример возвращает решение
`[2,2]` со значением цели 10.

## Формат задачи

Заголовок `LP1 max 3 2` задаёт версию, направление, число ограничений и
число переменных. Следом идут коэффициенты цели. Каждое ограничение
содержит коэффициенты строки и её правую часть.

```text
LP1 max 3 2
3 2
1 1 4
1 0 2
0 1 3
```


Передайте файл аргументом или подайте его через стандартный ввод. Параметры
`--tolerance VALUE` и `--max-iterations N` настраивают допуск и
лимит поворотов. Результат выводится одной строкой JSON.

```json
{"status":"optimal","iterations":2,"objective":10,"variables":[2,2]}
```


Другие исходы и детали формата описаны в [контракте](context/CONTRACTS.md).
Для сравнения с SciPy/HiGHS см. [воспроизведение экспериментов](experiments/README.md).

## Отчёт

Точка входа — [report/main.tex](report/main.tex). Для сборки нужны LuaLaTeX,
`latexmk`, Biber, пакеты TeX Live (включая TikZ и Latin Modern Math)
и установленный Times New Roman.

```sh
cmake --build build --target report
```


Результат — `report/main.pdf`. Кафедра, группа и данные преподавателя
задаются в [report/title.tex](report/title.tex).

## Контекст проекта

Распределение работы и результаты проверок описаны в
[context/TASKS.md](context/TASKS.md) и [context/INTEGRATION.md](context/INTEGRATION.md).
