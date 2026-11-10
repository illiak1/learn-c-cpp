# Content format

All lessons, exercises and exam tasks are plain text files that the app parses when it starts. Nothing is
compiled in, so you can add or fix content without rebuilding.

```
content/
  c/                      C track
    lessons/*.lesson      lecture notes
    exercises/*.exercises short questions (five types)
    exams/*.tasks         whole-function tasks and fixed exam sets
  cpp/                    C++ track (same layout; the tracks never mix)
```

Files are read in file-name order (`01_intro.lesson`, `02_pointers.lesson`, ...). Problems are collected, printed
to the console at start-up and checked by `make test`.

## Syntax rules (all file types)

* A line that starts with `@` in column 0 is a **directive**: `@name optional argument`.
* Every line after a directive, up to the next directive, is its **body**.
* Blank lines at the start and end of a body are dropped; indentation inside is kept.
* `@@` at the start of a line is a literal `@` (rarely needed in C/C++ code).
* `@rem` starts a comment; the directive and its body are ignored.
* Prose bodies (`@text`, `@note`, `@prompt`, `@hint`, `@explain`, `@description`) are **reflowed**: lines of one
  paragraph are joined with spaces and a blank line starts a new paragraph. Code-like bodies (`@code`, `@output`,
  `@stub`, `@solution`, `@tests`, `@expect`, `@main`) are kept verbatim.
* Lists (`@points`, `@mistakes`, `@checklist`) use `- item` lines; an indented line continues the previous item.

## Lessons: `*.lesson`

```
@lesson 01_intro            unique id (used by exercises and progress)
@title Intro to C
@summary One line shown on the lesson card.
@order 1                    sort key

@section 1.1 First program  "<id> <title>": first word is the section id
@text
Plain paragraphs.
@points
- Key point one.
- Key point two that wraps
  onto a second line.
@code c                     language: c, cpp or text (plain, for tables); optional: wrong, caption
#include <stdio.h>
int main() { return 0; }
@code cpp wrong Dangling pointer    "wrong" marks code that shows what NOT to do; the rest is a caption
@output Output (user types: b)      optional caption after the directive
What the program prints.
@mistakes
- Common mistake one.
@note
A highlighted remark.
```

Blocks inside a section are shown in the order they appear.

## Exercises: `*.exercises`

Each exercise starts with `@exercise <id>`; ids are unique per track.

| Directive | Meaning |
|-----------|---------|
| `@type` | `theory`, `choice`, `fill`, `predict` or `bug` |
| `@lesson` | id of the related lesson (optional; used for "practice this topic") |
| `@difficulty` | 1 (easy) to 3 (hard) |
| `@prompt` | the question text |
| `@code [lang]` | code shown with the question |
| `@answer` | accepted answers (see below) |
| `@choices` | options for `choice`: `- wrong`, `* correct` (exactly one `*`) |
| `@bugline` | for `bug`: line number(s) of the bug, 1-based, e.g. `7` or `7 8` (any of them counts) |
| `@hint` | optional hint, shown on request |
| `@explain` | explanation shown after answering |
| `@match` | options: `loose` (ignore all whitespace), `nocase`, `case`, `exact` |

### Answers and variants

* One `@answer` line = one answer. Variants are separated by `|`: `two | 2 | Two`.
  In `fill` exercises each line is the answer of one blank; in all other types the lines are joined into a single
  answer (so a multi-line program output can be written on several lines).
* To use a literal `|` in an answer write `\|`.
* Answers are compared after **normalising**: leading/trailing whitespace is removed and every run of whitespace
  (including newlines) becomes one space. `1 2 3` therefore equals `1\n2\n3`.
* `theory` exercises ignore letter case by default. `fill` and `bug` answers are code, so *all* whitespace is
  ignored (`i<n` equals `i < n`); use `@match exact` to switch that off, or `@match loose` to turn it on for another
  type.

### Per type

* **theory** - short text answer: `@prompt` + `@answer`.
* **choice** - `@prompt` + `@choices` (no `@answer`).
* **fill** - `@code` contains one `___` per blank; there is one `@answer` line per blank, in order. The app shows
  an input box inside the code at every blank. All blanks must be right.
* **predict** - `@code` + `@prompt`; `@answer` is the program output (may span lines).
* **bug** - `@code` is shown with line numbers; the student clicks the faulty line (`@bugline`). If `@answer` is
  given it is the corrected line and the student must type it too; with no `@answer` only the line counts.

## Tasks and exam sets: `*.tasks`

A **task** is one whole-function exercise (also used by exam mode):

```
@task c-is-leap-year
@title isLeapYear
@points 10
@lesson 01_intro
@statement                  verbatim text shown as the task description
@includes                   #include lines this task needs
#include <stdbool.h>
@stub                       what the student starts from
bool isLeapYear(int year) {
    // your code
}
@solution                   reference solution (shown in the review)
@tests                      statements placed inside main(); must be self-contained
printf("%d\n", isLeapYear(2016));
@expect                     output of the tests with a correct solution
1
@checklist                  what the review asks the student to confirm
- Handles years divisible by 100 but not by 400.
```

A **set** is a fixed exam made of tasks, in order:

```
@set c-exam1-full
@title Coding Exam 1 (original paper)
@minutes 90
@description
Optional text.
@tasks c-is-leap-year c-reversed ...
@main                       optional: a verbatim main() from the original paper
int main() { ... }
```

Exam mode builds one source file: all `@includes` (deduplicated), every task's `@stub`, then `main()` - the set's
`@main` if present, otherwise one `{ ... }` block per task holding its `@tests`. The student pastes that file into
Visual Studio, completes the functions and runs the tests. The app never compiles or runs code; the student marks
each task in the review.

## Progress file (`progress.txt`)

Not content, but stored next to the user's data: a tab-separated text file, one record per line. Exam results are
`exam<TAB>date<TAB>track<TAB>title<TAB>earned<TAB>total<TAB>seconds<TAB>fileRan[<TAB>HH:MM<TAB>id:earned/max;...]`.
The last two fields (time of day and per-task scores) were added later; files without them still load. Unreadable
lines are skipped (the app keeps a `progress.txt.bak` copy of a damaged file) and never stop the app from starting.
