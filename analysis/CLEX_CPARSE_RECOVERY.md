# M23 - recovered `clex.c` / `cparse.c` architecture

The Dec-1991 NT `CMD.EXE` retains COFF symbols and source-module attribution.
For the two parser modules the image identifies the following source contributions:

- `clex.c`: RVA `0x1680C` through `0x176B8`
- `cparse.c`: RVA `0x19C3C` through `0x1AD84`

The authoritative machine-code extracts are `NT1991_CLEX_DISASM.txt` and
`NT1991_CPARSE_DISASM.txt`.  `cmdparse.c` is a C89 semantic reconstruction,
not a claim that the original Microsoft C source has been recovered byte-for-byte.

## clex.c symbols

```text
InitLex      0x1680C
Lex          0x16838
TextCheck    0x16A60
GetByte      0x16BE0
UnGetByte    0x16C88
FillBuf      0x16C9C
LexCopy      0x170C8
PrintPrompt  0x17154
IsData       0x1734C
SubVar       0x17358
MSEnvVar     0x17634
```

## cparse.c symbols

```text
Parser          0x19C3C
ParseStatement  0x19CD0
ParseFor        0x19D0C
ParseIf         0x19E18
ParseDetach     0x19EE4
ParseRem        0x19F50
ParseS0         0x19FDC
ParseS1         0x1A010
ParseS2         0x1A02C
ParseS3         0x1A048
ParseS4         0x1A064
ParseS5         0x1A1F8
ParseCond       0x1A34C
ParseArgEqArg   0x1A440
ParseCmd        0x1A5DC
ParseRedir      0x1A718
BinaryOperator  0x1A8EC
BuildArgList    0x1A9F0
GetCheckStr     0x1AAD4
GeTexTok        0x1AAFC
GeToken         0x1AB68
LoadNodeTC      0x1ABCC
PError          0x1AC2C
PSError         0x1AC48
SpaceCat        0x1ACD4
```

## Recovered operator ladder

The calls from `ParseS0..ParseS3` pass literal operator strings to
`BinaryOperator`.  The literals are present in `.data` at the following RVAs:

```text
0x508E8  "&"
0x508EA  "||"
0x508ED  "&&"
0x508F0  "|"
```

This establishes the historical precedence directly:

```text
lowest   ParseS0   &
         ParseS1   ||
         ParseS2   &&
         ParseS3   |
         ParseS4   redirection
highest  ParseS5   command / special statement / (...) / @
```

`ParseS5` also compares the current text against `FOR`, `IF`, `DETACH`, and
`REM`; those strings are visible at RVAs `0x50228`, `0x50234`, `0x501C8/0x501D0`,
and `0x50278`.  Parenthesized command groups are explicit in the machine code.

## What M23 ports now

`cmdparse.c` keeps the recovered function/level names and implements:

- quote-aware metacharacter lexing;
- `%NAME%` environment substitution through the `SubVar`/`MSEnvVar` boundary;
- `@` command-prefix recognition;
- parenthesized groups;
- exact recovered precedence for `&`, `||`, `&&`, and `|`;
- `<`, `>`, and `>>` redirection nodes;
- REM consuming the remainder as comment data;
- parser error locations;
- a small AST consumed by `cmd32os2`.

M23 executes `&`, `&&`, `||`, groups, built-ins, and external LX programs.
Pipeline and redirection *parsing* is present, but their host-handle execution is
deliberately left for the next milestone.  `FOR`/`IF` are recognized at the
recovered `ParseS5` boundary but their batch semantics wait for the `cbatch.c`
lift.
