# sxceditor2

sxceditor2 is a simple, terminal-based text editor written in C. It provides basic text editing functionality with a vim-like interface.

## Features

- Vim-like modal editing (Normal, Insert, and Command modes)
- Basic file operations (open and save)
- Simple navigation using h, j, k, l keys
- Minimal terminal UI

## Building

To build sxceditor2, ensure you have a C compiler (like gcc) installed on your system. Then, compile the source code:

```
gcc -o sxceditor2 sxceditor2.c
```

## Usage

Run the editor:

```
./sxceditor2
```

### Modes

- Normal mode: Default mode for navigation
- Insert mode: For inserting text
- Command mode: For executing commands

### Commands

- `:open <filename>` - Open a file
- `:save <filename>` - Save the current file
- `:exit` or `:quit` or `:q` - Exit the editor

### Navigation

In normal mode:
- `h` - Move left
- `j` - Move down
- `k` - Move up
- `l` - Move right
- `i` - Enter insert mode
- `:` - Enter command mode

Press `Esc` to return to normal mode from insert or command mode.

## Limitations

- This is a basic implementation and lacks many features found in full-fledged text editors
- Limited error handling and edge case management
- No syntax highlighting or advanced editing features

## Contributing

Contributions to improve sxceditor2 are welcome. Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

MIT License