# e_crit
command line text editor with encryption

usage: compile the cpp file and run the resultant excecutable file giving the filename of the file you want to edit or
create as argument. This prompts you for the password which will be used to decrypt the file and encrypt it at the end of
editing. Re-enter the password and we are in the editor.

interface: there is a tilde at the start of a line which starts after a new line character, if we get to a new line simply
because of overflow, the line starts with a space.
Ctrl-s: save
Ctrl-q: quit

Ctrl-b: back one character.
Ctrl-f: forward one character.
Ctrl-n: go to next row keeping same column
Ctrl-p: go to previous row keeping same column.
