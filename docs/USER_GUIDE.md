# Baa User Guide

Welcome to Baa! This guide will help you write your first Arabic computer program.

## 1. Installation
Currently, you must build Baa from source.
1.  Download the source code.
2.  Run `cmake --build .` in the build directory.
3.  You will get `baa.exe`.

## 2. Your First Program
Create a file named `hello.b` (make sure to save it as UTF-8):


 ```baa
 // هذا برنامجي الأول
اطبع "مرحباً بالعالم".
 إرجع ٠.
 ```

## 3. Compiling
Open PowerShell and run:

```powershell
.\baa.exe hello.b
```

This creates `out.exe`.

## 4. Running
```powershell
.\out.exe
```

## 5. Troubleshooting
*   **"Lexer Error: Unknown char"**: Your file is not UTF-8. In VS Code, look at the bottom right bar, click the encoding, and select "Save with Encoding" -> "UTF-8".
```