# Решение задач линейного программирования симплекс-методом

**Часть 1, тема № 9.** Отчёт студентов группы 2304 по дисциплине
«Алгоритмы решения задач оптимизации». В документе изложены постановка
задачи, обзор методов решения и предварительное обоснование выбора
симплекс-метода. Перечень использованных источников завершает отчёт.

## Сборка отчёта

Нужны CMake, LuaLaTeX, `latexmk`, Biber, пакеты TeX Live
(включая TikZ и Latin Modern Math) и установленный Times New Roman.

```sh
cmake -S . -B build
cmake --build build --target report
```

Точка входа — [report/main.tex](report/main.tex). Готовый PDF создаётся
по пути `report/main.pdf`. Текст раздела расположен
в [report/sections/01_problem.tex](report/sections/01_problem.tex).
