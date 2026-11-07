# Решение задач линейного программирования симплекс-методом

Командный учебный проект СПбГЭТУ «ЛЭТИ»: C++20-приложение, тесты,
вычислительные эксперименты и единый отчёт в LaTeX.

Проект находится на стадии инициализации. Содержательные части выполняются
четырьмя агентами в отдельных feature-ветках; распределение приведено в
[`context/TASKS.md`](context/TASKS.md).

## Быстрый старт

Требования для приложения: CMake 3.20+, компилятор C++20 и CTest.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/simplex_cli
```

Требования для отчёта: LuaLaTeX, `latexmk`, Biber, пакеты из полного набора
TeX Live и установленный Times New Roman. Для Debian/Ubuntu обычно достаточно
`texlive-full latexmk biber`; Times New Roman устанавливается отдельно в
соответствии с лицензией Microsoft. Проверить доступность шрифта можно командой
`fc-match "Times New Roman"`.

```bash
cmake --build build --target report
# либо без CMake:
mkdir -p build/texmf-cache
TEXMFVAR="$PWD/build/texmf-cache" TEXMFCACHE="$PWD/build/texmf-cache" \
  latexmk -cd -lualatex -interaction=nonstopmode -halt-on-error report/main.tex
```

Результат сборки отчёта: `report/main.pdf`. Переменные титульного листа
заполняются в [`report/title.tex`](report/title.tex); неизвестные реквизиты
намеренно оставлены как параметры.

## Работа агента

Перед любым изменением прочитайте [`AGENTS.md`](AGENTS.md), общий контекст в
`context/` и свой журнал `context/agents/agent-N.md`. Работайте только в своей
feature-ветке и в своей зоне ответственности. По завершении обновите журнал и
`context/INTEGRATION.md`, сделайте обычный commit и передайте commit/ветку через
Git. Force push запрещён.

Python-эксперименты описаны в [`experiments/README.md`](experiments/README.md).
