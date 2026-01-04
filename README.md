# xedit

This is a lightweight, minimalistic text editor focused on writing code. It is based on the [slot pool](https://github.com/holysqualor/slot-pool) data structure, which provides extremely efficient memory usage. This editor uses lazy string initialization, which allows you to open large files instantly.


Cursor control is done with the arrow keys. UTF-8 is not supported.


## Compilation
```gcc src/main.c src/slpool.c include/slpool.h -o xedit -lncurses```
